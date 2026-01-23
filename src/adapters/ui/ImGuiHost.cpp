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





    static inline ImRect OffsetRect(const ImRect& r, float dx, float dy)
    {
        return ImRect(ImVec2(r.Min.x + dx, r.Min.y + dy),
            ImVec2(r.Max.x + dx, r.Max.y + dy));
    }

    bool BeginMenubar(const ImRect& barRectangle)
    {
        ImGuiWindow* window = ImGui::GetCurrentWindow();
        if (!window || window->SkipItems)
            return false;

        // Walnut had this check but commented out; keep it permissive for host usage.
        // if (!(window->Flags & ImGuiWindowFlags_MenuBar))
        //     return false;

        IM_ASSERT(!window->DC.MenuBarAppending);

        // Backup/restore state using group+ID the same way Walnut does.
        ImGui::BeginGroup();
        ImGui::PushID("##menubar");

        const ImVec2 padding = window->WindowPadding;

        // The caller passes a rectangle in *window-local coordinates*.
        // Walnut offsets the rect by padding.y to align nicely under the window padding.
        ImRect bar_rect = OffsetRect(barRectangle, 0.0f, padding.y);

        // Build a clip rect in *screen coordinates*.
        // This is essentially Walnut's logic, adapted without Walnut's helper.
        ImRect clip_rect(
            ImVec2(
                IM_ROUND(ImMax(window->Pos.x,
                    bar_rect.Min.x + window->WindowBorderSize + window->Pos.x - 10.0f)),
                IM_ROUND(bar_rect.Min.y + window->WindowBorderSize + window->Pos.y)
            ),
            ImVec2(
                IM_ROUND(ImMax(bar_rect.Min.x + window->Pos.x,
                    bar_rect.Max.x - ImMax(window->WindowRounding, window->WindowBorderSize))),
                IM_ROUND(bar_rect.Max.y + window->Pos.y)
            )
        );

        clip_rect.ClipWith(window->OuterRectClipped);
        ImGui::PushClipRect(clip_rect.Min, clip_rect.Max, false);

        // IMPORTANT: BeginGroup() resets CursorMaxPos to CursorPos. Walnut overwrites both.
        window->DC.CursorPos = window->DC.CursorMaxPos =
            ImVec2(bar_rect.Min.x + window->Pos.x, bar_rect.Min.y + window->Pos.y);

        window->DC.LayoutType = ImGuiLayoutType_Horizontal;
        window->DC.NavLayerCurrent = ImGuiNavLayer_Menu;
        window->DC.MenuBarAppending = true;

        ImGui::AlignTextToFramePadding();
        return true;
    }

    void EndMenubar()
    {
        ImGuiWindow* window = ImGui::GetCurrentWindow();
        if (!window || window->SkipItems)
            return;

        ImGuiContext& g = *GImGui;

        // Nav forwarding among sibling menus (same as Walnut)
        if (ImGui::NavMoveRequestButNoResultYet() &&
            (g.NavMoveDir == ImGuiDir_Left || g.NavMoveDir == ImGuiDir_Right) &&
            (g.NavWindow->Flags & ImGuiWindowFlags_ChildMenu))
        {
            ImGuiWindow* nav_earliest_child = g.NavWindow;
            while (nav_earliest_child->ParentWindow &&
                (nav_earliest_child->ParentWindow->Flags & ImGuiWindowFlags_ChildMenu))
            {
                nav_earliest_child = nav_earliest_child->ParentWindow;
            }

            if (nav_earliest_child->ParentWindow == window &&
                nav_earliest_child->DC.ParentLayoutType == ImGuiLayoutType_Horizontal &&
                (g.NavMoveFlags & ImGuiNavMoveFlags_Forwarded) == 0)
            {
                const ImGuiNavLayer layer = ImGuiNavLayer_Menu;
                IM_ASSERT(window->DC.NavLayersActiveMaskNext & (1 << layer));

                ImGui::FocusWindow(window);
                ImGui::SetNavID(window->NavLastIds[layer], layer, 0, window->NavRectRel[layer]);

                g.NavDisableHighlight = true;
                g.NavDisableMouseHover = g.NavMousePosDirty = true;

                ImGui::NavMoveRequestForward(
                    g.NavMoveDir, g.NavMoveClipDir, g.NavMoveFlags, g.NavMoveScrollFlags);
            }
        }

        IM_ASSERT(window->DC.MenuBarAppending);

        ImGui::PopClipRect();
        ImGui::PopID();

        // Save horizontal position so next append can reuse it
        window->DC.MenuBarOffset.x = window->DC.CursorPos.x - window->Pos.x;

        // Undo group "emit item" hack (Walnut)
        g.GroupStack.back().EmitItem = false;
        ImGui::EndGroup();

        window->DC.LayoutType = ImGuiLayoutType_Vertical;
        window->DC.NavLayerCurrent = ImGuiNavLayer_Main;
        window->DC.MenuBarAppending = false;
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

    bool ImGuiHost::shouldClose() const
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
        m_iconRestore = restore;
        m_iconClose = close;
        m_iconSize = size;
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


        float titlebarHeight = 96.0f; // includes menu row + ribbon row
        const bool isMaximized = IsMaximized();
        float titlebarVerticalOffset = isMaximized ? -6.0f : 0.0f;
        const ImVec2 windowPadding = ImGui::GetCurrentWindow()->WindowPadding;

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

            // ImGui::BeginHorizontal("Titlebar-2", { ImGui::GetWindowWidth() - windowPadding.y * 2.0f, ImGui::GetFrameHeightWithSpacing() });
             //HostUI::BeginHorizontal("Titlebar-2", { ImGui::GetWindowWidth() - windowPadding.y * 2.0f, ImGui::GetFrameHeightWithSpacing() });

            static float moveOffsetX;
            static float moveOffsetY;
            const float w = ImGui::GetContentRegionAvail().x;
            const float buttonsAreaWidth = 94;


            // Title bar drag area
            // On Windows we hook into the GLFW win32 window internals
            ImGui::SetCursorPos(ImVec2(windowPadding.x, windowPadding.y + titlebarVerticalOffset)); // Reset cursor pos
            // DEBUG DRAG BOUNDS
            //fgDrawList->AddRect(ImGui::GetCursorScreenPos(), ImVec2(ImGui::GetCursorScreenPos().x + w - buttonsAreaWidth, ImGui::GetCursorScreenPos().y + titlebarHeight), UI::Colors::Theme::invalidPrefab);
            ImGui::InvisibleButton("##titleBarDragZone", ImVec2(w - buttonsAreaWidth, titlebarHeight));

            
            ImGui::SetItemAllowOverlap(); // allow menubar/buttons drawn on top to receive clicks
m_TitleBarHovered = ImGui::IsItemHovered();

            const bool dragZoneHovered = ImGui::IsItemHovered(ImGuiHoveredFlags_RectOnly);
            const bool dragZoneClicked = ImGui::IsItemClicked(ImGuiMouseButton_Left); // first press

            // IMPORTANT:
            // The titlebar drag-zone covers the entire titlebar area. Even with SetItemAllowOverlap(),
            // it can still "eat" the first click intended for the menubar/ribbon buttons.
            // Suppress dragging when the mouse is over the menu/ribbon strip.
            const ImVec2 mouse = ImGui::GetMousePos();
            const float logoHorizontalOffset = 16.0f * 2.0f + 48.0f + windowPadding.x;
            const float menuTopY = viewport->Pos.y + (windowPadding.y + titlebarVerticalOffset);
            const float menuHeight = ImGui::GetFrameHeightWithSpacing();
            const float ribbonTopY = menuTopY + menuHeight;
            const float ribbonHeight = menuHeight; // one row of buttons

            const bool mouseOverMenu = (mouse.x >= viewport->Pos.x + logoHorizontalOffset) &&
                                      (mouse.y >= menuTopY) && (mouse.y <= menuTopY + menuHeight);
            const bool mouseOverRibbon = (mouse.x >= viewport->Pos.x + logoHorizontalOffset) &&
                                        (mouse.y >= ribbonTopY) && (mouse.y <= ribbonTopY + ribbonHeight);
            const bool mouseOverMenuOrRibbon = mouseOverMenu || mouseOverRibbon;
            // or: const bool dragZoneActive = ImGui::IsItemActive();

#ifdef _WIN32
            if (dragZoneClicked && !mouseOverMenuOrRibbon)  // only begin a drag when click begins in the zone
            {
                HWND hwnd = glfwGetWin32Window(m_window);

                // Optional: ignore double-click if you use it for maximize/restore
                // if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) { ... }

                ReleaseCapture();
                SendMessage(hwnd, WM_NCLBUTTONDOWN, HTCAPTION, 0);
            }
#endif

            // ImGui::End();
             // ImGui::PopStyleColor();
             // ImGui::PopStyleVar(2);

            if (m_menubarCallback) {

                //m_menubarCallback();   // menus only, no Begin/End

                ImGui::SuspendLayout();
                {
                    ImGui::SetItemAllowOverlap();
                    const float logoHorizontalOffset = 16.0f * 2.0f + 48.0f + windowPadding.x;
                    ImGui::SetCursorPos(ImVec2(logoHorizontalOffset, 6.0f + titlebarVerticalOffset));

                    const ImRect menuBarRect = { ImGui::GetCursorPos(), { ImGui::GetContentRegionAvail().x + ImGui::GetCursorScreenPos().x, ImGui::GetFrameHeightWithSpacing() } };

                    ImGui::BeginGroup();
                    if (HostUI::BeginMenubar(menuBarRect))
                    {
                        m_menubarCallback();   // menus only, no Begin/End
                    }

                    HostUI::EndMenubar();
                    ImGui::EndGroup();

                    // Ribbon bar (second row)
                    if (m_ribbonbarCallback) {
                        ImGui::SetCursorPos(ImVec2(logoHorizontalOffset, 6.0f + titlebarVerticalOffset + ImGui::GetFrameHeightWithSpacing()));
                        const ImRect ribbonRect = { ImGui::GetCursorPos(), { ImGui::GetContentRegionAvail().x + ImGui::GetCursorScreenPos().x, ImGui::GetFrameHeightWithSpacing() * 2.0f } };
                        ImGui::BeginGroup();
                        // Ribbon callback draws content only (no Begin/End)
                        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(6.0f, 4.0f));
                        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(6.0f, 4.0f));
                        m_ribbonbarCallback();
                        ImGui::PopStyleVar(2);
                        ImGui::EndGroup();
                    }


                    if (ImGui::IsItemHovered())
                        m_TitleBarHovered = false;
                }

                ImGui::ResumeLayout();

            }

            {
                // Centered Window title
                ImVec2 currentCursorPos = ImGui::GetCursorPos();
                ImVec2 textSize = ImGui::CalcTextSize("m_Specification");
                ImGui::SetCursorPos(ImVec2(ImGui::GetWindowWidth() * 0.5f - textSize.x * 0.5f, 2.0f + windowPadding.y + 6.0f));
                ImGui::Text("%s", "m_Specification"); // Draw title
                ImGui::SetCursorPos(currentCursorPos);
            }

            // Window buttons (top-right, grouped)
            const ImU32 buttonColN = UI::Colors::ColorWithMultipliedValue(UI::Colors::Theme::text, 0.9f);
            const ImU32 buttonColH = UI::Colors::ColorWithMultipliedValue(UI::Colors::Theme::text, 1.2f);
            const ImU32 buttonColP = UI::Colors::Theme::textDarker;


            ImGui::SetCursorPosY(ImGui::GetFrameHeightWithSpacing() / 2);

            auto layout = HostUI::BeginHorizontal();

            const float iconW = (m_iconSize.x > 0.0f) ? m_iconSize.x : 16.0f;
            const float iconH = (m_iconSize.y > 0.0f) ? m_iconSize.y : 16.0f;

            // Match your “feel” here (I used small, tight spacing)
            const float gap = 8.0f;
            const float rightPadding = 16.0f;

            // total width of 3 buttons + 2 gaps + right padding
            const float totalFromRight = rightPadding + (iconW * 3.0f) + (gap * 2.0f);

            // Jump cursor so the FIRST button starts at the correct top-right X
            HostUI::Spring(layout, totalFromRight);
            HostUI::ShiftCursorY(8.0f);

            ImGui::BeginGroup();

            // Minimize
            if (ImGui::InvisibleButton("Minimize", ImVec2(iconW, iconH)))
            {
                if (m_window) glfwIconifyWindow(m_window);
            }
            HostUI::DrawButtonImage(m_iconMinimize, buttonColN, buttonColH, buttonColP);

            ImGui::SameLine(0.0f, gap);

            // Maximize / Restore
           // const bool isMaximized = IsMaximized();
            if (ImGui::InvisibleButton("Maximize", ImVec2(iconW, iconH)))
            {
                if (isMaximized) glfwRestoreWindow(m_window);
                else             glfwMaximizeWindow(m_window);
            }
            HostUI::DrawButtonImage(isMaximized ? m_iconRestore : m_iconMaximize, buttonColN, buttonColH, buttonColP);

            ImGui::SameLine(0.0f, gap);

            // Close
            if (ImGui::InvisibleButton("Close", ImVec2(iconW, iconH)))
            {
                glfwSetWindowShouldClose(m_window, GLFW_TRUE);
            }
            HostUI::DrawButtonImage(
                m_iconClose,
                UI::Colors::Theme::text,
                UI::Colors::ColorWithMultipliedValue(UI::Colors::Theme::text, 1.4f),
                buttonColP
            );

            ImGui::EndGroup();


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

            window_flags |= ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_MenuBar
                | ImGuiWindowFlags_NoCollapse
                | ImGuiWindowFlags_NoResize
                | ImGuiWindowFlags_NoMove;

            window_flags |= ImGuiWindowFlags_NoBringToFrontOnFocus
                | ImGuiWindowFlags_NoNavFocus;

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

void ImGuiHost::setRibbonbarCallback(const std::function<void()>& ribbonbarCallback)
{
    m_ribbonbarCallback = ribbonbarCallback;
}



ports::UiPluginStatus ImGuiHost::getUiPluginStatus() const
{
    ports::UiPluginStatus s;
    s.enabled = m_uiPluginEnabled;
    // This host does not load plugins itself in this build; mark as not-loaded but enabled state is tracked.
    s.loaded = false;
    s.id = "pistachio_ui";
    s.name = "Pistachio UI";
    s.version = "0.0.0";
    s.featureGroup = "UI";
    if (m_uiPluginHotReloadRequested)
        s.lastError = "Hot reload requested (pending)";
    return s;
}

void ImGuiHost::setUiPluginEnabled(bool enabled)
{
    m_uiPluginEnabled = enabled;
}

void ImGuiHost::requestHotReloadUiPlugin()
{
    m_uiPluginHotReloadRequested = false; // auto-clear pending after reload trigger

}

} // namespace adapters
