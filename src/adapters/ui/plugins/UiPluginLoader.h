
#pragma once
#include "adapters/ui/plugins/UiModuleApi.h"
#include "domain/DataContext.h"

#include <string>

#include <filesystem>

namespace fs = std::filesystem;

class UiPluginLoader
{
public:
    UiPluginLoader() = default;
    ~UiPluginLoader();

    // Preferred API (explicit services)
    bool load(UiHostServices& svc,domain::DataContext& dataContext, const fs::path& filename);
    void unload(UiHostServices& svc, domain::DataContext& dataContext );
    bool reload(UiHostServices& svc, domain::DataContext& dataContext, const fs::path& filename);
    void render(UiHostServices& svc, domain::DataContext& dataContext);

    // Backwards-compatible helpers (use last bound services from load())
    void unload();      // calls unload(*m_lastSvc) if available
    bool reload(const fs::path& filename);      // calls reload(*m_lastSvc) if available
    void render();      // calls render(*m_lastSvc) if available

    bool isLoaded() const { return m_module != nullptr; }

    const std::string& sourcePath() const { return m_sourcePath; }
    const std::string& loadedPath() const { return m_loadedPath; }
    const std::string& lastError() const { return m_lastError; }

    // Manifest snapshot
    const std::string& manifestId() const { return m_manifestId; }
    const std::string& manifestName() const { return m_manifestName; }
    const std::string& manifestVersion() const { return m_manifestVersion; }
    const std::string& manifestFeatureGroup() const { return m_manifestFeatureGroup; }

private:
    bool shadowCopyToUnique(const std::string& sourceDllPath, std::string& outLoadedPath);
    void clearManifest();

private:
    void* m_lib = nullptr;
    IUiModule* m_module = nullptr;

    UiHostServices* m_lastSvc = nullptr; // non-owning, valid for app lifetime
    domain::DataContext* m_lastDataContext = nullptr;

    std::string m_sourcePath;
    std::string m_loadedPath;
    std::string m_lastError;

    std::string m_manifestId;
    std::string m_manifestName;
    std::string m_manifestVersion;
    std::string m_manifestFeatureGroup;
};
