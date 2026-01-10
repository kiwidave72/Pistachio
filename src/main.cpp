#include <iostream>
#include <memory>
#include "core/Application.h"
#include "adapters/ui/ImGuiAdapter.h"
#include "adapters/loaders/StepFileLoader.h"
#include "adapters/exporters/ObjExporter.h"
#include "adapters/exporters/StlExporter.h"
#include "adapters/rendering/OcctRenderer.h"
#include "adapters/persistence/JsonSketchDocumentAdapter.h"
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
        app->setUIAdapter(std::make_unique<adapters::ImGuiAdapter>(app.get()));
        app->setRenderer(std::make_unique<adapters::OcctRenderer>());
        app->addFileLoader(std::make_unique<adapters::StepFileLoader>());
        app->addExporter(std::make_unique<adapters::ObjExporter>());
        app->addExporter(std::make_unique<adapters::StlExporter>());



        std::cout << "Initializing application...\n";
        if (!app->initialize()) {
            std::cerr << "Failed to initialize application\n";
            return 1;
        }
       /* adapters::persistence::JsonSketchDocumentAdapter io;
        auto doc = io.loadDocument("../test.pistachio.json");
        io.saveDocument(*doc, "out.pistachio.json");*/


        std::cout << "Application started successfully!\n";
        std::cout << "Press ESC or close window to exit\n\n";
        
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