#pragma once

// -----------------------------------------------------------------------
// MenuContribution.h / RibbonContribution.h
//
// Live contribution system for menus and ribbon bars.
//
// - Any plugin (or the host) can contribute to any named menu/group
// - Contributions can be added/removed at any time — not just onLoad
// - Items have optional visibility/enabled conditions evaluated each frame
// - Priority controls ordering — lower = earlier/leftmost
// - Conflicts resolved silently: last-registered item with same id wins
// - No errors thrown — all operations are safe to call at any time
//
// Place at: src/ports/Contributions.h
// -----------------------------------------------------------------------

#include <functional>
#include <string>
#include <vector>
#include <algorithm>
#include <memory>

namespace ports {


    // -----------------------------------------------------------------------
    // DragDropContribution
    // Registers a named drop target that the host renders each frame.
    // The host calls renderTarget() inside the appropriate ImGui context.
    // -----------------------------------------------------------------------
    class DragDropContribution
    {
    public:
        DragDropContribution(std::string pluginId,
            std::string payloadType,
            int         priority = 100)
            : m_pluginId(std::move(pluginId))
            , m_payloadType(std::move(payloadType))
            , m_priority(priority)
        {
        }

        // Called by the host after ImGui::Image or any item that should
        // accept drops. Put your AcceptDragDropPayload logic here.
        std::function<void(const void* data, size_t size)> onDrop;

        // Optional — called every frame to render a visual highlight
        // when something is being dragged over this target
        std::function<void()> onHoverRender;

        const std::string& pluginId()    const { return m_pluginId; }
        const std::string& payloadType() const { return m_payloadType; }
        int                priority()    const { return m_priority; }

    private:
        std::string m_pluginId;
        std::string m_payloadType;
        int         m_priority = 100;
    };

    // -----------------------------------------------------------------------
    // Shared item condition callbacks
    // -----------------------------------------------------------------------
    using ConditionFn = std::function<bool()>;   // returns true = visible/enabled
    using ActionFn = std::function<void()>;   // click / toggle handler

    // -----------------------------------------------------------------------
    // RibbonItem
    // -----------------------------------------------------------------------
    struct RibbonItem
    {
        enum class Type { Button, Toggle, Separator, Custom };

        std::string  id;
        std::string  label;
        std::string  icon;
        int          priority = 0;
        Type         type = Type::Button;

        bool* togglePtr = nullptr;    // for Toggle items
        ActionFn     onClick;                 // for Button items
        ActionFn     renderFn;               // for Custom items (raw ImGui calls)

        ConditionFn  isVisible;              // nullptr = always visible
        ConditionFn  isEnabled;              // nullptr = always enabled
    };

    // -----------------------------------------------------------------------
    // RibbonContribution
    //
    // Represents one plugin's contribution to a named ribbon group.
    // The plugin holds a raw pointer; the host owns the object.
    // -----------------------------------------------------------------------
    class RibbonContribution
    {
    public:
        RibbonContribution(std::string pluginId, std::string groupName, int groupPriority)
            : m_pluginId(std::move(pluginId))
            , m_groupName(std::move(groupName))
            , m_groupPriority(groupPriority)
        {
        }

        // Add a button
        RibbonContribution& addButton(
            std::string id,
            std::string label,
            std::string icon,  
            int priority,
            ActionFn onClick,
            ConditionFn isVisible = nullptr,
            ConditionFn isEnabled = nullptr)
        {
            removeItem(id);
            RibbonItem item;
            item.id = std::move(id);
            item.label = std::move(label);
            item.icon = std::move(icon);
            item.priority = priority;
            item.type = RibbonItem::Type::Button;
            item.onClick = std::move(onClick);
            item.isVisible = std::move(isVisible);
            item.isEnabled = std::move(isEnabled);
            m_items.push_back(std::move(item));
            sortItems();
            return *this;
        }

        // Add a toggle
        RibbonContribution& addToggle(
            std::string id, std::string label, int priority,
            bool* value,
            ConditionFn isVisible = nullptr,
            ConditionFn isEnabled = nullptr)
        {
            removeItem(id);
            RibbonItem item;
            item.id = std::move(id);
            item.label = std::move(label);
            item.priority = priority;
            item.type = RibbonItem::Type::Toggle;
            item.togglePtr = value;
            item.isVisible = std::move(isVisible);
            item.isEnabled = std::move(isEnabled);
            m_items.push_back(std::move(item));
            sortItems();
            return *this;
        }

        // Add a separator
        RibbonContribution& addSeparator(int priority)
        {
            RibbonItem item;
            item.id = "__sep_" + std::to_string(priority);
            item.priority = priority;
            item.type = RibbonItem::Type::Separator;
            m_items.push_back(std::move(item));
            sortItems();
            return *this;
        }

        // Add custom ImGui content
        RibbonContribution& addCustom(
            std::string id, int priority,
            ActionFn renderFn,
            ConditionFn isVisible = nullptr)
        {
            removeItem(id);
            RibbonItem item;
            item.id = std::move(id);
            item.priority = priority;
            item.type = RibbonItem::Type::Custom;
            item.renderFn = std::move(renderFn);
            item.isVisible = std::move(isVisible);
            m_items.push_back(std::move(item));
            sortItems();
            return *this;
        }

        // Remove a single item by id
        void removeItem(const std::string& id)
        {
            m_items.erase(
                std::remove_if(m_items.begin(), m_items.end(),
                    [&](const RibbonItem& i) { return i.id == id; }),
                m_items.end());
        }

        bool hasItem(const std::string& id) const
        {
            for (const auto& i : m_items)
                if (i.id == id) return true;
            return false;
        }

        // Clear all items
        void clear() { m_items.clear(); }

        // Accessors used by the host renderer
        const std::string& pluginId()      const { return m_pluginId; }
        const std::string& groupName()     const { return m_groupName; }
        int                            groupPriority() const { return m_groupPriority; }
        const std::vector<RibbonItem>& items()         const { return m_items; }

    private:
        void sortItems()
        {
            std::stable_sort(m_items.begin(), m_items.end(),
                [](const RibbonItem& a, const RibbonItem& b) {
                    return a.priority < b.priority;
                });
        }

        std::string             m_pluginId;
        std::string             m_groupName;
        int                     m_groupPriority = 0;
        std::vector<RibbonItem> m_items;
    };

    // -----------------------------------------------------------------------
    // MenuItem
    // -----------------------------------------------------------------------
    struct MenuItem
    {
        enum class Type { Item, Toggle, Separator, SubMenu };

        std::string  id;
        std::string  label;
        std::string  shortcut;
        int          priority = 0;
        Type         type = Type::Item;

        bool* togglePtr = nullptr;
        ActionFn     onClick;

        // SubMenu — populated lazily each frame via callback
        std::function<void(class MenuContribution&)> populateSubMenu;

        ConditionFn  isVisible;
        ConditionFn  isEnabled;
    };

    // -----------------------------------------------------------------------
    // MenuContribution
    //
    // Represents one plugin's contribution to a named top-level menu.
    // The plugin holds a raw pointer; the host owns the object.
    // Plugins can contribute to existing host menus (e.g. "File", "Tools")
    // or create new top-level menus.
    // -----------------------------------------------------------------------
    class MenuContribution
    {
    public:
        MenuContribution(std::string pluginId, std::string menuName, int menuPriority)
            : m_pluginId(std::move(pluginId))
            , m_menuName(std::move(menuName))
            , m_menuPriority(menuPriority)
        {
        }

        // Add a menu item
        MenuContribution& addItem(
            std::string id, std::string label, int priority,
            ActionFn onClick,
            std::string shortcut = {},
            ConditionFn isVisible = nullptr,
            ConditionFn isEnabled = nullptr)
        {
            removeItem(id);
            MenuItem item;
            item.id = std::move(id);
            item.label = std::move(label);
            item.priority = priority;
            item.type = MenuItem::Type::Item;
            item.shortcut = std::move(shortcut);
            item.onClick = std::move(onClick);
            item.isVisible = std::move(isVisible);
            item.isEnabled = std::move(isEnabled);
            m_items.push_back(std::move(item));
            sortItems();
            return *this;
        }

        // Add a checkable item
        MenuContribution& addToggle(
            std::string id, std::string label, int priority,
            bool* value,
            ConditionFn isVisible = nullptr,
            ConditionFn isEnabled = nullptr)
        {
            removeItem(id);
            MenuItem item;
            item.id = std::move(id);
            item.label = std::move(label);
            item.priority = priority;
            item.type = MenuItem::Type::Toggle;
            item.togglePtr = value;
            item.isVisible = std::move(isVisible);
            item.isEnabled = std::move(isEnabled);
            m_items.push_back(std::move(item));
            sortItems();
            return *this;
        }

        // Add a separator
        MenuContribution& addSeparator(int priority)
        {
            MenuItem item;
            item.id = "__sep_" + std::to_string(priority);
            item.priority = priority;
            item.type = MenuItem::Type::Separator;
            m_items.push_back(std::move(item));
            sortItems();
            return *this;
        }

        // Add a submenu — populate callback is called each frame when open
        MenuContribution& addSubMenu(
            std::string id, std::string label, int priority,
            std::function<void(MenuContribution&)> populate,
            ConditionFn isVisible = nullptr,
            ConditionFn isEnabled = nullptr)
        {
            removeItem(id);
            MenuItem item;
            item.id = std::move(id);
            item.label = std::move(label);
            item.priority = priority;
            item.type = MenuItem::Type::SubMenu;
            item.populateSubMenu = std::move(populate);
            item.isVisible = std::move(isVisible);
            item.isEnabled = std::move(isEnabled);
            m_items.push_back(std::move(item));
            sortItems();
            return *this;
        }

        // Remove a single item by id
        void removeItem(const std::string& id)
        {
            m_items.erase(
                std::remove_if(m_items.begin(), m_items.end(),
                    [&](const MenuItem& i) { return i.id == id; }),
                m_items.end());
        }

        bool hasItem(const std::string& id) const
        {
            for (const auto& i : m_items)
                if (i.id == id) return true;
            return false;
        }

        // Clear all items
        void clear() { m_items.clear(); }

        // Accessors used by the host renderer
        const std::string& pluginId()      const { return m_pluginId; }
        const std::string& menuName()      const { return m_menuName; }
        int                            menuPriority()  const { return m_menuPriority; }
        const std::vector<MenuItem>& items()         const { return m_items; }

    private:
        void sortItems()
        {
            std::stable_sort(m_items.begin(), m_items.end(),
                [](const MenuItem& a, const MenuItem& b) {
                    return a.priority < b.priority;
                });
        }

        std::string            m_pluginId;
        std::string            m_menuName;
        int                    m_menuPriority = 0;
        std::vector<MenuItem>  m_items;
    };


    

   

 } // namespace ports