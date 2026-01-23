


#include <iostream>
#include <memory>
#include <filesystem>
#include <chrono>

#include "core/Application.h"
#include "ports/IUIPort.h"

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#endif

#include "adapters/ui/ImGuiHost.h"

// UI plugin loader (loads pistachio_ui.dll and calls PistachioUiModule)
#include "adapters/ui/plugins/UiPluginLoader.h"
#include "adapters/ui/plugins/UiModuleApi.h"

#include "adapters/loaders/StepFileLoader.h"
#include "adapters/exporters/ObjExporter.h"
#include "adapters/exporters/StlExporter.h"
#include "adapters/rendering/OcctRenderer.h"
#include "adapters/persistence/JsonSketchDocumentAdapter.h"
#include "adapters/solvers/BasicConstraintSolver.h"
#include "imgui_impl_opengl3.h"
#include "imgui.h"

static void RebuildImGuiFontsTexture()
{
    // After a hot-reload, plugin may have (re)registered embedded fonts/icons.
    // Ensure atlas is built and GPU texture exists BEFORE next NewFrame().
    ImGuiIO& io = ImGui::GetIO();
    if (!io.Fonts->IsBuilt())
        io.Fonts->Build();

    // Force renderer backend to recreate font texture
    ImGui_ImplOpenGL3_DestroyFontsTexture();
    ImGui_ImplOpenGL3_CreateFontsTexture();
}


namespace
{
    // Wrap the stable host (GLFW + ImGui context) and the UI plugin loader into a single IUIPort
    // so core::Application can drive it with beginFrame/render/endFrame.
    class HotReloadUiAdapter final : public ports::IUIPort
    {
    public:
        explicit HotReloadUiAdapter(core::Application* app)
            : m_app(app)
        {}

        bool initialize() override
        {
            if (!m_host.initialize())
                return false;

            // Load UI plugin immediately (so embedded fonts/icons are set up before first NewFrame)
            m_svc.app     = m_app;
            m_svc.window  = m_host.window();
            m_svc.guiHost = static_cast<IGuiHost*>(&m_host);

            // Default expected location (matches your build output)
            // Source DLL is copied to a unique "shadow" DLL so rebuilds don't get file-locked.
            (void)m_loader.load(m_svc);

            // Plugin registers embedded fonts/icons on load; ensure atlas+GPU texture are ready.
            RebuildImGuiFontsTexture();

            m_lastWrite = safeLastWriteTime(m_loader.sourcePath());
            return true;
        }

        void shutdown() override
        {
            m_loader.unload(m_svc);
            m_host.shutdown();
        }

        bool shouldClose() const override { return m_host.shouldClose(); }

        void beginFrame() override
        {
            // If the plugin was rebuilt, auto hot-reload (no manual "pending")
            if (m_enabled)
                maybeAutoReload();

            m_host.beginFrame();
        }

        void render() override
        {
            if (!m_enabled)
                return;

            // Render plugin UI
            m_loader.render(m_svc);
        }

        void endFrame() override
        {
            m_host.endFrame(); 
        // SAFE POINT: enable / disable plugin here (font atlas unlocked)
        if (m_disableRequested)
        {
            m_disableRequested = false;
            m_loader.unload();
        }

        if (m_enableRequested)
        {
            m_enableRequested = false;
            m_loader.load(m_svc);
            RebuildImGuiFontsTexture();
        }
// ImGui::Render() happens inside endFrame(); font atlas unlocked afterwards.

            if (!m_enabled)
                return;

            if (m_reloadPending) {
                // Try reload; if build is still writing/locked, keep pending and retry next frame.
                if (m_loader.reload(m_svc)) {
                    
                    RebuildImGuiFontsTexture();
m_reloadPending = false;
                    // Prefer the observed write time if we have it, otherwise read current.
                    if (m_pendingWrite != std::filesystem::file_time_type{})
                        m_lastWrite = m_pendingWrite;
                    else
                        m_lastWrite = safeLastWriteTime(m_loader.sourcePath());
                    m_pendingWrite = {};
                }
            }
        }

        void setMenubarCallback(const std::function<void()>& menubarCallback) override
        {
            m_host.setMenubarCallback(menubarCallback);
        }

        void setRibbonbarCallback(const std::function<void()>& ribbonbarCallback) override
        {
            m_host.setRibbonbarCallback(ribbonbarCallback);
        }

        // ----- plugin controls -----
        ports::UiPluginStatus getUiPluginStatus() const override
        {
            ports::UiPluginStatus s{};
            s.enabled = m_enabled;
            s.loaded  = m_loader.isLoaded();
            s.lastError  = m_loader.lastError();
            s.sourcePath = m_loader.sourcePath();
            s.loadedPath = m_loader.loadedPath();

            s.id           = m_loader.manifestId();
            s.name         = m_loader.manifestName();
            s.version      = m_loader.manifestVersion();
            s.featureGroup = m_loader.manifestFeatureGroup();
            return s;
        }

        void setUiPluginEnabled(bool enabled) override
        {
            // Update desired state immediately so UI reflects the toggle right away.
            m_enabled = enabled;

            // Defer actual load/unload until endFrame (safe point) to avoid ImFontAtlas lock asserts.
            if (enabled)
            {
                m_disableRequested = false;
                m_enableRequested = true;
            }
            else
            {
                m_enableRequested = false;
                m_disableRequested = true;
            }
        }

        void requestHotReloadUiPlugin() override
        {
            // Defer actual reload to a safe point (after endFrame), so we never touch ImGui font atlas mid-frame.
            m_reloadPending = true;
        }

    private:
        static std::filesystem::file_time_type safeLastWriteTime(const std::string& p)
        {
            try {
                if (!p.empty() && std::filesystem::exists(p))
                    return std::filesystem::last_write_time(p);
            } catch (...) {}
            return {};
        }

        void maybeAutoReload()
        {
            const auto nowWrite = safeLastWriteTime(m_loader.sourcePath());
            if (nowWrite == std::filesystem::file_time_type{})
                return;

            if (m_lastWrite != std::filesystem::file_time_type{} && nowWrite != m_lastWrite) {
                // Plugin DLL changed; defer reload to safe point after endFrame().
                m_reloadPending = true;
                m_pendingWrite = nowWrite;
            }
        }

    private:
        core::Application* m_app = nullptr;

        adapters::ImGuiHost m_host;
        UiPluginLoader m_loader;
        UiHostServices m_svc{};
        bool m_enabled = true;
        // Deferred enable/disable so we never touch ImFontAtlas while it's locked
        bool m_enableRequested = false;
        bool m_disableRequested = false;

        bool m_reloadPending = false;
        std::filesystem::file_time_type m_lastWrite{};
        std::filesystem::file_time_type m_pendingWrite{};
    };
}

int main(int argc, char** argv)
{
    try {
        std::cout << "===========================================\n";
        std::cout << "          Pistachio CAD Converter\n";
        std::cout << "     Hexagonal Architecture Demo\n";
        std::cout << "===========================================\n\n";

        std::cout << "CWD: " << std::filesystem::current_path().string() << std::endl;

        auto app = std::make_unique<core::Application>();

        std::cout << "Configuring adapters...\n";

        // UI: host + UI plugin (embedded Roboto + window buttons live in the plugin)
        app->setUIAdapter(std::make_unique<HotReloadUiAdapter>(app.get()));

        // Renderer / IO / Solver
        app->setRenderer(std::make_unique<adapters::OcctRenderer>());
        app->addFileLoader(std::make_unique<adapters::StepFileLoader>());
        app->addExporter(std::make_unique<adapters::ObjExporter>());
        app->addExporter(std::make_unique<adapters::StlExporter>());
        app->setResolverAdapter(std::make_unique<adapters::solver::BasicConstraintSolver>());

        std::cout << "Initializing application...\n";
        if (!app->initialize()) {
            std::cerr << "Failed to initialize application\n";
            return 1;
        }

        std::cout << "Setup application menus...\n";
        app->setupToolbarMenus();

        std::cout << "Load Sketch Document...\n";
        if (!app->loadSketchDocument("test.pistachio.json")) {
            std::cout << "[WARN] test.pistachio.json not found - creating default sketch document\n";
            app->createDefaultSketchDocument();
        }

        std::cout << "Application started successfully!\n";
        std::cout << "Press ESC or close window to exit\n\n";

        app->runSolver();
        app->run();

        std::cout << "\nShutting down...\n";
        app->shutdown();

        std::cout << "Application closed successfully\n";
        return 0;
    }
    catch (const std::exception& e) {
        std::cerr << "Fatal error: " << e.what() << "\n";
        return 1;
    }
}