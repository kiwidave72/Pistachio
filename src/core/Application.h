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
#include "ports/ISketchResolverPort.h"
#include "ports/IConfigPort.h"

// Host-owned config store
#include "core/ConfigStore.h"
#include "domain/Model.h"
#include "domain/SketchModel.h"

namespace core {

    class Application {
    public:
        Application();
        ~Application();

        Application(const Application&) = delete;
        Application& operator=(const Application&) = delete;
        Application(Application&&) = delete;
        Application& operator=(Application&&) = delete;

        void setResolverAdapter(std::unique_ptr<ports::ISketchResolverPort> resolver);
        void setUIAdapter(std::unique_ptr<ports::IUIPort> uiAdapter);
        void setRenderer(std::unique_ptr<ports::IRendererPort> renderer);
        void addFileLoader(std::unique_ptr<ports::IFileLoaderPort> loader);
        void addExporter(std::unique_ptr<ports::IExporterPort> exporter);

        bool initialize();
        void run();
        void shutdown();

        void setupToolbarMenus();
        void setRibbonbarCallback(const std::function<void()>& ribbonbarCallback);
        bool loadFile(const std::string& filepath);
        bool loadFileAsync(const std::string& filepath);
        bool exportFile(const std::string& filepath, const std::string& format);
        std::shared_ptr<domain::Model> getCurrentModel() const;
        ports::IRendererPort* getRenderer() const;

        // Host-owned configuration registry (persists across UI hot-reloads)
        ports::IConfigPort* getConfig() { return &m_config; }
        const ports::IConfigPort* getConfig() const { return &m_config; }

        // Optional helper for plugins: persist configuration immediately.
        void saveConfigNow();

        std::string getStatus() const;
        bool isLoading() const;
        float getLoadingProgress() const;

    private:
        std::unique_ptr<ports::IUIPort> m_uiAdapter;
        std::unique_ptr<ports::IRendererPort> m_renderer;
        std::vector<std::unique_ptr<ports::IFileLoaderPort>> m_loaders;
        std::vector<std::unique_ptr<ports::IExporterPort>> m_exporters;
        std::shared_ptr<domain::Model> m_currentModel;
        std::unique_ptr<ports::ISketchResolverPort> m_resolverAdapter;

        core::ConfigStore m_config;
        std::string m_configPath = "pistachio.config.json";

        std::string m_statusMessage;
        std::atomic<bool> m_isLoading;
        std::atomic<float> m_loadingProgress;
        mutable std::mutex m_statusMutex;
        std::thread m_loadingThread;

        void updateStatus(const std::string& message);
        void loadFileThreaded(const std::string& filepath);
        
        ports::IFileLoaderPort* findLoaderForFile(const std::string& filepath);
        ports::IExporterPort* findExporterForFormat(const std::string& format);

    public:
        bool loadSketchDocument(const std::string& filepath);
        // Creates an in-memory document with a single sketch and a few entities.
        // Useful when no file is present so the 2D sketch UI always has something to show.
        void createDefaultSketchDocument();
        bool runSolver();
        std::shared_ptr<domain::sketch::Document> getSketchDocument() const;

    private:
        std::shared_ptr<domain::sketch::Document> m_sketchDoc;
         

    };

} // namespace core