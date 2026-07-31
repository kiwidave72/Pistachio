#include "adapters/plugins/ServiceModuleRegistry.h"

#include <cstdio>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#endif

namespace fs = std::filesystem;

// ---------------------------------------------------------------------------
static fs::path getExeDir()
{
#ifdef _WIN32
    char buf[MAX_PATH] = {};
    GetModuleFileNameA(nullptr, buf, MAX_PATH);
    return fs::path(buf).parent_path();
#else
    return fs::current_path();
#endif
}

fs::path ServiceModuleRegistry::getServicesDir()
{
    return getExeDir() / "services";
}

// ---------------------------------------------------------------------------
int ServiceModuleRegistry::loadAll(core::IApplication& app)
{
    m_entries.clear();

    fs::path dir = getServicesDir();
    if (!fs::exists(dir))
    {
        printf("[ServiceModuleRegistry] services dir not found: %s (0 service plugins — expected until a real one exists)\n",
            dir.string().c_str());
        return 0;
    }

    int loaded = 0;
    std::error_code ec;

    for (const auto& entry : fs::directory_iterator(dir, ec))
    {
        if (ec) break;

        const fs::path& p = entry.path();
        if (!fs::is_regular_file(p)) continue;

#ifdef _WIN32
        if (p.extension() != ".dll") continue;
#else
        if (p.extension() != ".so") continue;
#endif

        printf("[ServiceModuleRegistry] loading: %s\n", p.filename().string().c_str());

        PluginEntry pe;
        pe.sourcePath = p;
        pe.loader = std::make_unique<ServiceModuleLoader>();

        if (pe.loader->load(app, p))
        {
            printf("[ServiceModuleRegistry] loaded: %s  id=%s\n",
                p.filename().string().c_str(),
                pe.loader->manifestId().c_str());
            ++loaded;
        }
        else
        {
            printf("[ServiceModuleRegistry] FAILED: %s  error=%s\n",
                p.filename().string().c_str(),
                pe.loader->lastError().c_str());
        }

        m_entries.push_back(std::move(pe));
    }

    printf("[ServiceModuleRegistry] loadAll: %d/%d loaded\n", loaded, (int)m_entries.size());
    return loaded;
}

// ---------------------------------------------------------------------------
void ServiceModuleRegistry::unloadAll(core::IApplication& app)
{
    for (auto& e : m_entries)
    {
        if (e.loader && e.loader->isLoaded())
        {
            printf("[ServiceModuleRegistry] unloading: %s\n", e.loader->manifestId().c_str());
            e.loader->unload(app);
        }
    }
    m_entries.clear();
}

// ---------------------------------------------------------------------------
int ServiceModuleRegistry::loadedCount() const
{
    int n = 0;
    for (const auto& e : m_entries)
        if (e.loader && e.loader->isLoaded()) ++n;
    return n;
}