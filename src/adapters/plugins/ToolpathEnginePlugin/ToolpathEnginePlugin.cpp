#ifdef _WIN32
#include <windows.h>
#endif

#include "adapters/plugins/ServiceModuleApi.h"
#include "core/IApplication.h"
#include "core/ServiceRegistry.h"
#include "domain/ModelCache.h"
#include "domain/WorkspaceStore.h"


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

        auto* modelCache = app.services().resolve<domain::v1::ModelCache>();
        auto* workspaceStore = app.services().resolve<domain::v1::WorkspaceStore>();

        printf("[ToolpathEngine] resolved ModelCache=%p WorkspaceStore=%p\n",
            (void*)modelCache, (void*)workspaceStore);

        // P0-P6 pipeline registration/setup goes here, once each phase exists.

        printf("[ToolpathEngine] onLoad done\n");
    }

    void onUnload(core::IApplication& app) override
    {
        printf("[ToolpathEngine] onUnload\n");
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