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
#include <unordered_map>
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

        m_adapter = new adapters::ImGuiAdapter(app, win, host, cfg);

        // Register UI-plugin settings (idempotent).
        if (cfg) {
            using ports::SettingInfo;
            using ports::SettingType;
            cfg->registerSetting(SettingInfo(
                "pistachio.UI", "config.showAdvanced", "Show advanced settings",
                "Show settings marked as advanced.", "Configuration", SettingType::Bool, false, false
            ));
            cfg->registerSetting(SettingInfo(
                "pistachio.UI", "config.windowOpen", "Config window open",
                "Whether the configuration editor window is visible.", "Configuration", SettingType::Bool, true, true
            ));

            // Views (dockable windows)
            cfg->registerSetting(SettingInfo(
                "pistachio.UI", "views.fileOperations", "File Operations",
                "Show/hide the File Operations window.", "Views", SettingType::Bool, true, true
            ));
            cfg->registerSetting(SettingInfo(
                "pistachio.UI", "views.status", "Status",
                "Show/hide the Status window.", "Views", SettingType::Bool, true, true
            ));
            cfg->registerSetting(SettingInfo(
                "pistachio.UI", "views.modelInfo", "Model Info",
                "Show/hide the Model Info window.", "Views", SettingType::Bool, true, true
            ));
            cfg->registerSetting(SettingInfo(
                "pistachio.UI", "views.viewport3d", "3D Viewport",
                "Show/hide the 3D Viewport window.", "Views", SettingType::Bool, true, true
            ));
            cfg->registerSetting(SettingInfo(
                "pistachio.UI", "views.sketch3dViewport", "Sketch 3D View",
                "Show/hide the Sketch 3D View window.", "Views", SettingType::Bool, true, true
            ));
            cfg->registerSetting(SettingInfo(
                "pistachio.UI", "views.sketchEditor", "Sketch Editor",
                "Show/hide the Sketch Editor window.", "Views", SettingType::Bool, true, true
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

    // ------------------------------
    // UE5-style Settings Window
    // ------------------------------
    nlohmann::json jOpen = m_config->get("pistachio.UI", "config.windowOpen");
    bool open = jOpen.is_boolean() ? jOpen.get<bool>() : true;
    if (!open)
        return;

    // Persisted UI state
    nlohmann::json jAdv = m_config->get("pistachio.UI", "config.showAdvanced");
    bool showAdvanced = jAdv.is_boolean() ? jAdv.get<bool>() : false;

    // Local UI state
    static float s_leftWidth = 220.0f;
    static char  s_search[256] = {};
    static bool  s_onlyModified = false;
    static bool  s_showIcons = true;
    static std::string s_selectedNs;

    // Text editing state (ImGui core InputText needs a persistent char buffer).
    static std::unordered_map<std::string, std::vector<char>> s_textBuf;
    static std::unordered_map<std::string, std::string> s_textLast;
    static std::unordered_map<std::string, bool> s_textEditing;


    auto* fontBody  = (m_adapter ? m_adapter->fontBody()  : nullptr);
    auto* fontGroup = (m_adapter ? m_adapter->fontGroup() : nullptr);
    auto* fontTitle = (m_adapter ? m_adapter->fontTitle() : nullptr);

    auto toLower = [](std::string v) {
        std::transform(v.begin(), v.end(), v.begin(), [](unsigned char c) { return (char)std::tolower(c); });
        return v;
    };

    auto rootForNs = [](const std::string& ns) -> std::string {
        if (ns.rfind("plugin.", 0) == 0) return "Plugins";
        // Default everything else to Editor to match Unreal-style "Editor Preferences"
        return "Editor";
    };

    auto labelForNs = [](const std::string& ns) -> std::string {
        std::string l = ns;
        if (l.rfind("pistachio.", 0) == 0) l = l.substr(std::string("pistachio.").size());
        if (l.rfind("plugin.", 0) == 0) l = l.substr(std::string("plugin.").size());
        return l;
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

    auto isModified = [&](const ports::SettingInfo& s) -> bool {
        nlohmann::json cur = m_config->get(s.ns, s.key);
        return cur != s.defaultValue;
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

    auto drawSidebarIcon = [&](ImU32 col) {
        if (!s_showIcons) return;
        ImVec2 p = ImGui::GetCursorScreenPos();
        const float sz = 10.0f;
        ImGui::GetWindowDrawList()->AddRectFilled(p, ImVec2(p.x + sz, p.y + sz), col, 2.0f);
        ImGui::Dummy(ImVec2(sz + 8.0f, sz)); // reserve space + gap
        ImGui::SameLine();
    };

    auto drawSectionHeader = [&](const char* title) {
        ImGui::Spacing();

        ImVec2 p0 = ImGui::GetCursorScreenPos();
        float w = ImGui::GetContentRegionAvail().x;
        float h = 30.0f;

        ImU32 bg = ImGui::GetColorU32(ImVec4(1, 1, 1, 0.04f));
        ImGui::GetWindowDrawList()->AddRectFilled(p0, ImVec2(p0.x + w, p0.y + h), bg, 4.0f);

        ImGui::SetCursorScreenPos(ImVec2(p0.x + 10.0f, p0.y + 6.0f));
        if (fontGroup) ImGui::PushFont(fontGroup);
        ImGui::TextUnformatted(title);
        if (fontGroup) ImGui::PopFont();

        ImGui::SetCursorScreenPos(ImVec2(p0.x, p0.y + h));
        ImGui::Dummy(ImVec2(0, 6.0f));
    };

    auto drawValueEditor = [&](const ports::SettingInfo& s) {
        nlohmann::json val = m_config->get(s.ns, s.key);
        bool modified = (val != s.defaultValue);

        ImGui::PushID((s.ns + "::" + s.key).c_str());

        // Right column: control + reset + modified mark
        // Make room for reset arrow + modified dot
        ImGui::SetNextItemWidth(-70.0f);

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
            float f = val.is_number() ? (float)val.get<double>() : (s.defaultValue.is_number() ? (float)s.defaultValue.get<double>() : 0.0f);
            if (ImGui::DragFloat("##v", &f, 0.01f))
                m_config->set(s.ns, s.key, f);
            break;
        }
        case ports::SettingType::String:
        {
            const std::string id = s.ns + "::" + s.key;
            auto& buf = s_textBuf[id];
            auto& last = s_textLast[id];
            auto& editing = s_textEditing[id];

            if (buf.empty())
                buf.assign(512, 0);

            std::string cur = val.is_string() ? val.get<std::string>()
                              : (s.defaultValue.is_string() ? s.defaultValue.get<std::string>() : std::string{});

            // If we're not actively editing, keep buffer in sync with current value.
            if (!editing)
            {
                if (last != cur)
                {
                    std::fill(buf.begin(), buf.end(), 0);
                    strncpy_s(buf.data(), buf.size(), cur.c_str(), _TRUNCATE);
                    last = cur;
                }
            }

            ImGuiInputTextFlags flags = ImGuiInputTextFlags_EnterReturnsTrue;
            const bool enter = ImGui::InputText("##v", buf.data(), buf.size(), flags);

            // Track editing state
            if (ImGui::IsItemActive())
                editing = true;

            auto commit = [&]() {
                std::string next(buf.data());
                if (next != cur)
                {
                    m_config->set(s.ns, s.key, next);
                    last = next;
                }
            };

            // Commit on Enter or when leaving the field
            if (enter)
            {
                commit();
                // Optionally drop focus like UE
                // ImGui::ClearActiveID() is not available in this ImGui fork; deactivation is handled by focus change.
                editing = false;
            }
            else if (editing && !ImGui::IsItemActive() && ImGui::IsItemDeactivated())
            {
                commit();
                editing = false;
            }

            break;
        }
case ports::SettingType::Json:
{
    // NOTE: This project uses core ImGui without imgui_stdlib overloads,
    // so we keep a local char buffer for multiline JSON editing.
    static std::string lastId;
    static std::string jsonText;
    static std::vector<char> jsonBuf;

    const std::string id = s.ns + "::" + s.key;
    if (lastId != id)
    {
        lastId = id;
        jsonText = val.dump(2);
        const size_t cap = std::max<size_t>(4096, jsonText.size() + 1024);
        jsonBuf.assign(cap, 0);
        strncpy_s(jsonBuf.data(), jsonBuf.size(), jsonText.c_str(), _TRUNCATE);
    }

    ImGui::SetNextItemWidth(-1);
    if (ImGui::InputTextMultiline("##v", jsonBuf.data(), jsonBuf.size(), ImVec2(-1.0f, 140.0f)))
    {
        jsonText = std::string(jsonBuf.data());
    }

    // Apply / Revert (UE-ish)
    if (ImGui::Button("Apply"))
    {
        try {
            auto parsed = nlohmann::json::parse(jsonText);
            m_config->set(s.ns, s.key, parsed);
            val = parsed;
        } catch (...) {
            // ignore parse errors for now (can add toast)
        }
    }
    ImGui::SameLine();
    if (ImGui::Button("Revert"))
    {
        jsonText = val.dump(2);
        const size_t cap = std::max<size_t>(4096, jsonText.size() + 1024);
        jsonBuf.assign(cap, 0);
        strncpy_s(jsonBuf.data(), jsonBuf.size(), jsonText.c_str(), _TRUNCATE);
    }

    break;
}
default:

            ImGui::TextDisabled("Unsupported");
            break;
        }

        // Reset arrow (UE-style)
        ImGui::SameLine();
        if (ImGui::SmallButton("Reset"))
        {
            m_config->resetToDefault(s.ns, s.key);
        }
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Reset to default");

        // Modified indicator dot/star
        ImGui::SameLine();
        if (modified)
        {
            ImGui::TextColored(ImVec4(1.0f, 0.78f, 0.20f, 1.0f), "*");
            if (ImGui::IsItemHovered())
                ImGui::SetTooltip("Modified from default");
        }
        else
        {
            ImGui::TextUnformatted(" ");
        }

        ImGui::PopID();
    };

    // ---- Collect settings and build sidebar model ----
    std::vector<ports::SettingInfo> allSettings = m_config->listSettings();

    // ns -> list<settings>
    std::map<std::string, std::vector<ports::SettingInfo>> byNs;
    for (const auto& s : allSettings)
    {
        if (!matchesFilter(s)) continue;
        byNs[s.ns].push_back(s);
    }

    // counts
    int filteredCount = 0;
    int modifiedCount = 0;
    for (const auto& s : allSettings)
    {
        if (!matchesFilter(s)) continue;
        filteredCount++;
        if (isModified(s)) modifiedCount++;
    }

    // Ensure something selected
    if (s_selectedNs.empty() && !byNs.empty())
        s_selectedNs = byNs.begin()->first;

    // ---- Window ----
    ImGui::SetNextWindowSize(ImVec2(1050, 700), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Settings", &open))
    {
        ImGui::End();
        m_config->set("pistachio.UI", "config.windowOpen", open);
        m_config->set("pistachio.UI", "config.showAdvanced", showAdvanced);
        return;
    }

    // Top toolbar (UE feel)
    {
        if (fontTitle) ImGui::PushFont(fontTitle);
        ImGui::TextUnformatted("Settings");
        if (fontTitle) ImGui::PopFont();

        ImGui::SameLine();
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + 10.0f);
        ImGui::SetNextItemWidth(420.0f);
        ImGui::InputTextWithHint("##search", "Search settings...", s_search, sizeof(s_search));

        ImGui::SameLine();
        ImGui::Checkbox("Advanced", &showAdvanced);

        ImGui::SameLine();
        ImGui::Checkbox("Only Modified", &s_onlyModified);

        ImGui::SameLine();
        ImGui::Checkbox("Icons", &s_showIcons);

        ImGui::SameLine();
        if (ImGui::Button("Save"))
        {
            if (m_app) m_app->saveConfigNow();
        }
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Save to pistachio.config.json");

        ImGui::SameLine();
        if (ImGui::Button("Reset Filtered"))
        {
            for (const auto& s : allSettings)
            {
                if (!matchesFilter(s)) continue;
                m_config->resetToDefault(s.ns, s.key);
            }
        }

        ImGui::SameLine();
        ImGui::TextDisabled("Results: %d  Modified: %d", filteredCount, modifiedCount);

        ImGui::Separator();
    }

    // Two panels (sidebar + details)
    ImVec2 avail = ImGui::GetContentRegionAvail();
    float rightWidth = avail.x - s_leftWidth - 6.0f;

    // Sidebar
    ImGui::BeginChild("cfg_left", ImVec2(s_leftWidth, 0), true);
    {
        // group namespaces under Editor / Plugins like Unreal
        std::map<std::string, std::vector<std::string>> roots;
        for (auto& [ns, vec] : byNs)
        {
            roots[rootForNs(ns)].push_back(ns);
        }
        for (auto& [root, vec] : roots)
            std::sort(vec.begin(), vec.end());

        auto drawRootHeader = [&](const char* title, ImU32 iconCol) {
            ImGui::Spacing();
            drawSidebarIcon(iconCol);
            if (fontGroup) ImGui::PushFont(fontGroup);
            ImGui::TextUnformatted(title);
            if (fontGroup) ImGui::PopFont();
            ImGui::Separator();
        };

        drawRootHeader("Editor",  ImGui::GetColorU32(ImVec4(0.20f, 0.55f, 1.00f, 1.0f)));
        for (const auto& ns : roots["Editor"])
        {
            std::string label = labelForNs(ns);
            bool selected = (s_selectedNs == ns);

            // Show per-namespace modified badge
            int nsMod = 0;
            for (const auto& s : byNs[ns]) if (isModified(s)) nsMod++;

            if (selected)
                ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(1, 1, 1, 0.10f));

            ImGui::Selectable((label + (nsMod > 0 ? "  *" : "")).c_str(), selected);

            if (selected)
                ImGui::PopStyleColor();

            if (ImGui::IsItemClicked())
                s_selectedNs = ns;
        }

        if (roots.find("Plugins") != roots.end() && !roots["Plugins"].empty())
        {
            ImGui::Spacing();
            ImGui::Spacing();
            drawRootHeader("Plugins", ImGui::GetColorU32(ImVec4(0.95f, 0.65f, 0.20f, 1.0f)));

            for (const auto& ns : roots["Plugins"])
            {
                std::string label = labelForNs(ns);
                bool selected = (s_selectedNs == ns);

                int nsMod = 0;
                for (const auto& s : byNs[ns]) if (isModified(s)) nsMod++;

                if (selected)
                    ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(1, 1, 1, 0.10f));

                ImGui::Selectable((label + (nsMod > 0 ? "  *" : "")).c_str(), selected);

                if (selected)
                    ImGui::PopStyleColor();

                if (ImGui::IsItemClicked())
                    s_selectedNs = ns;
            }
        }
    }
    ImGui::EndChild();

    splitterV(6.0f, &s_leftWidth, 240.0f, 400.0f);

    // Details panel
    ImGui::BeginChild("cfg_right", ImVec2(0, 0), true);
    {
        if (s_selectedNs.empty())
        {
            ImGui::TextDisabled("No settings.");
        }
        else
        {
            std::string root = rootForNs(s_selectedNs);
            std::string label = labelForNs(s_selectedNs);

            // Breadcrumb / title (UE style)
            std::string breadcrumb = root + " / " + label;

            if (fontTitle) ImGui::PushFont(fontTitle);
            ImGui::TextUnformatted(breadcrumb.c_str());
            if (fontTitle) ImGui::PopFont();

            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();

            // Group settings by group name (always expanded)
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

                    drawSectionHeader(groupLabel.c_str());

                    if (ImGui::BeginTable(("##tbl_" + groupLabel).c_str(), 2,
                        ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_SizingStretchProp))
                    {
                        ImGui::TableSetupColumn("Setting", ImGuiTableColumnFlags_WidthStretch, 0.60f);
                        ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch, 0.40f);

                        int rowIdx = 0;
                        for (const auto& s : vec)
                        {
                            bool mod = isModified(s);

                            ImGui::TableNextRow();

                            // Subtle modified row tint + left accent bar
                            if (mod)
                            {
                                ImU32 tint = ImGui::GetColorU32(ImVec4(1.0f, 0.78f, 0.20f, 0.10f));
                                ImGui::TableSetBgColor(ImGuiTableBgTarget_RowBg0, tint);

                                // draw accent on left edge of first cell
                                ImGui::TableSetColumnIndex(0);
                                ImVec2 pMin = ImGui::GetCursorScreenPos();
                                float rowH = ImGui::GetTextLineHeightWithSpacing() * 2.2f;
                                ImGui::GetWindowDrawList()->AddRectFilled(
                                    ImVec2(pMin.x - 6.0f, pMin.y),
                                    ImVec2(pMin.x - 3.0f, pMin.y + rowH),
                                    ImGui::GetColorU32(ImVec4(1.0f, 0.78f, 0.20f, 1.0f))
                                );
                            }

                            ImGui::TableSetColumnIndex(0);

                            // Label + description like UE details panel
                            const char* display = s.displayName.empty() ? s.key.c_str() : s.displayName.c_str();
                            ImGui::TextUnformatted(display);

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

                            rowIdx++;
                        }

                        ImGui::EndTable();
                    }
                }
            }
        }
    }
    ImGui::EndChild();

    ImGui::End();

    // Persist toggles
    m_config->set("pistachio.UI", "config.windowOpen", open);
    m_config->set("pistachio.UI", "config.showAdvanced", showAdvanced);
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
    "pistachio.UI",
    "Pistachio UI",
    "0.1.0",
    "Sketching"
};

PISTACHIO_UI_EXPORT const UiPluginManifestV1* pistachio_get_ui_manifest()
{
    return &g_manifest;
}