#include "UiModuleApi.h"
#include "core/Application.h"
#include "ports/IConfigPort.h"
#include "core/ColourUtils.h"
#include "ports/IMenuHost.h"
#include "ports/IRibbonHost.h"
#include "adapters/ui/HostMenuRibbonImp.h"
#include "adapters/ui/ContributionRegistry.h"

#include <imgui.h>
#include <cstdio>
#include <string>
#include <vector>
#include <map>
#include <unordered_map>
#include <unordered_set>
#include <algorithm>
#include <cctype>
#include <optional>

 
// -----------------------------------------------------------------------
// PistachioConfigPlugin
// Minimal hot-reload test plugin — no ImGuiAdapter, no custom fonts,
// no icon textures. Just the config editor rendered with the default font.
// -----------------------------------------------------------------------
class PistachioConfigPlugin final : public IUiModule
{

   


public:
    /*RibbonContribution* m_toolsGroup = nullptr;
    MenuContribution* m_sketchMenu = nullptr;
    bool                m_hasSelection = false;*/

    PistachioConfigPlugin() = default;
    ~PistachioConfigPlugin() override = default;

    void onLoad(UiHostServices& svc, domain::DataContext& dataContext) override
    {

        printf("[ConfigPlugin] onLoad\n");
        m_app = reinterpret_cast<core::Application*>(svc.app);
        m_config = reinterpret_cast<ports::IConfigPort*>(svc.config);
        m_registry = reinterpret_cast<adapters::ContributionRegistry*>(svc.registry);

        registerSettings();
        
        if (m_registry)
        {
            m_menuContrib = m_registry->contributeMenu("pistachio.config", "Views", 200);
            m_menuContrib->addToggle("config_window_open", "SettingsChanged", 900, &m_configWindowOpen);

            m_ribbonContrib = m_registry->contributeRibbon("pistachio.config", "Config", 900);
            m_ribbonContrib->addButton("open_settings", "Settings", 10,
                [this]() { 
                    m_configWindowOpen = !m_configWindowOpen;
                    m_config->set("pistachio.UI", "config.windowOpen", !m_config->get("pistachio.UI", "config.windowOpen"));
                });
        }



        printf("[ConfigPlugin] onLoad done\n");



    }

    void onUnload(UiHostServices& svc,domain::DataContext& dataContext ) override
    {
        printf("[ConfigPlugin] onUnload\n");
        auto* registry = reinterpret_cast<adapters::ContributionRegistry*>(svc.registry);
        if (registry)
            registry->removeAllContributions("pistachio.config");

        m_menuContrib = nullptr;
        m_ribbonContrib = nullptr;
        m_registry = nullptr;
        m_app = nullptr;
        m_config = nullptr;
    }

    void render(UiHostServices&,domain::DataContext& dataContext) override
    {
        renderConfigEditor();
    }

private:
    core::Application* m_app = nullptr;
    ports::IConfigPort* m_config = nullptr;
    adapters::ContributionRegistry* m_registry = nullptr;
    ports::MenuContribution* m_menuContrib = nullptr;
    ports::RibbonContribution* m_ribbonContrib = nullptr;
    bool                            m_configWindowOpen = true;

    // ------------------------------------------------------------------
    void registerSettings()
    {
        if (!m_config) return;
        using ports::SettingInfo;
        using ports::SettingType;

        m_config->registerSetting(SettingInfo(
            "pistachio.UI", "config.showAdvanced", "Show advanced settings -  ",
            "Show settings marked as advanced.", "Configuration", SettingType::Bool, false, "", false));
        m_config->registerSetting(SettingInfo(
            "pistachio.UI", "config.windowOpen", "Config window open",
            "Whether the configuration editor window is visible.", "Configuration", SettingType::Bool, true, "", true));
        m_config->registerSetting(SettingInfo(
            "pistachio.UI", "views.fileOperations", "File Operations",
            "Show/hide the File Operations window.", "Views", SettingType::Bool, true, "", true));
        m_config->registerSetting(SettingInfo(
            "pistachio.UI", "views.slicerOperations", "Slicer Operations",
            "Show/hide the Slicer Operations window.", "Views", SettingType::Bool, true, "", true));
        m_config->registerSetting(SettingInfo(
            "pistachio.UI", "views.status", "Status",
            "Show/hide the Status window.", "Views", SettingType::Bool, true, "", true));
        m_config->registerSetting(SettingInfo(
            "pistachio.UI", "views.modelInfo", "Model Info",
            "Show/hide the Model Info window.", "Views", SettingType::Bool, true, "", true));
        m_config->registerSetting(SettingInfo(
            "pistachio.UI", "views.viewport3d", "3D Viewport",
            "Show/hide the 3D Viewport window.", "Views", SettingType::Bool, true, "", true));
        m_config->registerSetting(SettingInfo(
            "pistachio.UI", "views.sketch3dViewport", "Sketch 3D View",
            "Show/hide the Sketch 3D View window.", "Views", SettingType::Bool, true, "", true));
        m_config->registerSetting(SettingInfo(
            "pistachio.UI", "views.sketchEditor", "Sketch Editor",
            "Show/hide the Sketch Editor window.", "Views", SettingType::Bool, true, "", true));
    }

    // ------------------------------------------------------------------
    void renderConfigEditor()
    {
        if (!m_config) return;

        nlohmann::json jOpen = m_config->get("pistachio.UI", "config.windowOpen");
        bool open = jOpen.is_boolean() ? jOpen.get<bool>() : true;
        if (!open) return;

        nlohmann::json jAdv = m_config->get("pistachio.UI", "config.showAdvanced");
        bool showAdvanced = jAdv.is_boolean() ? jAdv.get<bool>() : false;

        static float       s_leftWidth = 220.0f;
        static char        s_search[256] = {};
        static bool        s_onlyModified = false;
        static bool        s_showIcons = true;
        static std::string s_selectedNs;

        static std::unordered_map<std::string, std::vector<char>> s_textBuf;
        static std::unordered_map<std::string, std::string>       s_textLast;
        static std::unordered_map<std::string, bool>              s_textEditing;

        // Stub uses default ImGui font for everything
        ImFont* fontBody = nullptr; // nullptr = current font
        ImFont* fontGroup = nullptr;
        ImFont* fontTitle = nullptr;

        // ---- helpers ----
        auto toLower = [](std::string v) {
            std::transform(v.begin(), v.end(), v.begin(),
                [](unsigned char c) { return (char)std::tolower(c); });
            return v;
            };

        auto matchesFilter = [&](const ports::SettingInfo& s) -> bool {
            if (!showAdvanced && s.advanced) return false;
            if (s_onlyModified) {
                nlohmann::json cur = m_config->get(s.ns, s.key);
                if (cur == s.defaultValue) return false;
            }
            if (s_search[0] == 0) return true;
            std::string f = toLower(std::string(s_search));
            std::string hay = toLower(s.ns + " " + s.group + " " + s.key + " " + s.displayName + " " + s.description);
            return hay.find(f) != std::string::npos;
            };

        auto isModified = [&](const ports::SettingInfo& s) -> bool {
            return m_config->get(s.ns, s.key) != s.defaultValue;
            };

        auto rootForNs = [](const std::string& ns) -> std::string {
            if (ns.rfind("plugin.", 0) == 0) return "Plugins";
            if (ns.rfind("slicer.", 0) == 0) return "Slicer";
            if (ns.rfind("filament.", 0) == 0) return "Filament";
            return "Editor";
            };

        auto labelForNs = [&](const std::string& ns) -> std::string {
            std::string l = ns;
            if (l.rfind("pistachio.", 0) == 0) l = l.substr(10);
            if (l.rfind("plugin.", 0) == 0) l = l.substr(7);
            if (l.rfind("slicer.", 0) == 0) l = l.substr(7);
            if (ns.rfind("slicer.toolheads", 0) == 0) {
                auto& ni = m_config->getNamespace(ns);
                auto& pi = m_config->getNamespace(ni.parentNs);
                l = pi.displayName + " / " + ni.displayName;
            }
            if (ns.rfind("filament.Settings", 0) == 0) {
                auto& ni = m_config->getNamespace(ns);
                auto& pi = m_config->getNamespace(ni.parentNs);
                l = pi.displayName + " / " + ni.displayName;
            }
            return l;
            };

        auto drawSidebarIcon = [&](ImU32 col) {
            if (!s_showIcons) return;
            ImVec2 p = ImGui::GetCursorScreenPos();
            float lineH = ImGui::GetTextLineHeight();
            const float sz = 10.0f;
            float top = p.y + (lineH - sz) * 0.5f;
            ImGui::GetWindowDrawList()->AddRectFilled(
                ImVec2(p.x, top), ImVec2(p.x + sz, top + sz), col, 2.0f);
            ImGui::Dummy(ImVec2(sz + 2.0f, lineH));
            ImGui::SameLine();
            };

        auto drawSectionHeader = [&](const char* title) {
            ImGui::Spacing();
            ImVec2 p0 = ImGui::GetCursorScreenPos();
            float  w = ImGui::GetContentRegionAvail().x;
            float  h = 28.0f;
            ImGui::GetWindowDrawList()->AddRectFilled(
                p0, ImVec2(p0.x + w, p0.y + h),
                ImGui::GetColorU32(ImVec4(1, 1, 1, 0.04f)), 4.0f);
            ImGui::SetCursorScreenPos(ImVec2(p0.x, p0.y + 6.0f));
            if (fontGroup) ImGui::PushFont(fontGroup);
            ImGui::TextUnformatted(title);
            if (fontGroup) ImGui::PopFont();
            ImGui::SetCursorScreenPos(ImVec2(p0.x + 15.0f, p0.y + h));
            ImGui::Dummy(ImVec2(0, 8.0f));
            };

        auto splitterV = [](float thickness, float* left, float minLeft, float minRight) {
            ImGui::SameLine();
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(1, 1, 1, 0.06f));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(1, 1, 1, 0.10f));
            ImGui::Button("##cfg_splitter", ImVec2(thickness, -1));
            ImGui::PopStyleColor(3);
            if (ImGui::IsItemActive()) {
                float delta = ImGui::GetIO().MouseDelta.x;
                float newLeft = *left + delta;
                float avail = ImGui::GetContentRegionAvail().x;
                float newRight = avail - newLeft - thickness;
                if (newLeft >= minLeft && newRight >= minRight)
                    *left = newLeft;
            }
            ImGui::SameLine();
            };

        auto drawValueEditor = [&](const ports::SettingInfo& s) {
            nlohmann::json val = m_config->get(s.ns, s.key);
            bool           modified = (val != s.defaultValue);
            ImGui::PushID((s.ns + "::" + s.key).c_str());
            ImGui::SetNextItemWidth(-70.0f);

            switch (s.type)
            {
            case ports::SettingType::Bool: {
                bool b = val.is_boolean() ? val.get<bool>()
                    : (s.defaultValue.is_boolean() ? s.defaultValue.get<bool>() : false);
                if (ImGui::Checkbox("##v", &b)) m_config->set(s.ns, s.key, b);
                break;
            }
            case ports::SettingType::Int: {
                int i = val.is_number_integer() ? val.get<int>()
                    : (s.defaultValue.is_number_integer() ? s.defaultValue.get<int>() : 0);
                if (ImGui::DragInt("##v", &i, 1.0f)) m_config->set(s.ns, s.key, i);
                break;
            }
            case ports::SettingType::Float: {
                float f = val.is_number() ? (float)val.get<double>()
                    : (s.defaultValue.is_number() ? (float)s.defaultValue.get<double>() : 0.0f);
                if (ImGui::DragFloat("##v", &f, 0.01f)) m_config->set(s.ns, s.key, f);
                break;
            }
            case ports::SettingType::String: {
                const std::string id = s.ns + "::" + s.key;
                auto& buf = s_textBuf[id];
                auto& last = s_textLast[id];
                auto& editing = s_textEditing[id];
                if (buf.empty()) buf.assign(512, 0);
                std::string cur = val.is_string() ? val.get<std::string>()
                    : (s.defaultValue.is_string() ? s.defaultValue.get<std::string>() : "");
                if (!editing && last != cur) {
                    std::fill(buf.begin(), buf.end(), 0);
                    strncpy_s(buf.data(), buf.size(), cur.c_str(), _TRUNCATE);
                    last = cur;
                }
                bool enter = ImGui::InputText("##v", buf.data(), buf.size(),
                    ImGuiInputTextFlags_EnterReturnsTrue);
                if (ImGui::IsItemActive()) editing = true;
                auto commit = [&]() {
                    std::string next(buf.data());
                    if (next != cur) { m_config->set(s.ns, s.key, next); last = next; }
                    };
                if (enter) { commit(); editing = false; }
                else if (editing && !ImGui::IsItemActive() && ImGui::IsItemDeactivated())
                {
                    commit(); editing = false;
                }
                break;
            }
            case ports::SettingType::Colour: {
                std::string hexVal = val.is_string() ? val.get<std::string>()
                    : (s.defaultValue.is_string() ? s.defaultValue.get<std::string>() : "#FFFFFFFF");
                ImVec4 colour = utils::hexToImVec4(hexVal);
                const std::string popupId = "##colpick_" + s.ns + "_" + s.key;
                if (ImGui::ColorButton(("##colbtn_" + s.ns + "_" + s.key).c_str(), colour,
                    ImGuiColorEditFlags_AlphaPreview | ImGuiColorEditFlags_NoTooltip,
                    ImVec2(ImGui::CalcItemWidth(), ImGui::GetFrameHeight())))
                    ImGui::OpenPopup(popupId.c_str());
                if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", hexVal.c_str());
                if (ImGui::BeginPopup(popupId.c_str())) {
                    static std::unordered_map<std::string, ImVec4> s_pickerState;
                    const std::string stateKey = s.ns + "::" + s.key;
                    if (!s_pickerState.count(stateKey) || ImGui::IsWindowAppearing())
                        s_pickerState[stateKey] = colour;
                    ImVec4& editing = s_pickerState[stateKey];
                    bool changed = ImGui::ColorPicker4("##picker", (float*)&editing,
                        ImGuiColorEditFlags_AlphaBar | ImGuiColorEditFlags_PickerHueWheel);
                    ImGui::Spacing(); ImGui::Separator(); ImGui::Spacing();
                    if (ImGui::Button("Apply", ImVec2(80, 0))) {
                        m_config->set(s.ns, s.key, utils::imVec4ToHex(editing));
                        s_pickerState.erase(stateKey);
                        ImGui::CloseCurrentPopup();
                    }
                    ImGui::SameLine();
                    if (ImGui::Button("Cancel", ImVec2(80, 0))) {
                        s_pickerState.erase(stateKey);
                        ImGui::CloseCurrentPopup();
                    }
                    ImGui::SameLine();
                    static bool s_liveUpdate = false;
                    ImGui::Checkbox("Live", &s_liveUpdate);
                    if (s_liveUpdate && changed)
                        m_config->set(s.ns, s.key, utils::imVec4ToHex(editing));
                    ImGui::EndPopup();
                }
                break;
            }
            case ports::SettingType::Enum: {
                std::string cur = val.is_string() ? val.get<std::string>()
                    : (s.defaultValue.is_string() ? s.defaultValue.get<std::string>() : "");
                std::vector<std::string> options;
                for (const auto& child : m_config->listSettingsInNamespace(s.enumOptionsNamespace)) {
                    std::optional<ports::SettingInfo> name = m_config->get(child.ns + ":name");
                    if (name.has_value()) {
                        nlohmann::json v = m_config->get(name.value().ns, "name");
                        options.push_back(v.get<std::string>());
                    }
                }
                std::unordered_set<std::string> seen;
                options.erase(std::remove_if(options.begin(), options.end(),
                    [&seen](const std::string& s) { return !seen.insert(s).second; }), options.end());
                if (options.empty()) { ImGui::TextDisabled("(no options)"); break; }
                int idx = 0;
                for (int i = 0; i < (int)options.size(); ++i)
                    if (options[i] == cur) { idx = i; break; }
                ImGui::SetNextItemWidth(-70.0f);
                if (ImGui::BeginCombo(("##enum_" + s.ns + "_" + s.key).c_str(), options[idx].c_str())) {
                    for (int i = 0; i < (int)options.size(); ++i) {
                        bool sel = (i == idx);
                        if (ImGui::Selectable(options[i].c_str(), sel))
                            m_config->set(s.ns, s.key, options[i]);
                        if (sel) ImGui::SetItemDefaultFocus();
                    }
                    ImGui::EndCombo();
                }
                break;
            }
            default:
                ImGui::TextDisabled("Unsupported");
                break;
            }

            ImGui::SameLine();
            if (ImGui::SmallButton("Reset"))
                m_config->resetToDefault(s.ns, s.key);
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("Reset to default");
            ImGui::SameLine();
            if (modified) {
                ImGui::TextColored(ImVec4(1.0f, 0.78f, 0.20f, 1.0f), "*");
                if (ImGui::IsItemHovered()) ImGui::SetTooltip("Modified from default");
            }
            else {
                ImGui::TextUnformatted(" ");
            }
            ImGui::PopID();
            };

        // ---- collect settings ----
        std::vector<ports::SettingInfo> allSettings = m_config->listSettings();
        std::map<std::string, std::vector<ports::SettingInfo>> byNs;
        for (const auto& s : allSettings) {
            if (!matchesFilter(s)) continue;
            byNs[s.ns].push_back(s);
        }
        int filteredCount = 0, modifiedCount = 0;
        for (const auto& s : allSettings) {
            if (!matchesFilter(s)) continue;
            filteredCount++;   
            if (isModified(s)) modifiedCount++;
        }
        if (s_selectedNs.empty() && !byNs.empty())
            s_selectedNs = byNs.begin()->first;


        ImGuiViewport* vp = ImGui::GetMainViewport();

        ImGuiWindowFlags flags =
            ImGuiWindowFlags_NoDocking
            | ImGuiWindowFlags_NoCollapse
            | ImGuiWindowFlags_NoResize
            ;

        ImVec2 center = vp->GetCenter();
        center.y = center.y + 50.0f;

        ImGui::SetNextWindowPos(
            center,
            ImGuiCond_Always,
            ImVec2(0.5f, 0.5f));

        ImGui::SetNextWindowSize(
            ImVec2(1050, 700),
            ImGuiCond_Always);

        // ---- window ----
        //ImGui::SetNextWindowSize(ImVec2(1050, 700), ImGuiCond_FirstUseEver);
        
        if (!ImGui::Begin("Settings", &open, flags)) {
            ImGui::End();
            m_config->set("pistachio.UI", "config.windowOpen", open);
            m_config->set("pistachio.UI", "config.showAdvanced", showAdvanced);
            return;
        }

        // toolbar
        {
            if (fontTitle) ImGui::PushFont(fontTitle);
            ImGui::TextUnformatted("Settings");
            if (fontTitle) ImGui::PopFont();
            ImGui::SameLine();
            ImGui::SetNextItemWidth(420.0f);
            ImGui::InputTextWithHint("##search", "Search settings...", s_search, sizeof(s_search));
            ImGui::SameLine(); ImGui::Checkbox("Advanced", &showAdvanced);
            ImGui::SameLine(); ImGui::Checkbox("Only Modified", &s_onlyModified);
            ImGui::SameLine(); ImGui::Checkbox("Icons", &s_showIcons);
            ImGui::SameLine();
            if (ImGui::Button("Save") && m_app) m_app->saveConfigNow();
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("Save to pistachio.config.json");
            ImGui::SameLine();
            if (ImGui::Button("Reset Filtered")) {
                for (const auto& s : allSettings)
                    if (matchesFilter(s)) m_config->resetToDefault(s.ns, s.key);
            }
            ImGui::SameLine();
            ImGui::TextDisabled("Results: %d  Modified: %d", filteredCount, modifiedCount);
            ImGui::Separator();
        }

        // build root groups
        std::map<std::string, std::vector<std::string>> roots;
        for (auto& [ns, vec] : byNs) roots[rootForNs(ns)].push_back(ns);
        for (auto& [r, vec] : roots) std::sort(vec.begin(), vec.end());

        auto drawRootHeader = [&](const char* title, ImU32 iconCol) {
            ImGui::Spacing();
            drawSidebarIcon(iconCol);
            if (fontGroup) ImGui::PushFont(fontGroup);
            ImGui::TextUnformatted(title);
            if (fontGroup) ImGui::PopFont();
            ImGui::Separator();
            };

        auto renderNamespaceNode = [&](auto&& self, const std::string& ns) -> void {
            for (const auto& child : m_config->listChildNamespaces(ns)) {
                if (!m_config->listChildNamespaces(child.ns).empty()) {
                    if (ImGui::TreeNodeEx(child.displayName.c_str(), ImGuiTreeNodeFlags_DefaultOpen)) {
                        self(self, child.ns);
                        ImGui::TreePop();
                    }
                }
                else {
                    bool selected = (s_selectedNs == child.ns);
                    std::optional<ports::SettingInfo> colour = m_config->get(child.ns + ":colour");
                    if (colour.has_value()) {
                        nlohmann::json cv = m_config->get(colour.value().ns, "colour");
                        drawSidebarIcon(ImGui::GetColorU32(utils::hexToImVec4(cv.get<std::string>())));
                    }
                    if (selected) ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(1, 1, 1, 0.10f));
                    ImGui::Selectable(child.displayName.c_str(), selected);
                    if (selected) ImGui::PopStyleColor();
                    if (ImGui::IsItemClicked()) s_selectedNs = child.ns;
                }
            }
            };

        auto renderSettingsTree = [&](auto&& self, const std::string& section, const std::string& ns) -> void {
            if (ImGui::TreeNodeEx(section.c_str(), ImGuiTreeNodeFlags_DefaultOpen)) {
                for (const auto& root : m_config->listChildNamespaces(ns)) {
                    if (ImGui::TreeNodeEx(root.displayName.c_str(), ImGuiTreeNodeFlags_DefaultOpen)) {
                        renderNamespaceNode(renderNamespaceNode, root.ns);
                        ImGui::TreePop();
                    }
                }
                ImGui::TreePop();
            }
            };

        // ---- sidebar ----
        ImGui::BeginChild("cfg_left", ImVec2(s_leftWidth, 0), true);
        {
            auto drawGroup = [&](const char* title, ImU32 col, const std::string& key,
                bool skipPrefix = false, const std::string& treeNs = "") {
                    if (roots.find(key) == roots.end() || roots[key].empty()) return;
                    drawRootHeader(title, col);
                    for (const auto& ns : roots[key]) {
                        if (!treeNs.empty() && ns.rfind(treeNs, 0) == 0) continue;
                        std::string label = labelForNs(ns);
                        bool selected = (s_selectedNs == ns);
                        int nsMod = 0;
                        for (const auto& s : byNs[ns]) if (isModified(s)) nsMod++;
                        if (selected) ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(1, 1, 1, 0.10f));
                        ImGui::Selectable((label + (nsMod > 0 ? "  *" : "")).c_str(), selected);
                        if (selected) ImGui::PopStyleColor();
                        if (ImGui::IsItemClicked()) s_selectedNs = ns;
                    }
                    if (!treeNs.empty())
                        renderSettingsTree(renderSettingsTree, title, treeNs);
                };

            drawGroup("Editor", ImGui::GetColorU32(ImVec4(0.20f, 0.55f, 1.00f, 1)), "Editor");
            drawGroup("Plugins", ImGui::GetColorU32(ImVec4(0.95f, 0.65f, 0.20f, 1)), "Plugins");
            drawGroup("Slicer", ImGui::GetColorU32(ImVec4(0.95f, 0.20f, 0.20f, 1)), "Slicer", false, "slicer.toolheads");
            drawGroup("Filament", ImGui::GetColorU32(ImVec4(0.20f, 0.95f, 0.20f, 1)), "Filament", false, "filament.Settings");
        }
        ImGui::EndChild();

        splitterV(6.0f, &s_leftWidth, 240.0f, 400.0f);

        // ---- details panel ----
        ImGui::BeginChild("cfg_right", ImVec2(0, 0), true);
        {
            if (s_selectedNs.empty()) {
                ImGui::TextDisabled("No settings.");
            }
            else {
                std::string breadcrumb = rootForNs(s_selectedNs) + " / " + labelForNs(s_selectedNs);
                if (fontTitle) ImGui::PushFont(fontTitle);
                ImGui::TextUnformatted(breadcrumb.c_str());
                if (fontTitle) ImGui::PopFont();
                ImGui::Spacing(); ImGui::Separator(); ImGui::Spacing();

                std::map<std::string, std::vector<ports::SettingInfo>> groups;
                for (const auto& s : allSettings) {
                    if (s.ns != s_selectedNs || !matchesFilter(s)) continue;
                    groups[s.group].push_back(s);
                }

                if (groups.empty()) {
                    ImGui::TextDisabled("No settings match your filter.");
                }
                else {
                    for (auto& [group, vec] : groups) {
                        std::string groupLabel = group.empty() ? "General" : group;
                        std::sort(vec.begin(), vec.end(), [](const auto& a, const auto& b) {
                            return a.displayName < b.displayName;
                            });
                        drawSectionHeader(groupLabel.c_str());

                        if (ImGui::BeginTable(("##tbl_" + groupLabel).c_str(), 2,
                            ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerV |
                            ImGuiTableFlags_SizingStretchProp))
                        {
                            ImGui::TableSetupColumn("Setting", ImGuiTableColumnFlags_WidthStretch, 0.60f);
                            ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch, 0.40f);

                            for (const auto& s : vec) {
                                bool mod = isModified(s);
                                ImGui::TableNextRow();
                                if (mod) {
                                    ImGui::TableSetBgColor(ImGuiTableBgTarget_RowBg0,
                                        ImGui::GetColorU32(ImVec4(1.0f, 0.78f, 0.20f, 0.10f)));
                                    ImGui::TableSetColumnIndex(0);
                                    ImVec2 pMin = ImGui::GetCursorScreenPos();
                                    float  rowH = ImGui::GetTextLineHeightWithSpacing() * 2.2f;
                                    ImGui::GetWindowDrawList()->AddRectFilled(
                                        ImVec2(pMin.x - 6, pMin.y), ImVec2(pMin.x - 3, pMin.y + rowH),
                                        ImGui::GetColorU32(ImVec4(1.0f, 0.78f, 0.20f, 1.0f)));
                                }
                                ImGui::TableSetColumnIndex(0);
                                const char* display = s.displayName.empty() ? s.key.c_str() : s.displayName.c_str();
                                ImGui::TextUnformatted(display);
                                if (!s.description.empty()) {
                                    ImGui::PushStyleColor(ImGuiCol_Text,
                                        ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
                                    ImGui::TextWrapped("%s", s.description.c_str());
                                    ImGui::PopStyleColor();
                                    ImGui::Dummy(ImVec2(0, 10));
                                }
                                ImGui::PushStyleColor(ImGuiCol_Text,
                                    ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
                                ImGui::Text("[%s]", s.key.c_str());
                                ImGui::PopStyleColor();
                                ImGui::Dummy(ImVec2(0, 30));

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

        m_config->set("pistachio.UI", "config.windowOpen", open);
        m_config->set("pistachio.UI", "config.showAdvanced", showAdvanced);
    }
};

// -----------------------------------------------------------------------
// Required exports
// -----------------------------------------------------------------------
extern "C" __declspec(dllexport)
IUiModule* pistachio_create_ui_module() { return new PistachioConfigPlugin(); }

extern "C" __declspec(dllexport)
void pistachio_destroy_ui_module(IUiModule* m) { delete m; }

static const UiPluginManifestV1 g_manifest = {
    sizeof(UiPluginManifestV1), 1,
    "pistachio.config", "Pistachio Config", "0.1.0", "UI"
};

extern "C" __declspec(dllexport)
const UiPluginManifestV1* pistachio_get_ui_manifest() { return &g_manifest; }