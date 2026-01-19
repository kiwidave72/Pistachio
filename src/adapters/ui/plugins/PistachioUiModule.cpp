#include "core/Application.h"
#include "adapters/ui/ImGuiAdapter.h"
#include "adapters/ui/IGuiHost.h"
#include "adapters/ui/plugins/UiModuleApi.h"

#include <GLFW/glfw3.h>
#include <imgui.h>

// ------------------------------
// Concrete module implementation
// ------------------------------
class PistachioUiModule final : public IUiModule {
public:
    PistachioUiModule() = default;
    ~PistachioUiModule() override = default;

    void onLoad(UiHostServices& svc) override {
        if (m_adapter) return;

        if (ImGui::GetCurrentContext() == nullptr) {
            // if this triggers, plugin is being loaded before host CreateContext()
            printf("[UI] onLoad: no ImGui context\n");
        }
        auto* app = reinterpret_cast<core::Application*>(svc.app);
        auto* win = reinterpret_cast<GLFWwindow*>(svc.window);
        auto* host = reinterpret_cast<IGuiHost*>(svc.guiHost);

        m_adapter = new adapters::ImGuiAdapter(app, win, host);

        // IMPORTANT: only do resource/font setup BEFORE first NewFrame
        // Your host must call module->onLoad() before ImGui::NewFrame().
        m_adapter->initializeResources();
    }

    void onUnload(UiHostServices&) override {
        delete m_adapter;
        m_adapter = nullptr;
    }

    void render(UiHostServices&) override {
        if (!m_adapter) return;
        IM_ASSERT(ImGui::GetCurrentContext() != nullptr);
        IM_ASSERT(ImGui::GetFrameCount() >= 0); // should be increasing each frame

        ImGui::Begin("UI Plugin Alive");
        ImGui::Text("FrameCount: %d", ImGui::GetFrameCount());
        ImGui::End();

        m_adapter->render();
    }

private:
    adapters::ImGuiAdapter* m_adapter = nullptr;
};

// ------------------------------
// Required exports the loader wants
// ------------------------------
PISTACHIO_UI_EXPORT IUiModule* pistachio_create_ui_module()
{
    return new PistachioUiModule();
}

PISTACHIO_UI_EXPORT void pistachio_destroy_ui_module(IUiModule* m)
{
    delete m;
}
