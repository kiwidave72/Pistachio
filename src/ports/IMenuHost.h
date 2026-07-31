#pragma once

#include <functional>
#include <string>

// -----------------------------------------------------------------------
// IMenuHost / IMenuGroup
//
// Plugin-facing interface for contributing items to the application menubar.
// Plugins call addMenu() from onLoad() and removeMenu() from onUnload().
// The host renders all registered menus inside its BeginMenuBar block.
// -----------------------------------------------------------------------

struct IMenuGroup
{
    virtual ~IMenuGroup() = default;

    // Add a clickable menu item
    virtual void addItem(const char* id, const char* label,
        std::function<void()> onClick,
        const char* shortcut = nullptr) = 0;

    // Add a checkable menu item (reads/writes *value)
    virtual void addToggle(const char* id, const char* label,
        bool* value) = 0;

    // Add a separator line
    virtual void addSeparator() = 0;

    // Add a submenu — populate callback is called each frame when open
    virtual void addSubMenu(const char* id, const char* label,
        std::function<void(IMenuGroup&)> populate) = 0;
};

struct IMenuHost
{
    virtual ~IMenuHost() = default;

    // Register a top-level menu entry (e.g. "Sketch", "Slicer").
    // Returns a handle to populate with items.
    // pluginId must be unique per plugin.
    virtual IMenuGroup* addMenu(const char* pluginId,
        const char* menuLabel) = 0;

    // Remove all menus registered by this plugin — call from onUnload().
    virtual void removeMenu(const char* pluginId) = 0;
};