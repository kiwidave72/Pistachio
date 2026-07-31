#pragma once

#include "adapters/plugins/ServiceModuleLoader.h"
#include "core/IApplication.h"

#include <string>
#include <vector>
#include <filesystem>
#include <memory>

// -----------------------------------------------------------------------
// ServiceModuleRegistry
//
// Scans the services/ directory for all *.dll files and loads each one
// as an IServiceModule via ServiceModuleLoader. Mirrors UiPluginRegistry's
// directory-scan shape, but simpler: no auto-reload tick/settle-timer
// machinery yet (see ServiceModuleLoader.h for why).
//
// Intended call order in main.cpp:
//   1. app->registerCoreServices()
//   2. serviceRegistry.loadAll(*app)      <-- Phase 1: services register themselves
//   3. uiPluginRegistry.loadAll(svc, dc)  <-- Phase 2: UI plugins resolve what they need
// -----------------------------------------------------------------------
class ServiceModuleRegistry
{
public:
    ServiceModuleRegistry() = default;
    ~ServiceModuleRegistry() = default;

    // Load all service plugins found in the services/ directory.
    // Returns number of plugins successfully loaded.
    int loadAll(core::IApplication& app);

    // Unload all loaded service plugins.
    void unloadAll(core::IApplication& app);

    int count()       const { return (int)m_entries.size(); }
    int loadedCount() const;

    struct PluginEntry
    {
        std::filesystem::path sourcePath;
        std::unique_ptr<ServiceModuleLoader> loader;
    };

    const std::vector<PluginEntry>& entries() const { return m_entries; }

private:
    static std::filesystem::path getServicesDir();

    std::vector<PluginEntry> m_entries;
};