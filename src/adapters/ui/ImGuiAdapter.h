#pragma once
#include <functional>
#include "ports/IUIPort.h"
#include <memory>
#include <vector>
#include "imgui.h" 
#include "../../ImGui/Image.h"

#include "adapters/ui/SketchTooling.h"

// Command history lives in core/commands
#include "core/commands/CommandHistory.h"
struct GLFWwindow;

namespace core {
    class Application;
}

namespace adapters {

class ImGuiAdapter : public ports::IUIPort {
public:
    explicit ImGuiAdapter(core::Application* app);
    ~ImGuiAdapter() override;
    
    void setMenubarCallback(const std::function<void()>& menubarCallback);

    bool IsMaximized() const;

    bool initialize() override;
    void shutdown() override;
    bool shouldClose() override;
    void beginFrame() override;
    void endFrame() override;
    void render() override;

private:
    GLFWwindow* m_window;
    core::Application* m_app;
    
    bool m_TitleBarHovered = false;

    bool IsTitleBarHovered() const { return m_TitleBarHovered; }

    //std::shared_ptr<Image> GetApplicationIcon() const { return m_AppHeaderIcon; }
    std::function<void()> m_MenubarCallback;


    void renderMainMenu();
    void renderStatusBar();
    void renderModelInfo();
    void render3DView();
    void renderCameraGizmo();
    void renderSketchEditor();
    void UI_DrawTitlebar(float& outTitlebarHeight);
    void UI_DrawMenubar();

    void DrawViewport();

    char m_filePathBuffer[512];
    char m_exportPathBuffer[512];

    // Mouse interaction
    bool m_isRotating;
    bool m_isPanning;
    ImVec2 m_lastMousePos;
    
    bool HexButtonTrueHit(const char* label, float radius, bool pointy_top = false);
    bool ParallelogramButtonTrueHit(const char* label, ImVec2 size, float skew_x = 18.0f);
    bool ParallelogramButtonTrueHit(const char* label, ImVec2 pos, ImVec2 size, float skew_x = 18.0f);
    bool TrapeziumButtonTrueHit(const char* label, ImVec2 size, float top_inset_x = 18.0f);
    bool TrapeziumButtonTrueHit(const char* label, ImVec2 pos, ImVec2 size, float top_inset_x = 18.0f);
    bool RibbonButtonIconTextWithDropDown(
        const char* id,
        ImTextureID icon_tex,
        ImVec2 icon_size,
        const char* label,
        const char* const* items,
        int item_count,
        int* selected_index,
        ImVec2 size,
        float square_size
    );

    bool ImGuiAdapter::TrapeziumButtonTrueHit(
        const char* label,
        ImVec2 pos,
        ImVec2 size,
        float inset_x = 18.0f,
        bool short_edge_on_bottom = false);// false = short top, true = short bottom

    std::shared_ptr<Walnut::Image> m_AppHeaderIcon;
    std::shared_ptr<Walnut::Image> m_IconClose;
    std::shared_ptr<Walnut::Image> m_IconMinimize;
    std::shared_ptr<Walnut::Image> m_IconMaximize;
    std::shared_ptr<Walnut::Image> m_IconRestore;
    std::shared_ptr<Walnut::Image> m_ToolBarLineIcon;
    std::shared_ptr<Walnut::Image> m_ToolBarCircleIcon;
    std::shared_ptr<Walnut::Image> m_ToolBarArcIcon;
    std::shared_ptr<Walnut::Image> m_ToolBarRectIcon;
    ImFont* m_smallFont;

    // --- 2D sketch tooling ---
    core::commands::CommandHistory m_cmdHistory;
    adapters::sketchui::ToolManager m_toolManager;
    bool m_toolingInitialized = false;
    int m_activeSketchIndex = 0;
    int m_activeConstraintIcon = -1; // UI-only selection for constraint toolbar

    // Solve-on-dirty flags for sketch constraints
    bool m_sketchNeedsSolve = true;
    uint64_t m_sketchChangeSerial = 0;

    // UI selection highlight (hover + current pick sequence)
    std::vector<domain::sketch::EntityId> m_uiPickedIds;
    domain::sketch::EntityId m_uiHoverId = 0;


    // Canvas state (pan/zoom)
    ImVec2 m_sketchPan{ 0,0 };
    float  m_sketchZoom = 40.0f;
    adapters::sketchui::Canvas2D m_canvas2D{};

};

} // namespace adapters