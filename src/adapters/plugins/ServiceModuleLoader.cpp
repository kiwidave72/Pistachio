#include "adapters/plugins/ServiceModuleLoader.h"

#ifdef _WIN32
#include <windows.h>
#endif

#include <filesystem>
#include <chrono>
#include <thread>
#include <cstdio>

namespace fs = std::filesystem;

static std::string nowMillis()
{
    using namespace std::chrono;
    auto ms = duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();
    return std::to_string(ms);
}

ServiceModuleLoader::~ServiceModuleLoader()
{
    if (m_module || m_lib)
    {
        // Best-effort teardown if the owner forgot to call unload() explicitly.
        // No IApplication& available here, so onUnload() can't be called safely —
        // callers should always call unload(app) before destruction.
        printf("[ServiceModuleLoader] WARNING: destroyed while still loaded (%s) — call unload(app) first\n",
            m_manifestId.c_str());
    }
}

void ServiceModuleLoader::clearManifest()
{
    m_manifestId.clear();
    m_manifestName.clear();
    m_manifestVersion.clear();
    m_manifestFeatureGroup.clear();
}

bool ServiceModuleLoader::shadowCopyToUnique(const std::string& sourceDllPath, std::string& outLoadedPath)
{
    try
    {
        fs::path src(sourceDllPath);
        if (!fs::exists(src))
            return false;

        fs::path loadedDir = src.parent_path() / "_loaded";
        fs::create_directories(loadedDir);

#ifdef _WIN32
        std::error_code ec;
        const std::string pidSuffix = "_" + std::to_string(::GetCurrentProcessId());
        for (auto& entry : fs::directory_iterator(loadedDir, ec))
        {
            const std::string fname = entry.path().filename().string();
            if (fname.find(src.stem().string()) != std::string::npos &&
                fname.find(pidSuffix) != std::string::npos)
            {
                fs::remove(entry.path(), ec); // ignore errors — file may still be loaded
            }
        }

        for (int i = 0; i < 50; ++i)
        {
            fs::path dst = loadedDir / (src.stem().string() + "_" + nowMillis() + pidSuffix + src.extension().string());

            HANDLE h = CreateFileA(
                src.string().c_str(),
                GENERIC_READ,
                FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                nullptr,
                OPEN_EXISTING,
                FILE_ATTRIBUTE_NORMAL,
                nullptr);

            if (h != INVALID_HANDLE_VALUE)
            {
                CloseHandle(h);
                if (CopyFileA(src.string().c_str(), dst.string().c_str(), FALSE))
                {
                    outLoadedPath = dst.string();
                    return true;
                }
                fs::remove(dst, ec);
            }

            std::this_thread::sleep_for(std::chrono::milliseconds(1000));
        }
        return false;
#else
        fs::path dst = loadedDir / (src.stem().string() + "_" + nowMillis() + src.extension().string());
        fs::copy_file(src, dst, fs::copy_options::overwrite_existing);
        outLoadedPath = dst.string();
        return true;
#endif
    }
    catch (...)
    {
        return false;
    }
}

bool ServiceModuleLoader::load(core::IApplication& app, const fs::path& filename)
{
    m_lastApp = &app;
    m_lastError.clear();
    clearManifest();

    if (m_module)
        return true;

    m_sourcePath = filename.string();

    std::string loaded;
    if (!shadowCopyToUnique(m_sourcePath, loaded))
    {
        m_lastError = "Failed to shadow-copy: " + m_sourcePath;
        return false;
    }
    m_loadedPath = loaded;

#ifdef _WIN32
    HMODULE lib = LoadLibraryA(m_loadedPath.c_str());
    if (!lib)
    {
        m_lastError = "LoadLibrary failed: " + m_loadedPath;
        return false;
    }

    auto createFn = (decltype(&pistachio_create_service_module))GetProcAddress(lib, "pistachio_create_service_module");
    auto destroyFn = (decltype(&pistachio_destroy_service_module))GetProcAddress(lib, "pistachio_destroy_service_module");
    auto manFn = (decltype(&pistachio_get_service_manifest))GetProcAddress(lib, "pistachio_get_service_manifest");

    if (!createFn || !destroyFn)
    {
        FreeLibrary(lib);
        m_lastError = "Missing required exports (create/destroy)";
        return false;
    }

    if (manFn)
    {
        const ServicePluginManifestV1* m = manFn();
        if (m && m->api_version == 1 && m->struct_size >= sizeof(ServicePluginManifestV1))
        {
            if (m->id)            m_manifestId = m->id;
            if (m->name)          m_manifestName = m->name;
            if (m->version)       m_manifestVersion = m->version;
            if (m->feature_group) m_manifestFeatureGroup = m->feature_group;
        }
    }

    IServiceModule* mod = createFn();
    if (!mod)
    {
        FreeLibrary(lib);
        m_lastError = "Create module returned null";
        return false;
    }

    m_lib = (void*)lib;
    m_module = mod;
    printf("[ServiceModuleLoader] calling onLoad: %s\n", m_manifestId.c_str());
    m_module->onLoad(app);
    printf("[ServiceModuleLoader] onLoad done: %s\n", m_manifestId.c_str());

    return true;
#else
    m_lastError = "Non-Windows loader not implemented.";
    return false;
#endif
}

void ServiceModuleLoader::unload(core::IApplication& app)
{
    if (!m_module || !m_lib)
        return;

#ifdef _WIN32
    auto lib = (HMODULE)m_lib;
    auto destroyFn = (decltype(&pistachio_destroy_service_module))GetProcAddress(lib, "pistachio_destroy_service_module");

    printf("[ServiceModuleLoader] calling onUnload: %s\n", m_manifestId.c_str());
    m_module->onUnload(app);

    if (destroyFn)
        destroyFn(m_module);

    m_module = nullptr;

    FreeLibrary(lib);
    m_lib = nullptr;
    printf("[ServiceModuleLoader] unload done\n");
    clearManifest();
#endif
}