#include "core/Application.h"
#include "adapters/ui/ImGuiAdapter.h"
#include "adapters/ui/IGuiHost.h"
#include "adapters/ui/plugins/UiModuleApi.h"
#include "ports/IConfigPort.h"

#include <GLFW/glfw3.h>
#include <imgui.h>
#include <cstdio>
#include <string>
#include <vector>
#include <map>
#include <algorithm>
#include <cctype>

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

        // ------------------------------
        // Unreal-style Settings Window
        // ------------------------------
        nlohmann::json jOpen = m_config->get("pistachio.ui", "config.windowOpen");
        bool open = jOpen.is_boolean() ? jOpen.get<bool>() : true;
        if (!open)
            return;

        // Persisted UI state
        nlohmann::json jAdv = m_config->get("pistachio.ui", "config.showAdvanced");
        bool showAdvanced = jAdv.is_boolean() ? jAdv.get<bool>() : false;

        static float s_leftWidth = 300.0f;
        static char s_search[256] = {};
        static bool s_onlyModified = false;
        static std::string s_selectedNs;

        auto toLower = [](std::string v) {
            std::transform(v.begin(), v.end(), v.begin(), [](unsigned char c) { return (char)std::tolower(c); });
            return v;
        };

        auto matchesFilter = [&](const ports::SettingInfo& s) -> bool {
            if (!showAdvanced && s.advanced) return false;

            if (s_onlyModified)
            {
                nlohmann::json cur = m_config->get(s.ns, s.key);
                if (cur == s.defaultValue) return false;
            }

            if (s_search[0] == 0) return true;

            std::string f = toLower(std::string(s_search));
            std::string hay = s.ns + " " + s.group + " " + s.key + " " + s.displayName + " " + s.description;
            hay = toLower(hay);
            return hay.find(f) != std::string::npos;
        };

        auto splitNs = [](const std::string& ns) {
            std::vector<std::string> parts;
            std::string cur;
            for (char c : ns) {
                if (c == '.') {
                    if (!cur.empty()) parts.push_back(cur);
                    cur.clear();
                } else cur.push_back(c);
            }
            if (!cur.empty()) parts.push_back(cur);
            return parts;
        };

        auto rootForNs = [](const std::string& ns) -> std::string {
            if (ns.rfind("plugin.", 0) == 0) return "Plugins";
            // Default everything else to Editor to match Unreal-style "Editor Preferences"
            return "Editor";
        };

        auto splitterV = [](float thickness, float* left, float minLeft, float minRight) {
            ImVec2 avail = ImGui::GetContentRegionAvail();
            float right = avail.x - (*left) - thickness;
            if (right < minRight) right = minRight;

            ImGui::SameLine();
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(1, 1, 1, 0.06f));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(1, 1, 1, 0.10f));
            ImGui::Button("##cfg_splitter", ImVec2(thickness, -1));
            ImGui::PopStyleColor(3);

            if (ImGui::IsItemActive()) {
                float delta = ImGui::GetIO().MouseDelta.x;
                float newLeft = *left + delta;
                float newRight = avail.x - newLeft - thickness;
                if (newLeft >= minLeft && newRight >= minRight)
                    *left = newLeft;
            }
            ImGui::SameLine();
        };

        auto drawValueEditor = [&](const ports::SettingInfo& s) {
            nlohmann::json val = m_config->get(s.ns, s.key);
            bool modified = (val != s.defaultValue);

            // Right column: control + reset + modified mark
            ImGui::PushID((s.ns + "::" + s.key).c_str());

            // Control width tries to look like Unreal "details panel"
            ImGui::SetNextItemWidth(-60.0f);

            switch (s.type)
            {
            case ports::SettingType::Bool:
            {
                bool b = val.is_boolean() ? val.get<bool>() : (s.defaultValue.is_boolean() ? s.defaultValue.get<bool>() : false);
                if (ImGui::Checkbox("##v", &b))
                    m_config->set(s.ns, s.key, b);
                break;
            }
            case ports::SettingType::Int:
            {
                int i = val.is_number_integer() ? val.get<int>() : (s.defaultValue.is_number_integer() ? s.defaultValue.get<int>() : 0);
                if (ImGui::DragInt("##v", &i, 1.0f))
                    m_config->set(s.ns, s.key, i);
                break;
            }
            case ports::SettingType::Float:
            {
                float f = 0.0f;
                if (val.is_number()) f = (float)val.get<double>();
                else if (s.defaultValue.is_number()) f = (float)s.defaultValue.get<double>();
                if (ImGui::DragFloat("##v", &f, 0.01f))
                    m_config->set(s.ns, s.key, f);
                break;
            }
            case ports::SettingType::String:
            {
                std::string str = val.is_string() ? val.get<std::string>() : (s.defaultValue.is_string() ? s.defaultValue.get<std::string>() : "");
                char buf[512] = {};
                std::snprintf(buf, sizeof(buf), "%s", str.c_str());
                if (ImGui::InputText("##v", buf, sizeof(buf)))
                    m_config->set(s.ns, s.key, std::string(buf));
                break;
            }
            case ports::SettingType::Json:
            default:
            {
                std::string str = val.is_null() ? std::string("null") : val.dump();
                if (str.size() > 100) str = str.substr(0, 100) + "...";
                ImGui::TextUnformatted(str.c_str());
                break;
            }
            }

            ImGui::SameLine();

            // Reset button (Unreal has a little arrow icon; we keep it simple)
            if (ImGui::SmallButton("Reset"))
                m_config->resetToDefault(s.ns, s.key);

            if (modified) {
                ImGui::SameLine();
                ImGui::TextUnformatted("*");
                if (ImGui::IsItemHovered())
                    ImGui::SetTooltip("Modified (differs from default)");
            }

            ImGui::PopID();
        };

        ImGui::Begin("Settings", &open);
        if (!open)
        {
            m_config->set("pistachio.ui", "config.windowOpen", false);
            ImGui::End();
            return;
        }

        // --- Top toolbar (Unreal-ish) ---
        ImGui::TextUnformatted("Settings");
        ImGui::SameLine();
        ImGui::SetNextItemWidth(420.0f);
        ImGui::InputTextWithHint("##cfgSearch", "Search settings...", s_search, sizeof(s_search));

        ImGui::SameLine();
        if (ImGui::Checkbox("Show Advanced", &showAdvanced))
            m_config->set("pistachio.ui", "config.showAdvanced", showAdvanced);

        ImGui::SameLine();
        ImGui::Checkbox("Only Modified", &s_onlyModified);

        ImGui::SameLine();
        if (ImGui::Button("Save"))
        {
            if (m_app) m_app->saveConfigNow();
        }

        ImGui::SameLine();
        if (ImGui::Button("Reset Filtered"))
        {
            auto all = m_config->listSettings();
            for (const auto& s : all)
            {
                if (!matchesFilter(s)) continue;
                m_config->resetToDefault(s.ns, s.key);
            }
        }

        ImGui::Separator();

        // Gather settings once per frame
        auto allSettings = m_config->listSettings();

        // Ensure selection exists
        if (s_selectedNs.empty())
        {
            for (const auto& s : allSettings)
            {
                if (!matchesFilter(s)) continue;
                s_selectedNs = s.ns;
                break;
            }
        }

        // Build namespace lists by root
        std::map<std::string, std::vector<std::string>> roots;
        for (const auto& s : allSettings)
        {
            if (!matchesFilter(s)) continue;
            roots[rootForNs(s.ns)].push_back(s.ns);
        }
        for (auto& [root, list] : roots)
        {
            std::sort(list.begin(), list.end());
            list.erase(std::unique(list.begin(), list.end()), list.end());
        }

        // --- Left / Right panels ---
        ImVec2 avail = ImGui::GetContentRegionAvail();
        float rightMin = 420.0f;
        if (s_leftWidth > avail.x - rightMin) s_leftWidth = std::max(240.0f, avail.x - rightMin);

        ImGui::BeginChild("##cfg_left", ImVec2(s_leftWidth, 0), true);
        {
            // Sidebar tree
            for (auto& [root, list] : roots)
            {
                ImGuiTreeNodeFlags rootFlags = ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanFullWidth;
                bool rootOpen = ImGui::TreeNodeEx(root.c_str(), rootFlags);

                if (rootOpen)
                {
                    // Build a dotted-namespace tree under each root (Editor/Plugins)
                    // Simple: show 1 level label (strip common prefix like "pistachio.")
                    for (const auto& ns : list)
                    {
                        std::string label = ns;
                        if (label.rfind("pistachio.", 0) == 0)
                            label = label.substr(std::string("pistachio.").size());

                        ImGuiTreeNodeFlags nsFlags = ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen | ImGuiTreeNodeFlags_SpanFullWidth;
                        if (ns == s_selectedNs) nsFlags |= ImGuiTreeNodeFlags_Selected;

                        ImGui::TreeNodeEx((label + "##" + ns).c_str(), nsFlags);
                        if (ImGui::IsItemClicked())
                            s_selectedNs = ns;
                    }
                    ImGui::TreePop();
                }
            }
        }
        ImGui::EndChild();

        splitterV(6.0f, &s_leftWidth, 220.0f, 420.0f);

        ImGui::BeginChild("##cfg_right", ImVec2(0, 0), true);
        {
            // Details header
            std::string root = rootForNs(s_selectedNs);
            std::string label = s_selectedNs;
            if (label.rfind("pistachio.", 0) == 0)
                label = label.substr(std::string("pistachio.").size());

            ImGui::Text("%s / %s", root.c_str(), label.c_str());
            ImGui::Separator();

            // Group settings by group name (collapsible headers)
            std::map<std::string, std::vector<ports::SettingInfo>> groups;
            for (const auto& s : allSettings)
            {
                if (s.ns != s_selectedNs) continue;
                if (!matchesFilter(s)) continue;
                groups[s.group].push_back(s);
            }

            if (groups.empty())
            {
                ImGui::TextDisabled("No settings match your filter.");
            }
            else
            {
                for (auto& [group, vec] : groups)
                {
                    std::string groupLabel = group.empty() ? "General" : group;
                    std::sort(vec.begin(), vec.end(), [](const auto& a, const auto& b) {
                        return a.displayName < b.displayName;
                    });

                    ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_DefaultOpen;
                    if (ImGui::CollapsingHeader(groupLabel.c_str(), flags))
                    {
                        // 2-col table: name/desc | value
                        if (ImGui::BeginTable(("##tbl_" + groupLabel).c_str(), 2,
                            ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_SizingStretchProp))
                        {
                            ImGui::TableSetupColumn("Setting", ImGuiTableColumnFlags_WidthStretch, 0.60f);
                            ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch, 0.40f);

                            for (const auto& s : vec)
                            {
                                ImGui::TableNextRow();

                                ImGui::TableSetColumnIndex(0);
                                // Label + description like Unreal
                                ImGui::TextUnformatted(s.displayName.empty() ? s.key.c_str() : s.displayName.c_str());
                                if (!s.description.empty())
                                {
                                    ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
                                    ImGui::TextWrapped("%s", s.description.c_str());
                                    ImGui::PopStyleColor();
                                }
                                ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
                                ImGui::Text("%s", s.key.c_str());
                                ImGui::PopStyleColor();

                                ImGui::TableSetColumnIndex(1);
                                drawValueEditor(s);
                            }

                            ImGui::EndTable();
                        }
                    }
                }
            }
        }
        ImGui::EndChild();

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
