#include "core/Application.h"
#include <algorithm>

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
        if (!m_uiAdapter) {
            m_statusMessage = "Error: No UI adapter set";
            return false;
        }

        if (!m_uiAdapter->initialize()) {
            m_statusMessage = "Error: Failed to initialize UI";
            return false;
        }

        if (m_renderer && !m_renderer->initialize()) {
            m_statusMessage = "Error: Failed to initialize renderer";
            return false;
        }

        m_statusMessage = "Application initialized";
        return true;
    }

    void Application::run() {
        if (!m_uiAdapter) return;

        while (!m_uiAdapter->shouldClose()) {
            m_uiAdapter->beginFrame();

            if (m_renderer) {
                m_renderer->render();
            }

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

} // namespace core