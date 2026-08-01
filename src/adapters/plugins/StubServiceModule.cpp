#ifdef _WIN32
#include <windows.h>
#endif

#include "adapters/plugins/ServiceModuleApi.h"
#include "core/IApplication.h"
#include "core/ServiceRegistry.h"
#include "ports/IEventBus.h"

#include <cstdio>

// -----------------------------------------------------------------------
// StubServiceModule
//
// Minimal hot-reload test plugin for the IServiceModule kind — mirrors
// StubUiModule's role (prove the loading mechanism works) but for
// services: no ImGui, no render(). onLoad() resolves IEventBus (already
// registered by Application::registerCoreServices() before this plugin
// loads) and publishes a test event, proving the whole chain works:
// registration -> load -> cross-boundary service resolution.
// -----------------------------------------------------------------------

#ifdef _WIN32
BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved)
{
    if (ul_reason_for_call == DLL_PROCESS_ATTACH)
        printf("[StubServiceModule] DllMain DLL_PROCESS_ATTACH\n");
    else if (ul_reason_for_call == DLL_PROCESS_DETACH)
        printf("[StubServiceModule] DllMain DLL_PROCESS_DETACH\n");
    return TRUE;
}
#endif

class StubServiceModule final : public IServiceModule
{
public:
    StubServiceModule() = default;
    ~StubServiceModule() override = default;

    void onLoad(core::IApplication& app) override
    {
        printf("[StubServiceModule] onLoad\n");

        auto* eventBus = app.services().resolve<ports::IEventBus>();
        if (eventBus)
        {
            m_subscriptionId = eventBus->subscribe("stub.ping", [](const std::string& payload)
                {
                    printf("[StubServiceModule] received stub.ping: %s\n", payload.c_str());
                });

            eventBus->publish("stub.ping", "hello from StubServiceModule::onLoad");
            printf("[StubServiceModule] resolved IEventBus and published/subscribed OK\n");
        }
        else
        {
            printf("[StubServiceModule] WARNING: could not resolve IEventBus\n");
        }

        printf("[StubServiceModule] onLoad done\n");
    }

    void onUnload(core::IApplication& app) override
    {
        printf("[StubServiceModule] onUnload\n");

        auto* eventBus = app.services().resolve<ports::IEventBus>();
        if (eventBus)
            eventBus->unsubscribe("stub.ping", m_subscriptionId);

        printf("[StubServiceModule] onUnload done\n");
    }

private:
    ports::SubscriptionId m_subscriptionId = 0;
};

// -----------------------------------------------------------------------
// Required exports
// -----------------------------------------------------------------------
extern "C" __declspec(dllexport)
IServiceModule* pistachio_create_service_module()
{
    printf("[StubServiceModule] pistachio_create_service_module called\n");
    return new StubServiceModule();
}

extern "C" __declspec(dllexport)
void pistachio_destroy_service_module(IServiceModule* m) { delete m; }

static const ServicePluginManifestV1 g_manifest = {
    sizeof(ServicePluginManifestV1), 1,
    "pistachio.service_stub", "Pistachio Service Stub", "0.1.0", "Services"
};

extern "C" __declspec(dllexport)
const ServicePluginManifestV1* pistachio_get_service_manifest() { return &g_manifest; }