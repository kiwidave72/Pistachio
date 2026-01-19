
#include "adapters/ui/plugins/UiPluginLoader.h"

#ifdef _WIN32
#include <windows.h>
#endif
#include <filesystem>
#include <chrono>
#include <iostream>

namespace fs = std::filesystem;

#ifdef _WIN32
static std::filesystem::path getExeDir()
{
    char buf[MAX_PATH] = {};
    DWORD n = GetModuleFileNameA(nullptr, buf, MAX_PATH);
    if (n == 0 || n >= MAX_PATH)
        return {};
    std::filesystem::path p(buf);
    return p.parent_path();
}
#endif

static std::string nowStamp() {
    using namespace std::chrono;
    auto ms = duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();
    return std::to_string(ms);
}

bool UiPluginLoader::shadowCopyFile(const std::string& sourceDllPath, std::string& outLoadedPath) {
    try {
        fs::path src(sourceDllPath);
        if (!fs::exists(src)) return false;

        fs::path loadedDir = src.parent_path() / "_loaded";
        fs::create_directories(loadedDir);

#ifdef _WIN32
        DWORD pid = GetCurrentProcessId();
#else
        int pid = 0;
#endif

        fs::path dst = loadedDir / (src.stem().string() + "_" + nowStamp() + "_" + std::to_string((int)pid) + src.extension().string());
        fs::copy_file(src, dst, fs::copy_options::overwrite_existing);

        fs::path srcPdb = src; srcPdb.replace_extension(".pdb");
        if (fs::exists(srcPdb)) {
            fs::path dstPdb = dst; dstPdb.replace_extension(".pdb");
            fs::copy_file(srcPdb, dstPdb, fs::copy_options::overwrite_existing);
        }

        outLoadedPath = dst.string();
        return true;
    } catch (...) {
        return false;
    }
}

bool UiPluginLoader::load(const std::string& sourceDllPath, UiHostServices& svc) {
#ifdef _WIN32
    auto dir = GetPluginDir();
    std::cout << "[Plugin] Searching in: " << dir.string() << "\n";

    fs::path resolved;

    for (auto& e : std::filesystem::directory_iterator(dir))
    {
        std::cout << "  found: " << e.path().filename().string() << "\n";

        if (!e.is_regular_file()) { std::cout << "    skip: not a file\n"; continue; }
        if (e.path().extension() != ".dll") {
            std::cout << "    skip: not dll\n";
            continue;
        }
        else
        {
            resolved = e.path();
            break;
        }
     }
    
    unload(svc);

    if (resolved.empty()) {
        std::cout << "[Plugin] [ERROR] No .dll found in plugins directory." << std::endl;
        return false;
    }


   
    // Resolve plugin path reliably:
    // - Visual Studio's working directory is often the project folder, not the EXE folder.
    // - If the user passes a relative path, resolve it against the executable directory.
    //fs::path resolved = fs::path(sourceDllPath);
    //if (!resolved.is_absolute()) {
    //    fs::path exeDir = getExeDir();
    //    if (!exeDir.empty()) {
    //        fs::path candidate = exeDir / resolved;
    //        if (fs::exists(candidate))
    //            resolved = candidate;
    //        else {
    //            // Common convention: plugins live next to the EXE in a "plugins" folder.
    //            fs::path candidate2 = exeDir / "plugins" / resolved.filename();
    //            if (fs::exists(candidate2))
    //                resolved = candidate2;
    //            else
    //                resolved = fs::absolute(resolved);
    //        }
    //    } else {
    //        resolved = fs::absolute(resolved);
    //    }
    //}

    m_sourcePath = resolved.string();
    m_loadedPath.clear();

    std::cout << "[Plugin] Using source: " << m_sourcePath << std::endl;

    std::string shadowPath;
    if (!shadowCopyFile(m_sourcePath, shadowPath)) {
        std::cout << "[Plugin] [ERROR] Failed to shadow-copy: " << m_sourcePath << std::endl;
        return false;
    }
    m_loadedPath = shadowPath;

    std::cout << "[Plugin] Shadow copy: " << m_loadedPath << std::endl;

    m_lib = (void*)LoadLibraryA(m_loadedPath.c_str());
    if (!m_lib) {
        DWORD err = GetLastError();
        std::cout << "[Plugin] [ERROR] LoadLibraryA failed. GetLastError=" << err << std::endl;
        return false;
    }

    auto createFn = (IUiModule*(*)())GetProcAddress((HMODULE)m_lib, "pistachio_create_ui_module");
    if (!createFn) {
        DWORD err = GetLastError();
        std::cout << "[Plugin] [ERROR] GetProcAddress(pistachio_create_ui_module) failed. GetLastError=" << err << std::endl;
        FreeLibrary((HMODULE)m_lib);
        m_lib = nullptr;
        return false;
    }

    m_module = createFn();
    if (!m_module) {
        std::cout << "[Plugin] [ERROR] createFn() returned null module" << std::endl;
        FreeLibrary((HMODULE)m_lib);
        m_lib = nullptr;
        return false;
    }

    std::cout << "[Plugin] Module created: " << m_module << std::endl;

    m_module->onLoad(svc);
    std::cout << "[Plugin] onLoad() completed" << std::endl;
    return true;
#else
    (void)sourceDllPath; (void)svc;
    return false;
#endif
}

bool UiPluginLoader::reload(UiHostServices& svc) {
    if (m_sourcePath.empty()) return false;
    return load(m_sourcePath, svc);
}

void UiPluginLoader::unload(UiHostServices& svc) {
#ifdef _WIN32
    if (m_module) {
        m_module->onUnload(svc);

        auto destroyFn = (void(*)(IUiModule*))GetProcAddress((HMODULE)m_lib, "pistachio_destroy_ui_module");
        if (destroyFn) destroyFn(m_module);
        m_module = nullptr;
    }
    if (m_lib) {
        FreeLibrary((HMODULE)m_lib);
        m_lib = nullptr;
    }
#else
    (void)svc;
#endif
}

void UiPluginLoader::render(UiHostServices& svc) {
    if (m_module) m_module->render(svc);
}
