#include "adapters/ui/TaskProgressReporterImGui.h"
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
#include <combaseapi.h>
#endif


#include "domain/DataContext.h"
#include "adapters/ui/ImGuiHost.h"
#include "adapters/ui/plugins/UiPluginRegistry.h"
#include "adapters/ui/plugins/UiPluginLoader.h"
#include "adapters/ui/plugins/UiModuleApi.h"
#include "adapters/plugins/ServiceModuleRegistry.h"


#include "adapters/loaders/StlMeshLoader.h"
#ifdef USE_OPENCASCADE
#include "adapters/loaders/StepFileLoader.h"
#endif
#include "adapters/exporters/ObjExporter.h"
#include "adapters/exporters/StlExporter.h"


// Default renderer for the 3D viewport.
// OCCT is still available, but this OpenGL renderer guarantees we see a 3D scene
// even when OCCT isn't wired up yet.
#include "adapters/rendering/GlCubeViewRenderer.h"
#ifdef USE_OPENCASCADE
#include "adapters/rendering/OcctRenderer.h"
#endif
#include "adapters/persistence/JsonSketchDocumentAdapter.h"
#include "adapters/solvers/BasicConstraintSolver.h"

#include "adapters/slicers/PlanarSlicer.h"




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
        {
        }

        bool initialize() override
        {
            if (!m_host.initialize())
                return false;
            m_svc.application = m_app;


            m_svc.app = m_app;
            m_svc.window = m_host.window();
            m_svc.guiHost = static_cast<IGuiHost*>(&m_host);
            m_svc.config = (m_app ? (void*)m_app->getConfig() : nullptr);

            m_svc.taskRunner = m_app->taskRunner.get();

            m_svc.registry = &m_host.contributionRegistry();  // ← wire registry


            m_dataContext = m_app->getDataContext();


            if (m_app)
                m_app->registerCoreServices();

            if (m_app)
                m_app->attachTaskProgressReporter(
                    std::make_unique<adapters::ui::TaskProgressReporterImGui>(*m_app->taskRunner));


            int servicesLoaded = m_serviceRegistry.loadAll(*m_app);
            printf("[HotReload] initialize: %d service plugin(s) loaded\n", servicesLoaded);

            int loaded = m_registry.loadAll(m_svc, *m_dataContext);
            printf("[HotReload] initialize: %d plugin(s) loaded\n", loaded);

            RebuildImGuiFontsTexture();
            return true;
        }

        void shutdown() override
        {
            m_registry.unloadAll(m_svc, *m_dataContext);
            m_serviceRegistry.unloadAll(*m_app);
            m_host.shutdown();
        }


        bool shouldClose() const override { return m_host.shouldClose(); }

        //void beginFrame() override
        //{
        //    // If the plugin was rebuilt, auto hot-reload (no manual "pending")
        //    if (m_enabled)
        //        maybeAutoReload();

        //    m_host.beginFrame();
        //}
        //void beginFrame() override
        //{
        //    printf("[HotReload] beginFrame: enter (reloadPending=%d enabled=%d loaded=%d)\n",
        //        m_reloadPending, m_enabled, m_loader.isLoaded());

        //    if (m_enabled && m_reloadPending)
        //    {
        //        printf("[HotReload] beginFrame: attempting reload\n");

        //        // Clear atlas before reload so onLoad/initializeResources starts clean.
        //        // Do NOT build yet — let onLoad register all fonts first.
        //        /*ImGuiIO& io = ImGui::GetIO();
        //        io.Fonts->Clear();
        //        io.FontDefault = nullptr;*/

        //        if (m_loader.reload(m_svc))
        //        {
        //            ImGuiIO& io = ImGui::GetIO();
        //            io.Fonts->Build();
        //            RebuildImGuiFontsTexture();

        //            // onLoad has registered all fonts — now build and upload
        //            printf("[HotReload] beginFrame: reload succeeded, building atlas\n");
        //           /* io.Fonts->Build();
        //            io.FontDefault = io.Fonts->Fonts.Size > 0 ? io.Fonts->Fonts[0] : nullptr;

        //            printf("[HotReload] beginFrame: FontsCount=%d\n", io.Fonts->Fonts.Size);
        //            for (int i = 0; i < io.Fonts->Fonts.Size; i++)
        //                printf("[HotReload]   font[%d] Scale=%.2f Loaded=%d\n",
        //                    i, io.Fonts->Fonts[i]->Scale, io.Fonts->Fonts[i]->IsLoaded());

        //            RebuildImGuiFontsTexture();*/
        //            m_reloadPending = false;

        //            if (m_pendingWrite != std::filesystem::file_time_type{})
        //                m_lastWrite = m_pendingWrite;
        //            else
        //                m_lastWrite = safeLastWriteTime(m_loader.sourcePath());
        //            m_pendingWrite = {};

        //            printf("[HotReload] beginFrame: done, module=%s\n",
        //                m_loader.isLoaded() ? "loaded" : "NOT loaded");
        //        }
        //        else
        //        {
        //            // Reload failed (linker still writing) — restore fallback font so
        //            // NewFrame() doesn't AV on a null/stale atlas.
        //            printf("[HotReload] beginFrame: reload failed (will retry next frame) - %s\n",
        //                m_loader.lastError().c_str());
        //            /*io.Fonts->AddFontDefault();
        //            io.Fonts->Build();
        //            io.FontDefault = io.Fonts->Fonts[0];
        //            RebuildImGuiFontsTexture();*/
        //        }

        //        m_host.beginFrame();
        //        return;
        //    }

        //    if (m_enabled)
        //        maybeAutoReload();

        //    m_host.beginFrame();
        //}
        /*void beginFrame() override
        {
            printf("[HotReload] beginFrame: enter (reloadPending=%d enabled=%d loaded=%d)\n",
                m_reloadPending, m_enabled, m_loader.isLoaded());

            if (m_enabled && m_reloadPending)
            {
                printf("[HotReload] beginFrame: attempting reload\n");

                if (m_loader.reload(m_svc))
                {
                    printf("[HotReload] beginFrame: reload succeeded\n");

                    ImGuiIO& io = ImGui::GetIO();
                    io.Fonts->TexReady = false;
                    io.Fonts->Build();
                    RebuildImGuiFontsTexture();

                    m_reloadPending = false;

                    if (m_pendingWrite != std::filesystem::file_time_type{})
                        m_lastWrite = m_pendingWrite;
                    else
                        m_lastWrite = safeLastWriteTime(m_loader.sourcePath());
                    m_pendingWrite = {};

                    printf("[HotReload] beginFrame: done, module=%s\n",
                        m_loader.isLoaded() ? "loaded" : "NOT loaded");
                }
                else
                {
                    printf("[HotReload] beginFrame: reload failed (will retry next frame) - %s\n",
                        m_loader.lastError().c_str());
                }

                m_host.beginFrame();
                return;
            }

            if (m_enabled)
                maybeAutoReload();

            m_host.beginFrame();
        }*/

        void beginFrame() override
        {
            // Reload first — acts on pending flags set LAST frame
            if (m_enabled && (m_reloadPending || m_registry.hasReloadPending()))
            {
                printf("[HotReload] beginFrame: reloading\n");
                m_registry.reloadAll(m_svc, *m_dataContext);
                m_reloadPending = false;
                m_registry.clearReloadPending();
                printf("[HotReload] beginFrame: reload done\n");
            }

            // THEN detect changes — sets pending for NEXT frame
            if (m_enabled)
                m_registry.tickAutoReload(m_svc);

            m_host.beginFrame();
        }

        void endFrame() override
        {
            m_host.endFrame();

            if (m_disableRequested)
            {
                printf("[HotReload] endFrame: disabling plugins\n");
                m_disableRequested = false;
                m_registry.unloadAll(m_svc, *m_dataContext);
                m_enabled = false;
            }

            if (m_enableRequested)
            {
                printf("[HotReload] endFrame: enabling plugins\n");
                m_enableRequested = false;
                m_registry.loadAll(m_svc, *m_dataContext);
                m_enabled = true;
            }
        }
        void render() override
        {
            if (!m_enabled) return;
            m_registry.renderAll(m_svc, *m_dataContext);
        }

        //        void endFrame() override
        //        {
        //            m_host.endFrame(); 
        //        // SAFE POINT: enable / disable plugin here (font atlas unlocked)
        //        if (m_disableRequested)
        //        {
        //            m_disableRequested = false;
        //            m_loader.unload();
        //        }
        //
        //        if (m_enableRequested)
        //        {
        //            m_enableRequested = false;
        //            m_loader.load(m_svc);
        //            RebuildImGuiFontsTexture();
        //        }
        //// ImGui::Render() happens inside endFrame(); font atlas unlocked afterwards.
        //
        //            if (!m_enabled)
        //                return;
        //
        //            if (m_reloadPending) {
        //                // Try reload; if build is still writing/locked, keep pending and retry next frame.
        //                if (m_loader.reload(m_svc)) {
        //                    
        //                    RebuildImGuiFontsTexture();
        //m_reloadPending = false;
        //                    // Prefer the observed write time if we have it, otherwise read current.
        //                    if (m_pendingWrite != std::filesystem::file_time_type{})
        //                        m_lastWrite = m_pendingWrite;
        //                    else
        //                        m_lastWrite = safeLastWriteTime(m_loader.sourcePath());
        //                    m_pendingWrite = {};
        //                }
        //            }
        //        }

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
            //s.enabled = m_enabled;
            ////s.loaded  = m_loader.isLoaded();

            //s.lastError  = m_loader.lastError();
            //s.sourcePath = m_loader.sourcePath();
            //s.loadedPath = m_loader.loadedPath();

            //s.id           = m_loader.manifestId();
            //s.name         = m_loader.manifestName();
            //s.version      = m_loader.manifestVersion();
            //s.featureGroup = m_loader.manifestFeatureGroup();
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
            m_registry.requestReloadAll();
            m_reloadPending = true;
        }

    private:
        static std::filesystem::file_time_type safeLastWriteTime(const std::string& p)
        {
            try {
                if (!p.empty() && std::filesystem::exists(p))
                    return std::filesystem::last_write_time(p);
            }
            catch (...) {}
            return {};
        }

        void maybeAutoReload()
        {
            //const auto nowWrite = safeLastWriteTime(m_loader.sourcePath());

            //if (nowWrite == std::filesystem::file_time_type{})
            //    return;

            //if (nowWrite == m_lastWrite)
            //{
            //    // File hasn't changed, clear any pending settle timer
            //    m_settleStart = std::chrono::steady_clock::time_point{};
            //    return;
            //}

            //// File has changed — start or check the settle timer
            //if (m_settleStart == std::chrono::steady_clock::time_point{})
            //{
            //    // First detection of this change — record when we saw it
            //    m_settleStart = std::chrono::steady_clock::now();
            //    m_pendingWrite = nowWrite;
            //    printf("[HotReload] maybeAutoReload: DLL changed, waiting for write to settle...\n");
            //    return;
            //}

            //if (nowWrite != m_pendingWrite)
            //{
            //    // Write time changed again — linker still writing, reset timer
            //    printf("[HotReload] maybeAutoReload: DLL still changing, resetting settle timer\n");
            //    m_settleStart = std::chrono::steady_clock::now();
            //    m_pendingWrite = nowWrite;
            //    return;
            //}

            //// Same change detected — check if enough time has passed
            //const auto elapsed = std::chrono::steady_clock::now() - m_settleStart;
            //if (elapsed >= std::chrono::milliseconds(8000))
            //{
            //    printf("[HotReload] maybeAutoReload: DLL settled, triggering reload\n");
            //    m_reloadPending = true;
            //    m_settleStart = {};
            //}
        }

    private:
        core::Application* m_app = nullptr;

        adapters::ImGuiHost m_host;

        UiPluginRegistry m_registry;
        ServiceModuleRegistry m_serviceRegistry;

        UiHostServices m_svc{};
        domain::DataContext* m_dataContext;
        bool m_enabled = true;
        // Deferred enable/disable so we never touch ImFontAtlas while it's locked
        bool m_enableRequested = false;
        bool m_disableRequested = false;

        bool m_reloadPending = false;
        std::filesystem::file_time_type m_lastWrite{};
        std::filesystem::file_time_type m_pendingWrite{};
        std::chrono::steady_clock::time_point m_settleStart{};
    };
}

int main(int argc, char** argv)
{
#ifdef _WIN32
    // Required before any COM usage, including the native IFileOpenDialog
    // used by core::showOpenFileDialog(). STA (COINIT_APARTMENTTHREADED) is
    // required for that API specifically. This must live on the UI thread
    // for the app's lifetime, so it's done once here rather than per-call.
    // Without this, CoCreateInstance(CLSID_FileOpenDialog, ...) fails with
    // CO_E_NOTINITIALIZED (0x800401F0) regardless of which thread calls it.
    HRESULT comHr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    bool comInitializedHere = SUCCEEDED(comHr);
    if (comHr == RPC_E_CHANGED_MODE) {
        std::cerr << "[main] Main thread COM already initialized as MTA; "
            "native file dialogs will be unavailable.\n";
    }
#endif

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
        // For now we default to a lightweight OpenGL demo renderer so the 3D viewport
        // always shows a scene (grid + cube) even before OCCT is fully wired up.
       // app->setRenderer(std::make_unique<adapters::GlCubeViewRenderer>());
        //app->setRenderer(std::make_unique<adapters::OcctRenderer>());

#ifdef USE_OPENCASCADE
        app->addFileLoader(std::make_unique<adapters::StepFileLoader>());
#endif
        app->addFileLoader(std::make_unique<adapters::StlMeshLoader>());
        app->addExporter(std::make_unique<adapters::ObjExporter>());
        app->addExporter(std::make_unique<adapters::StlExporter>());
        app->setResolverAdapter(std::make_unique<adapters::solver::BasicConstraintSolver>());

        app->setSlicerAdapter(std::make_unique<adapters::PlanarSlicer>());


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
#ifdef _WIN32
        if (comInitializedHere) CoUninitialize();
#endif
        return 0;
    }
    catch (const std::exception& e) {
        std::cerr << "Fatal error: " << e.what() << "\n";
#ifdef _WIN32
        if (comInitializedHere) CoUninitialize();
#endif
        return 1;
    }
}