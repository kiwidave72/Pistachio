#include "core/Application.h"
#include <algorithm>
#include "adapters/persistence/JsonSketchDocumentAdapter.h"
#include <iostream>

namespace core {

    Application::Application()
        : m_statusMessage("Ready"),
        m_isLoading(false),
        m_loadingProgress(0.0f) {
    }

    Application::~Application() {
        if (m_loadingThread.joinable()) {
            m_loadingThread.join();
        }
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

        m_statusMessage = "Application initialized";
        std::cout << "=================================\n" << std::endl;
        return true;
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

        m_statusMessage = "Application shut down";
    }

    void Application::updateStatus(const std::string& message) {
        std::lock_guard<std::mutex> lock(m_statusMutex);
        m_statusMessage = message;
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

    bool Application::loadSketchDocument(const std::string& filepath)
    {
        std::cout << "\n=== LOADING SKETCH DOCUMENT ===" << std::endl;
        std::cout << "File: " << filepath << std::endl;

        adapters::persistence::JsonSketchDocumentAdapter io;
        m_sketchDoc = io.loadDocument(filepath);

        if (m_sketchDoc) {
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
        }
        else {
            std::cout << "[X] Failed to load sketch document" << std::endl;
        }
        std::cout << "===============================\n" << std::endl;

        updateStatus("Loaded sketch: " + filepath);
        return (m_sketchDoc != nullptr);
    }

    std::shared_ptr<domain::sketch::Document> Application::getSketchDocument() const
    {
        return m_sketchDoc;
    }

} // namespace core