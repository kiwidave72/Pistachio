
#pragma once

struct UiHostServices {
    void* app;    // core::Application*
    void* window; // GLFWwindow*
    void* guiHost; // IGuiHost* (host-owned, stable across hot reload)
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
