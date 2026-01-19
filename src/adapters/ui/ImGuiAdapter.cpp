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
