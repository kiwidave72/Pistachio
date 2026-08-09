
#include "core/Application.h"
#include "adapters/eventbus/EventBus.h"
#include "domain/WorkspaceStore.h"
#include <algorithm>
#include <variant>
#include "adapters/persistence/JsonSketchDocumentAdapter.h"
#include <iostream>
#include <sstream>
#include <imgui.h>
#include "imgui_internal.h"
#include <cmath>
#include <ctime>

namespace core {

    Application::Application()
        : m_statusMessage("Ready"),
        m_isLoading(false),
        m_loadingProgress(0.0f){
    }
    Application::~Application() {
        if (m_loadingThread.joinable()) {
            m_loadingThread.join();
        }
    }
    
    void Application::registerCoreServices() {
        m_services.registerService<ports::IConfigPort>(getConfig());
        m_services.registerService<TaskRunner>(taskRunner.get());

        m_eventBus = std::make_unique<adapters::EventBus>();
        m_services.registerService<ports::IEventBus>(m_eventBus.get());

        m_modelCache = std::make_unique<domain::v1::ModelCache>();
        m_services.registerService<domain::v1::ModelCache>(m_modelCache.get());

        m_workspaceStore = std::make_unique<domain::v1::WorkspaceStore>(m_eventBus.get());
        m_services.registerService<domain::v1::WorkspaceStore>(m_workspaceStore.get());

        m_viewportRendererRegistry = std::make_unique<core::ViewportRendererRegistry>();
        m_services.registerService<ports::IViewportRendererRegistry>(m_viewportRendererRegistry.get());

 
        m_toolpathStore = std::make_unique<domain::v1::ToolpathStore>(m_eventBus.get());
        m_services.registerService<domain::v1::ToolpathStore>(m_toolpathStore.get());
    }
    void Application::attachTaskProgressReporter(std::unique_ptr<ports::ITaskProgressReporter> reporter)
    {
        m_taskProgressReporter = std::move(reporter);
        m_services.registerService<ports::ITaskProgressReporter>(m_taskProgressReporter.get());
    }

    void Application::setSlicerAdapter(std::unique_ptr<ports::ISlicerPort> slicer) {
        m_slicerAdapter = std::move(slicer);
    }

    void Application::setResolverAdapter(std::unique_ptr<ports::ISketchResolverPort> resolver) {
        m_resolverAdapter = std::move(resolver);
    }

    void Application::setUIAdapter(std::unique_ptr<ports::IUIPort> uiAdapter) {
        m_uiAdapter = std::move(uiAdapter);
    }

    void Application::setRenderer(std::unique_ptr<ports::IRendererPort> renderer) {
        m_renderer = std::move(renderer);
    }

    void Application::addFileLoader(std::unique_ptr<ports::IFileLoaderPort> loader) {
        m_loaders.push_back(std::move(loader));
    }
 
    void Application::addExporter(std::unique_ptr<ports::IExporterPort> exporter) {
        m_exporters.push_back(std::move(exporter));
    }

     

    bool Application::initialize() {
        std::cout << "\n=== APPLICATION INITIALIZATION ===" << std::endl;

        initializeConfig();
       
        // ------------------------------------------------------------
        // 1) Slicer  MAY NEED TO MOVE
        // ------------------------------------------------------------

        if (m_slicerAdapter && !m_slicerAdapter->initialize(m_config)) {
            m_statusMessage = "Error: Failed to initialize slicer";
            std::cout << "[X] Slicer initialization failed" << std::endl;
            return false;
        }

        // ------------------------------------------------------------
        // 2) Renderer (creates window + GL context)
        // ------------------------------------------------------------
        if (m_renderer && !m_renderer->initialize()) {
            m_statusMessage = "Error: Failed to initialize renderer";
            std::cout << "[X] Renderer initialization failed" << std::endl;
            return false;
        }

        if (m_renderer) {
            std::cout << "[OK] Renderer initialized" << std::endl;
            auto backend = m_renderer->getBackend();
            std::cout << "  Backend: " <<
                (backend == ports::RendererBackend::Occt ? "OCCT" : "OpenGL")
                << std::endl;
        }

        // ------------------------------------------------------------
        // 3) UI adapter AFTER window/context exists
        // ------------------------------------------------------------
        if (!m_uiAdapter) {
            m_statusMessage = "Error: No UI adapter set";
            std::cout << "[X] No UI adapter" << std::endl;
            return false;
        }
        std::cout << "[OK] UI adapter set" << std::endl;

        if (!m_uiAdapter->initialize()) {
            m_statusMessage = "Error: Failed to initialize UI";
            std::cout << "[X] UI initialization failed" << std::endl;
            return false;
        }
        std::cout << "[OK] UI initialized" << std::endl;

        // Ensure there's always a sketch document so the 2D sketch UI has something to show.
        if (!m_sketchDoc) {
            createDefaultSketchDocument();
        }

        m_statusMessage = "Application initialized";
        std::cout << "=================================\n" << std::endl;
        return true;
    }

    void Application::createDefaultSketchDocument() {
        auto doc = std::make_shared<domain::sketch::Document>();
        doc->id = 1;
        doc->name = "Untitled";

        domain::sketch::Sketch sk;
        sk.id = 1;
        sk.name = "Sketch 1";
        sk.visible = true;

        // A small starter sketch so you can see the 2D viewport immediately.
        // NOTE: In this data model, entities store geometry directly (Vec2), not references to other entities.
        {
            const domain::sketch::EntityId p1Id = sk.nextEntityId++;
            domain::sketch::Point2D p1;
            p1.h.id = p1Id;
            p1.h.name = "P1";
            p1.p = { 100.0, 100.0 };
            sk.entities.addPoint(p1);

            const domain::sketch::EntityId p2Id = sk.nextEntityId++;
            domain::sketch::Point2D p2;
            p2.h.id = p2Id;
            p2.h.name = "P2";
            p2.p = { 250.0, 180.0 };
            sk.entities.addPoint(p2);

            const domain::sketch::EntityId l1Id = sk.nextEntityId++;
            domain::sketch::Line2D ln;
            ln.h.id = l1Id;
            ln.h.name = "L1";
            ln.a = p1.p;
            ln.b = p2.p;
            sk.entities.addLine(ln);

            const domain::sketch::EntityId c1Id = sk.nextEntityId++;
            domain::sketch::Circle2D c;
            c.h.id = c1Id;
            c.h.name = "C1";
            c.center = { 200.0, 140.0 };
            c.radius = 60.0;
            sk.entities.addCircle(c);

            // Add one example constraint (Fix point P1) to exercise the solver path.
            domain::sketch::GeometricConstraint fix;
            fix.meta.id = sk.nextConstraintId++;
            fix.meta.name = "Fix P1";
            fix.meta.enabled = true;
            fix.type = domain::sketch::GeometricConstraintType::Fix;
            fix.refs.push_back(domain::sketch::EntityRef{ p1Id, domain::sketch::EntityAnchor::Point });
            sk.constraints.push_back(std::move(fix));
        }

        doc->sketches.push_back(std::move(sk));
        m_sketchDoc = std::move(doc);

        updateStatus("Created default sketch document");
    }

    void Application::run() {
        if (!m_uiAdapter) return;


        //// Trigger hot reload 3 seconds after startup for testing
        //std::thread([this]() {
        //    std::this_thread::sleep_for(std::chrono::seconds(5));
        //    printf("[Application] auto-triggering hot reload\n");
        //     m_uiAdapter->requestHotReloadUiPlugin();
        //    }).detach();

      

        std::cout << "Starting main loop...\n" << std::endl;



        while (!m_uiAdapter->shouldClose()) {
            m_uiAdapter->beginFrame();

            // UI adapter will call renderer in DrawViewport()
            m_uiAdapter->render();
            
            

            m_uiAdapter->endFrame();
        }


    }

    void Application::shutdown() {
        if (m_loadingThread.joinable()) {
            m_loadingThread.join();
        }

        if (m_renderer) {
            m_renderer->shutdown();
        }

        if (m_uiAdapter) {
            m_uiAdapter->shutdown();
        }

        // Best-effort persist user config values.
        (void)m_config.saveToFile(m_configPath);

        m_statusMessage = "Application shut down";
    }

    void Application::updateStatus(const std::string& message) {
        std::lock_guard<std::mutex> lock(m_statusMutex);
        m_statusMessage = message;
    }

    void Application::setRibbonbarCallback(const std::function<void()>& ribbonbarCallback)
    {
        if (m_uiAdapter) {

            printf("[application] setRibbonbarCallback: calling m_uiAdapter.setRibbonbarCallback.\n");
            printf("[application] setRibbonbarCallback: m_uiAdapter=%p\n", (void*)m_uiAdapter.get());

           
            m_uiAdapter-> setRibbonbarCallback(ribbonbarCallback);
            printf("[application] setRibbonbarCallback: calling m_uiAdapter.setRibbonbarCallback.Done.\n");
        }
        else {
            printf("[application] setRibbonbarCallback: FAILED .\n");

        }
    }

    void Application::saveConfigNow()
    {
        (void)m_config.saveToFile(m_configPath);
    }

    bool Application::loadFile(const std::string& filepath) {
        return loadFileAsync(filepath);
    }



    bool Application::loadFileAsync(const std::string& filepath) {
        if (m_isLoading) {
            updateStatus("Error: Already loading a file");
            return false;
        }

        // Join previous thread if it exists
        if (m_loadingThread.joinable()) {
            m_loadingThread.join();
        }

        // Start loading in background thread
        m_loadingThread = std::thread(&Application::loadFileThreaded, this, filepath);

         

        return true;
    }

    void Application::loadFileThreaded(const std::string& filepath) {
        m_isLoading = true;
        m_loadingProgress = 0.0f;

        auto loader = findLoaderForFile(filepath);

        if (!loader) {
            updateStatus("Error: No loader found for file: " + filepath);
            m_isLoading = false;
            return;
        }
             

        try {
            updateStatus("Loading file...");

            // Progress callback
            auto progressCallback = [this](const std::string& message, float progress) {
                m_loadingProgress = progress;
                updateStatus(message);
                };

            auto model = loader->load(filepath, progressCallback);

            if (model && !model->isEmpty()) {
                m_currentModel = model;

                if (m_renderer) {
                    m_renderer->setModel(m_currentModel);
                    m_renderer->fitAll();
                }

                updateStatus("Loaded: " + filepath);
            }
            else {
                updateStatus("Error: Failed to load model");
            }

        }
        catch (const std::exception& e) {
            updateStatus("Error loading file: " + std::string(e.what()));
        }

        m_loadingProgress = 100.0f;
        m_isLoading = false;
    }
    

    bool Application::exportFile(const std::string& filepath, const std::string& format) {
        if (!m_currentModel || m_currentModel->isEmpty()) {
            m_statusMessage = "Error: No model to export";
            return false;
        }

        auto exporter = findExporterForFormat(format);
        if (!exporter) {
            m_statusMessage = "Error: No exporter found for format: " + format;
            return false;
        }

        try {
            if (exporter->exportModel(*m_currentModel, filepath)) {
                m_statusMessage = "Exported: " + filepath;
                return true;
            }
        }
        catch (const std::exception& e) {
            m_statusMessage = "Error exporting file: " + std::string(e.what());
        }

        return false;
    }

    std::shared_ptr<domain::Model> Application::getCurrentModel() const {
        return m_currentModel;
    }

    ports::IRendererPort* Application::getRenderer() const {
        return m_renderer.get();
    }
    ports::ISlicerPort* Application::getSlicer()  {
        return m_slicerAdapter.get();
    }
    std::string Application::getStatus() const {
        std::lock_guard<std::mutex> lock(m_statusMutex);
        return m_statusMessage;
    }

    bool Application::isLoading() const {
        return m_isLoading;
    }

    float Application::getLoadingProgress() const {
        return m_loadingProgress;
    }

    ports::IFileLoaderPort* Application::findLoaderForFile(const std::string& filepath) {
        for (auto& loader : m_loaders) {
            if (loader->canLoad(filepath)) {
                return loader.get();
            }
        }
        return nullptr;
    }

    ports::IExporterPort* Application::findExporterForFormat(const std::string& format) {
        for (auto& exporter : m_exporters) {
            if (exporter->getSupportedExtension() == format) {
                return exporter.get();
            }
        }
        return nullptr;
    }
    void Application::dropToolbarMenus() {
        m_uiAdapter->setMenubarCallback([]() {});

    }
    void Application::setupToolbarMenus() {
        //m_uiAdapter->setMenubarCallback([this]()
        //    {

                //if (ImGui::BeginMenu("File"))
                //{
                //    if (ImGui::MenuItem("Open")) {}
                //    ImGui::Separator();
                //    if (ImGui::MenuItem("Save", "Ctrl+S")) {
                //        // Save to the current file (test.pistachio.json for now)
                //        if (saveSketchDocument("test.pistachio.json")) {
                //            updateStatus("Sketch saved successfully");
                //        }
                //    }
                //    if (ImGui::MenuItem("Save as ...")) {
                //        // TODO: Show file dialog to choose save location
                //        // For now, save to a timestamped file
                //        auto now = std::time(nullptr);
                //        char filename[256];
                //        std::strftime(filename, sizeof(filename), "sketch_%Y%m%d_%H%M%S.pistachio.json", std::localtime(&now));
                //        if (saveSketchDocument(filename)) {
                //            updateStatus(std::string("Sketch saved as: ") + filename);
                //        }
                //    }
                //    ImGui::Separator();
                //    if (ImGui::MenuItem("Import Sketch")) {}
                //    ImGui::Separator();
                //    if (ImGui::MenuItem("Exit"))
                //    {
                //        this->shutdown();
                //    }
                //    ImGui::EndMenu();
                //}
                //if (ImGui::BeginMenu("Sketch"))
                //{
                //    ImGui::EndMenu();
                //}
                //if (ImGui::BeginMenu("Options"))
                //{
                //    ImGui::EndMenu();
                //}
                //if (ImGui::BeginMenu("Tools"))
                //{
                //    // NOTE: you probably want tools here
                //    ImGui::EndMenu();
                //}
                //if (ImGui::BeginMenu("Views"))
                //{
                //    auto getBool = [&](const char* ns, const char* key, bool defVal) {
                //        nlohmann::json v = m_config.get(ns, key);
                //        return v.is_boolean() ? v.get<bool>() : defVal;
                //        };

                //    auto toggle = [&](const char* key, const char* label) {
                //        bool open = getBool("pistachio.UI", key, true);
                //        if (ImGui::MenuItem(label, nullptr, open))
                //            m_config.set("pistachio.UI", key, !open);
                //        };

                //    toggle("views.fileOperations", "File Operations");
                //    toggle("views.slicerOperations", "Slicer Operations");
                //    toggle("views.status", "Status");
                //    toggle("views.modelInfo", "Model Info");
                //    toggle("views.viewport3d", "3D Viewport");
                //    toggle("views.sketchEditor", "Sketch Editor");

                //    ImGui::Separator();
                //    if (ImGui::MenuItem("Settings"))
                //        m_config.set("pistachio.UI", "config.windowOpen", true);

                //        ImGui::EndMenu();
                //}

                //if (ImGui::BeginMenu("Help"))
                //{
                //    if (ImGui::MenuItem("About"))
                //    {
                //        //exampleLayer->ShowAboutModal();
                //    }
                //    ImGui::EndMenu();
                //}

                /*    if (ImGui::BeginMenu("Plugins"))
                    {
                        auto st = m_uiAdapter->getUiPluginStatus();
                        bool enabled = st.enabled;
                        if (ImGui::MenuItem("Enabled", nullptr, &enabled))
                        {
                            m_uiAdapter->setUiPluginEnabled(enabled);
                        }
                        if (ImGui::MenuItem("Hot Reload", "Ctrl+R"))
                        {
                            m_uiAdapter->requestHotReloadUiPlugin();
                        }
                        ImGui::Separator();
                        ImGui::TextDisabled("ID: %s", st.id.c_str());
                        ImGui::TextDisabled("Name: %s", st.name.c_str());
                        ImGui::TextDisabled("Version: %s", st.version.c_str());
                        ImGui::TextDisabled("Group: %s", st.featureGroup.c_str());
                        if (!st.lastError.empty())
                            ImGui::TextColored(ImVec4(1,0.4f,0.4f,1), "Error: %s", st.lastError.c_str());
                        ImGui::EndMenu();
                    }*/


          //  });
    }

    bool Application::runSolver() {
        std::cout << "\n=== RUNING BASIC RESOLVER ===" << std::endl;

        ports::ResolvedSketch output = m_resolverAdapter->solve(getSketchDocument()->sketches[0]);

        if (output.report.converged) {
            std::cout << "[OK] Sketch Resolved successfully" << std::endl;

            output.sketch.name = "Updated with resolver";

            m_sketchDoc->sketches[0] = output.sketch;
            m_sketchDoc->name = "Updated with resolver";

        }
        else {
            std::cout << "[X] Resolver failed" << std::endl;
        }

        std::cout << "  Iterations: " << output.report.iterations << std::endl;
        return true;
    }

    bool Application::loadSketchDocument(const std::string& filepath)
    {
        std::cout << "\n=== LOADING SKETCH DOCUMENT ===" << std::endl;
        std::cout << "File: " << filepath << std::endl;

        adapters::persistence::JsonSketchDocumentAdapter io;
        auto loaded = io.loadDocument(filepath);

        if (loaded) {
            m_sketchDoc = loaded;
            std::cout << "[OK] Sketch document loaded successfully" << std::endl;
            std::cout << "  Sketches in document: " << m_sketchDoc->sketches.size() << std::endl;

            if (!m_sketchDoc->sketches.empty()) {
                auto& sketch = m_sketchDoc->sketches[0];
                std::cout << "  First sketch entities:" << std::endl;
                std::cout << "    Points: " << sketch.entities.points().size() << std::endl;
                std::cout << "    Lines: " << sketch.entities.lines().size() << std::endl;
                std::cout << "    Circles: " << sketch.entities.circles().size() << std::endl;
                std::cout << "    Arcs: " << sketch.entities.arcs().size() << std::endl;
                std::cout << "    Ellipses: " << sketch.entities.ellipses().size() << std::endl;
                std::cout << "    Curves: " << sketch.entities.curves().size() << std::endl;
            }

            // Ensure per-sketch ID generators are set after loading.
            for (auto& sk : m_sketchDoc->sketches) {
                domain::sketch::EntityId maxEnt = 0;
                for (const auto& p : sk.entities.points())   maxEnt = std::max(maxEnt, p.h.id);
                for (const auto& l : sk.entities.lines())    maxEnt = std::max(maxEnt, l.h.id);
                for (const auto& c : sk.entities.circles())  maxEnt = std::max(maxEnt, c.h.id);
                for (const auto& a : sk.entities.arcs())     maxEnt = std::max(maxEnt, a.h.id);
                for (const auto& e : sk.entities.ellipses()) maxEnt = std::max(maxEnt, e.h.id);
                for (const auto& cu : sk.entities.curves())  maxEnt = std::max(maxEnt, cu.h.id);
                sk.nextEntityId = maxEnt + 1;

                domain::sketch::ConstraintId maxC = 0;
                for (const auto& cst : sk.constraints) {
                    std::visit([&](auto&& c) { maxC = std::max(maxC, c.meta.id); }, cst);
                }
                sk.nextConstraintId = maxC + 1;
            }
            updateStatus("Loaded sketch: " + filepath);
        }
        else {
            std::cout << "[X] Failed to load sketch document" << std::endl;
            updateStatus("Failed to load sketch: " + filepath + " (using current document)");
        }
        std::cout << "===============================\n" << std::endl;

        return (loaded != nullptr);
    }

    bool Application::saveSketchDocument(const std::string& filepath)
    {
        std::cout << "\n=== SAVING SKETCH DOCUMENT ===" << std::endl;
        std::cout << "File: " << filepath << std::endl;

        if (!m_sketchDoc) {
            std::cout << "[X] No sketch document to save" << std::endl;
            updateStatus("Error: No sketch document loaded");
            return false;
        }

        try {
            adapters::persistence::JsonSketchDocumentAdapter io;
            io.saveDocument(*m_sketchDoc, filepath);
            
            std::cout << "[OK] Sketch document saved successfully" << std::endl;
            std::cout << "  Sketches in document: " << m_sketchDoc->sketches.size() << std::endl;
            
            if (!m_sketchDoc->sketches.empty()) {
                auto& sketch = m_sketchDoc->sketches[0];
                std::cout << "  First sketch entities:" << std::endl;
                std::cout << "    Points: " << sketch.entities.points().size() << std::endl;
                std::cout << "    Lines: " << sketch.entities.lines().size() << std::endl;
                std::cout << "    Circles: " << sketch.entities.circles().size() << std::endl;
                std::cout << "    Arcs: " << sketch.entities.arcs().size() << std::endl;
                std::cout << "    Constraints: " << sketch.constraints.size() << std::endl;
            }
            
            updateStatus("Sketch document saved: " + filepath);
            std::cout << "===============================\n" << std::endl;
            return true;
        }
        catch (const std::exception& e) {
            std::cout << "[X] Failed to save sketch document: " << e.what() << std::endl;
            updateStatus("Error saving sketch: " + std::string(e.what()));
            std::cout << "===============================\n" << std::endl;
            return false;
        }
    }

    std::shared_ptr<domain::sketch::Document> Application::getSketchDocument() const
    {
        return m_sketchDoc;
    }

    void Application::initializeConfig() {
        // Register a few host-level settings.
        // These persist even if the UI plugin hot-reloads.
        using ports::SettingInfo;
        using ports::SettingType;
        using ports::NamespaceInfo;

        m_config.registerNamespace(NamespaceInfo("pistachio.UI.theme.selections", "pistachio", "", "", 0));
       
        m_config.registerSetting(SettingInfo(
            "pistachio.UI.theme.selections.0", "name", "name", "UI theme name", "UI", SettingType::String, "DARK", "", false
        ));
        m_config.registerSetting(SettingInfo(
            "pistachio.UI.theme.selections.1", "name", "name", "UI theme name", "UI", SettingType::String, "WHITE", "", false
        ));

        m_config.registerSetting(SettingInfo(
            "pistachio.UI", "theme", "Theme", "UI theme name", "UI", SettingType::Enum, "DARK", "pistachio.UI.theme.selections", false
        ));





      

        m_config.registerSetting(SettingInfo(
            "pistachio.Sketch", "grid.spacing", "Grid spacing", "Grid spacing in sketch units", "Sketch", SettingType::Float, 10.0, "", false
        ));
        m_config.registerSetting(SettingInfo(
            "pistachio.Sketch", "snap.enabled", "Snap", "Enable snapping in the sketch canvas", "Sketch", SettingType::Bool, true, "", false
        ));

        m_config.registerSetting(SettingInfo(
            "pistachio.Render", "msaa.samples", "MSAA samples", "Multisample AA samples (restart may be required)", "Rendering", SettingType::Int, 4,"", true
        ));

        /* m_config.registerSetting(SettingInfo(
             "plugin.Render", "plugin.msaa.samples", "MSAA samples", "Multisample AA samples (restart may be required)", "Render", SettingType::Int, 4, true
         ));*/





        m_config.registerNamespace(NamespaceInfo("slicer.Settings", "slicer", "Slicer Settings", "", 0));

        m_config.registerSetting(SettingInfo(
            "slicer.Settings", "bed.size.X", "Bed Size X", "Printable Bed Size in X", "Printable Bed Size", SettingType::Int, 250,"", false
        ));
        m_config.registerSetting(SettingInfo(
            "slicer.Settings", "bed.size.Y", "Bed Size Y", "Printable Bed Size in Y", "Printable Bed Size", SettingType::Int, 250, "",false
        ));

        m_config.registerSetting(SettingInfo(
            "slicer.Settings", "bed.temp", "Bed Temp C", "Bed Temp in C", "", SettingType::Int, 250, "",false
        ));


        m_config.registerSetting(SettingInfo(
            "slicer.Settings", "slicer.firstLayerHeight", "First Layer Height", "", SettingType::Float, 0.2, "",false
        ));
        m_config.registerSetting(SettingInfo(
            "slicer.Settings", "slicer.layerHeight", "Layer Height", "", SettingType::Float, 0.2, "", false
        ));
        

        m_config.registerSetting(SettingInfo(
            "slicer.Settings", "slicer.wallCount", "Wall Count", "Number of perimeter loops", "Walls", SettingType::Int, 2, "", false
        ));
        m_config.registerSetting(SettingInfo(
            "slicer.Settings", "slicer.topLayerCount", "Top Layers", "Solid layers before top surface", "Walls", SettingType::Int, 4, "", false
        ));
        m_config.registerSetting(SettingInfo(
            "slicer.Settings", "slicer.bottomLayerCount", "Bottom Layers", "Solid layers before infill begins", "Walls", SettingType::Int, 4, "", false
        ));
        m_config.registerSetting(SettingInfo(
            "slicer.Settings", "slicer.maxSpeed", "Max Speed", "Global speed ceiling (mm/s) — may be overridden by printer/kinematics profile later", "Speed", SettingType::Float, 100.0, "", false
        ));
        m_config.registerSetting(SettingInfo(
            "slicer.Settings", "slicer.infill", "Infill", "Which registered infill strategy is currently in use", "Infill", SettingType::String, "rectilinear", "", false
        ));

        
        m_config.registerSetting(SettingInfo(
            "slicer.infill.rectilinear.settings", "density", "Infill Density", "Percentage fill (0-100)", "Infill", SettingType::Float, 20.0, "", false
        ));
        m_config.registerSetting(SettingInfo(
            "slicer.infill.rectilinear.settings", "angle", "Infill Angle", "Degrees, alternates 90 per layer for cross-hatching", "Infill", SettingType::Float, 45.0, "", false
        ));

        initializeConfigFilament(0,"ABS");
        initializeConfigFilament(1, "PLA");

        initializeConfigToolhead(0);
        initializeConfigToolhead(1);


        
        std::vector<ports::SettingInfo> allSettings = m_config.listSettings();

        std::cout << "=== CONFIGURATION INITIALIZATION ===" << std::endl;

        std::cout << "=== Settings ===" << std::endl;

        for (const auto& s : allSettings)
        {
            std::cout << s.ns << "->" << s.key << std::endl;
        }
        
        std::cout << "=== Namespaces ===" << std::endl;
        std::vector<ports::NamespaceInfo> allNamespaces = m_config.listNamespaces();
        for (const auto& s : allNamespaces)
        {
            std::cout << s.ns << "->" << s.parentNs << "->" << s.displayName << std::endl;
        }

        std::cout << "=== CONFIGURATION END ===" << std::endl;

        // Load persisted config (values only). Settings metadata is registered
        // by the host and by plugins at runtime.
        (void)m_config.loadFromFile(m_configPath);
        std::cout << "=== CONFIGURATION FILE LOADED ===" << std::endl;
    }

    void Application::initializeConfigFilament(int index,std::string name )  {
        using ports::SettingInfo;
        using ports::SettingType;
        using ports::NamespaceInfo;
         
       

        std::string ns = "filament.Settings." + std::to_string(index);
         
        m_config.registerNamespace(NamespaceInfo(ns , "filament.Settings", "Filament " + std::to_string(index), "", 0));
        m_config.registerNamespace(NamespaceInfo(ns +".generalSettings", ns, "General Settings"));

        m_config.registerSetting(SettingInfo(
            ns + ".generalSettings", "name", "Filament Name", "Generic ABS,PLA,ect", "", SettingType::String, name, "", false
        ));

        m_config.registerSetting(SettingInfo(
            ns + ".generalSettings", "colour", "Filament Colour", "", "", SettingType::Colour,"#FF6600FF", "", false
        ));

        m_config.registerSetting(SettingInfo(
            ns + ".generalSettings", "temp", "Filament Temp", "", "", SettingType::Int, 230, "",false
        ));

       
    }
    void Application::initializeConfigToolhead(int index) {
        
        using ports::SettingInfo;
        using ports::SettingType;
        using ports::NamespaceInfo;

        std::string ns = "slicer.toolheads." + std::to_string(index);

        m_config.registerNamespace(NamespaceInfo(ns, "slicer.toolheads", "Toolhead " + std::to_string(index), "toolicon", 0));
        m_config.registerNamespace(NamespaceInfo(ns + ".generalSettings", ns, "General Settings"));

        m_config.registerSetting(SettingInfo(
            ns +".generalSettings", "nozzle.size", "Nozzle Size", "Nozzle Size(0.2cm, 0.4cm, 0.5cm) - Description", "Tool Head "+ std::to_string(index) +" Nozzle", SettingType::Float, 0.2, "",false
        ));
        m_config.registerSetting(SettingInfo(
            ns+".generalSettings", "hotEndMaxTemp", "HotEnd Max Temp C", "HotEnd Max Temp C - Description", "Tool Head " + std::to_string(index) + " HotEnd", SettingType::Int, 250, "", false
        ));
        m_config.registerSetting(SettingInfo(
            ns + ".generalSettings", "hotEndFanSpeed", "HotEnd Fan Speed", "HotEnd Fan Speed - Description", "Tool Head " + std::to_string(index) + " HotEnd", SettingType::Int, 250, "", false
        ));
        m_config.registerSetting(SettingInfo(
            ns + ".generalSettings", "hotEndFanSpeedLayers", "HotEnd Fan Speed Override Layers", "HotEnd Fan Speed for layers - Description", "Tool Head " + std::to_string(index) + " HotEnd Fan Speed Overrides", SettingType::String, "0,1,2,3", "", true
        ));
        m_config.registerSetting(SettingInfo(
            ns + ".generalSettings", "hotEndFanSpeedLayersSpeed", "HotEnd Fan Speed Override", "HotEnd Fan Speed for layers - Description", "Tool Head " + std::to_string(index) + " HotEnd Fan Speed Overrides", SettingType::Int, 80, "", true
        ));

        m_config.registerSetting(SettingInfo(
            ns + ".generalSettings", "toolHead.ExtrusionMaxSpeed", "Extrusion Max Speed", "Extrusion Max Speed in mm3 sec - Description", "Tool Head " + std::to_string(index) + " Flow", SettingType::Int, 250, "", true
        ));


        m_config.registerSetting(SettingInfo(
            ns + ".generalSettings", "extrusionWidth.percent", "Extrusion Width", "% of nozzle diameter (100% = nozzle.size) - Description", "Tool Head " + std::to_string(index) + " Extrusion", SettingType::Float, 112.0, "", false
        ));
        m_config.registerSetting(SettingInfo(
            ns + ".generalSettings", "speeds.perimeter.percent", "Perimeter Speed", "% of slicer.maxSpeed - Description", "Tool Head " + std::to_string(index) + " Speed", SettingType::Float, 60.0, "", true
        ));
        m_config.registerSetting(SettingInfo(
            ns + ".generalSettings", "speeds.infill.percent", "Infill Speed", "% of slicer.maxSpeed - Description", "Tool Head " + std::to_string(index) + " Speed", SettingType::Float, 100.0, "", true
        ));
        m_config.registerSetting(SettingInfo(
            ns + ".generalSettings", "speeds.travel.percent", "Travel Speed", "% of slicer.maxSpeed - Description", "Tool Head " + std::to_string(index) + " Speed", SettingType::Float, 100.0, "", true
        ));
        
        m_config.registerSetting(SettingInfo(
            ns + ".generalSettings", "filament", "Filament", "Printing Filament",  SettingType::Enum, "ABS", "filament.Settings", false
        ));

       
    }

} // namespace core