#pragma once

// -----------------------------------------------------------------------
// RibbonHostImpl / MenuHostImpl
//
// Concrete implementations of IRibbonHost and IMenuHost that live in the
// host EXE (ImGuiHost). All group/item storage is host-owned so it is
// safe across plugin hot-reloads.
//
// Place this file at: src/adapters/ui/HostMenuRibbonImpl.h
// Include it in ImGuiHost.cpp only.
// -----------------------------------------------------------------------

#include "ports/IRibbonHost.h"
#include "ports/IMenuHost.h"

#include <imgui.h>
#include <imgui_internal.h>
#include <string>
#include <vector>
#include <functional>
#include <algorithm>

// -----------------------------------------------------------------------
// Ribbon
// -----------------------------------------------------------------------

struct RibbonItem
{
    enum class Type { Button, Toggle, Separator, Custom };

    Type                  type = Type::Button;
    std::string           id;
    std::string           label;
    bool* togglePtr = nullptr;
    std::function<void()> onClick;
    std::function<void()> renderFn;
};

struct RibbonGroupImpl : IRibbonGroup
{
    std::string              pluginId;
    std::string              label;
    std::vector<RibbonItem>  items;

    void addButton(const char* id, const char* lbl,
        std::function<void()> onClick) override
    {
        RibbonItem item;
        item.type = RibbonItem::Type::Button;
        item.id = id;
        item.label = lbl;
        item.onClick = std::move(onClick);
        items.push_back(std::move(item));
    }

    void addToggle(const char* id, const char* lbl, bool* value) override
    {
        RibbonItem item;
        item.type = RibbonItem::Type::Toggle;
        item.id = id;
        item.label = lbl;
        item.togglePtr = value;
        items.push_back(std::move(item));
    }

    void addSeparator() override
    {
        RibbonItem item;
        item.type = RibbonItem::Type::Separator;
        items.push_back(std::move(item));
    }

    void addCustom(const char* id, std::function<void()> renderFn) override
    {
        RibbonItem item;
        item.type = RibbonItem::Type::Custom;
        item.id = id;
        item.renderFn = std::move(renderFn);
        items.push_back(std::move(item));
    }

    void render() const
    {
        for (const auto& item : items)
        {
            switch (item.type)
            {
            case RibbonItem::Type::Button:
                if (ImGui::Button(item.label.c_str()) && item.onClick)
                    item.onClick();
                ImGui::SameLine();
                break;

            case RibbonItem::Type::Toggle:
                if (item.togglePtr)
                {
                    ImGui::Checkbox(item.label.c_str(), item.togglePtr);
                    ImGui::SameLine();
                }
                break;

            case RibbonItem::Type::Separator:
                ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);
                ImGui::SameLine();
                break;

            case RibbonItem::Type::Custom:
                if (item.renderFn)
                    item.renderFn();
                ImGui::SameLine();
                break;
            }
        }
    }
};

class RibbonHostImpl : public IRibbonHost
{
public:
    IRibbonGroup* addGroup(const char* pluginId,
        const char* groupLabel) override
    {
        // Remove existing group for this plugin first
        removeGroup(pluginId);

        auto g = std::make_unique<RibbonGroupImpl>();
        g->pluginId = pluginId;
        g->label = groupLabel;
        auto* ptr = g.get();
        m_groups.push_back(std::move(g));
        return ptr;
    }

    void removeGroup(const char* pluginId) override
    {
        m_groups.erase(
            std::remove_if(m_groups.begin(), m_groups.end(),
                [&](const std::unique_ptr<RibbonGroupImpl>& g) {
                    return g->pluginId == pluginId;
                }),
            m_groups.end());
    }

    // Called by ImGuiHost each frame inside the ribbon bar area
    void renderAll() const
    {
        bool first = true;
        for (const auto& g : m_groups)
        {
            if (g->items.empty()) continue;

            if (!first)
            {
                ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);
                ImGui::SameLine();
            }
            first = false;

            g->render();
        }
    }

private:
    std::vector<std::unique_ptr<RibbonGroupImpl>> m_groups;
};

// -----------------------------------------------------------------------
// Menu
// -----------------------------------------------------------------------

struct MenuItem
{
    enum class Type { Item, Toggle, Separator, SubMenu };

    Type                                        type = Type::Item;
    std::string                                 id;
    std::string                                 label;
    std::string                                 shortcut;
    bool* togglePtr = nullptr;
    std::function<void()>                       onClick;
    std::function<void(IMenuGroup&)>            populate;
    std::vector<MenuItem>                       children; // for submenus
};

struct MenuGroupImpl : IMenuGroup
{
    std::string            pluginId;
    std::string            label;
    std::vector<MenuItem>  items;

    void addItem(const char* id, const char* lbl,
        std::function<void()> onClick,
        const char* shortcut = nullptr) override
    {
        MenuItem item;
        item.type = MenuItem::Type::Item;
        item.id = id;
        item.label = lbl;
        item.shortcut = shortcut ? shortcut : "";
        item.onClick = std::move(onClick);
        items.push_back(std::move(item));
    }

    void addToggle(const char* id, const char* lbl, bool* value) override
    {
        MenuItem item;
        item.type = MenuItem::Type::Toggle;
        item.id = id;
        item.label = lbl;
        item.togglePtr = value;
        items.push_back(std::move(item));
    }

    void addSeparator() override
    {
        MenuItem item;
        item.type = MenuItem::Type::Separator;
        items.push_back(std::move(item));
    }

    void addSubMenu(const char* id, const char* lbl,
        std::function<void(IMenuGroup&)> populate) override
    {
        MenuItem item;
        item.type = MenuItem::Type::SubMenu;
        item.id = id;
        item.label = lbl;
        item.populate = std::move(populate);
        items.push_back(std::move(item));
    }

    void render()
    {
        for (auto& item : items)
        {
            switch (item.type)
            {
            case MenuItem::Type::Item:
                if (ImGui::MenuItem(item.label.c_str(),
                    item.shortcut.empty() ? nullptr : item.shortcut.c_str()))
                {
                    if (item.onClick) item.onClick();
                }
                break;

            case MenuItem::Type::Toggle:
                if (item.togglePtr)
                    ImGui::MenuItem(item.label.c_str(), nullptr, item.togglePtr);
                break;

            case MenuItem::Type::Separator:
                ImGui::Separator();
                break;

            case MenuItem::Type::SubMenu:
                if (ImGui::BeginMenu(item.label.c_str()))
                {
                    // Use a temporary MenuGroupImpl to collect and render children
                    MenuGroupImpl sub;
                    sub.pluginId = pluginId;
                    sub.label = item.label;
                    if (item.populate) item.populate(sub);
                    sub.render();
                    ImGui::EndMenu();
                }
                break;
            }
        }
    }
};

class MenuHostImpl : public IMenuHost
{
public:
    IMenuGroup* addMenu(const char* pluginId,
        const char* menuLabel) override
    {
        removeMenu(pluginId);

        auto g = std::make_unique<MenuGroupImpl>();
        g->pluginId = pluginId;
        g->label = menuLabel;
        auto* ptr = g.get();
        m_menus.push_back(std::move(g));
        return ptr;
    }

    void removeMenu(const char* pluginId) override
    {
        m_menus.erase(
            std::remove_if(m_menus.begin(), m_menus.end(),
                [&](const std::unique_ptr<MenuGroupImpl>& g) {
                    return g->pluginId == pluginId;
                }),
            m_menus.end());
    }

    // Called by ImGuiHost each frame inside BeginMenuBar
    void renderAll()
    {
        for (auto& m : m_menus)
        {
            if (ImGui::BeginMenu(m->label.c_str()))
            {
                m->render();
                ImGui::EndMenu();
            }
        }
    }

private:
    std::vector<std::unique_ptr<MenuGroupImpl>> m_menus;
};