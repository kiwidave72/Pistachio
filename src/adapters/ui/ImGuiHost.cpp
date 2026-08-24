#include "ImGuiHost.h"

#include "IconsFontAwesomeRegular.h"

#include "../ImGui/ImGuiTheme.h"
#include "../ImGui/Image.h"


#include <imgui.h>
#include "imgui_internal.h"

#include <backends/imgui_impl_glfw.h>
#include <backends/imgui_impl_opengl3.h>
#include "../Roboto-Regular.embed"
#include "../../../Walnut-Icon.embed"
#include "../../../WindowImages.embed"

 

#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include "stb_image.h"
#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3native.h>

 
#include <cstdio>
#include <cassert>


// HostUI.h (or at top of ImGuiHost.cpp)
#pragma once
#include <imgui.h>

static void* GetOpenGLProcAddress(const char* name)
{
    // Try WGL first (requires a current context).
    void* p = (void*)wglGetProcAddress(name);

    // wglGetProcAddress returns small sentinel values on failure.
    if (p == nullptr || p == (void*)0x1 || p == (void*)0x2 || p == (void*)0x3 || p == (void*)-1)
    {
        static HMODULE s_opengl32 = ::GetModuleHandleA("opengl32.dll");
        if (!s_opengl32)
            s_opengl32 = ::LoadLibraryA("opengl32.dll");
        if (s_opengl32)
            p = (void*)::GetProcAddress(s_opengl32, name);
    }
    return p;
}

static bool EnsureGladLoaded()
{
    static bool s_loaded = false;
    if (s_loaded)
        return true;

    if (!gladLoadGLLoader((GLADloadproc)GetOpenGLProcAddress))
        return false;

    s_loaded = true;
    return true;
}


static GLuint CreateGLTextureRGBA_Minimal(const unsigned char* rgba, int w, int h)
{
    if (!EnsureGladLoaded())
        return 0;
    GLuint tex = 0;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    // Avoid enums missing in your build:
    // - no GL_TEXTURE_WRAP_S/T
    // - no GL_CLAMP_TO_EDGE
    // - no GL_UNPACK_ALIGNMENT
    // - no GL_RGBA8

    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, rgba);

    glBindTexture(GL_TEXTURE_2D, 0);
    return tex;
}

static bool CreateTextureFromEmbeddedPng(
    const unsigned char* bytes,
    int bytesSize,
    GLuint& outTex,
    ImTextureID& outId,
    ImVec2& outSize)
{
    int w = 0, h = 0, comp = 0;

    // Force RGBA output
    stbi_uc* data = stbi_load_from_memory(bytes, bytesSize, &w, &h, &comp, 4);
    if (!data || w <= 0 || h <= 0)
        return false;

    // Minimal upload (avoids GL_CLAMP_TO_EDGE / GL_RGBA8 / GL_UNPACK_ALIGNMENT)
    outTex = CreateGLTextureRGBA_Minimal(data, w, h);

    stbi_image_free(data);

    // ImGui OpenGL convention: ImTextureID is the GLuint cast to void*
    outId = (ImTextureID)(intptr_t)outTex;
    outSize = ImVec2((float)w, (float)h);
    return outTex != 0;
}



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
    void* ImGuiHost::nativeWindowHandle() const {
        return static_cast<void*>(glfwGetWin32Window(m_window));
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

        GLFWwindow* w = glfwCreateWindow(1200, 600, "Pistachio - CAD Converter", nullptr, nullptr);
        if (!w) {
            std::printf("[ImGuiHost][ERROR] glfwCreateWindow() failed\n");
            return nullptr;
        }

        glfwMakeContextCurrent(w);
        // Load OpenGL function pointers (GLAD)
        if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
            std::printf("[ImGuiHost][ERROR] gladLoadGLLoader() failed\n");

            return nullptr;
        }
        glfwSwapInterval(0); // vsync



        // Load embedded Roboto font
       
        //io.FontDefault = m_bodyFont;

         //io.FontDefault = io.Fonts->AddFontFromMemoryTTF((void*)g_RobotoRegular, sizeof(g_RobotoRegular), 17.0f, &fontConfig);
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

       // taskRunner = std::make_unique<TaskRunner>();
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

        glfwSetScrollCallback(m_window, [](GLFWwindow*, double xoff, double yoff) {
            ImGuiIO& io = ImGui::GetIO();
            io.MouseWheelH += (float)xoff;
            io.MouseWheel += (float)yoff;
            });

        // Docking is fine
        io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;

        // IMPORTANT: Disable multi-viewport to avoid monitor assertion in your imgui fork
        io.ConfigFlags &= ~ImGuiConfigFlags_ViewportsEnable;

        io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

        ImGui::StyleColorsDark();

        ImFontConfig fontConfig;
        fontConfig.FontDataOwnedByAtlas = false;

        

        io.FontDefault = io.Fonts->AddFontFromMemoryTTF((void*)g_RobotoRegular, sizeof(g_RobotoRegular), 14.0f, &fontConfig);

        static const ImWchar iconRanges[] = { ICON_MIN_FA, ICON_MAX_FA, 0 };

        ImFontConfig iconConfig;
        iconConfig.MergeMode = true;          // merge into previous font instead of replacing it
        iconConfig.PixelSnapH = true;
        iconConfig.GlyphMinAdvanceX = 16.0f;  // keep icons monospaced-ish so they don't jitter
        io.Fonts->AddFontFromFileTTF("assets/fonts/fa-regular-400.ttf", 16.0f, &iconConfig, iconRanges);
    
        ImFontConfig icons_config_solid;
        icons_config_solid.MergeMode = true;      // <-- key: merges into previous font instead of replacing
        icons_config_solid.PixelSnapH = true;
        io.Fonts->AddFontFromFileTTF("assets/fonts/fa-solid-900.ttf", 16.0f, &icons_config_solid, iconRanges);
        
        
        io.Fonts->Build();

        // CRITICAL FIX: Initialize ImGui KeyMap for keyboard navigation
        // This must be done BEFORE we install callbacks
        io.KeyMap[ImGuiKey_Tab] = GLFW_KEY_TAB;
        io.KeyMap[ImGuiKey_LeftArrow] = GLFW_KEY_LEFT;
        io.KeyMap[ImGuiKey_RightArrow] = GLFW_KEY_RIGHT;
        io.KeyMap[ImGuiKey_UpArrow] = GLFW_KEY_UP;
        io.KeyMap[ImGuiKey_DownArrow] = GLFW_KEY_DOWN;
        io.KeyMap[ImGuiKey_PageUp] = GLFW_KEY_PAGE_UP;
        io.KeyMap[ImGuiKey_PageDown] = GLFW_KEY_PAGE_DOWN;
        io.KeyMap[ImGuiKey_Home] = GLFW_KEY_HOME;
        io.KeyMap[ImGuiKey_End] = GLFW_KEY_END;
        io.KeyMap[ImGuiKey_Insert] = GLFW_KEY_INSERT;
        io.KeyMap[ImGuiKey_Delete] = GLFW_KEY_DELETE;
        io.KeyMap[ImGuiKey_Backspace] = GLFW_KEY_BACKSPACE;
        io.KeyMap[ImGuiKey_Space] = GLFW_KEY_SPACE;
        io.KeyMap[ImGuiKey_Enter] = GLFW_KEY_ENTER;
        io.KeyMap[ImGuiKey_Escape] = GLFW_KEY_ESCAPE;
        io.KeyMap[ImGuiKey_KeyPadEnter] = GLFW_KEY_KP_ENTER;
        io.KeyMap[ImGuiKey_A] = GLFW_KEY_A;
        io.KeyMap[ImGuiKey_C] = GLFW_KEY_C;
        io.KeyMap[ImGuiKey_V] = GLFW_KEY_V;
        io.KeyMap[ImGuiKey_X] = GLFW_KEY_X;
        io.KeyMap[ImGuiKey_Y] = GLFW_KEY_Y;
        io.KeyMap[ImGuiKey_Z] = GLFW_KEY_Z;

        // Install GLFW callbacks that forward to ImGui
        // Store the window pointer for use in lambda callbacks
        GLFWwindow* win = m_window;
        
        // Install key callback that forwards to ImGui
        glfwSetKeyCallback(m_window, [](GLFWwindow* window, int key, int scancode, int action, int mods) {
            // Let ImGui handle navigation keys
            ImGuiIO& io = ImGui::GetIO();
            if (action == GLFW_PRESS) {
                io.KeysDown[key] = true;
            }
            if (action == GLFW_RELEASE) {
                io.KeysDown[key] = false;
            }
            io.KeyCtrl = io.KeysDown[GLFW_KEY_LEFT_CONTROL] || io.KeysDown[GLFW_KEY_RIGHT_CONTROL];
            io.KeyShift = io.KeysDown[GLFW_KEY_LEFT_SHIFT] || io.KeysDown[GLFW_KEY_RIGHT_SHIFT];
            io.KeyAlt = io.KeysDown[GLFW_KEY_LEFT_ALT] || io.KeysDown[GLFW_KEY_RIGHT_ALT];
            io.KeySuper = io.KeysDown[GLFW_KEY_LEFT_SUPER] || io.KeysDown[GLFW_KEY_RIGHT_SUPER];
        });
        
        // Install char callback for text input
        glfwSetCharCallback(m_window, [](GLFWwindow* window, unsigned int c) {
            ImGuiIO& io = ImGui::GetIO();
            if (c > 0 && c < 0x10000) {
                io.AddInputCharacter((unsigned short)c);
            }
        });
        
        ImGui_ImplGlfw_InitForOpenGL(m_window, false); // Don't install callbacks, we did it manually
        ImGui_ImplOpenGL3_Init("#version 330");

        // WORKAROUND: Ensure the window has input focus
        glfwFocusWindow(m_window);
        
        std::printf("[ImGuiHost] Manually installed keyboard callbacks and KeyMap\n");

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

   /* TaskRunner* ImGuiHost::getTaskRunner() {
        return taskRunner;
    }*/

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

    
    void ImGuiHost::render()
    {
        // Host does not render content; plugins do.
        if (m_diagStdout) {
            std::printf("[ImGuiHost] render() frame=%llu\n",
                (unsigned long long)m_frameIndex);
        }
        //taskRunner.get()->renderUI();

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
        printf("[ImGuiHost] setRibbonbarCallback: this=%p m_ribbonbarCallback=%p\n",
            (void*)this,
            (void*)&m_ribbonbarCallback);
        std::function<void()> empty;
        m_ribbonbarCallback.swap(empty);
        printf("[ImGuiHost] setRibbonbarCallback: old swapped, assigning new\n");
        m_ribbonbarCallback = ribbonbarCallback;
        printf("[ImGuiHost] setRibbonbarCallback: done\n");
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
        m_uiPluginHotReloadRequested = true; // auto-clear pending after reload trigger

    }
    // -----------------------------------------------------------------------
// ImGuiHost::beginFrame() — cleaned up
// Replace the existing beginFrame() in ImGuiHost.cpp with this.
// The large monolithic method is split into focused private helpers.
// -----------------------------------------------------------------------

void ImGuiHost::beginFrame()
{
    ++m_frameIndex;
    if (m_diagStdout)
        std::printf("[ImGuiHost] >>> beginFrame frame=%llu\n",
            (unsigned long long)m_frameIndex);

    


    beginFrame_PollAndNewFrame();
    beginFrame_RenderTitlebar();
    beginFrame_RenderDockSpace();

   
}

// -----------------------------------------------------------------------
// Private helpers — add declarations to ImGuiHost.h:
//
//   void beginFrame_PollAndNewFrame();
//   void beginFrame_RenderTitlebar();
//   void beginFrame_RenderDockSpace();
//   void beginFrame_RenderLogo(ImDrawList* fg, const ImVec2& windowPadding, float titlebarVerticalOffset);
//   void beginFrame_RenderMenuAndRibbon(const ImVec2& windowPadding, float titlebarVerticalOffset);
//   void beginFrame_RenderWindowTitle(const ImVec2& windowPadding, float titlebarVerticalOffset);
//   void beginFrame_RenderWindowButtons();
//   bool beginFrame_HandleDragZone(const ImVec2& windowPadding, float titlebarVerticalOffset, float titlebarHeight);
// -----------------------------------------------------------------------

void ImGuiHost::beginFrame_PollAndNewFrame()
{
    glfwMakeContextCurrent(m_window);
    glfwPollEvents();

    ImGuiIO& io = ImGui::GetIO();

    // Backend NewFrame — fork clobbers DisplaySize so we fix it after
    ImGui_ImplGlfw_NewFrame();
    ImGui_ImplOpenGL3_NewFrame();

    // Force DisplaySize (must be non-zero)
    {
        int ww = 0, wh = 0, fbw = 0, fbh = 0;
        glfwGetWindowSize(m_window, &ww, &wh);
        glfwGetFramebufferSize(m_window, &fbw, &fbh);
        if (ww <= 0) ww = 1;
        if (wh <= 0) wh = 1;
        io.DisplaySize = ImVec2((float)ww, (float)wh);
        if (ww > 0 && wh > 0)
            io.DisplayFramebufferScale = ImVec2((float)fbw / ww, (float)fbh / wh);
    }

    // Force mouse state after backend NewFrame
    {
        double mx, my;
        glfwGetCursorPos(m_window, &mx, &my);
        io.MousePos     = ImVec2((float)mx, (float)my);
        io.MouseDown[0] = glfwGetMouseButton(m_window, GLFW_MOUSE_BUTTON_LEFT)   == GLFW_PRESS;
        io.MouseDown[1] = glfwGetMouseButton(m_window, GLFW_MOUSE_BUTTON_RIGHT)  == GLFW_PRESS;
        io.MouseDown[2] = glfwGetMouseButton(m_window, GLFW_MOUSE_BUTTON_MIDDLE) == GLFW_PRESS;
    }

 
    ImGui::NewFrame();


    ImVec2 szMin, szMax, szRes, szClose;

    bool ok1 = CreateTextureFromEmbeddedPng(g_WindowMinimizeIcon, (int)sizeof(g_WindowMinimizeIcon), m_glTexMinimize, m_iconMinimize, szMin);
    bool ok2 = CreateTextureFromEmbeddedPng(g_WindowMaximizeIcon, (int)sizeof(g_WindowMaximizeIcon), m_glTexMaximize, m_iconMaximize, szMax);
    bool ok3 = CreateTextureFromEmbeddedPng(g_WindowRestoreIcon, (int)sizeof(g_WindowRestoreIcon), m_glTexRestore, m_iconRestore, szRes);
    bool ok4 = CreateTextureFromEmbeddedPng(g_WindowCloseIcon, (int)sizeof(g_WindowCloseIcon), m_glTexClose, m_iconClose, szClose);

    if (!(ok1 && ok2 && ok3 && ok4))
        return;

    // Pick a consistent button icon size (you can scale in draw code too)
    ImVec2 m_iconSize = ImVec2(16, 16);

    // Push to host

    setWindowControlIcons(
        m_iconMinimize,
        m_iconMaximize,
        m_iconRestore,
        m_iconClose,
        m_iconSize
    );
}

void ImGuiHost::beginFrame_RenderTitlebar()
{
    constexpr float k_titlebarHeight        = 96.0f;
    const bool      isMaximized             = IsMaximized();
    const float     titlebarVerticalOffset  = isMaximized ? 6.0f : 6.0f;

    // Position titlebar window at top of viewport
    ImGuiViewport* vp = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(vp->Pos);
    ImGui::SetNextWindowSize(ImVec2(vp->Size.x, k_titlebarHeight));
    ImGui::SetNextWindowViewport(vp->ID);

    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    //ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));

    constexpr ImGuiWindowFlags k_flags =
        ImGuiWindowFlags_NoTitleBar   |
        ImGuiWindowFlags_NoResize     |
        ImGuiWindowFlags_NoMove       |
        ImGuiWindowFlags_NoScrollbar  |
        ImGuiWindowFlags_NoScrollWithMouse |
        ImGuiWindowFlags_NoDocking;

    ImGui::Begin("##Titlebar", nullptr, k_flags);
    {
        const ImVec2 windowPadding = ImGui::GetCurrentWindow()->WindowPadding;

        // Background fill
        {
            ImGui::SetCursorPos(ImVec2(windowPadding.x, windowPadding.y + titlebarVerticalOffset));
            const ImVec2 tMin = ImGui::GetCursorScreenPos();
            const ImVec2 tMax = {
                tMin.x + ImGui::GetWindowWidth() - windowPadding.y * 2.0f,
                tMin.y + k_titlebarHeight
            };
            ImGui::GetBackgroundDrawList()->AddRectFilled(tMin, tMax, UI::Colors::Theme::titlebar);
            //ImGui::GetBackgroundDrawList()->AddRectFilled(tMin, tMax, UI::Colors::ColorWithMultipliedValue(IM_COL32(255, 0, 0, 255),0));
        }

        beginFrame_RenderLogo(ImGui::GetForegroundDrawList(), windowPadding, titlebarVerticalOffset);

        beginFrame_HandleDragZone(windowPadding, titlebarVerticalOffset, k_titlebarHeight);

        beginFrame_RenderMenuAndRibbon(windowPadding, titlebarVerticalOffset);

        beginFrame_RenderWindowTitle(windowPadding, titlebarVerticalOffset);

        beginFrame_RenderWindowButtons();
    }
    ImGui::End();
    ImGui::PopStyleVar(2);
}

void ImGuiHost::beginFrame_RenderLogo(
    ImDrawList* fg,
    const ImVec2& windowPadding,
    float titlebarVerticalOffset)
{
    if (!m_AppHeaderIcon || m_AppHeaderIcon->GetTextureID() == 0)
        return;

    constexpr float k_logoW = 48.0f;
    constexpr float k_logoH = 48.0f;

    const ImVec2 offset(16.0f + windowPadding.x, 5.0f + windowPadding.y + titlebarVerticalOffset);
    const ImVec2 rMin = { ImGui::GetItemRectMin().x + offset.x, ImGui::GetItemRectMin().y + offset.y };
    const ImVec2 rMax = { rMin.x + k_logoW, rMin.y + k_logoH };

    fg->AddImage(m_AppHeaderIcon->GetDescriptorSet(), rMin, rMax);
}



void ImGuiHost::beginFrame_HandleDragZone(
    const ImVec2& windowPadding,
    float titlebarVerticalOffset,
    float titlebarHeight)
{
    ImGuiViewport* vp = ImGui::GetMainViewport();
//
//    const float w               = ImGui::GetContentRegionAvail().x;
//    constexpr float k_btnArea   = 94.0f;
//
//    ImGui::SetCursorPos(ImVec2(windowPadding.x, windowPadding.y + titlebarVerticalOffset));
//    ImGui::InvisibleButton("##titleBarDragZone", ImVec2(w - k_btnArea, titlebarHeight));
//
//
//
//    ImVec2 min = ImGui::GetItemRectMin();
//    ImVec2 max = ImGui::GetItemRectMax();
//
//    ImGui::GetForegroundDrawList()->AddRect(
//        min,
//        max,
//        IM_COL32(255, 0, 0, 255)
//    );
//
//    ImGui::SetItemAllowOverlap();
//    m_TitleBarHovered = ImGui::IsItemHovered();
//
//    const bool dragClicked = ImGui::IsItemClicked(ImGuiMouseButton_Left);
//
    // Suppress drag when mouse is over menu or ribbon strip
    const ImVec2  mouse             = ImGui::GetMousePos();
    const float   logoOffset        = 16.0f * 2.0f + 48.0f + windowPadding.x;
    const float   menuTopY          = vp->Pos.y + windowPadding.y + titlebarVerticalOffset;
    const float   menuH             = ImGui::GetFrameHeightWithSpacing();
    const bool    overMenuOrRibbon  =
        (mouse.x >= vp->Pos.x + logoOffset) &&
        (mouse.y >= menuTopY) &&
        (mouse.y <= menuTopY + menuH * 2.0f);
//
//#ifdef _WIN32
//    if (dragClicked && !overMenuOrRibbon)
//    {
//        ReleaseCapture();
//        SendMessage(glfwGetWin32Window(m_window), WM_NCLBUTTONDOWN, HTCAPTION, 0);
//    }
//#endif



    if (ImGui::IsMouseClicked(ImGuiMouseButton_Left))
    {
        ImVec2 mouse = ImGui::GetMousePos();

        bool inTitleBar =
            mouse.y >= vp->Pos.y &&
            mouse.y <= vp->Pos.y + titlebarHeight;

        if (inTitleBar && !overMenuOrRibbon)
        {
            ReleaseCapture();
            SendMessage(
                glfwGetWin32Window(m_window),
                WM_NCLBUTTONDOWN,
                HTCAPTION,
                0);
        }
    }
}
void ImGuiHost::beginFrame_RenderMenuAndRibbon(
    const ImVec2& windowPadding,
    float         titlebarVerticalOffset)
{
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(14.0f, 6.0f));

    const float logoOffset = 16.0f * 2.0f + 48.0f + windowPadding.x;
    const float menubarOffset = 3.0f;
    ImGui::SuspendLayout();
    
    ImGui::SetItemAllowOverlap();

    titlebarVerticalOffset = titlebarVerticalOffset + menubarOffset;
    ImGui::SetCursorPos(ImVec2(logoOffset, titlebarVerticalOffset));
    {
        const ImRect menuRect = {
            ImGui::GetCursorPos(),
            { ImGui::GetContentRegionAvail().x + ImGui::GetCursorScreenPos().x,
              ImGui::GetFrameHeightWithSpacing() }    
        };
        ImGui::BeginGroup();

        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(14.0f, 10.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(6.0f, 8.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(6.0f, 8.0f));

        if (HostUI::BeginMenubar(menuRect))
        {
            m_registry.renderMenuBar();
        }
        HostUI::EndMenubar();
        ImGui::PopStyleVar(3);
        ImGui::EndGroup();
    }

    ImGui::PopStyleVar();

    //ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(1.0f, 0.3f, 0.3f, 1.0f)); // red border, this button only
    ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 2.0f);

    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(14.0f, 6.0f));
    // Ribbon row
    {
        titlebarVerticalOffset = titlebarVerticalOffset + 12.0f + ImGui::GetFrameHeightWithSpacing();
        ImGui::SetCursorPos(ImVec2(
            logoOffset, titlebarVerticalOffset ));

        ImGui::BeginGroup();
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(14.0f, 10.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(6.0f, 4.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(6.0f, 4.0f));

        // Host ribbon items via callback
        //if (m_ribbonbarCallback)
        //    m_ribbonbarCallback();

        // Plugin ribbon contributions via registry
        m_registry.renderRibbonBar();

        ImGui::PopStyleVar(3);
        ImGui::EndGroup();
    }
    ImGui::PopStyleVar();
    ImGui::PopStyleVar();
    //ImGui::PopStyleColor();

    if (ImGui::IsItemHovered())
        m_TitleBarHovered = false;

    ImGui::ResumeLayout();
}


//void ImGuiHost::beginFrame_RenderMenuAndRibbon(
//    const ImVec2& windowPadding,
//    float         titlebarVerticalOffset)
//{
//    const float logoOffset = 16.0f * 2.0f + 48.0f + windowPadding.x;
// 
//    ImGui::SuspendLayout();
//    ImGui::SetItemAllowOverlap();
// 
//    // Menubar row
//    ImGui::SetCursorPos(ImVec2(logoOffset, 6.0f + titlebarVerticalOffset));
//    {
//        const ImRect menuRect = {
//            ImGui::GetCursorPos(),
//            { ImGui::GetContentRegionAvail().x + ImGui::GetCursorScreenPos().x,
//              ImGui::GetFrameHeightWithSpacing() }
//        };
//        ImGui::BeginGroup();
//        if (HostUI::BeginMenubar(menuRect))
//        {
//            // Host menus first (File, Views, Plugins etc.) via callback
//            if (m_menubarCallback)
//                m_menubarCallback();
// 
//            // Then plugin-contributed menus via registry
//            m_registry.renderMenuBar();
//        }
//        HostUI::EndMenubar();
//        ImGui::EndGroup();
//    }
// 
//    // Ribbon row
//    {
//        ImGui::SetCursorPos(ImVec2(
//            logoOffset,
//            6.0f + titlebarVerticalOffset + ImGui::GetFrameHeightWithSpacing()));
// 
//        ImGui::BeginGroup();
//        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(6.0f, 4.0f));
//        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing,  ImVec2(6.0f, 4.0f));
// 
//        // Host ribbon items via callback
//        if (m_ribbonbarCallback)
//            m_ribbonbarCallback();
// 
//        // Plugin ribbon contributions via registry
//        m_registry.renderRibbonBar();
// 
//        ImGui::PopStyleVar(2);
//        ImGui::EndGroup();
//    }
// 
//    if (ImGui::IsItemHovered())
//        m_TitleBarHovered = false;
// 
//    ImGui::ResumeLayout();
//}
// 
//void ImGuiHost::beginFrame_RenderMenuAndRibbon(
//    const ImVec2& windowPadding,
//    float titlebarVerticalOffset)
//{
//    if (!m_menubarCallback)
//        return;
//
//    const float logoOffset = 16.0f * 2.0f + 48.0f + windowPadding.x;
//
//    ImGui::SuspendLayout();
//    ImGui::SetItemAllowOverlap();
//
//    // Menubar row
//    ImGui::SetCursorPos(ImVec2(logoOffset, 6.0f + titlebarVerticalOffset));
//    {
//        const ImRect menuRect = {
//            ImGui::GetCursorPos(),
//            { ImGui::GetContentRegionAvail().x + ImGui::GetCursorScreenPos().x,
//              ImGui::GetFrameHeightWithSpacing() }
//        };
//        ImGui::BeginGroup();
//        if (HostUI::BeginMenubar(menuRect))
//            m_menubarCallback();
//        HostUI::EndMenubar();
//        ImGui::EndGroup();
//    }
//
//    // Ribbon row
//    if (m_ribbonbarCallback)
//    {
//        ImGui::SetCursorPos(ImVec2(
//            logoOffset,
//            6.0f + titlebarVerticalOffset + ImGui::GetFrameHeightWithSpacing()));
//
//        ImGui::BeginGroup();
//        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(6.0f, 4.0f));
//        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing,  ImVec2(6.0f, 4.0f));
//        m_ribbonbarCallback();
//        ImGui::PopStyleVar(2);
//        ImGui::EndGroup();
//    }
//
//    if (ImGui::IsItemHovered())
//        m_TitleBarHovered = false;
//
//    ImGui::ResumeLayout();
//}

void ImGuiHost::beginFrame_RenderWindowTitle(
    const ImVec2& windowPadding,
    float titlebarVerticalOffset)
{
    const ImVec2 saved  = ImGui::GetCursorPos();
    const ImVec2 tSize  = ImGui::CalcTextSize(m_windowTitle.c_str());
    ImGui::SetCursorPos(ImVec2(
        ImGui::GetWindowWidth() * 0.5f - tSize.x * 0.5f,
        2.0f + windowPadding.y + 10.0f));
    ImGui::TextUnformatted(m_windowTitle.c_str());
    ImGui::SetCursorPos(saved);
}

void ImGuiHost::beginFrame_RenderWindowButtons()
{
    const ImU32 colN = UI::Colors::ColorWithMultipliedValue(UI::Colors::Theme::text, 0.9f);
    const ImU32 colH = UI::Colors::ColorWithMultipliedValue(UI::Colors::Theme::text, 1.2f);
    const ImU32 colP = UI::Colors::Theme::textDarker;

    const float iconW = (m_iconSize.x > 0.0f) ? m_iconSize.x : 16.0f;
    const float iconH = (m_iconSize.y > 0.0f) ? m_iconSize.y : 16.0f;
    constexpr float k_gap          = 8.0f;
    constexpr float k_rightPadding = 16.0f;

    ImGui::SetCursorPosY(ImGui::GetFrameHeightWithSpacing() / 2);

    auto layout = HostUI::BeginHorizontal();
    HostUI::Spring(layout, k_rightPadding + iconW * 3.0f + k_gap * 2.0f);
    HostUI::ShiftCursorY(8.0f);

    ImGui::BeginGroup();

    // Minimize
    if (ImGui::InvisibleButton("Minimize", ImVec2(iconW, iconH)))
        if (m_window) glfwIconifyWindow(m_window);
    HostUI::DrawButtonImage(m_iconMinimize, colN, colH, colP);

    ImGui::SameLine(0.0f, k_gap);

    // Maximize / Restore
    const bool isMax = IsMaximized();
    if (ImGui::InvisibleButton("Maximize", ImVec2(iconW, iconH)))
        isMax ? glfwRestoreWindow(m_window) : glfwMaximizeWindow(m_window);
    HostUI::DrawButtonImage(isMax ? m_iconRestore : m_iconMaximize, colN, colH, colP);

    ImGui::SameLine(0.0f, k_gap);

    // Close
    if (ImGui::InvisibleButton("Close", ImVec2(iconW, iconH)))
        glfwSetWindowShouldClose(m_window, GLFW_TRUE);
    HostUI::DrawButtonImage(
        m_iconClose,
        UI::Colors::Theme::text,
        UI::Colors::ColorWithMultipliedValue(UI::Colors::Theme::text, 1.4f),
        colP);

    ImGui::EndGroup();
}

void ImGuiHost::beginFrame_RenderDockSpace()
{
    const float k_titlebarHeight = 96.0f;

    ImGuiViewport* vp = ImGui::GetMainViewport();

    ImGui::SetNextWindowPos(ImVec2(vp->Pos.x, vp->Pos.y + k_titlebarHeight));
    ImGui::SetNextWindowSize(ImVec2(vp->Size.x, vp->Size.y - k_titlebarHeight));
    ImGui::SetNextWindowViewport(vp->ID);

    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding,   0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 2.0f);

    const bool isMax = IsMaximized();
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,
        isMax ? ImVec2(10.0f, 10.0f) : ImVec2(10.0f, 10.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 3.0f);
    ImGui::PushStyleColor(ImGuiCol_MenuBarBg, ImVec4(0, 0, 0, 0));

    constexpr ImGuiWindowFlags k_flags =
        ImGuiWindowFlags_NoDocking          |
        ImGuiWindowFlags_NoTitleBar         |
        ImGuiWindowFlags_MenuBar            |
        ImGuiWindowFlags_NoCollapse         |
        ImGuiWindowFlags_NoResize           |
        ImGuiWindowFlags_NoMove             |
        ImGuiWindowFlags_NoBringToFrontOnFocus |
        ImGuiWindowFlags_NoNavFocus;

    ImGui::Begin("DockSpaceWindow", nullptr, k_flags);
    ImGui::PopStyleColor(); // MenuBarBg
    ImGui::PopStyleVar(4);

    ImGui::DockSpace(
        ImGui::GetID("MainDockspace"),
        ImVec2(0.0f, 0.0f),
        ImGuiDockNodeFlags_PassthruCentralNode);
    ImGui::End();
}
} // namespace adapters
