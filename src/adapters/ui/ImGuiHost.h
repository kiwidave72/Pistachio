
#pragma once
 

#include <functional>
#include "ports/IUIPort.h"
#include <memory>
#include <vector>
#include "imgui.h" 

#include "adapters/ui/IGuiHost.h"

// NOTE: ImGuiHost should prefer to depend on ImGui/GLFW only.
// Some legacy code still uses Walnut::Image for the app header icon.
#include "../../ImGui/Image.h"

#include "adapters/ui/SketchTooling.h"

struct GLFWwindow;

namespace adapters {

class ImGuiHost final : public ports::IUIPort, public IGuiHost {
public:
    ImGuiHost();
    ~ImGuiHost() override;

    bool initialize() override;
    void shutdown() override;
    bool shouldClose() override;
    void beginFrame() override;
    void endFrame() override;
    void render() override;
    void setMenubarCallback(const std::function<void()>& menubarCallback) override;

    // IGuiHost
    void setWindowControlIcons(
        ImTextureID minimize,
        ImTextureID maximize,
        ImTextureID restore,
        ImTextureID close,
        ImVec2 size
    ) override;

    // Diagnostics
    // When enabled, the host prints ImGui/GLFW/OpenGL information to stdout.
    // Useful when the UI is not visible and you need a text-only breadcrumb trail.
    void setStdoutDiagnostics(bool enabled) { m_diagStdout = enabled; }

    GLFWwindow* window() const { return m_window; }


private:
    void diagPrintInitState();
    void diagPrintFrameState(const char* stage);
    void diagCheckAndRestoreGlfwContext(const char* stage);
    void diagCheckOpenGLErrors(const char* stage);

    bool IsMaximized() const;

    GLFWwindow* m_window = nullptr;
    bool m_initialized = false;
    std::function<void()> m_menubarCallback;
    bool m_TitleBarHovered = false;

    bool IsTitleBarHovered() const { return m_TitleBarHovered; }
    // ---- diagnostics ----
    bool m_diagStdout = true;
    uint64_t m_frameIndex = 0;
    uint32_t m_diagOnceMask = 0;


    std::shared_ptr<Walnut::Image> m_AppHeaderIcon;

    // Window chrome icons (owned by adapter/plugin; host stores GPU handles only)
    ImTextureID m_iconMinimize = nullptr;
    ImTextureID m_iconMaximize = nullptr;
    ImTextureID m_iconRestore  = nullptr;
    ImTextureID m_iconClose    = nullptr;
    ImVec2      m_iconSize     = ImVec2(16.0f, 16.0f);
   
};

} // namespace adapters
