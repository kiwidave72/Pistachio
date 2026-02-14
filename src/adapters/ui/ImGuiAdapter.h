#pragma once

#include <memory>
#include <functional>
#include <vector>

#include <glm/vec3.hpp>

#include "adapters/ui/IGuiHost.h"
#include "adapters/ui/SketchTooling.h"
#include "adapters/rendering/Sketch3DPlane.h"

struct GLFWwindow;
struct ImFont;

namespace ports { class IConfigPort; class IRendererPort; }

namespace core { class Application; }

namespace adapters {

    class ImGuiAdapter {
    public:
        // config is optional (may be nullptr). When provided, ImGuiAdapter will
        // persist view visibility state (close buttons / Views menu toggles).
        ImGuiAdapter(core::Application* app, GLFWwindow* hostWindow, IGuiHost* host, ports::IConfigPort* config = nullptr);
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

        // Fonts (Roboto) created by initializeResources()
        ::ImFont* fontBody() const { return m_bodyFont; }
        ::ImFont* fontGroup() const { return m_groupFont; }
        ::ImFont* fontTitle() const { return m_titleFont; }

    private:
        void renderMainMenu();
        void renderStatusBar();
        void renderModelInfo();
        void render3DView();
        void renderSketch3DView();
        void renderCameraGizmo();
        void renderSketchEditor();

    private:
        core::Application* m_app = nullptr;
        GLFWwindow* m_window = nullptr;
        IGuiHost* m_host = nullptr;
        ports::IConfigPort* m_config = nullptr;

        // New: second docked viewport renderer (kept separate from the app's primary renderer)
        std::unique_ptr<ports::IRendererPort> m_sketch3dRenderer;

        // View visibility (defaults to true; persisted via config when available)
        bool m_viewFileOperations = true;
        bool m_viewStatus = true;
        bool m_viewModelInfo = true;
        bool m_view3DViewport = true;
        bool m_viewSketch3DViewport = true;
        bool m_viewSketchEditor = true;

        // Sketch 3D View (Phase 3): choose which orthogonal plane the 2D sketch is shown on.
        adapters::Sketch3DPlane m_sketch3dActivePlane = adapters::Sketch3DPlane::XY;
        bool m_sketch3dShowOtherPlanes = true;
        bool m_sketch3dHasHoverPlane = false;
        adapters::Sketch3DPlane m_sketch3dHoverPlane = adapters::Sketch3DPlane::XY;


        struct Sketch3DSegment {
            adapters::Sketch3DPlane plane;
            glm::vec3 a;
            glm::vec3 b;
        };
        std::vector<Sketch3DSegment> m_sketch3dSegments;

        // Ray visualization for showing why a plane was selected
        glm::vec3 m_hoverRayOrigin{0,0,0};
        glm::vec3 m_hoverRayHitPoint{0,0,0};
        glm::vec3 m_hoverSegmentA{0,0,0};
        glm::vec3 m_hoverSegmentB{0,0,0};
        bool m_hasHoverRayData = false;


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
        ::ImFont* m_bodyFont = nullptr;
        ::ImFont* m_smallFont = nullptr;
        ::ImFont* m_groupFont = nullptr;
        ::ImFont* m_titleFont = nullptr;
        ImTextureID m_iconMinimize = nullptr;
        ImTextureID m_iconMaximize = nullptr;
        ImTextureID m_iconRestore = nullptr;
        ImTextureID m_iconClose = nullptr;
        unsigned int m_glTexMinimize = 0;
        unsigned int m_glTexMaximize = 0;
        unsigned int m_glTexRestore = 0;
        unsigned int m_glTexClose = 0;
        
        // Active viewport selection: whichever viewport window is focused / clicked receives camera/tool input.
        enum class ActiveViewport { None, Cube, Sketch3D };
        ActiveViewport m_activeViewport = ActiveViewport::None;

        // Last drawn 3D viewport image rect (screen-space). Used for overlays like the camera gizmo.
        ImVec2 m_viewportImageMin{0,0};
        ImVec2 m_viewportImageMax{0,0};
        bool   m_viewportImageValid = false;
// rest of your existing members unchanged…
    };

} // namespace adapters
