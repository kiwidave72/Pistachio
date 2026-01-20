#include "ImGuiHost.h"

#include "../ImGui/ImGuiTheme.h"
#include "../ImGui/Image.h"


#include <imgui.h>
#include "imgui_internal.h"

#include <backends/imgui_impl_glfw.h>
#include <backends/imgui_impl_opengl3.h>
#include "../Roboto-Regular.embed"
#include "../../../Walnut-Icon.embed"
#include "../../../WindowImages.embed"

#include <GLFW/glfw3.h>
#include "stb_image.h"

#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3native.h>

#include <cstdio>
#include <cassert>


// HostUI.h (or at top of ImGuiHost.cpp)
#pragma once
#include <imgui.h>

namespace HostUI
{

    struct HorizontalLayout
    {
        float startX;
        float cursorY;
        float rightX;
    };
    inline HorizontalLayout BeginHorizontal()
    {
        HorizontalLayout l;
        l.startX = ImGui::GetCursorPosX();
        l.cursorY = ImGui::GetCursorPosY();
        l.rightX = ImGui::GetWindowContentRegionMax().x;

        return l;
    }

    inline void EndHorizontal(const HorizontalLayout&)
    {
        // No-op, kept for symmetry
    }

    inline void ShiftCursorY(float dy)
    {
        ImGui::SetCursorPosY(ImGui::GetCursorPosY() + dy);
    }

    inline ImRect GetItemRect()
    {
        return ImRect(ImGui::GetItemRectMin(), ImGui::GetItemRectMax());
    }

    inline ImRect RectExpanded(const ImRect& r, float x, float y)
    {
        ImRect out = r;
        out.Min.x -= x; out.Min.y -= y;
        out.Max.x += x; out.Max.y += y;
        return out;
    }

    // Draw an image over the last item rect (usually an InvisibleButton)
    inline void DrawButtonImage(ImTextureID tex, ImU32 colN, ImU32 colH, ImU32 colP, const ImRect* rectOverride = nullptr)
    {
        if (!tex)
            return;

        const bool hovered = ImGui::IsItemHovered();
        const bool held = ImGui::IsItemActive();
        const ImU32 tint = held ? colP : (hovered ? colH : colN);

        ImRect r = rectOverride ? *rectOverride : ImRect(ImGui::GetItemRectMin(), ImGui::GetItemRectMax());
        ImGui::GetWindowDrawList()->AddImage(tex, r.Min, r.Max, ImVec2(0, 0), ImVec2(1, 1), tint);
    }
    inline void Spring(const HorizontalLayout& l, float widthFromRight)
    {
        // Move cursor so that the next item is widthFromRight from the right edge
        ImGui::SetCursorPosX(l.rightX - widthFromRight);
        ImGui::SetCursorPosY(l.cursorY);
    }

    inline void Spring(const HorizontalLayout& l)
    {
        // Simple version: jump to right edge
        ImGui::SetCursorPosX(l.rightX);
        ImGui::SetCursorPosY(l.cursorY);
    }

    // Equivalent to Walnut's "Spring": push cursor to the right (simple, predictable)
    inline void SpringRight(float rightPadding = 0.0f)
    {
        // Move cursor to the far right of the current content region.
        float avail = ImGui::GetContentRegionAvail().x;
        if (avail > rightPadding)
            ImGui::SetCursorPosX(ImGui::GetCursorPosX() + avail - rightPadding);
    }
    inline void Spring(const HorizontalLayout& l, float /*weight*/, float spacing)
    {
        // Advance cursor by spacing
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + spacing);
        ImGui::SetCursorPosY(l.cursorY);
    }
    //// Simple horizontal group helpers (replacement for BeginHorizontal/EndHorizontal)
    //inline void BeginHorizontal(const char* id)
    //{
    //    ImGui::BeginGroup();
    //    ImGui::PushID(id);
    //}

    //inline void EndHorizontal()
    //{
    //    ImGui::PopID();
    //    ImGui::EndGroup();
    //}
}


namespace adapters {

    // Track whether ImGuiHost created the GLFW window (so we can destroy it)
    static bool g_ownsGlfwWindow = false;
    static bool g_glfwInitedByHost = false;

    // ============================================================
    // helpers
    // ============================================================

    static void PrintGLFWState(GLFWwindow* w)
    {
        if (!w) {
            std::printf("[ImGuiHost] GLFW window = NULL\n");
            return;
        }

        int ww = 0, wh = 0;
        int fbw = 0, fbh = 0;
        glfwGetWindowSize(w, &ww, &wh);
        glfwGetFramebufferSize(w, &fbw, &fbh);

        int visible = glfwGetWindowAttrib(w, GLFW_VISIBLE);
        int iconified = glfwGetWindowAttrib(w, GLFW_ICONIFIED);

        std::printf(
            "[ImGuiHost]   Window=%dx%d FB=%dx%d Visible=%d Iconified=%d\n",
            ww, wh, fbw, fbh, visible, iconified
        );
    }

    static GLFWwindow* CreateHostWindow()
    {
        // If GLFW isn't initialized by someone else, initialize it here.
        if (!glfwInit()) {
            std::printf("[ImGuiHost][ERROR] glfwInit() failed (cannot create UI window)\n");
            return nullptr;
        }

        g_glfwInitedByHost = true;

        const char* glsl_version = "#version 130";
        glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
        glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
        glfwWindowHint(GLFW_TITLEBAR, false);

#if defined(__APPLE__)
        glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif

        GLFWwindow* w = glfwCreateWindow(1280, 800, "Pistachio - CAD Converter", nullptr, nullptr);
        if (!w) {
            std::printf("[ImGuiHost][ERROR] glfwCreateWindow() failed\n");
            return nullptr;
        }

        glfwMakeContextCurrent(w);
        glfwSwapInterval(1); // vsync

       

        // Load embedded Roboto font
       // ImFontConfig fontConfig;
       // fontConfig.FontDataOwnedByAtlas = false;
       // io.FontDefault = io.Fonts->AddFontFromMemoryTTF((void*)g_RobotoRegular, sizeof(g_RobotoRegular), 17.0f, &fontConfig);
     /*   m_smallFont = io.Fonts->AddFontFromMemoryTTF((void*)g_RobotoRegular, sizeof(g_RobotoRegular), 14.0f, &fontConfig);

        m_ToolBarLineIcon = LoadIcon("assets/icons/SketchTwoPointLine_256.png");
        m_ToolBarCircleIcon = LoadIcon("assets/icons/SketchTwoPointCircle_256.png");
        m_ToolBarArcIcon = LoadIcon("assets/icons/SketchTwoPointArc_256.png");
        m_ToolBarRectIcon = LoadIcon("assets/icons/SketchTwoPointRectangle_256.png");*/
        g_ownsGlfwWindow = true;

        std::printf("[ImGuiHost] Created host GLFW window successfully\n");
        PrintGLFWState(w);

        return w;
    }

    // ============================================================
    // ctor / dtor
    // ============================================================

    ImGuiHost::ImGuiHost()
    {
        if (m_diagStdout)
            std::printf("[ImGuiHost] ctor\n");
    }

    ImGuiHost::~ImGuiHost()
    {
        if (m_diagStdout)
            std::printf("[ImGuiHost] dtor\n");
    }

    // ============================================================
    // initialization
    // ============================================================

    bool ImGuiHost::initialize()
    {
        if (m_initialized)
            return true;

        if (m_diagStdout)
            std::printf("[ImGuiHost] >>> initialize()\n");

        // 1) Prefer explicitly assigned window
        // 2) Fall back to current context if available
        if (!m_window)
            m_window = glfwGetCurrentContext();

        // If still no window/context, create our own GLFW UI window.
        if (!m_window) {
            std::printf(
                "[ImGuiHost][WARN] No current GLFW context. Creating a dedicated ImGui GLFW window...\n"
            );
            m_window = CreateHostWindow();
            if (!m_window) {
                std::printf(
                    "[ImGuiHost][ERROR] Failed to create GLFW window/context for ImGui.\n"
                );
                return false;
            }
        }

        // Ensure context is current now
        glfwMakeContextCurrent(m_window);

        IMGUI_CHECKVERSION();
        ImGui::CreateContext();

        ImGuiIO& io = ImGui::GetIO();

        // Docking is fine
        io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;

        // IMPORTANT: Disable multi-viewport to avoid monitor assertion in your imgui fork
        io.ConfigFlags &= ~ImGuiConfigFlags_ViewportsEnable;

        io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
  
        ImGui::StyleColorsDark();

        ImGui_ImplGlfw_InitForOpenGL(m_window, true);
        ImGui_ImplOpenGL3_Init("#version 330");

        // windows icons
        {
            uint32_t w, h;
            void* data = Walnut::Image::Decode(g_WalnutIcon, sizeof(g_WalnutIcon), w, h);
            if (data) {
                m_AppHeaderIcon = std::make_shared<Walnut::Image>(w, h, Walnut::ImageFormat::RGBA, data);
                // Free the decoded data (Image has copied it)
                stbi_image_free(data);
            }
        }



        m_initialized = true;

        diagPrintInitState();

        if (m_diagStdout)
            std::printf("[ImGuiHost] <<< initialize()\n");

        return true;
    }

    void ImGuiHost::shutdown()
    {
        if (!m_initialized)
            return;

        if (m_diagStdout)
            std::printf("[ImGuiHost] >>> shutdown()\n");

        ImGui_ImplOpenGL3_Shutdown();
        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext();

        m_initialized = false;

        // If we created the GLFW window, destroy it.
        if (g_ownsGlfwWindow && m_window) {
            std::printf("[ImGuiHost] Destroying owned GLFW window\n");
            glfwDestroyWindow(m_window);
            m_window = nullptr;
            g_ownsGlfwWindow = false;
        }

        // Only terminate GLFW if we initialized it AND GLFW is only used for the ImGui window.
        if (g_glfwInitedByHost) {
            std::printf("[ImGuiHost] glfwTerminate() (host initialized GLFW)\n");
            glfwTerminate();
            g_glfwInitedByHost = false;
        }

        if (m_diagStdout)
            std::printf("[ImGuiHost] <<< shutdown()\n");
    }

    // ============================================================
    // diagnostics helpers
    // ============================================================

    void ImGuiHost::diagPrintInitState()
    {
        if (!m_diagStdout)
            return;

        ImGuiIO& io = ImGui::GetIO();

        std::printf("[ImGuiHost] ImGui ctx=%p\n", (void*)ImGui::GetCurrentContext());
        std::printf("[ImGuiHost] BackendPlatform=%s\n", io.BackendPlatformName);
        std::printf("[ImGuiHost] BackendRenderer=%s\n", io.BackendRendererName);

        PrintGLFWState(m_window);
    }

    void ImGuiHost::diagPrintFrameState(const char* stage)
    {
        if (!m_diagStdout)
            return;

        ImGuiIO& io = ImGui::GetIO();

        std::printf(
            "[ImGuiHost] frame=%llu stage=%s ctx=%p\n"
            "  DisplaySize=%.0f,%.0f DeltaTime=%.6f "
            "WantCaptureMouse=%d WantCaptureKeyboard=%d\n",
            (unsigned long long)m_frameIndex,
            stage,
            (void*)ImGui::GetCurrentContext(),
            io.DisplaySize.x,
            io.DisplaySize.y,
            io.DeltaTime,
            (int)io.WantCaptureMouse,
            (int)io.WantCaptureKeyboard
        );

        PrintGLFWState(m_window);
    }

    void ImGuiHost::diagCheckAndRestoreGlfwContext(const char* stage)
    {
        if (glfwGetCurrentContext() != m_window) {
            std::printf(
                "[ImGuiHost][WARN] GLFW context changed before %s — restoring\n",
                stage
            );
            glfwMakeContextCurrent(m_window);
        }
    }

    void ImGuiHost::diagCheckOpenGLErrors(const char* stage)
    {
        if (!m_diagStdout)
            return;

        GLenum err;
        while ((err = glGetError()) != GL_NO_ERROR) {
            std::printf(
                "[ImGuiHost][GL ERROR] stage=%s code=0x%X\n",
                stage,
                err
            );
        }
    }

    // ============================================================
    // frame lifecycle
    // ============================================================

    bool ImGuiHost::shouldClose()
    {
        return m_window ? glfwWindowShouldClose(m_window) : true;
    }
    bool ImGuiHost::IsMaximized() const
    {
        return (bool)glfwGetWindowAttrib(m_window, GLFW_MAXIMIZED);
    }

    void ImGuiHost::setWindowControlIcons(
        ImTextureID minimize,
        ImTextureID maximize,
        ImTextureID restore,
        ImTextureID close,
        ImVec2 size
    )
    {
        m_iconMinimize = minimize;
        m_iconMaximize = maximize;
        m_iconRestore  = restore;
        m_iconClose    = close;
        m_iconSize     = size;
    }

    void ImGuiHost::beginFrame()
    {
        ++m_frameIndex;

        if (m_diagStdout)
            std::printf("[ImGuiHost] >>> beginFrame frame=%llu\n",
                (unsigned long long)m_frameIndex);
        glfwMakeContextCurrent(m_window);
        glfwPollEvents();

        ImGuiIO& io = ImGui::GetIO();

        // Backend NewFrame first (your fork is clobbering DisplaySize)
        ImGui_ImplGlfw_NewFrame();
        ImGui_ImplOpenGL3_NewFrame();

        // NOW force DisplaySize (must be non-zero)
        {
            int ww = 0, wh = 0;
            int fbw = 0, fbh = 0;
            glfwGetWindowSize(m_window, &ww, &wh);
            glfwGetFramebufferSize(m_window, &fbw, &fbh);

            if (ww <= 0) ww = 1;
            if (wh <= 0) wh = 1;

            io.DisplaySize = ImVec2((float)ww, (float)wh);

            if (ww > 0 && wh > 0)
                io.DisplayFramebufferScale = ImVec2((float)fbw / (float)ww, (float)fbh / (float)wh);
        }

        // (optional) force mouse after backend NewFrame too
        {
            double mx, my;
            glfwGetCursorPos(m_window, &mx, &my);
            io.MousePos = ImVec2((float)mx, (float)my);
            io.MouseDown[0] = (glfwGetMouseButton(m_window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS);
            io.MouseDown[1] = (glfwGetMouseButton(m_window, GLFW_MOUSE_BUTTON_RIGHT) == GLFW_PRESS);
            io.MouseDown[2] = (glfwGetMouseButton(m_window, GLFW_MOUSE_BUTTON_MIDDLE) == GLFW_PRESS);
        }

        ImGui::NewFrame();


        ImGui::Begin("##HostProof", nullptr, ImGuiWindowFlags_AlwaysAutoResize);
        ImGui::Text("Frame %llu", (unsigned long long)m_frameIndex);
        ImGui::Text("DisplaySize %.0f x %.0f", io.DisplaySize.x, io.DisplaySize.y);
        ImGui::Text("Mouse %.1f %.1f L=%d", io.MousePos.x, io.MousePos.y, io.MouseDown[0]);
        ImGui::End();




        //// Always ensure our UI window/context is current BEFORE backend NewFrame
        //glfwMakeContextCurrent(m_window);

        //diagCheckAndRestoreGlfwContext("beginFrame");

        //glfwPollEvents();

        //ImGui_ImplOpenGL3_NewFrame();
        //ImGui_ImplGlfw_NewFrame();

        //// Robust DisplaySize update (don’t rely on backend when other systems swap contexts)
        //{
        //    int ww = 0, wh = 0;
        //    int fbw = 0, fbh = 0;
        //    glfwGetWindowSize(m_window, &ww, &wh);
        //    glfwGetFramebufferSize(m_window, &fbw, &fbh);

        //    ImGuiIO& io = ImGui::GetIO();
        //    io.DisplaySize = ImVec2((float)ww, (float)wh);

        //    if (ww > 0 && wh > 0) {
        //        io.DisplayFramebufferScale = ImVec2((float)fbw / (float)ww, (float)fbh / (float)wh);
        //    }

        //    if (m_diagStdout && (m_frameIndex <= 3)) {
        //        std::printf("[ImGuiHost] DisplaySize set from GLFW = %dx%d (FB=%dx%d)\n", ww, wh, fbw, fbh);
        //    }
        //}

        //ImGui::NewFrame();
        float titlebarHeight = 50.0f;

        ImGuiWindowFlags titlebar_flags =
            ImGuiWindowFlags_NoTitleBar |
            ImGuiWindowFlags_NoResize |
            ImGuiWindowFlags_NoMove |
            ImGuiWindowFlags_NoScrollbar |
            ImGuiWindowFlags_NoScrollWithMouse |
            ImGuiWindowFlags_NoDocking;
        // Position at top of viewport
        ImGuiViewport* viewport = ImGui::GetMainViewport();
        ImGui::SetNextWindowPos(viewport->Pos);
        ImGui::SetNextWindowSize(ImVec2(viewport->Size.x, titlebarHeight)); // Adjust height as needed
        ImGui::SetNextWindowViewport(viewport->ID);

        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));

        ImGui::Begin("##Titlebar", nullptr, titlebar_flags);
        {
           // UI_DrawTitlebar(titlebarHeight);

            //const float titlebarHeight = outTitlebarHeight;// 60.0f;
            const bool isMaximized = IsMaximized();
            float titlebarVerticalOffset = isMaximized ? -6.0f : 0.0f;
            const ImVec2 windowPadding = ImGui::GetCurrentWindow()->WindowPadding;

            ImGui::SetCursorPos(ImVec2(windowPadding.x, windowPadding.y + titlebarVerticalOffset));
            const ImVec2 titlebarMin = ImGui::GetCursorScreenPos();
            const ImVec2 titlebarMax = { ImGui::GetCursorScreenPos().x + ImGui::GetWindowWidth() - windowPadding.y * 2.0f,
                                         ImGui::GetCursorScreenPos().y + titlebarHeight };
            auto* bgDrawList = ImGui::GetBackgroundDrawList();
            auto* fgDrawList = ImGui::GetForegroundDrawList();
            bgDrawList->AddRectFilled(titlebarMin, titlebarMax, UI::Colors::Theme::titlebar);
            // DEBUG TITLEBAR BOUNDS
            //fgDrawList->AddRect(titlebarMin, titlebarMax, UI::Colors::Theme::invalidPrefab);

            // Logo
            {
                const int logoWidth = 48;// m_LogoTex->GetWidth();
                const int logoHeight = 48;// m_LogoTex->GetHeight();
                const ImVec2 logoOffset(16.0f + windowPadding.x, 5.0f + windowPadding.y + titlebarVerticalOffset);
                const ImVec2 logoRectStart = { ImGui::GetItemRectMin().x + logoOffset.x, ImGui::GetItemRectMin().y + logoOffset.y };
                const ImVec2 logoRectMax = { logoRectStart.x + logoWidth, logoRectStart.y + logoHeight };

                fgDrawList->AddImage(m_AppHeaderIcon->GetDescriptorSet(), logoRectStart, logoRectMax);
            }


       

        // Window buttons
        const ImU32 buttonColN = UI::Colors::ColorWithMultipliedValue(UI::Colors::Theme::text, 0.9f);
        const ImU32 buttonColH = UI::Colors::ColorWithMultipliedValue(UI::Colors::Theme::text, 1.2f);
        const ImU32 buttonColP = UI::Colors::Theme::textDarker;
        const float buttonWidth = 14.0f;
        const float buttonHeight = 14.0f;

        //// Minimize Button
        auto layout = HostUI::BeginHorizontal();

        // Minimize
        HostUI::Spring(layout, 16 * 3 + 32);
        //HostUI::Spring();
        HostUI::ShiftCursorY(8.0f);
        {
            const int iconWidth = 16;
            const int iconHeight = 16;
            const float padY = (buttonHeight - (float)iconHeight) / 2.0f;
            if (ImGui::InvisibleButton("Minimize", ImVec2(iconWidth, iconHeight)))
            {
                // TODO: move this stuff to a better place, like Window class
                if (m_window)
                {
                    glfwIconifyWindow(m_window);
                    // we need to send the event so that Application knows its minimizing.
                    //   // Application::Get().QueueEvent([windowHandle = m_Window]() { glfwIconifyWindow(windowHandle); });
                }
            }

            HostUI::DrawButtonImage(m_iconMinimize, buttonColN, buttonColH, buttonColP);//, HostUI::RectExpanded(HostUI::GetItemRect(), 0.0f, -padY));
        }


        //// Maximize Button
        HostUI::Spring(layout ,-1.0f, 17.0f);
        HostUI::ShiftCursorY(8.0f);
        {
            const int iconWidth =16;
            const int iconHeight = 16;

            const bool isMaximized = IsMaximized();

            if (ImGui::InvisibleButton("Maximize", ImVec2(iconWidth, iconHeight)))
            {

                if (isMaximized)
                    glfwRestoreWindow(m_window);
                else
                    glfwMaximizeWindow(m_window);

                // TOO DN add event queue
               /* Application::Get().QueueEvent([isMaximized, windowHandle = m_WindowHandle]()
                    {
                        if (isMaximized)
                            glfwRestoreWindow(windowHandle);
                        else
                            glfwMaximizeWindow(windowHandle);
                    });*/
            }

            HostUI::DrawButtonImage(isMaximized ? m_iconRestore : m_iconMaximize, buttonColN, buttonColH, buttonColP);
        }

        // Close Button
        HostUI::Spring(layout ,-1.0f, 15.0f);
        HostUI::ShiftCursorY(8.0f);
        {
            const int iconWidth = 16;//m_iconClose->GetWidth();
            const int iconHeight = 16;//m_iconClose->GetHeight();
            if (ImGui::InvisibleButton("Close", ImVec2(iconWidth, iconHeight)))
            {
                glfwSetWindowShouldClose(m_window, GLFW_TRUE);
                // TODO DN send the event to the application
               //Application::Get().Close();
            }
            HostUI::DrawButtonImage(m_iconClose, UI::Colors::Theme::text, UI::Colors::ColorWithMultipliedValue(UI::Colors::Theme::text, 1.4f), buttonColP);
        }

        HostUI::Spring(layout ,-1.0f, 18.0f);
        
    } // toolbar group
        ImGui::End();
        ImGui::PopStyleVar(3);
        {
            ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoDocking;
            ImGuiViewport* viewport = ImGui::GetMainViewport();

            //float titlebarHeight = 130.0f; // Should match the titlebar window height
            ImVec2 size = viewport->Size;

            // Position BELOW the titlebar
            ImGui::SetNextWindowPos(ImVec2(viewport->Pos.x, viewport->Pos.y + titlebarHeight));
            ImGui::SetNextWindowSize(ImVec2(viewport->Size.x, viewport->Size.y - titlebarHeight));
            ImGui::SetNextWindowViewport(viewport->ID);

            ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
            ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);

            window_flags |= ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse |
                ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove;
            window_flags |= ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus;

            const bool isMaximized = IsMaximized();
            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, isMaximized ? ImVec2(6.0f, 6.0f) : ImVec2(1.0f, 1.0f));
            ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 3.0f);
            ImGui::PushStyleColor(ImGuiCol_MenuBarBg, ImVec4{ 0.0f, 0.0f, 0.0f, 0.0f });

            ImGui::Begin("DockSpaceWindow", nullptr, window_flags);
            ImGui::PopStyleColor(); // MenuBarBg
            ImGui::PopStyleVar(4);

            {
                ImGui::PushStyleColor(ImGuiCol_Border, IM_COL32(50, 50, 50, 255));
                // Draw window border if needed
                ImGui::PopStyleColor(); // ImGuiCol_Border
            }

            // Create the docking space (single dockspace for the whole app)
            ImGuiID dockspace_id = ImGui::GetID("MainDockspace");
            ImGui::DockSpace(dockspace_id, ImVec2(0.0f, 0.0f), ImGuiDockNodeFlags_PassthruCentralNode);

            ImGui::End();
        }

        // IMPORTANT: Only ONE dockspace should exist. A second dockspace window will break docking/layout.

        //diagPrintFrameState("after NewFrame");

        //// ========================================================
        //// GUARANTEED HOST OVERLAY (diagnostic)
        //// ========================================================
        //if (m_diagStdout) {
        //    std::printf("[ImGuiHost] SUBMIT HostOverlay frame=%llu\n",
        //        (unsigned long long)m_frameIndex);
        //}

        //ImGui::Begin("##HostOverlay", nullptr,
        //    ImGuiWindowFlags_NoDecoration |
        //    ImGuiWindowFlags_AlwaysAutoResize |
        //    ImGuiWindowFlags_NoMove |
        //    ImGuiWindowFlags_NoSavedSettings);

        //ImGuiIO& io = ImGui::GetIO();
        //ImGui::Text("Host overlay alive");
        //ImGui::Text("Frame: %llu", (unsigned long long)m_frameIndex);
        //ImGui::Text("Display: %.0fx%.0f", io.DisplaySize.x, io.DisplaySize.y);
        //ImGui::Text("Context: %p", (void*)ImGui::GetCurrentContext());
        //ImGui::End();

        if (m_menubarCallback)
            m_menubarCallback();

        /*if (m_diagStdout)
            std::printf("[ImGuiHost] <<< beginFrame\n");*/
    }

    void ImGuiHost::render()
    {
        // Host does not render content; plugins do.
        if (m_diagStdout) {
            std::printf("[ImGuiHost] render() frame=%llu\n",
                (unsigned long long)m_frameIndex);
        }
    }

    void ImGuiHost::endFrame()
    {
        if (m_diagStdout)
            std::printf("[ImGuiHost] >>> endFrame frame=%llu\n",
                (unsigned long long)m_frameIndex);

        ImGui::Render();

        ImDrawData* dd = ImGui::GetDrawData();
        if (m_diagStdout) {
            if (dd) {
                std::printf(
                    "[ImGuiHost] DrawData: CmdLists=%d Vtx=%d Idx=%d DisplaySize=%.0f,%.0f\n",
                    dd->CmdListsCount,
                    dd->TotalVtxCount,
                    dd->TotalIdxCount,
                    dd->DisplaySize.x,
                    dd->DisplaySize.y
                );

                if (dd->CmdListsCount == 0) {
                    std::printf("[ImGuiHost][WARN] zero command lists (no UI submitted)\n");
                }
            }
            else {
                std::printf("[ImGuiHost][WARN] DrawData is NULL\n");
            }
        }

        // Always ensure our UI window/context is current before rendering
        glfwMakeContextCurrent(m_window);

        diagCheckOpenGLErrors("before RenderDrawData");

        int fbw = 0, fbh = 0;
        glfwGetFramebufferSize(m_window, &fbw, &fbh);
        glViewport(0, 0, fbw, fbh);
        glClear(GL_COLOR_BUFFER_BIT);

        ImGui_ImplOpenGL3_RenderDrawData(dd);

        // Viewports disabled, so no platform windows block here.

        glfwSwapBuffers(m_window);

        diagCheckOpenGLErrors("after RenderDrawData");

        if (m_diagStdout)
            std::printf("[ImGuiHost] <<< endFrame\n");
    }

    // ============================================================
    // misc
    // ============================================================

    void ImGuiHost::setMenubarCallback(const std::function<void()>& menubarCallback)
    {
        m_menubarCallback = menubarCallback;
    }

} // namespace adapters
