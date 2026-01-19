
#pragma once
#include "adapters/ui/plugins/UiModuleApi.h"
#include <string>

#include <windows.h>
#include <filesystem>
#include <windows.h>
#include <filesystem>

static std::filesystem::path GetExeDir()
{
    char buf[MAX_PATH]{};
    GetModuleFileNameA(nullptr, buf, MAX_PATH);
    return std::filesystem::path(buf).parent_path();
}

static std::filesystem::path GetPluginDir()
{
    return GetExeDir() / "plugins";
}


class UiPluginLoader {
public:
    bool load(const std::string& sourceDllPath, UiHostServices& svc);
    bool reload(UiHostServices& svc);
    void unload(UiHostServices& svc);
    void render(UiHostServices& svc);

    const std::string& sourcePath() const { return m_sourcePath; }
    const std::string& loadedPath() const { return m_loadedPath; }

private:
    static bool shadowCopyFile(const std::string& sourceDllPath, std::string& outLoadedPath);

private:
    void* m_lib = nullptr;
    IUiModule* m_module = nullptr;
    std::string m_sourcePath;
    std::string m_loadedPath;
};
