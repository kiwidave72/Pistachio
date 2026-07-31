#include "adapters/ui/plugins/UiPluginRegistry.h"

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

fs::path UiPluginRegistry::getPluginsDir()
{
    return getExeDir() / "plugins";
}

static fs::file_time_type safeLastWriteTime(const fs::path& p)
{
    try {
        if (fs::exists(p))
            return fs::last_write_time(p);
    }
    catch (...) {}
    return {};
}

// ---------------------------------------------------------------------------
int UiPluginRegistry::loadAll(UiHostServices& svc,domain::DataContext& dataContext)
{
    m_entries.clear();

    fs::path dir = getPluginsDir();
    if (!fs::exists(dir))
    {
        printf("[UiPluginRegistry] plugins dir not found: %s\n", dir.string().c_str());
        return 0;
    }

    int loaded = 0;
    std::error_code ec;

    for (const auto& entry : fs::directory_iterator(dir, ec))
    {
        if (ec) break;

        const fs::path& p = entry.path();

        // Only top-level DLLs — skip the _loaded shadow-copy subdirectory
        if (!fs::is_regular_file(p)) continue;

#ifdef _WIN32
        if (p.extension() != ".dll") continue;

        // Exclusion list — legacy or non-plugin DLLs that live in the plugins
        // folder but should not be loaded as IUiModule plugins.
        static const char* k_excluded[] = {
            "pistachio_ui.dll",       // old monolithic plugin (replaced by registry)
            "pistachio_ui_stub.dll",  // stub kept for reference
        };
        bool excluded = false;
        for (const char* ex : k_excluded)
            if (p.filename() == ex) { excluded = true; break; }
        if (excluded)
        {
            printf("[UiPluginRegistry] skipping excluded DLL: %s\n",
                p.filename().string().c_str());
            continue;
        }
#else
        if (p.extension() != ".so") continue;
#endif

        printf("[UiPluginRegistry] loading: %s\n", p.filename().string().c_str());

        PluginEntry pe;
        pe.sourcePath = p;
        pe.lastWrite = safeLastWriteTime(p);
        pe.loader = std::make_unique<UiPluginLoader>();

        if (pe.loader->load(svc,dataContext, p))
        {
            printf("[UiPluginRegistry] loaded: %s  id=%s\n",
                p.filename().string().c_str(),
                pe.loader->manifestId().c_str());
            ++loaded;
        }
        else
        {
            printf("[UiPluginRegistry] FAILED: %s  error=%s\n",
                p.filename().string().c_str(),
                pe.loader->lastError().c_str());
        }

        // Keep entry even if load failed so we can retry on reload
        m_entries.push_back(std::move(pe));
    }

    printf("[UiPluginRegistry] loadAll: %d/%d loaded\n", loaded, (int)m_entries.size());
    return loaded;
}

// ---------------------------------------------------------------------------
void UiPluginRegistry::unloadAll(UiHostServices& svc, domain::DataContext& dataContext)
{
    for (auto& e : m_entries)
    {
        if (e.loader && e.loader->isLoaded())
        {
            printf("[UiPluginRegistry] unloading: %s\n",
                e.loader->manifestId().c_str());
            e.loader->unload(svc,dataContext);
        }
    }
    m_entries.clear();
}

// ---------------------------------------------------------------------------
void UiPluginRegistry::reloadAll(UiHostServices& svc, domain::DataContext& dataContext)
{
    for (auto& e : m_entries)
    {
        if (!e.loader) continue;
        printf("[UiPluginRegistry] reloading: %s (%s)\n",
            e.loader->manifestId().c_str(),
            e.sourcePath.filename().string().c_str());

        if (e.loader->reload(svc,dataContext, e.sourcePath))
            e.lastWrite = safeLastWriteTime(e.sourcePath);
        else
            printf("[UiPluginRegistry] reload failed: %s\n",
                e.loader->lastError().c_str());
    }
}

// ---------------------------------------------------------------------------
int UiPluginRegistry::reloadChanged(UiHostServices& svc, domain::DataContext& dataContext)
{
    int reloaded = 0;
    for (auto& e : m_entries)
    {
        if (!e.loader) continue;

        const auto nowWrite = safeLastWriteTime(e.sourcePath);
        if (nowWrite == fs::file_time_type{}) continue;
        if (nowWrite == e.lastWrite)          continue;

        printf("[UiPluginRegistry] source changed, reloading: %s\n",
            e.sourcePath.filename().string().c_str());

        if (e.loader->reload(svc,dataContext, e.sourcePath))
        {
            e.lastWrite = safeLastWriteTime(e.sourcePath);
            ++reloaded;
        }
        else
        {
            printf("[UiPluginRegistry] reload failed: %s\n",
                e.loader->lastError().c_str());
        }
    }
    return reloaded;
}
// ---------------------------------------------------------------------------


int UiPluginRegistry::tickAutoReload(UiHostServices& svc, std::chrono::milliseconds settleTime)
{
    int triggered = 0;
    for (auto& e : m_entries)
    {
        if (!e.loader) continue;

        const auto nowWrite = safeLastWriteTime(e.sourcePath);
        if (nowWrite == fs::file_time_type{}) continue;

        if (nowWrite == e.lastWrite)
        {
            e.pendingWrite = {};
            e.settleStart = {};
            continue;
        }

        if (e.settleStart == std::chrono::steady_clock::time_point{})
        {
            e.settleStart = std::chrono::steady_clock::now();
            e.pendingWrite = nowWrite;
            printf("[UiPluginRegistry] %s changed, waiting to settle...\n",
                e.sourcePath.filename().string().c_str());
            continue;
        }

        if (nowWrite != e.pendingWrite)
        {
            e.settleStart = std::chrono::steady_clock::now();
            e.pendingWrite = nowWrite;
            continue;
        }

        const auto elapsed = std::chrono::steady_clock::now() - e.settleStart;
        if (elapsed < settleTime) continue;

        printf("[UiPluginRegistry] %s settled, marking for reload\n",
            e.sourcePath.filename().string().c_str());

        // Don't reload here — just mark pending so the top of beginFrame handles it
        e.reloadPending = true;
        e.pendingWrite = {};
        e.settleStart = {};
        ++triggered;
    }
    return triggered;
}
// ---------------------------------------------------------------------------
void UiPluginRegistry::clearReloadPending()
{
    for (auto& e : m_entries)
        e.reloadPending = false;
}
// ---------------------------------------------------------------------------
bool UiPluginRegistry::hasReloadPending() const
{
    for (const auto& e : m_entries)
        if (e.reloadPending) return true;
    return false;
}
// ---------------------------------------------------------------------------
void UiPluginRegistry::renderAll(UiHostServices& svc, domain::DataContext& dataContext)
{
    for (auto& e : m_entries)
    {
        if (e.loader && e.loader->isLoaded())
            e.loader->render(svc,dataContext);
    }
}

// ---------------------------------------------------------------------------
void UiPluginRegistry::requestReloadAll()
{
    m_reloadRequested = true;
}

// ---------------------------------------------------------------------------
int UiPluginRegistry::loadedCount() const
{
    int n = 0;
    for (const auto& e : m_entries)
        if (e.loader && e.loader->isLoaded()) ++n;
    return n;
}