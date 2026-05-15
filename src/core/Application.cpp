#include "core/Application.h"
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
        m_loadingProgress(0.0f) {

        // Register a few host-level settings.
        // These persist even if the UI plugin hot-reloads.
        using ports::SettingInfo;
        using ports::SettingType;

        m_config.registerSetting(SettingInfo(
            "pistachio.UI", "theme", "Theme", "UI theme name", "UI", SettingType::String, "Pistachio", false
        ));

        m_config.registerSetting(SettingInfo(
            "pistachio.Sketch", "grid.spacing", "Grid spacing", "Grid spacing in sketch units", "Sketch", SettingType::Float, 10.0, false
        ));
        m_config.registerSetting(SettingInfo(
            "pistachio.Sketch", "snap.enabled", "Snap", "Enable snapping in the sketch canvas", "Sketch", SettingType::Bool, true, false
        ));

        m_config.registerSetting(SettingInfo(
            "pistachio.Render", "msaa.samples", "MSAA samples", "Multisample AA samples (restart may be required)", "Rendering", SettingType::Int, 4, true
        ));
    }

    Application::~Application() {
        if (m_loadingThread.joinable()) {
            m_loadingThread.join();
        }
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

        // Load persisted config (values only). Settings metadata is registered
        // by the host and by plugins at runtime.
        (void)m_config.loadFromFile(m_configPath);

        // ------------------------------------------------------------
        // 1) Renderer FIRST (creates window + GL context)
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
        // 2) UI adapter AFTER window/context exists
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
        if (m_uiAdapter)
            m_uiAdapter->setRibbonbarCallback(ribbonbarCallback);
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

    void Application::setupToolbarMenus() {
        m_uiAdapter->setMenubarCallback([this]()
            {

                if (ImGui::BeginMenu("File"))
                {
                    if (ImGui::MenuItem("Open")) {}
                    ImGui::Separator();
                    if (ImGui::MenuItem("Save", "Ctrl+S")) {
                        // Save to the current file (test.pistachio.json for now)
                        if (saveSketchDocument("test.pistachio.json")) {
                            updateStatus("Sketch saved successfully");
                        }
                    }
                    if (ImGui::MenuItem("Save as ...")) {
                        // TODO: Show file dialog to choose save location
                        // For now, save to a timestamped file
                        auto now = std::time(nullptr);
                        char filename[256];
                        std::strftime(filename, sizeof(filename), "sketch_%Y%m%d_%H%M%S.pistachio.json", std::localtime(&now));
                        if (saveSketchDocument(filename)) {
                            updateStatus(std::string("Sketch saved as: ") + filename);
                        }
                    }
                    ImGui::Separator();
                    if (ImGui::MenuItem("Import Sketch")) {}
                    ImGui::Separator();
                    if (ImGui::MenuItem("Exit"))
                    {
                        this->shutdown();
                    }
                    ImGui::EndMenu();
                }
                if (ImGui::BeginMenu("Sketch"))
                {
                    ImGui::EndMenu();
                }
                if (ImGui::BeginMenu("Options"))
                {
                    ImGui::EndMenu();
                }
                if (ImGui::BeginMenu("Tools"))
                {
                    // NOTE: you probably want tools here
                    ImGui::EndMenu();
                }
                if (ImGui::BeginMenu("Views"))
                {
                    auto getBool = [&](const char* ns, const char* key, bool defVal) {
                        nlohmann::json v = m_config.get(ns, key);
                        return v.is_boolean() ? v.get<bool>() : defVal;
                        };

                    auto toggle = [&](const char* key, const char* label) {
                        bool open = getBool("pistachio.UI", key, true);
                        if (ImGui::MenuItem(label, nullptr, open))
                            m_config.set("pistachio.UI", key, !open);
                        };

                    toggle("views.fileOperations", "File Operations");
                    toggle("views.status", "Status");
                    toggle("views.modelInfo", "Model Info");
                    toggle("views.viewport3d", "3D Viewport");
                    toggle("views.sketchEditor", "Sketch Editor");

                    ImGui::Separator();
                    if (ImGui::MenuItem("Settings"))
                        m_config.set("pistachio.UI", "config.windowOpen", true);

                        ImGui::EndMenu();
                    }

                    if (ImGui::BeginMenu("Help"))
                    {
                        if (ImGui::MenuItem("About"))
                        {
                            //exampleLayer->ShowAboutModal();
                        }
                        ImGui::EndMenu();
                    }

                    if (ImGui::BeginMenu("Plugins"))
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
                    }


            });
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

} // namespace core