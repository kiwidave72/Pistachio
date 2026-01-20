#pragma once

#include <memory>
#include <functional>

#include "adapters/ui/IGuiHost.h"

struct GLFWwindow;
struct ImFont;

namespace core { class Application; }

namespace adapters {

    class ImGuiAdapter {
    public:
        ImGuiAdapter(core::Application* app, GLFWwindow* hostWindow, IGuiHost* host);
        ~ImGuiAdapter();

        // NEW: must be called once, before first ImGui::NewFrame()
        void initializeResources();

        // Allow adapter UI code to supply window-chrome icons to the host.
        // This is safe across hot-reload because the host object is stable.
        void setWindowControlIcons(
            ImTextureID minimize,
            ImTextureID maximize,
            ImTextureID restore,
            ImTextureID close,
            ImVec2 size
        );

        void render();

        void setMenubarCallback(const std::function<void()>& menubarCallback);

    private:
        void renderMainMenu();
        void renderStatusBar();
        void renderModelInfo();
        void render3DView();
        void renderCameraGizmo();
        void renderSketchEditor();

    private:
        core::Application* m_app = nullptr;
        GLFWwindow* m_window = nullptr;
        IGuiHost* m_host = nullptr;

        bool m_resourcesInitialized = false; // NEW

        std::function<void()> m_MenubarCallback;

        // fonts
        ::ImFont* m_smallFont = nullptr;
        ImTextureID m_iconMinimize = nullptr;
        ImTextureID m_iconMaximize = nullptr;
        ImTextureID m_iconRestore = nullptr;
        ImTextureID m_iconClose = nullptr;
        unsigned int m_glTexMinimize = 0;
        unsigned int m_glTexMaximize = 0;
        unsigned int m_glTexRestore = 0;
        unsigned int m_glTexClose = 0;
        // rest of your existing members unchanged…
    };

} // namespace adapters
