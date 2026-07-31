
#include "adapters/ui/plugins/UiPluginLoader.h"
#include "domain/dataContext.h"

#ifdef _WIN32
  #include <windows.h>
#endif

#include <filesystem>
#include <chrono>
#include <thread>

namespace fs = std::filesystem;

#ifdef _WIN32
static fs::path getExeDir()
{
    char buf[MAX_PATH] = {};
    DWORD n = GetModuleFileNameA(nullptr, buf, MAX_PATH);
    if (n == 0 || n >= MAX_PATH) return {};
    return fs::path(buf).parent_path();
}
#else
static fs::path getExeDir()
{
    return fs::current_path();
}
#endif

static std::string nowMillis()
{
    using namespace std::chrono;
    auto ms = duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();
    return std::to_string(ms);
}

UiPluginLoader::~UiPluginLoader()
{
    if (m_module || m_lib)
        unload();
}

void UiPluginLoader::clearManifest()
{
    m_manifestId.clear();
    m_manifestName.clear();
    m_manifestVersion.clear();
    m_manifestFeatureGroup.clear();
}

bool UiPluginLoader::shadowCopyToUnique(const std::string& sourceDllPath, std::string& outLoadedPath)
{
    try
    {
        fs::path src(sourceDllPath);
        if (!fs::exists(src))
            return false;

        fs::path loadedDir = src.parent_path() / "_loaded";
        fs::create_directories(loadedDir);

#ifdef _WIN32
        // Clean up old shadow copies for this process before making a new one
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
            // Generate a new unique dst name each retry so partial files don't block us
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
                    printf("[UiPluginLoader]  ::shadowCopyToUnique done\n");
                     return true;
                }
                // CopyFileA failed — delete partial dst before retry
                fs::remove(dst, ec);
            }
            
            printf("[UiPluginLoader]  ::shadowCopyToUnique RETRY sleeping for 1000 milliseconds\n");
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

bool UiPluginLoader::load(UiHostServices& svc, domain::DataContext& dataContext, const fs::path& filename)
{


    m_lastSvc = &svc;
	m_lastDataContext = &dataContext;

    m_lastError.clear();
    clearManifest();

    printf("[UiPluginLoader]  ::load calling clearManifest done\n");

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
    printf("[UiPluginLoader]  shadowCopyToUnique done\n");

#ifdef _WIN32
    printf("[UiPluginLoader]  call LoadLibraryA\n");
    HMODULE lib = LoadLibraryA(m_loadedPath.c_str());
    if (!lib)
    {
        m_lastError = "LoadLibrary failed: " + m_loadedPath;
        return false;
    }


    auto createFn  = (decltype(&pistachio_create_ui_module))GetProcAddress(lib, "pistachio_create_ui_module");
    auto destroyFn = (decltype(&pistachio_destroy_ui_module))GetProcAddress(lib, "pistachio_destroy_ui_module");
    auto manFn     = (decltype(&pistachio_get_ui_manifest))GetProcAddress(lib, "pistachio_get_ui_manifest");

    if (!createFn || !destroyFn)
    {
        FreeLibrary(lib);
        m_lastError = "Missing required exports (create/destroy)";
        return false;
    }


    if (manFn)
    {
        const UiPluginManifestV1* m = manFn();
        if (m && m->api_version == 1 && m->struct_size >= sizeof(UiPluginManifestV1))
        {
            if (m->id)            m_manifestId = m->id;
            if (m->name)          m_manifestName = m->name;
            if (m->version)       m_manifestVersion = m->version;
            if (m->feature_group) m_manifestFeatureGroup = m->feature_group;
        }
    }
    printf("[UiPluginLoader]  found  createFn,destroyFn, manFn .\n");

    IUiModule* mod = createFn();
    if (!mod)
    {
        FreeLibrary(lib);
        m_lastError = "Create module returned null";
        return false;
    }
    printf("[UiPluginLoader]  Create module done.\n");

    m_lib = (void*)lib;
    m_module = mod;
    printf("[UiPluginLoader]  call module onload.\n");
    m_module->onLoad(svc,dataContext);
    printf("[UiPluginLoader]  call module onload done\n");

    return true;
#else
    m_lastError = "Non-Windows loader not implemented.";
    return false;
#endif
}

void UiPluginLoader::unload(UiHostServices& svc, domain::DataContext& dataContext)
{
    (void)svc;
    unload();
}
 
void UiPluginLoader::unload()
{
    if (!m_module || !m_lib)
    {
        printf("[HotReload] unload: skipped (module=%p lib=%p)\n", m_module, m_lib);
        return;
    }

#ifdef _WIN32
    auto lib = (HMODULE)m_lib;
    auto destroyFn = (decltype(&pistachio_destroy_ui_module))GetProcAddress(lib, "pistachio_destroy_ui_module");

    printf("[HotReload] unload: calling onUnload (module=%p)\n", (void*)m_module);
    if (m_lastSvc)
        m_module->onUnload(*m_lastSvc,*m_lastDataContext);
    else
        printf("[HotReload] unload: WARNING - m_lastSvc is null, onUnload skipped!\n");

    printf("[HotReload] unload: calling destroyFn\n");
    if (destroyFn)
        destroyFn(m_module);

    m_module = nullptr;

    printf("[HotReload] unload: FreeLibrary\n");
    FreeLibrary(lib);
    m_lib = nullptr;
    printf("[HotReload] unload: done\n");
    clearManifest();
#endif
}

bool UiPluginLoader::reload(UiHostServices& svc, domain::DataContext& dataContext, const fs::path& filename)
{
    (void)svc;
    return reload(filename);
}

 
bool UiPluginLoader::reload(const fs::path& filename)
{
    if (!m_lastSvc)
    {
        m_lastError = "Reload called before load() bound services";
        return false;
    }
    unload();
    return load(*m_lastSvc,*m_lastDataContext, filename);
}
void UiPluginLoader::render(UiHostServices& svc, domain::DataContext& dataContext)
{
    (void)svc;
    render();
}

void UiPluginLoader::render()
{
    if (m_module && m_lastSvc)
        m_module->render(*m_lastSvc,*m_lastDataContext);
}
