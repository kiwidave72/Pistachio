
#pragma once
#include <stdint.h>

struct UiHostServices {
    void* app;     // core::Application*
    void* window;  // GLFWwindow*
    void* guiHost; // host UI
    void* config;  // ports::IConfigPort* (host-owned)
};

struct IUiModule {
    virtual ~IUiModule() = default;
    virtual void onLoad(UiHostServices&) = 0;
    virtual void onUnload(UiHostServices&) = 0;
    virtual void render(UiHostServices&) = 0;
};

#ifdef _WIN32
#define PISTACHIO_UI_EXPORT extern "C" __declspec(dllexport)
#else
#define PISTACHIO_UI_EXPORT extern "C"
#endif

PISTACHIO_UI_EXPORT IUiModule* pistachio_create_ui_module();
PISTACHIO_UI_EXPORT void       pistachio_destroy_ui_module(IUiModule*);

struct UiPluginManifestV1 {
    uint32_t struct_size;
    uint32_t api_version;
    const char* id;
    const char* name;
    const char* version;
    const char* feature_group;
};

PISTACHIO_UI_EXPORT const UiPluginManifestV1* pistachio_get_ui_manifest();
