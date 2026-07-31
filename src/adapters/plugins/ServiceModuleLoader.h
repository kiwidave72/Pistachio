#pragma once

#include "adapters/plugins/ServiceModuleApi.h"
#include "core/IApplication.h"

#include <string>
#include <filesystem>

namespace fs = std::filesystem;

// -----------------------------------------------------------------------
// ServiceModuleLoader
//
// Loads a single IServiceModule DLL. Mirrors UiPluginLoader's shape
// (shadow-copy-to-unique, manifest reading, load/unload) but simpler:
// no render(), no auto-reload settle-timer machinery yet — that can be
// added later, once there's a real service plugin to test hot-reload
// against.
// -----------------------------------------------------------------------
class ServiceModuleLoader
{
public:
    ServiceModuleLoader() = default;
    ~ServiceModuleLoader();

    bool load(core::IApplication& app, const fs::path& filename);
    void unload(core::IApplication& app);

    bool isLoaded() const { return m_module != nullptr; }

    const std::string& sourcePath() const { return m_sourcePath; }
    const std::string& loadedPath() const { return m_loadedPath; }
    const std::string& lastError() const { return m_lastError; }

    const std::string& manifestId() const { return m_manifestId; }
    const std::string& manifestName() const { return m_manifestName; }
    const std::string& manifestVersion() const { return m_manifestVersion; }
    const std::string& manifestFeatureGroup() const { return m_manifestFeatureGroup; }

private:
    bool shadowCopyToUnique(const std::string& sourceDllPath, std::string& outLoadedPath);
    void clearManifest();

private:
    void* m_lib = nullptr;
    IServiceModule* m_module = nullptr;

    core::IApplication* m_lastApp = nullptr; // non-owning, valid for app lifetime

    std::string m_sourcePath;
    std::string m_loadedPath;
    std::string m_lastError;

    std::string m_manifestId;
    std::string m_manifestName;
    std::string m_manifestVersion;
    std::string m_manifestFeatureGroup;
};