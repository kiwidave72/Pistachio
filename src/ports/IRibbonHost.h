#pragma once

#include <functional>
#include <string>

// -----------------------------------------------------------------------
// IRibbonHost / IRibbonGroup
//
// Plugin-facing interface for contributing to the application ribbon bar.
// Plugins call addGroup() from onLoad() and removeGroup() from onUnload().
// The host owns all group/item storage — no cross-DLL lambdas are stored
// beyond what the plugin explicitly registers and clears.
// -----------------------------------------------------------------------

struct IRibbonGroup
{
    virtual ~IRibbonGroup() = default;

    // Add a clickable button
    virtual void addButton(const char* id, const char* label,
        std::function<void()> onClick) = 0;

    // Add a toggle button (reads/writes *value each frame)
    virtual void addToggle(const char* id, const char* label,
        bool* value) = 0;

    // Add a vertical separator
    virtual void addSeparator() = 0;

    // Add arbitrary ImGui content rendered inline
    virtual void addCustom(const char* id,
        std::function<void()> renderFn) = 0;
};

struct IRibbonHost
{
    virtual ~IRibbonHost() = default;

    // Register a named group for this plugin.
    // Returns a handle to populate with buttons/toggles/custom items.
    // pluginId must be unique per plugin (used to identify the group).
    virtual IRibbonGroup* addGroup(const char* pluginId,
        const char* groupLabel) = 0;

    // Remove all groups registered by this plugin — call from onUnload().
    virtual void removeGroup(const char* pluginId) = 0;
};