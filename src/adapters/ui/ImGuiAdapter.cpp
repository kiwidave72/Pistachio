#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <GL/gl.h>

#include "adapters/ui/ImGuiAdapter.h"
#include "core/Application.h"

#include <imgui.h>
#include <imgui_internal.h>
#include <GLFW/glfw3.h>
#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3native.h>

#include <cstring>
#include <algorithm>

#include "../Roboto-Regular.embed"
#include "../../../Walnut-Icon.embed"
#include "../../../WindowImages.embed"

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h" // or wherever you include it (likely already in your project)

static GLuint CreateGLTextureRGBA_Minimal(const unsigned char* rgba, int w, int h)
{
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



namespace adapters {

    // ------------------------------------------------------------
    // ctor / dtor
    // ------------------------------------------------------------

    ImGuiAdapter::ImGuiAdapter(core::Application* app, GLFWwindow* hostWindow, IGuiHost* host)
        : m_app(app), m_window(hostWindow), m_host(host)
    {
    }

    ImGuiAdapter::~ImGuiAdapter() = default;

    // ------------------------------------------------------------
    // SAFE ONE-TIME RESOURCE INIT (fonts etc)
    // ------------------------------------------------------------

    void ImGuiAdapter::initializeResources()
    {
        if (m_resourcesInitialized)
            return;

        ImGuiIO& io = ImGui::GetIO();

        ImFontConfig cfg;
        cfg.FontDataOwnedByAtlas = false;

        // Default font
        if (!io.FontDefault)
        {
            io.FontDefault = io.Fonts->AddFontFromMemoryTTF(
                (void*)g_RobotoRegular,
                sizeof(g_RobotoRegular),
                17.0f,
                &cfg
            );
        }

        // Small UI font
        if (!m_smallFont)
        {
            m_smallFont = io.Fonts->AddFontFromMemoryTTF(
                (void*)g_RobotoRegular,
                sizeof(g_RobotoRegular),
                14.0f,
                &cfg
            );
        }
        
            
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
        


        m_resourcesInitialized = true;
    }

    void ImGuiAdapter::setWindowControlIcons(
        ImTextureID minimize,
        ImTextureID maximize,
        ImTextureID restore,
        ImTextureID close,
        ImVec2 size
    )
    {
        if (m_host) {
            m_host->setWindowControlIcons(minimize, maximize, restore, close, size);
        }
    }

    // ------------------------------------------------------------
    // FRAME RENDER (NO FONT MUTATION HERE)
    // ------------------------------------------------------------

    void ImGuiAdapter::render()
    {
        printf("[PLUGIN] ctx=%p\n", (void*)ImGui::GetCurrentContext());

        // ⚠️ DO NOT touch ImGuiIO.Fonts here

        renderMainMenu();
        renderStatusBar();
        renderModelInfo();
        render3DView();
        renderSketchEditor();
    }

    // ------------------------------------------------------------
    // UI SECTIONS (unchanged behavior)
    // ------------------------------------------------------------

    void ImGuiAdapter::setMenubarCallback(const std::function<void()>& cb)
    {
        m_MenubarCallback = cb;
    }

    void ImGuiAdapter::renderMainMenu()
    {
        ImGui::Begin("File Operations");

        static char filePath[512] = {};
        ImGui::InputTextWithHint("##file", "STEP file path...", filePath, sizeof(filePath));

        if (ImGui::Button("Load STEP"))
        {
            if (m_app && filePath[0])
                m_app->loadFile(filePath);
        }

        ImGui::End();
    }

    void ImGuiAdapter::renderStatusBar()
    {
        ImGui::Begin("Status", nullptr, ImGuiWindowFlags_NoScrollbar);

        if (m_app)
            ImGui::Text("Status: %s", m_app->getStatus().c_str());

        ImGui::End();
    }

    void ImGuiAdapter::renderModelInfo()
    {
        ImGui::Begin("Model Info");

        if (!m_app)
        {
            ImGui::TextDisabled("No application");
            ImGui::End();
            return;
        }

        auto model = m_app->getCurrentModel();
        if (!model)
        {
            ImGui::TextDisabled("No model loaded");
            ImGui::End();
            return;
        }

        ImGui::Text("Model loaded");
        ImGui::End();
    }

    void ImGuiAdapter::render3DView()
    {
        ImGui::Begin("3D Viewport");

        if (m_app && m_app->getRenderer())
        {
            ImVec2 size = ImGui::GetContentRegionAvail();
            void* tex = m_app->getRenderer()->getFramebufferTexture();
            if (tex)
                ImGui::Image(tex, size);
        }
        else
        {
            ImGui::TextDisabled("No renderer");
        }

        ImGui::End();
    }

    void ImGuiAdapter::renderSketchEditor()
    {
        ImGui::Begin("Sketch Editor");
        ImGui::Text("Sketch UI goes here");
        ImGui::End();
    }

} // namespace adapters
