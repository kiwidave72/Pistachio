#pragma once
#include "ports/IUIPort.h"
#include <memory>
#include "imgui.h" 

struct GLFWwindow;

namespace core {
    class Application;
}

namespace adapters {

class ImGuiAdapter : public ports::IUIPort {
public:
    explicit ImGuiAdapter(core::Application* app);
    ~ImGuiAdapter() override;
    
    bool initialize() override;
    void shutdown() override;
    bool shouldClose() override;
    void beginFrame() override;
    void endFrame() override;
    void render() override;

private:
    GLFWwindow* m_window;
    core::Application* m_app;

    void renderMainMenu();
    void renderStatusBar();
    void renderModelInfo();
    void render3DView();
    void renderCameraGizmo();

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
    
    bool ImGuiAdapter::TrapeziumButtonTrueHit(
        const char* label,
        ImVec2 pos,
        ImVec2 size,
        float inset_x = 18.0f,
        bool short_edge_on_bottom = false);// false = short top, true = short bottom

       
};

} // namespace adapters