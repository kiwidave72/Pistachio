#pragma once

#include "adapters/ui/plugins/UiPluginLoader.h"
#include "adapters/ui/plugins/UiModuleApi.h"

#include <string>
#include <vector>
#include <filesystem>
#include <memory>

// -----------------------------------------------------------------------
// UiPluginRegistry
//
// Scans the plugins/ directory for all *.dll files, loads each one as
// a separate UiPluginLoader instance, and drives them all together.
//
// Each PluginEntry pairs a loader with the source path it was loaded from
// so reload() knows which file to shadow-copy.
// -----------------------------------------------------------------------
class UiPluginRegistry
{
public:
    UiPluginRegistry() = default;
    ~UiPluginRegistry() = default;

    // Load all plugins found in the plugins/ directory.
    // Returns number of plugins successfully loaded.
    int loadAll(UiHostServices& svc,domain::DataContext& dataContext);

    // Unload all loaded plugins.
    void unloadAll(UiHostServices& svc, domain::DataContext& dataContext);

    // Reload all loaded plugins (e.g. after manual hot-reload request).
    void reloadAll(UiHostServices& svc, domain::DataContext& dataContext);

    // Reload only plugins whose source DLL write time has changed.
    // Returns number of plugins reloaded.
    int reloadChanged(UiHostServices& svc, domain::DataContext& dataContext);

    // Call render() on all loaded plugins.
    void renderAll(UiHostServices& svc, domain::DataContext& dataContext);

    // Mark all plugins for reload on next beginFrame.
    void requestReloadAll();
    bool isReloadRequested() const { return m_reloadRequested; }
    void clearReloadRequest() { m_reloadRequested = false; }

    int count()       const { return (int)m_entries.size(); }
    int loadedCount() const;
 
    int tickAutoReload(UiHostServices& svc, std::chrono::milliseconds settleTime = std::chrono::seconds(10));
    bool hasReloadPending() const;
    void clearReloadPending();

    struct PluginEntry
    {
        fs::path                             sourcePath;
        fs::file_time_type                   lastWrite{};
        fs::file_time_type                   pendingWrite{};
        std::unique_ptr<UiPluginLoader>      loader;
        std::chrono::steady_clock::time_point settleStart{};
        bool reloadPending = false;
    };

    const std::vector<PluginEntry>& entries() const { return m_entries; }

private:
    static std::filesystem::path getPluginsDir();

    std::vector<PluginEntry> m_entries;
    bool m_reloadRequested = false;
};