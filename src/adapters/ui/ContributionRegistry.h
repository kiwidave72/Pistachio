#pragma once

// -----------------------------------------------------------------------
// ContributionRegistry.h
//
// Host-owned registry of all MenuContribution and RibbonContribution
// objects registered by plugins and the host itself.
//
// - contributeMenu()   / removeMenuContributions()
// - contributeRibbon() / removeRibbonContributions()
// - renderMenuBar()    — call inside HostUI::BeginMenubar block
// - renderRibbonBar()  — call inside the ribbon area
//
// Place at: src/adapters/ui/ContributionRegistry.h
// Include in ImGuiHost.cpp only — depends on ImGui.
// -----------------------------------------------------------------------

#include "ports/Contributions.h"
#include <imgui.h>
#include <imgui_internal.h>

#include <string>
#include <vector>
#include <memory>
#include <algorithm>
#include <map>

namespace adapters {

    class ContributionRegistry
    {
    public:
        // -----------------------------------------------------------------------
        // Registration — returns a raw pointer the caller holds.
        // The registry owns the object. Safe to call at any time.
        // -----------------------------------------------------------------------
         // Register a drop target — returns raw pointer the plugin holds
        ports::DragDropContribution* contributeDragDrop(
            const std::string& pluginId,
            const std::string& payloadType,
            int                priority = 100)
        {
            auto c = std::make_unique< ports::DragDropContribution>(pluginId, payloadType, priority);
            auto* ptr = c.get();
            m_dragDrops.push_back(std::move(c));
            return ptr;
        }

        void removeDragDropContributions(const std::string& pluginId)
        {
            m_dragDrops.erase(
                std::remove_if(m_dragDrops.begin(), m_dragDrops.end(),
                    [&](const std::unique_ptr< ports::DragDropContribution>& c) {
                        return c->pluginId() == pluginId;
                    }),
                m_dragDrops.end());
        }

        // Call this after ImGui::Image or any drop-receivable item
        void renderDragDropTargets(const std::string& payloadType)
        {
            ImVec2 imageRectMin = ImGui::GetItemRectMin(); // Top-Left corner pixel
            ImVec2 imageRectMax = ImGui::GetItemRectMax(); // Bottom-Right corner pixel

            ImVec2 mousePos = ImGui::GetMousePos();

            // 4. Manual Collision Check: Is the cursor over the Build Plate image?
            if (mousePos.x >= imageRectMin.x && mousePos.x <= imageRectMax.x &&
                mousePos.y >= imageRectMin.y && mousePos.y <= imageRectMax.y)
            {
                // Optional: Draw a nice highlight border directly over the image to show it's active
                //ImGui::GetWindowDrawList()->AddRect(imageRectMin, imageRectMax, IM_COL32(0, 255, 0, 255), 2.0f);

                // 5. Detect drop intent when the user releases left click
                if (ImGui::IsMouseReleased(ImGuiMouseButton_Left)) {

                    for (const auto& c : m_dragDrops)
                    {
                        if (c->payloadType() != payloadType) continue;

                        if (const ImGuiPayload* payload = ImGui::GetDragDropPayload())
                        {
                            if (payload->IsDataType(payloadType.c_str()))
                                if (c->onDrop)
                                    c->onDrop(payload->Data, (size_t)payload->DataSize);
                                else
                                    if (c->onHoverRender)
                                        c->onHoverRender();
                        }
                    }


                    // Extract your payload data safely here!
                    // MyAssetData* droppedAsset = *(MyAssetData**)globalPayload->Data;
                    
                    //std::out << "Dropped successfully using global bypass method!" << std::endl;
                }
            }


            /*if (!ImGui::BeginDragDropTarget()) return;

            for (const auto& c : m_dragDrops)
            {
                if (c->payloadType() != payloadType) continue;

                if (const ImGuiPayload* payload =
                    ImGui::AcceptDragDropPayload(payloadType.c_str()))
                {
                    if (c->onDrop)
                        c->onDrop(payload->Data, (size_t)payload->DataSize);
                }

                if (c->onHoverRender)
                    c->onHoverRender();
            }

            ImGui::EndDragDropTarget();*/

            //ImGuiWindow* window = ImGui::GetCurrentWindow();
            //ImRect windowRect = window->ContentRegionRect;
            //ImGuiHoveredFlags flags = ImGuiHoveredFlags_AllowWhenBlockedByActiveItem
            //    | ImGuiHoveredFlags_RootWindow;

            //if(ImGui::IsWindowHovered(flags)) {
            //    // Uses the raw window space rect rather than the last item box
            //    if (ImGui::BeginDragDropTargetCustom(windowRect, ImGui::GetID("ViewportDropTarget"))) {
            //        if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ASSET_ID")) {
            //            for (const auto& c : m_dragDrops)
            //            {
            //                if (c->payloadType() != payloadType) continue;

            //                if (const ImGuiPayload* payload =
            //                    ImGui::AcceptDragDropPayload(payloadType.c_str()))
            //                {
            //                    if (c->onDrop)
            //                        c->onDrop(payload->Data, (size_t)payload->DataSize);
            //                }

            //                if (c->onHoverRender)
            //                    c->onHoverRender();
            //            }
            //        }
            //        ImGui::EndDragDropTarget();
            //    }
            //}
            //ImGui::Begin("Asset Viewport");

            //// 1. Draw your FBO Image texture normally
            //ImGui::Image((ImTextureID)(intptr_t)m_fboColor, ImVec2((float)w, (float)h), ImVec2(0, 1), ImVec2(1, 0));

            //// 2. Safely grab the current structural window pointer
            //ImGuiWindow* window = ImGui::GetCurrentWindow();

            //// ContentRegionRect spans the entire usable area inside the window (minus titlebar/scrollbars)
            //ImRect windowRect = window->ContentRegionRect;

            //// 3. Catch window bounds when an asset item is dragging over it
            //if (ImGui::IsWindowHovered(ImGuiHoveredFlags_AllowWhenBlockedByActiveItem)) {

            //    // Use the internal custom target function with your derived bounding box
            //    if (ImGui::BeginDragDropTargetCustom(windowRect, ImGui::GetID("WholeViewportDropZone"))) {
            //        if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("PROJECT_ASSET")) {
            //            // Asset successfully received! 
            //            // Target processing logic goes here...
            //        }
            //        ImGui::EndDragDropTarget();
            //    }
            //}

            //ImGui::End();

        }



        ports::MenuContribution* contributeMenu(
            const std::string& pluginId,
            const std::string& menuName,
            int                menuPriority = 100)
        {
            auto c = std::make_unique<ports::MenuContribution>(pluginId, menuName, menuPriority);
            auto* ptr = c.get();
            m_menus.push_back(std::move(c));
            return ptr;
        }

        ports::RibbonContribution* contributeRibbon(
            const std::string& pluginId,
            const std::string& groupName,
            int                groupPriority = 100)
        {
            auto c = std::make_unique<ports::RibbonContribution>(pluginId, groupName, groupPriority);
            auto* ptr = c.get();
            m_ribbons.push_back(std::move(c));
            return ptr;
        }

        // Remove all contributions from a given plugin (call from onUnload)
        void removeMenuContributions(const std::string& pluginId)
        {
            m_menus.erase(
                std::remove_if(m_menus.begin(), m_menus.end(),
                    [&](const std::unique_ptr<ports::MenuContribution>& c) {
                        return c->pluginId() == pluginId;
                    }),
                m_menus.end());
        }

        void removeRibbonContributions(const std::string& pluginId)
        {
            m_ribbons.erase(
                std::remove_if(m_ribbons.begin(), m_ribbons.end(),
                    [&](const std::unique_ptr<ports::RibbonContribution>& c) {
                        return c->pluginId() == pluginId;
                    }),
                m_ribbons.end());
        }

        void removeAllContributions(const std::string& pluginId)
        {
            removeMenuContributions(pluginId);
            removeRibbonContributions(pluginId);
            removeDragDropContributions(pluginId);
        }

        // -----------------------------------------------------------------------
        // Rendering — call every frame inside the appropriate ImGui context
        // -----------------------------------------------------------------------

        // Call inside HostUI::BeginMenubar / EndMenubar block.
        // Merges all contributions into named menus sorted by priority.
        void renderMenuBar()
        {
            // Build a merged map: menuName -> sorted list of (priority, contribution*)
            struct MergedMenu
            {
                std::string name;
                int         priority = 0; // lowest priority value wins for ordering
                std::vector<const ports::MenuContribution*> contributions;
            };

            std::map<std::string, MergedMenu> merged;
            for (const auto& c : m_menus)
            {
                auto& mm = merged[c->menuName()];
                mm.name = c->menuName();
                mm.priority = std::min(mm.priority, c->menuPriority());
                mm.contributions.push_back(c.get());
            }

            // Sort menus by priority
            std::vector<MergedMenu*> sorted;
            for (auto& [name, mm] : merged)
                sorted.push_back(&mm);
            std::stable_sort(sorted.begin(), sorted.end(),
                [](const MergedMenu* a, const MergedMenu* b) {
                    return a->priority < b->priority;
                });

            for (auto* mm : sorted)
            {
                if (!ImGui::BeginMenu(mm->name.c_str())) continue;

                // Collect all items from all contributions for this menu, sorted by priority
                std::vector<const ports::MenuItem*> allItems;
                for (const auto* contrib : mm->contributions)
                    for (const auto& item : contrib->items())
                        allItems.push_back(&item);

                std::stable_sort(allItems.begin(), allItems.end(),
                    [](const ports::MenuItem* a, const ports::MenuItem* b) {
                        return a->priority < b->priority;
                    });

                for (const auto* item : allItems)
                    renderMenuItem(*item);

                ImGui::EndMenu();
            }
        }

        // Call inside the ribbon bar area.
        // Merges all contributions into named groups sorted by priority.
        void renderRibbonBar()
        {
            // Build merged groups
            struct MergedGroup
            {
                std::string name;
                int         priority = 0;
                std::vector<const ports::RibbonContribution*> contributions;
            };

            std::map<std::string, MergedGroup> merged;
            for (const auto& c : m_ribbons)
            {
                auto& mg = merged[c->groupName()];
                mg.name = c->groupName();
                mg.priority = std::min(mg.priority, c->groupPriority());
                mg.contributions.push_back(c.get());
            }

            // Sort groups by priority
            std::vector<MergedGroup*> sorted;
            for (auto& [name, mg] : merged)
                sorted.push_back(&mg);
            std::stable_sort(sorted.begin(), sorted.end(),
                [](const MergedGroup* a, const MergedGroup* b) {
                    return a->priority < b->priority;
                });

            bool firstGroup = true;
            for (auto* mg : sorted)
            {
                // Collect all items across contributions, sorted by priority
                std::vector<const ports::RibbonItem*> allItems;
                for (const auto* contrib : mg->contributions)
                    for (const auto& item : contrib->items())
                        allItems.push_back(&item);

                if (allItems.empty()) continue;

                std::stable_sort(allItems.begin(), allItems.end(),
                    [](const ports::RibbonItem* a, const ports::RibbonItem* b) {
                        return a->priority < b->priority;
                    });

                // Separator between groups
                if (!firstGroup)
                {
                    ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);
                    ImGui::SameLine();
                }
                firstGroup = false;

                for (const auto* item : allItems)
                    renderRibbonItem(*item);
            }
        }

    private:
        // -----------------------------------------------------------------------
        // Item renderers
        // -----------------------------------------------------------------------

        void renderMenuItem(const ports::MenuItem& item)
        {
            // Visibility condition
            if (item.isVisible && !item.isVisible()) return;

            const bool enabled = !item.isEnabled || item.isEnabled();

            switch (item.type)
            {
            case ports::MenuItem::Type::Item:
                if (!enabled) ImGui::BeginDisabled();
                if (ImGui::MenuItem(
                    item.label.c_str(),
                    item.shortcut.empty() ? nullptr : item.shortcut.c_str()))
                {
                    if (item.onClick) item.onClick();
                }
                if (!enabled) ImGui::EndDisabled();
                break;

            case ports::MenuItem::Type::Toggle:
                if (!enabled) ImGui::BeginDisabled();
                if (item.togglePtr)
                    ImGui::MenuItem(item.label.c_str(), nullptr, item.togglePtr);
                if (!enabled) ImGui::EndDisabled();
                break;

            case ports::MenuItem::Type::Separator:
                ImGui::Separator();
                break;

            case ports::MenuItem::Type::SubMenu:
                if (!enabled) ImGui::BeginDisabled();
                if (ImGui::BeginMenu(item.label.c_str()))
                {
                    if (item.populateSubMenu)
                    {
                        // Create a temporary contribution to collect sub-items
                        ports::MenuContribution sub("__sub", item.label, 0);
                        item.populateSubMenu(sub);
                        for (const auto& subItem : sub.items())
                            renderMenuItem(subItem);
                    }
                    ImGui::EndMenu();
                }
                if (!enabled) ImGui::EndDisabled();
                break;
            }
        }

        void renderRibbonItem(const ports::RibbonItem& item)
        {
            // Visibility condition
            if (item.isVisible && !item.isVisible()) return;

            const bool enabled = !item.isEnabled || item.isEnabled();

            switch (item.type)
            {
            case ports::RibbonItem::Type::Button:
                if (!enabled) ImGui::BeginDisabled();
                
                ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(24.0f, 8.0f));

                if (ImGui::Button(item.label.c_str()) && item.onClick)
                    item.onClick();
                ImGui::PopStyleVar();
                if (!enabled) ImGui::EndDisabled();
                ImGui::SameLine();
                break;

            case ports::RibbonItem::Type::Toggle:
                if (!enabled) ImGui::BeginDisabled();
                if (item.togglePtr)
                    ImGui::Checkbox(item.label.c_str(), item.togglePtr);
                if (!enabled) ImGui::EndDisabled();
                ImGui::SameLine();
                break;

            case ports::RibbonItem::Type::Separator:
                ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);
                ImGui::SameLine();
                break;

            case ports::RibbonItem::Type::Custom:
                if (item.renderFn) item.renderFn();
                ImGui::SameLine();
                break;
            }
        }

       
        std::vector<std::unique_ptr<ports::MenuContribution>>   m_menus;
        std::vector<std::unique_ptr<ports::RibbonContribution>> m_ribbons;
        std::vector<std::unique_ptr<ports::DragDropContribution>>  m_dragDrops;
    };

} // namespace adapters