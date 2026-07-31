#pragma once

// -----------------------------------------------------------------------
// ServiceModuleApi.h
//
// Mirrors adapters/ui/plugins/UiModuleApi.h's shape (IUiModule /
// UiPluginManifestV1 / DLL export macros), but for the "service plugin"
// kind: no ImGui dependency, no render(). A service plugin's only job is
// to register services into Application's ServiceRegistry.
//
// Loading order (once the two-phase loader exists):
//   Phase 1 — every IServiceModule loads and registers its services.
//   Phase 2 — every IUiModule loads and resolves whatever it needs.
//
// This file is intentionally not wired into the plugin loader yet.
// Creating the interface now; the loader change and the first real
// service plugin are separate follow-up work.
// -----------------------------------------------------------------------

#include "core/IApplication.h"

#include <cstdint>

// -----------------------------------------------------------------------
// IServiceModule
// -----------------------------------------------------------------------
struct IServiceModule
{
    virtual ~IServiceModule() = default;

    // Called once, during the service-loading phase, before any IUiModule
    // loads. Register services into app.services() here.
    virtual void onLoad(core::IApplication& app) = 0;

    // Called before the module is unloaded. Unregister anything this
    // module registered, if it needs to be torn down cleanly.
    virtual void onUnload(core::IApplication& app) = 0;
};

// -----------------------------------------------------------------------
// Plugin manifest — same shape as UiPluginManifestV1, kept as a distinct
// type so the loader can tell the two plugin kinds apart at load time.
// -----------------------------------------------------------------------
struct ServicePluginManifestV1
{
    uint32_t    struct_size = 0;
    uint32_t    api_version = 0;
    const char* id = nullptr;
    const char* name = nullptr;
    const char* version = nullptr;
    const char* feature_group = nullptr;
};

// -----------------------------------------------------------------------
// DLL exports
// -----------------------------------------------------------------------
#ifdef _WIN32
#define PISTACHIO_SERVICE_EXPORT extern "C" __declspec(dllexport)
#else
#define PISTACHIO_SERVICE_EXPORT extern "C" __attribute__((visibility("default")))
#endif

PISTACHIO_SERVICE_EXPORT IServiceModule* pistachio_create_service_module();
PISTACHIO_SERVICE_EXPORT void                          pistachio_destroy_service_module(IServiceModule*);
PISTACHIO_SERVICE_EXPORT const ServicePluginManifestV1* pistachio_get_service_manifest();