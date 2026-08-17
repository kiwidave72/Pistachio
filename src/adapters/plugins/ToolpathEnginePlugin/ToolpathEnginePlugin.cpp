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

#include "adapters/plugins/ToolpathEnginePlugin/LayerBitmapDebug.h"

#include "adapters/plugins/ToolpathEnginePlugin/WallGenerationPhase.h"
#include "adapters/plugins/ToolpathEnginePlugin/RectilinearInfillStrategy.h"
#include "adapters/plugins/ToolpathEnginePlugin/InfillRegionPhase.h"

#include "domain/ToolpathSerialization.h"
#include "domain/ToolpathBinaryIO.h"



#include <cstdio>

// -----------------------------------------------------------------------
// ToolpathEnginePlugin
//
// Skeleton only — no P0-P6 pipeline logic yet. Proves the plugin loads
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

        kinetica::ImportPhase importPhase(*m_modelCache, *m_config);
        auto geometry = importPhase.run(plate);

        printf("[ToolpathEngine] P0 produced %zu UnifiedGeometry objects\n", geometry.size());

        kinetica::ValidationPhase validationPhase;
        auto validated = validationPhase.run(std::move(geometry));

        int cleanCount = 0;
        for (auto& v : validated)
            if (v.report.isClean()) ++cleanCount;

        printf("[ToolpathEngine] P1 complete: %d/%zu instances clean\n", cleanCount, validated.size());


        kinetica::AccelerationPhase accelerationPhase(*m_config);
        auto accelerated = accelerationPhase.run(std::move(validated));

        printf("[ToolpathEngine] P2 complete: %zu instances accelerated\n", accelerated.size());

        kinetica::SlicingPhase slicingPhase(*m_config);
        auto sliced = slicingPhase.run(std::move(accelerated));

        printf("[ToolpathEngine] P3 complete: %zu instances sliced\n", sliced.size());


        kinetica::ExtractionPhase extractionPhase;
        auto extracted = extractionPhase.run(std::move(sliced));

         printf("[ToolpathEngine] P4 complete: %zu instances extracted\n", extracted.size());
        if (!extracted.empty())
            printf("[ToolpathEngine]   instance 0 has %zu layers\n", extracted[0].layers.size());


        kinetica::TopologyPhase topologyPhase;
        auto topologized = topologyPhase.run(extracted);

        printf("[ToolpathEngine] P5 complete: %zu instances topologized\n", topologized.size());

        domain::v1::Toolpath toolpath;  
        for (auto& topoInst : topologized)
        {
            auto& layers = topoInst.topology.layers;
            toolpath.layers.resize(layers.size());   // pre-sized — rule #1, no push_back across threads

            auto parallelTiming = core::parallelFor(layers.size(), [&](size_t start, size_t end)
                {
                    // Fresh instances per chunk — rule #3, sidesteps the "is this class
                    // genuinely stateless between calls" question entirely rather than
                    // needing to prove it.
                    kinetica::WallGenerationPhase wallGenerationPhase(*m_config);
                    kinetica::RectilinearInfillStrategy rectilinearInfill;

                    for (size_t i = start; i < end; ++i)
                    {
                        auto& topoLayer = layers[i];

                        domain::v1::ToolpathLayer tpLayer;
                        tpLayer.z = topoLayer.z;

                        auto wallResult = wallGenerationPhase.run(topoLayer);
                        tpLayer.segments = wallResult.segments;

                        auto infillRegion = kinetica::InfillRegionPhase::run(wallResult);
                        auto infillSegments = rectilinearInfill.generate(infillRegion, wallResult, topoLayer.z, *m_config);
                        tpLayer.segments.insert(tpLayer.segments.end(), infillSegments.begin(), infillSegments.end());

                        toolpath.layers[i] = std::move(tpLayer);   // indexed write — safe, no shared mutable state
                    }
                });

            printf("[ToolpathEngine] P6 wall+infill (parallel): %.1fms\n", parallelTiming.milliseconds);
        }
        //kinetica::WallGenerationPhase wallGenerationPhase(*m_config);
        //kinetica::RectilinearInfillStrategy rectilinearInfill;

        //domain::v1::Toolpath toolpath;   // single-toolhead scope — one Toolpath, no ToolheadToolpath wrapper yet
        //int totalWallSegments = 0;
       
        //
        //int totalInfillSegments = 0;
        //int totalHolesFilteredAsSpurious = 0;
       

        //for (auto& topoInst : topologized)
        //{
        //    toolpath.layers.clear();   // TEMP: currently overwrites per-instance — multi-instance
        //    // Toolpath assembly not designed yet, single-instance
        //    // testing only for now

        //    for (auto& topoLayer : topoInst.topology.layers)
        //    {
        //        domain::v1::ToolpathLayer tpLayer;
        //        tpLayer.z = topoLayer.z;

        //        auto wallResult = wallGenerationPhase.run(topoLayer);
        //        tpLayer.segments = wallResult.segments;
        //        totalWallSegments += (int)wallResult.segments.size();

        //        auto infillRegion = kinetica::InfillRegionPhase::run(wallResult);
        //        totalHolesFilteredAsSpurious += infillRegion.holesFilteredAsSpurious;

        //        auto infillSegments = rectilinearInfill.generate(infillRegion, wallResult, topoLayer.z, *m_config);
        //        tpLayer.segments.insert(tpLayer.segments.end(), infillSegments.begin(), infillSegments.end());
        //        totalInfillSegments += (int)infillSegments.size();

        //        tpLayer.comments.push_back("P6: walls + infill, no skin yet");

        //        toolpath.layers.push_back(std::move(tpLayer));
        //    }
        //}

       /* printf("[ToolpathEngine] P6 (walls only, partial) complete: %d layers, %d wall segments total\n",
            (int)toolpath.layers.size(), totalWallSegments);


        printf("[ToolpathEngine] P6 (walls + infill) complete: %d layers, %d wall segments, %d infill segments, %d holes filtered\n",
            (int)toolpath.layers.size(), totalWallSegments, totalInfillSegments, totalHolesFilteredAsSpurious);
        */

        if (m_toolPathStore)
            m_toolPathStore->set(toolpath);   // publishes "toolpath.updated" internally, no payload needed

        m_eventBus->publish("toolpath.updated", "");

        if (m_taskRunner)
        {
            auto extractedCopy = extracted;        // full copies — safe, nothing else will touch these again
            auto topologizedCopy = topologized;
            auto toolpathCopy = toolpath;
            m_taskRunner->submit(
                [extractedCopy, topologizedCopy, toolpathCopy](std::shared_ptr<TaskProgress>) {
                    //kinetica::LayerBitmapDebug::writeRunReport(extractedCopy, topologizedCopy, "C:\\temp\\layer_debug");
                    
                    //domain::v1::saveToolpathToFile(toolpathCopy, "C:\\temp\\layer_debug\\toolpath.json");
                    domain::v1::saveToolpathBinary(toolpathCopy, "C:\\temp\\layer_debug\\toolpath.bin");
                    //for (size_t i = 0; i < toolpathCopy.layers.size(); ++i)
                    //    kinetica::LayerBitmapDebug::dumpToolpathLayer(toolpathCopy.layers[i], (int)i, "C:\\temp\\layer_debug", "wall_test", 1024);
                    //kinetica::LayerBitmapDebug::dumpAllTopologyLayers(topologizedCopy, "C:\\temp\\layer_debug");
                    //kinetica::LayerBitmapDebug::dumpAllChainLayers(extractedCopy, "C:\\temp\\layer_debug");

                },
                "Debug PNG dump", false, false, false);
        }
        else
        {
            kinetica::LayerBitmapDebug::writeRunReport(extracted, topologized, "C:\\temp\\layer_debug");
            for (size_t i = 0; i < toolpath.layers.size(); ++i)
                kinetica::LayerBitmapDebug::dumpToolpathLayer(toolpath.layers[i], (int)i, "C:\\temp\\layer_debug", "wall_test", 1024);
            
            //kinetica::LayerBitmapDebug::dumpAllTopologyLayers(topologized, "C:\\temp\\layer_debug");
            //kinetica::LayerBitmapDebug::dumpAllChainLayers(extracted, "C:\\temp\\layer_debug");

        }
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