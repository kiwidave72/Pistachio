#pragma once
#include <memory>
#include <vector>
#include <string>
#include <thread>
#include <atomic>
#include <mutex>
#include "ports/IUIPort.h"
#include "ports/IFileLoaderPort.h"
#include "ports/IExporterPort.h"
#include "ports/IRendererPort.h"
#include "domain/Model.h"

namespace core {

    class Application {
    public:
        Application();
        ~Application();

        Application(const Application&) = delete;
        Application& operator=(const Application&) = delete;
        Application(Application&&) = delete;
        Application& operator=(Application&&) = delete;


        void setUIAdapter(std::unique_ptr<ports::IUIPort> uiAdapter);
        void setRenderer(std::unique_ptr<ports::IRendererPort> renderer);
        void addFileLoader(std::unique_ptr<ports::IFileLoaderPort> loader);
        void addExporter(std::unique_ptr<ports::IExporterPort> exporter);

        bool initialize();
        void run();
        void shutdown();

        bool loadFile(const std::string& filepath);
        bool loadFileAsync(const std::string& filepath);
        bool exportFile(const std::string& filepath, const std::string& format);
        std::shared_ptr<domain::Model> getCurrentModel() const;
        ports::IRendererPort* getRenderer() const;

        std::string getStatus() const;
        bool isLoading() const;
        float getLoadingProgress() const;

    private:
        std::unique_ptr<ports::IUIPort> m_uiAdapter;
        std::unique_ptr<ports::IRendererPort> m_renderer;
        std::vector<std::unique_ptr<ports::IFileLoaderPort>> m_loaders;
        std::vector<std::unique_ptr<ports::IExporterPort>> m_exporters;
        std::shared_ptr<domain::Model> m_currentModel;

        std::string m_statusMessage;
        std::atomic<bool> m_isLoading;
        std::atomic<float> m_loadingProgress;
        mutable std::mutex m_statusMutex;
        std::thread m_loadingThread;

        void updateStatus(const std::string& message);
        void loadFileThreaded(const std::string& filepath);

        ports::IFileLoaderPort* findLoaderForFile(const std::string& filepath);
        ports::IExporterPort* findExporterForFormat(const std::string& format);
    };

} // namespace core