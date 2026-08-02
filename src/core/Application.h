#pragma once
#include <memory>
#include <vector>
#include <string>
#include <thread>
#include <atomic>
#include <mutex>
#include "domain/DataContext.h"
#include "ports/IUIPort.h"
#include "ports/IFileLoaderPort.h"
#include "ports/IMeshFileLoaderPort.h"
#include "ports/IExporterPort.h"
#include "ports/IRendererPort.h"
#include "ports/ISketchResolverPort.h"
#include "ports/IConfigPort.h"
#include "ports/ISlicerPort.h"
#include "core/TaskRunner.h"
#include "core/IApplication.h"
#include "core/ServiceRegistry.h"
#include "ports/IEventBus.h"
// Host-owned config store

#include "core/ConfigStore.h"

#include "domain/Model.h"
#include "domain/SketchModel.h"

namespace adapters { class EventBus; }
namespace domain::v1 { class WorkspaceStore; }

namespace core {


    class Application : public IApplication {
    public:
        Application();
        ~Application();

        Application(const Application&) = delete;
        Application& operator=(const Application&) = delete;
        Application(Application&&) = delete;
        Application& operator=(Application&&) = delete;

        void setResolverAdapter(std::unique_ptr<ports::ISketchResolverPort> resolver);
        void setSlicerAdapter(std::unique_ptr<ports::ISlicerPort> slicer);

        void setUIAdapter(std::unique_ptr<ports::IUIPort> uiAdapter);
        void setRenderer(std::unique_ptr<ports::IRendererPort> renderer);
        void addFileLoader(std::unique_ptr<ports::IFileLoaderPort> loader);
        void addExporter(std::unique_ptr<ports::IExporterPort> exporter);
        //void registerToolheadSchema(int index);

        bool initialize();
        void run();
        void shutdown();

        void setupToolbarMenus();
        void dropToolbarMenus();

        void setRibbonbarCallback(const std::function<void()>& ribbonbarCallback);
        bool loadFile(const std::string& filepath);
        bool loadFileAsync(const std::string& filepath);
        bool exportFile(const std::string& filepath, const std::string& format);


		domain::DataContext* getDataContext() { 
              return &m_dataContext; }

        std::shared_ptr<domain::Model> getCurrentModel() const;
        ports::IRendererPort* getRenderer() const;
        ports::ISlicerPort* getSlicer() ;

        // Host-owned configuration registry (persists across UI hot-reloads)
        ports::IConfigPort* getConfig() { return &m_config; }
        const ports::IConfigPort* getConfig() const { return &m_config; }

        // Optional helper for plugins: persist configuration immediately.
        void saveConfigNow();

        std::string getStatus() const;
        bool isLoading() const;
        float getLoadingProgress() const;

        std::unique_ptr<TaskRunner> taskRunner = std::make_unique<TaskRunner>();
        
        ServiceRegistry& services() override { return m_services; }
        void registerCoreServices();

        std::unique_ptr<adapters::EventBus> m_eventBus;
        std::unique_ptr<domain::v1::WorkspaceStore> m_workspaceStore;

    private:

		domain::DataContext m_dataContext ; // non-owning, valid for app lifetime
        
        std::unique_ptr<domain::v1::ModelCache> m_modelCache;

        std::unique_ptr<ports::IUIPort> m_uiAdapter;
        std::unique_ptr<ports::IRendererPort> m_renderer;
        std::vector<std::unique_ptr<ports::IFileLoaderPort>> m_loaders;
        std::vector<std::unique_ptr<ports::IExporterPort>> m_exporters;
        std::shared_ptr<domain::Model> m_currentModel;
        std::unique_ptr<ports::ISketchResolverPort> m_resolverAdapter;
        std::unique_ptr<ports::ISlicerPort> m_slicerAdapter;


        core::ConfigStore m_config;
        std::string m_configPath = "pistachio.config.json";


        ServiceRegistry m_services;
       

        std::string m_statusMessage;
        std::atomic<bool> m_isLoading;
        std::atomic<float> m_loadingProgress;
        mutable std::mutex m_statusMutex;
        std::thread m_loadingThread;


        void loadFileThreaded(const std::string& filepath);
        void meshLoadFileThreaded(const std::string& filepath);

        void initializeConfig();
        void initializeConfigToolhead(int index);
        void initializeConfigFilament(int index,std::string name);
        
        ports::IFileLoaderPort* findLoaderForFile(const std::string& filepath);
        ports::IExporterPort* findExporterForFormat(const std::string& format);

    public:
        void updateStatus(const std::string& message);

        bool loadSketchDocument(const std::string& filepath);
        bool saveSketchDocument(const std::string& filepath);
        // Creates an in-memory document with a single sketch and a few entities.
        // Useful when no file is present so the 2D sketch UI always has something to show.
        void createDefaultSketchDocument();
        bool runSolver();
        std::shared_ptr<domain::sketch::Document> getSketchDocument() const;

    private:
        std::shared_ptr<domain::sketch::Document> m_sketchDoc;
         

    };

} // namespace core