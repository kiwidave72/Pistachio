#include <iostream>
#include <memory>
#include "core/Application.h"
#include "adapters/ui/ImGuiHost.h"
#include "adapters/ui/plugins/UiModuleApi.h"
#include "adapters/ui/plugins/UiPluginLoader.h"
#include "ports/IUIPort.h"
#include "adapters/loaders/StepFileLoader.h"
#include "adapters/exporters/ObjExporter.h"
#include "adapters/exporters/StlExporter.h"
#include "adapters/rendering/OcctRenderer.h"
#include "adapters/persistence/JsonSketchDocumentAdapter.h"
#include "adapters/solvers/BasicConstraintSolver.h"
#include <filesystem>
#include <iostream>

int main(int argc, char** argv) {
    try {
        std::cout << "===========================================\n";
        std::cout << "          Pistachio CAD Converter\n";
        std::cout << "     Hexagonal Architecture Demo\n";
        std::cout << "===========================================\n\n";
        

        std::cout << "CWD: " << std::filesystem::current_path().string() << std::endl;

        auto app = std::make_unique<core::Application>();
        
        // Configure adapters following hexagonal architecture
        std::cout << "Configuring adapters...\n";

        // Host UI: owns GLFW window + ImGui context/backends.
        // Plugin UI: contains ImGuiAdapter/UI code and can be hot-reloaded.
        class HotReloadUiAdapter final : public ports::IUIPort {
        public:
            explicit HotReloadUiAdapter(core::Application* app, std::string pluginPath = "plugins/pistachio_ui.dll")
                : m_app(app), m_pluginPath(std::move(pluginPath)) {
                m_svc.app = app;
                m_svc.window = nullptr;
                m_svc.guiHost = nullptr;
            }

            bool initialize() override {
                std::cout << "[HotReloadUiAdapter] initialize()" << std::endl;
                if (!m_host.initialize()) {
                    std::cout << "[HotReloadUiAdapter] [ERROR] ImGuiHost.initialize() failed" << std::endl;
                    return false;
                }
                m_svc.window = m_host.window();
                m_svc.guiHost = static_cast<IGuiHost*>(&m_host);
                std::cout << "[HotReloadUiAdapter] GLFW window from host: " << m_svc.window << std::endl;

                std::cout << "[HotReloadUiAdapter] Loading plugin (requested): " << m_pluginPath << std::endl;
                m_loaded = m_loader.load(m_pluginPath, m_svc);
                std::cout << "[HotReloadUiAdapter] Plugin loaded: " << (m_loaded ? "YES" : "NO")
                          << " (source='" << m_loader.sourcePath() << "' loaded='" << m_loader.loadedPath() << "')" << std::endl;
                return m_loaded;
            }

            void shutdown() override {
                if (m_loaded) {
                    m_loader.unload(m_svc);
                    m_loaded = false;
                }
                m_host.shutdown();
            }

            bool shouldClose() override { return m_host.shouldClose(); }
            void beginFrame() override { m_host.beginFrame(); }

            void render() override {
                static bool s_warned = false;
                if (m_loaded) {
                    m_loader.render(m_svc);
                } else {
                    if (!s_warned) {
                        s_warned = true;
                        std::cout << "[HotReloadUiAdapter] [WARN] render() called but plugin is not loaded" << std::endl;
                    }
                }
            }

            void endFrame() override { m_host.endFrame(); }

            void setMenubarCallback(const std::function<void()>& menubarCallback) override {
                m_host.setMenubarCallback(menubarCallback);
            }

            bool reload() {
                if (!m_loaded) return false;
                return m_loader.reload(m_svc);
            }

        private:
            core::Application* m_app = nullptr;
            std::string m_pluginPath;
            adapters::ImGuiHost m_host;
            UiPluginLoader m_loader;
            UiHostServices m_svc{};
            bool m_loaded = false;
        };

        app->setUIAdapter(std::make_unique<HotReloadUiAdapter>(app.get()));
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

        
        //adapters::persistence::JsonSketchDocumentAdapter io;
        //auto doc = io.loadDocument("../test.pistachio.json");
        //io.saveDocument(*doc, "out.pistachio.json");*/


        std::cout << "Application started successfully!\n";
        std::cout << "Press ESC or close window to exit\n\n";
        
        app->runSolver();
        
        
        app->run();
        
        std::cout << "\nShutting down...\n";
        app->shutdown();
        
        std::cout << "Application closed successfully\n";
        return 0;
        
    } catch (const std::exception& e) {
        std::cerr << "Fatal error: " << e.what() << "\n";
        return 1;
    }
}