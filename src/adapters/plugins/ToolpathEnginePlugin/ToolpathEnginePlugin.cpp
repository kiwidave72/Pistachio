#ifdef _WIN32
#include <windows.h>
#endif

#include "adapters/plugins/ServiceModuleApi.h"
#include "core/IApplication.h"
#include "core/ServiceRegistry.h"
#include "domain/ModelCache.h"
#include "domain/WorkspaceStore.h"
#include "ports/IConfigPort.h"
#include "ports/IEventBus.h"
#include "adapters/plugins/ToolpathEnginePlugin/ImportPhase.h"
#include "adapters/plugins/ToolpathEnginePlugin/ValidationPhase.h"


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

        printf("[ToolpathEngine] resolved ModelCache=%p WorkspaceStore=%p IConfigPort=%p IEventBus=%p\n",
            (void*)m_modelCache, (void*)m_workspaceStore, (void*)m_config, (void*)m_eventBus);

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
    ports::IConfigPort* m_config = nullptr;
    ports::IEventBus* m_eventBus = nullptr;

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