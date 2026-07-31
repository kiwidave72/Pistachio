#pragma once

#include "adapters/ui/IGuiHost.h"
#include "domain/DataContext.h"

#include <cstdint>

namespace core { class IApplication; }


// -----------------------------------------------------------------------
// UiHostServices
// Passed to every IUiModule callback.
// All pointers are host-owned and remain valid for the host's lifetime.
//
// Cast guide:
//   app      -> core::Application*
//   window   -> GLFWwindow*
//   guiHost  -> IGuiHost* (already typed)
//   config   -> ports::IConfigPort*
//   registry -> adapters::ContributionRegistry*
//              (#include "adapters/ui/ContributionRegistry.h" to use)
// -----------------------------------------------------------------------
struct UiHostServices
{
    void* app = nullptr;
    void* window = nullptr;
    IGuiHost* guiHost = nullptr;
    void* config = nullptr;
    void* registry = nullptr; // adapters::ContributionRegistry*
    void* taskRunner = nullptr;


    // NEW — typed access to the service registry. Prefer this for new code:
    //   svc.application->services().resolve<T>()
    // The fields above are kept for backwards compatibility; no plugin
    // needs to change to keep working.
    core::IApplication* application = nullptr;
};

// -----------------------------------------------------------------------
// IUiModule
// -----------------------------------------------------------------------
struct IUiModule
{
    virtual ~IUiModule() = default;

    // Called after LoadLibrary, before first NewFrame.
    virtual void onLoad(UiHostServices& svc,domain::DataContext& dataContext) = 0;

    // Called before FreeLibrary.
    // Cast registry and call removeAllContributions(pluginId).
    virtual void onUnload(UiHostServices& svc,domain::DataContext& dataContext) = 0;

    // Called every frame.
    virtual void render(UiHostServices& svc,domain::DataContext& dataContext) = 0;
};

// -----------------------------------------------------------------------
// Plugin manifest
// -----------------------------------------------------------------------
struct UiPluginManifestV1
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
#define PISTACHIO_UI_EXPORT extern "C" __declspec(dllexport)
#else
#define PISTACHIO_UI_EXPORT extern "C" __attribute__((visibility("default")))
#endif

PISTACHIO_UI_EXPORT IUiModule* pistachio_create_ui_module();
PISTACHIO_UI_EXPORT void                      pistachio_destroy_ui_module(IUiModule*);
PISTACHIO_UI_EXPORT const UiPluginManifestV1* pistachio_get_ui_manifest();