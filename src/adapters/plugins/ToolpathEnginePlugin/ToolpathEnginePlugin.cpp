#ifdef _WIN32
#include <windows.h>
#endif

#include "adapters/plugins/ServiceModuleApi.h"
#include "core/TaskRunner.h"
#include "core/IApplication.h"
#include "core/ServiceRegistry.h"
#include "core/ParallelFor.h"

#include "domain/ModelCache.h"
#include "domain/WorkspaceStore.h"
#include "domain/ToolpathStore.h"
#include "ports/IConfigPort.h"
#include "ports/IEventBus.h"
#include "adapters/plugins/ToolpathEnginePlugin/ImportPhase.h"
#include "adapters/plugins/ToolpathEnginePlugin/ValidationPhase.h"
#include "adapters/plugins/ToolpathEnginePlugin/AccelerationPhase.h"
#include "adapters/plugins/ToolpathEnginePlugin/SlicingPhase.h"
#include "adapters/plugins/ToolpathEnginePlugin/ExtractionPhase.h"
#include "adapters/plugins/ToolpathEnginePlugin/TopologyPhase.h"

#include "adapters/plugins/ToolpathEnginePlugin/TopologyRepairPhase.h"

#include "adapters/plugins/ToolpathEnginePlugin/LayerBitmapDebug.h"

#include "adapters/plugins/ToolpathEnginePlugin/WallGenerationPhase.h"
#include "adapters/plugins/ToolpathEnginePlugin/RectilinearInfillStrategy.h"
#include "adapters/plugins/ToolpathEnginePlugin/InfillRegionPhase.h"

#include "domain/ToolpathSerialization.h"
#include "domain/ToolpathBinaryIO.h"
#include "domain/WallGenerationResult.h"



#include <cstdio>
#include <algorithm>
#include <memory>
#include <atomic>

// -----------------------------------------------------------------------
// ToolpathEnginePlugin
//
// Skeleton only � no P0-P6 pipeline logic yet. Proves the plugin loads
// and can resolve the services it will actually need (ModelCache,
// WorkspaceStore) before any real algorithm code is written, same
// discipline used for pistachio_service_stub.
// -----------------------------------------------------------------------

#ifdef _WIN32
BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved)
{
    if (ul_reason_for_call == DLL_PROCESS_ATTACH)
        printf("[ToolpathEngine] DllMain DLL_PROCESS_ATTACH\n");
    else if (ul_reason_for_call == DLL_PROCESS_DETACH)
        printf("[ToolpathEngine] DllMain DLL_PROCESS_DETACH\n");
    return TRUE;
}
#endif

namespace
{
    // Holds all per-run pipeline state. Owned via shared_ptr and captured
    // BY VALUE into every step lambda, so its lifetime spans the async
    // TaskGroup run regardless of when onRunPipeline() itself returns.
    // (Previously these were locals in onRunPipeline captured by reference
    // -- submit() is async, so onRunPipeline's stack frame was gone by the
    // time later steps ran, and steps were writing through dangling
    // references into reclaimed stack memory. That's what caused the
    // STATUS_STACK_BUFFER_OVERRUN / heap corruption on step 1's move-assign.)
    struct InstanceWallData
    {
        // Per-layer wall results for one model instance, same indexing as
        // topologized[instIdx].topology.layers. Bridges the Wall Generation
        // step to the Infill step.
        std::vector<domain::v1::WallGenerationResult> wallResults;
    };

    struct PipelineContext
    {
        std::vector<domain::v1::UnifiedGeometry> geometry;
        std::vector<kinetica::ValidatedGeometry> validated;
        std::vector<kinetica::AcceleratedGeometry> accelerated;
        std::vector<kinetica::SlicedGeometry> sliced;
        std::vector<kinetica::ExtractedGeometry> extracted;
        std::vector<kinetica::TopologizedGeometry> topologized;
        kinetica::TopologyRepairResult repairResult;
        std::vector<InstanceWallData> wallData;
        domain::v1::Toolpath toolpath;
    };
}

class ToolpathEnginePlugin final : public IServiceModule
{
public:
    ToolpathEnginePlugin() = default;
    ~ToolpathEnginePlugin() override = default;

    void onLoad(core::IApplication& app) override
    {
        printf("[ToolpathEngine] onLoad\n");

        m_modelCache = app.services().resolve<domain::v1::ModelCache>();
        m_workspaceStore = app.services().resolve<domain::v1::WorkspaceStore>();
        m_config = app.services().resolve<ports::IConfigPort>();
        m_eventBus = app.services().resolve<ports::IEventBus>();
        m_taskRunner = app.services().resolve<TaskRunner>();

        printf("[ToolpathEngine] resolved ModelCache=%p WorkspaceStore=%p IConfigPort=%p IEventBus=%p\n",
            (void*)m_modelCache, (void*)m_workspaceStore, (void*)m_config, (void*)m_eventBus);

        m_toolPathStore = app.services().resolve<domain::v1::ToolpathStore>();


        if (m_eventBus && m_modelCache && m_workspaceStore && m_config)
        {
            // payload = buildPlateId. Fired by the UI (SlicerCorePlugin)
            // whenever the user actually triggers a slice, not automatic.
            m_eventBus->subscribe("run.pipeline", [this](const std::string& buildPlateId)
                {
                    onRunPipeline(buildPlateId);
                });


        }

        printf("[ToolpathEngine] onLoad done\n");
    }

    void onUnload(core::IApplication& app) override
    {
        printf("[ToolpathEngine] onUnload\n");
    }



private:
    domain::v1::ModelCache* m_modelCache = nullptr;
    domain::v1::WorkspaceStore* m_workspaceStore = nullptr;
    domain::v1::ToolpathStore* m_toolPathStore = nullptr;
    ports::IConfigPort* m_config = nullptr;
    ports::IEventBus* m_eventBus = nullptr;
    TaskRunner* m_taskRunner = nullptr;

    void onRunPipeline(const std::string& buildPlateId)
    {
        domain::v1::BuildPlate* plate = nullptr;

        m_workspaceStore->read([&](const domain::v1::Workspace& ws)
            {
                for (auto* project : ws.projects)
                {
                    if (!project) continue;
                    for (auto* bp : project->buildPlates)
                    {
                        if (bp && bp->Id == buildPlateId) { plate = bp; return; }
                    }
                }
            });

        if (!plate)
        {
            printf("[ToolpathEngine] run.pipeline: no BuildPlate found for id '%s'\n", buildPlateId.c_str());
            return;
        }

        printf("[ToolpathEngine] run.pipeline: running P0 against plate '%s' (%zu instances)\n",
            buildPlateId.c_str(), plate->modelInstances.size());

        // Heap-allocated, ref-counted pipeline state -- see PipelineContext
        // comment above for why this replaced stack locals + reference
        // captures. Every step captures [this, plate, ctx] / [this, ctx]
        // (ctx by value: cheap shared_ptr copy) and reads/writes ctx->xxx.
        auto ctx = std::make_shared<PipelineContext>();

        m_taskRunner->group("Slicing Pipeline")
            .sequential()
            .stopOnFailure(true)
            .step("Import Phase", [this, plate, ctx](std::shared_ptr<TaskProgress> progress)
                {
                    kinetica::ImportPhase importPhase(*m_modelCache, *m_config);
                    ctx->geometry = importPhase.run(plate);
                    printf("[ToolpathEngine] P0 produced %zu UnifiedGeometry objects\n", ctx->geometry.size());
                })
            .step("Validation Phase", [this, ctx](std::shared_ptr<TaskProgress> progress) {

            kinetica::ValidationPhase validationPhase;
            ctx->validated = validationPhase.run(std::move(ctx->geometry));
            int cleanCount = 0;
            for (auto& v : ctx->validated)
                if (v.report.isClean()) ++cleanCount;

            printf("[ToolpathEngine] P1 complete: %d/%zu instances clean\n", cleanCount, ctx->validated.size());
                })
            .step("Acceleration Phase", [this, ctx](std::shared_ptr<TaskProgress> progress)
                {
                    kinetica::AccelerationPhase accelerationPhase(*m_config);
                    ctx->accelerated = accelerationPhase.run(std::move(ctx->validated));

                    printf("[ToolpathEngine] P2 complete: %zu instances accelerated\n", ctx->accelerated.size());
                })
            .step("Slicing Phase", [this, ctx](std::shared_ptr<TaskProgress> progress)
                {
                    kinetica::SlicingPhase slicingPhase(*m_config);
                    ctx->sliced = slicingPhase.run(std::move(ctx->accelerated));

                    printf("[ToolpathEngine] P3 complete: %zu instances sliced\n", ctx->sliced.size());
                })
            .step("Extraction Phase", [this, ctx](std::shared_ptr<TaskProgress> progress)
                {
                    kinetica::ExtractionPhase extractionPhase;
                    ctx->extracted = extractionPhase.run(std::move(ctx->sliced));

                    printf("[ToolpathEngine] P4 complete: %zu instances extracted\n", ctx->extracted.size());
                    if (!ctx->extracted.empty())
                        printf("[ToolpathEngine]   instance 0 has %zu layers\n", ctx->extracted[0].layers.size());
                })
            .step("Topology Phase", [this, ctx](std::shared_ptr<TaskProgress> progress) {

            kinetica::TopologyPhase topologyPhase;
            ctx->topologized = topologyPhase.run(ctx->extracted);

            printf("[ToolpathEngine] P5 complete: %zu instances topologized\n", ctx->topologized.size());
                })
            .step("Topology Repair Phase", [this, ctx](std::shared_ptr<TaskProgress> progress)
                {
                    kinetica::TopologyRepairPhase topologyRepairPhase;
                    ctx->repairResult = topologyRepairPhase.run(std::move(ctx->topologized));
                    ctx->topologized = std::move(ctx->repairResult.geometry);

                    printf("[ToolpathEngine] P5R complete: %d repaired, %d flagged-but-not-repaired\n",
                        ctx->repairResult.report.totalRepaired, ctx->repairResult.report.totalFlaggedNotRepaired);
                })
            .step("Wall Generation Phase", [this, ctx](std::shared_ptr<TaskProgress> progress)
                {
                    size_t totalLayers = 0;
                    for (auto& topoInst : ctx->topologized)
                        totalLayers += topoInst.topology.layers.size();

                    ctx->wallData.clear();
                    ctx->wallData.resize(ctx->topologized.size());

                    std::atomic<size_t> completed{ 0 };
                    progress->fraction.store(totalLayers > 0 ? 0.f : 1.f);
                    progress->setMessage(totalLayers > 0
                        ? ("Processing 0 / " + std::to_string(totalLayers))
                        : "No layers to process");

                    for (size_t instIdx = 0; instIdx < ctx->topologized.size(); ++instIdx)
                    {
                        auto& topoInst = ctx->topologized[instIdx];
                        auto& layers = topoInst.topology.layers;
                        auto& wallResults = ctx->wallData[instIdx].wallResults;
                        wallResults.resize(layers.size());

                        auto parallelTiming = core::parallelFor(layers.size(), [&](size_t start, size_t end)
                            {
                                // Fresh instance per chunk � rule #3, sidesteps the "is this class
                                // genuinely stateless between calls" question entirely rather than
                                // needing to prove it.
                                kinetica::WallGenerationPhase wallGenerationPhase(*m_config);

                                for (size_t i = start; i < end; ++i)
                                {
                                    wallResults[i] = wallGenerationPhase.run(layers[i]);

                                    // Progress: safe from any worker thread -- fraction is atomic,
                                    // setMessage is separately mutex-guarded. totalLayers counts
                                    // work units (instance-layers), not distinct Z-heights.
                                    size_t done = completed.fetch_add(1, std::memory_order_relaxed) + 1;
                                    progress->fraction.store(
                                        totalLayers > 0 ? static_cast<float>(done) / static_cast<float>(totalLayers) : 1.f,
                                        std::memory_order_relaxed);
                                    progress->setMessage("Processing " + std::to_string(done) + " / " + std::to_string(totalLayers));
                                }
                            });

                        printf("[ToolpathEngine] P6a wall generation (parallel): %.1fms\n", parallelTiming.milliseconds);
                    }
                })
            .step("Infill Phase", [this, ctx](std::shared_ptr<TaskProgress> progress)
                {
                    size_t maxLayerCount = 0;
                    size_t totalLayers = 0;
                    for (auto& topoInst : ctx->topologized)
                    {
                        maxLayerCount = (std::max)(maxLayerCount, topoInst.topology.layers.size());
                        totalLayers += topoInst.topology.layers.size();
                    }

                    domain::v1::Toolpath& toolpath = ctx->toolpath;
                    toolpath.layers.resize(maxLayerCount);

                    std::atomic<size_t> completed{ 0 };
                    progress->fraction.store(totalLayers > 0 ? 0.f : 1.f);
                    progress->setMessage(totalLayers > 0
                        ? ("Processing 0 / " + std::to_string(totalLayers))
                        : "No layers to process");

                    for (size_t instIdx = 0; instIdx < ctx->topologized.size(); ++instIdx)
                    {
                        auto& topoInst = ctx->topologized[instIdx];
                        auto& layers = topoInst.topology.layers;
                        auto& wallResults = ctx->wallData[instIdx].wallResults;

                        std::vector<domain::v1::ToolpathLayer> instanceLayers(layers.size());

                        auto parallelTiming = core::parallelFor(layers.size(), [&](size_t start, size_t end)
                            {
                                kinetica::RectilinearInfillStrategy rectilinearInfill;

                                for (size_t i = start; i < end; ++i)
                                {
                                    auto& topoLayer = layers[i];
                                    auto& wallResult = wallResults[i];

                                    domain::v1::ToolpathLayer tpLayer;
                                    tpLayer.z = topoLayer.z;
                                    tpLayer.segments = wallResult.segments;

                                    auto infillRegion = kinetica::InfillRegionPhase::run(wallResult);
                                    auto infillSegments = rectilinearInfill.generate(infillRegion, wallResult, topoLayer.z, *m_config);
                                    tpLayer.segments.insert(tpLayer.segments.end(), infillSegments.begin(), infillSegments.end());

                                    instanceLayers[i] = std::move(tpLayer);   // indexed write into this instance's OWN buffer � safe, no shared mutable state

                                    // Progress: same pattern as Wall Generation step above.
                                    size_t done = completed.fetch_add(1, std::memory_order_relaxed) + 1;
                                    progress->fraction.store(
                                        totalLayers > 0 ? static_cast<float>(done) / static_cast<float>(totalLayers) : 1.f,
                                        std::memory_order_relaxed);
                                    progress->setMessage("Processing " + std::to_string(done) + " / " + std::to_string(totalLayers));
                                }
                            });

                        printf("[ToolpathEngine] P6b infill (parallel): %.1fms\n", parallelTiming.milliseconds);

                        // Merge (append) this instance's segments into the shared
                        // toolpath � not a write/overwrite � so multiple parts at
                        // the same layer index both end up represented.
                        for (size_t i = 0; i < instanceLayers.size(); ++i)
                        {
                            if (toolpath.layers[i].segments.empty())
                                toolpath.layers[i].z = instanceLayers[i].z;
                            toolpath.layers[i].segments.insert(
                                toolpath.layers[i].segments.end(),
                                instanceLayers[i].segments.begin(),
                                instanceLayers[i].segments.end());
                        }
                    }

                    if (m_toolPathStore)
                        m_toolPathStore->set(toolpath);   // publishes "toolpath.updated" internally, no payload needed
                })
            .completed([this](bool success) {
            if (success)
            {
                printf("[ToolpathEngine] run.pipeline completed successfully\n");
                m_eventBus->publish("toolpathStore.updated", "");
            }
            else
            {
                printf("[ToolpathEngine] run.pipeline failed\n");
            }
                })
            .submit();


        //auto extractedCopy = ctx->extracted;        // full copies � safe, nothing else will touch these again
        //auto topologizedCopy = ctx->topologized;
        //auto toolpathCopy = ctx->toolpath;
        //m_taskRunner->submit(
        //    [extractedCopy, topologizedCopy, toolpathCopy](std::shared_ptr<TaskProgress>) {
        //        kinetica::LayerBitmapDebug::writeRunReport(extractedCopy, topologizedCopy, "C:\\temp\\layer_debug");

        //        //domain::v1::saveToolpathToFile(toolpathCopy, "C:\\temp\\layer_debug\\toolpath.json");
        //        domain::v1::saveToolpathBinary(toolpathCopy, "C:\\temp\\layer_debug\\toolpath.bin");
        //        //for (size_t i = 0; i < toolpathCopy.layers.size(); ++i)
        //        //   kinetica::LayerBitmapDebug::dumpToolpathLayer(toolpathCopy.layers[i], (int)i, "C:\\temp\\layer_debug", "wall_test", 1024);
        //        //kinetica::LayerBitmapDebug::dumpAllTopologyLayers(topologizedCopy, "C:\\temp\\layer_debug");
        //        //kinetica::LayerBitmapDebug::dumpAllChainLayers(extractedCopy, "C:\\temp\\layer_debug");

        //    },
        //    "Debug PNG dump", false, false, false);

    }


};

// -----------------------------------------------------------------------
// Required exports
// -----------------------------------------------------------------------
extern "C" __declspec(dllexport)
IServiceModule* pistachio_create_service_module()
{
    printf("[ToolpathEngine] pistachio_create_service_module called\n");
    return new ToolpathEnginePlugin();
}

extern "C" __declspec(dllexport)
void pistachio_destroy_service_module(IServiceModule* m) { delete m; }

static const ServicePluginManifestV1 g_manifest = {
    sizeof(ServicePluginManifestV1), 1,
    "pistachio.toolpath_engine", "Toolpath Engine", "0.1.0", "Slicer"
};

extern "C" __declspec(dllexport)
const ServicePluginManifestV1* pistachio_get_service_manifest() { return &g_manifest; }