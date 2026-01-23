#pragma once

#include <memory>
#include <functional>

#include "adapters/ui/IGuiHost.h"
#include "adapters/ui/SketchTooling.h"

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
        void renderRibbonBar();

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

        // --- 2D sketch tooling ---
        core::commands::CommandHistory m_cmdHistory;
        adapters::sketchui::ToolManager m_toolManager;

        // When a toolbar/ribbon button activates a tool, the same mouse click can be
        // observed by the canvas/tool update later in the frame (causing the newly
        // activated tool to immediately place/cancel). We skip one tool-update pass
        // after activation to avoid requiring a "double click".
        bool m_skipToolUpdateOnce = false;
        bool m_toolingInitialized = false;
        int m_activeSketchIndex = 0;
        int m_activeConstraintIcon = -1;
        bool m_sketchNeedsSolve = true;
        uint64_t m_sketchChangeSerial = 0;
        std::vector<domain::sketch::EntityId> m_uiPickedIds;
        domain::sketch::EntityId m_uiHoverId = 0;
        ImVec2 m_sketchPan{0,0};
        float m_sketchZoom = 40.0f;
        adapters::sketchui::Canvas2D m_canvas2D{};

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
