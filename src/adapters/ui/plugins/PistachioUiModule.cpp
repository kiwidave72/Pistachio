#include "core/Application.h"
#include "adapters/ui/ImGuiAdapter.h"
#include "adapters/ui/IGuiHost.h"
#include "adapters/ui/plugins/UiModuleApi.h"
#include "ports/IConfigPort.h"

#include <GLFW/glfw3.h>
#include <imgui.h>
#include <cstdio>

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
        m_app = app;
        auto* win = reinterpret_cast<GLFWwindow*>(svc.window);
        auto* host = reinterpret_cast<IGuiHost*>(svc.guiHost);
        auto* cfg = reinterpret_cast<ports::IConfigPort*>(svc.config);
        m_config = cfg; // host-owned

        m_adapter = new adapters::ImGuiAdapter(app, win, host);

        // Register UI-plugin settings (idempotent).
        if (cfg) {
            using ports::SettingInfo;
            using ports::SettingType;
            cfg->registerSetting(SettingInfo(
                "pistachio.ui", "config.showAdvanced", "Show advanced settings",
                "Show settings marked as advanced.", "Configuration", SettingType::Bool, false, false
            ));
            cfg->registerSetting(SettingInfo(
                "pistachio.ui", "config.windowOpen", "Config window open",
                "Whether the configuration editor window is visible.", "Configuration", SettingType::Bool, true, true
            ));
        }

        // IMPORTANT: only do resource/font setup BEFORE first NewFrame
        // Your host must call module->onLoad() before ImGui::NewFrame().
        m_adapter->initializeResources();

        // Provide ribbon bar contents via host callback (like menubar)
        if (app)
            app->setRibbonbarCallback([this]() { if (m_adapter) m_adapter->renderRibbonBar(); });
    }

    void onUnload(UiHostServices& svc) override {
        auto* app = reinterpret_cast<core::Application*>(svc.app);
        if (app)
            app->setRibbonbarCallback([](){});
        delete m_adapter;
        m_adapter = nullptr;
        m_app = nullptr;
        m_config = nullptr;
    }

    void render(UiHostServices&) override {
        if (!m_adapter) return;
        IM_ASSERT(ImGui::GetCurrentContext() != nullptr);
        IM_ASSERT(ImGui::GetFrameCount() >= 0); // should be increasing each frame

        ImGui::Begin("UI Plugin Alive");
        ImGui::Text("FrameCount: %d", ImGui::GetFrameCount());
        ImGui::End();

        m_adapter->render();

        // Render configuration editor (host-owned config).
        renderConfigEditor();
    }

private:
    adapters::ImGuiAdapter* m_adapter = nullptr;
    core::Application* m_app = nullptr;
    ports::IConfigPort* m_config = nullptr;

    void renderConfigEditor()
    {
        if (!m_config)
            return;

        // Persisted toggle
        nlohmann::json jOpen = m_config->get("pistachio.ui", "config.windowOpen");
        bool open = jOpen.is_boolean() ? jOpen.get<bool>() : true;
        if (!open)
            return;

        nlohmann::json jAdv = m_config->get("pistachio.ui", "config.showAdvanced");
        bool showAdvanced = jAdv.is_boolean() ? jAdv.get<bool>() : false;

        ImGui::Begin("Configuration", &open);

        if (!open)
        {
            m_config->set("pistachio.ui", "config.windowOpen", false);
            ImGui::End();
            return;
        }

        // Header controls
        ImGui::Checkbox("Show advanced", &showAdvanced);
        m_config->set("pistachio.ui", "config.showAdvanced", showAdvanced);
        ImGui::SameLine();
        if (ImGui::Button("Save"))
        {
            if (m_app)
                m_app->saveConfigNow();
        }

        ImGui::Separator();

        static char filter[128] = {};
        ImGui::InputTextWithHint("##cfgFilter", "Filter (namespace, key, name)...", filter, sizeof(filter));

        // Gather settings
        auto settings = m_config->listSettings();

        // Tree by namespace
        std::string currentNs;
        for (const auto& s : settings)
        {
            if (!showAdvanced && s.advanced)
                continue;

            // Simple filter
            if (filter[0])
            {
                const std::string f = filter;
                const std::string hay = s.ns + " " + s.key + " " + s.displayName + " " + s.group;
                if (hay.find(f) == std::string::npos)
                    continue;
            }

            if (s.ns != currentNs)
            {
                if (!currentNs.empty())
                    ImGui::TreePop();

                currentNs = s.ns;
                ImGui::SetNextItemOpen(true, ImGuiCond_Once);
                if (!ImGui::TreeNode(currentNs.c_str()))
                {
                    currentNs.clear();
                    continue;
                }
            }

            ImGui::PushID((s.ns + ":" + s.key).c_str());

            // Row layout
            ImGui::Text("%s", s.displayName.empty() ? s.key.c_str() : s.displayName.c_str());
            if (!s.description.empty())
            {
                ImGui::SameLine();
                ImGui::TextDisabled("(?)");
                if (ImGui::IsItemHovered())
                {
                    ImGui::BeginTooltip();
                    ImGui::TextUnformatted(s.description.c_str());
                    ImGui::EndTooltip();
                }
            }

            nlohmann::json val = m_config->get(s.ns, s.key);

            // Editor
            switch (s.type)
            {
                case ports::SettingType::Bool:
                {
                    bool b = val.is_boolean() ? val.get<bool>() : (s.defaultValue.is_boolean() ? s.defaultValue.get<bool>() : false);
                    if (ImGui::Checkbox("##value", &b))
                        m_config->set(s.ns, s.key, b);
                    break;
                }
                case ports::SettingType::Int:
                {
                    int i = val.is_number_integer() ? val.get<int>() : (s.defaultValue.is_number_integer() ? s.defaultValue.get<int>() : 0);
                    if (ImGui::InputInt("##value", &i))
                        m_config->set(s.ns, s.key, i);
                    break;
                }
                case ports::SettingType::Float:
                {
                    float f = val.is_number() ? val.get<float>() : (s.defaultValue.is_number() ? s.defaultValue.get<float>() : 0.0f);
                    if (ImGui::InputFloat("##value", &f))
                        m_config->set(s.ns, s.key, f);
                    break;
                }
                case ports::SettingType::String:
                {
                    std::string str = val.is_string() ? val.get<std::string>() : (s.defaultValue.is_string() ? s.defaultValue.get<std::string>() : "");
                    char buf[256];
                    std::snprintf(buf, sizeof(buf), "%s", str.c_str());
                    if (ImGui::InputText("##value", buf, sizeof(buf)))
                        m_config->set(s.ns, s.key, std::string(buf));
                    break;
                }
                case ports::SettingType::Json:
                default:
                {
                    std::string str = val.is_null() ? std::string("null") : val.dump();
                    if (str.size() > 240) str = str.substr(0, 240) + "...";
                    ImGui::TextUnformatted(str.c_str());
                    break;
                }
            }

            ImGui::SameLine();
            if (ImGui::Button("Reset"))
                m_config->resetToDefault(s.ns, s.key);

            ImGui::PopID();
        }

        if (!currentNs.empty())
            ImGui::TreePop();

        ImGui::End();
    }
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


// ------------------------------
// Embedded manifest
// ------------------------------
static const UiPluginManifestV1 g_manifest = {
    sizeof(UiPluginManifestV1),
    1,
    "pistachio.ui",
    "Pistachio UI",
    "0.1.0",
    "Sketching"
};

PISTACHIO_UI_EXPORT const UiPluginManifestV1* pistachio_get_ui_manifest()
{
    return &g_manifest;
}

