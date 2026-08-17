#include "adapters/ui/plugins/UiModuleApi.h"
#include "adapters/ui/plugins/ToolpathVisualizationPlugin/TestGridGLRender.h"
#include "adapters/ui/plugins/ToolpathVisualizationPlugin/ToolpathRibbonGLRender.h"
#include "adapters/ui/plugins/ToolpathVisualizationPlugin/DebugComparisonGLRender.h"

#include "core/IApplication.h"
#include "ports/IViewportRendererRegistry.h"
#include "core/ServiceRegistry.h"
#include "ports/IEventBus.h"
#include "domain/ToolpathStore.h"
#include "domain/ToolpathBinaryIO.h"

#include <memory>
#include <cstdio>

// -----------------------------------------------------------------------
// ToolpathVisualizationPlugin
//
// IUiModule (not IServiceModule) — needs UI-thread/GL context access,
// Phase 2 of the two-phase loader. render() is deliberately near-empty:
// registers a renderer at onLoad(), does nothing per-frame afterward.
// SlicerCorePlugin's own ViewportController decides when/whether
// TestGridGLRender::render() actually gets called — never automatic.
//
// Test-proof scope only: TestGridGLRender exists purely to confirm
// registration/switching/rendering works end to end. Real ribbon
// rendering of domain::v1::Toolpath replaces it once proven — this
// plugin becomes the real one, not thrown away.
// -----------------------------------------------------------------------

class ToolpathVisualizationPlugin final : public IUiModule
{
public:
    void onLoad(UiHostServices& svc, domain::DataContext& dataContext) override
    {
        printf("[ToolpathVisualization] onLoad\n");

        if (!svc.application)
        {
            printf("[ToolpathVisualization] WARNING: svc.application is null\n");
            return;
        }

        auto* registry = svc.application->services().resolve<ports::IViewportRendererRegistry>();
        if (!registry)
        {
            printf("[ToolpathVisualization] WARNING: IViewportRendererRegistry not resolved\n");
            return;
        }

        m_testGrid = std::make_unique<TestGridGLRender>();
        registry->registerRenderer("toolpath", m_testGrid.get());
        printf("[ToolpathVisualization] registered 'toolpath' renderer\n");


        m_ribbon = std::make_unique<ToolpathRibbonGLRender>();
        registry->registerRenderer("toolpath_ribbon", m_ribbon.get());
        printf("[ToolpathVisualization] registered 'toolpath_ribbon' renderer\n");



        m_debugComparison = std::make_unique<DebugComparisonGLRender>();
        registry->registerRenderer("debug_comparison", m_debugComparison.get());
        printf("[ToolpathVisualization] registered 'debug_comparison' renderer\n");


        m_eventBus = svc.application->services().resolve<ports::IEventBus>();



        m_eventBus->subscribe("toolpath.updated", [this, &svc](const std::string&) {
            auto* store = svc.application->services().resolve<domain::v1::ToolpathStore>();
            if (store && store->hasToolpath())
                m_ribbon->setToolpath(*store->get());
            });
         
        auto* modelCache = svc.application->services().resolve<domain::v1::ModelCache>();

       

         

       
      
        /*auto* toolpathStore = svc.application->services().resolve<domain::v1::ToolpathStore>();
        if (toolpathStore && toolpathStore->hasToolpath())
            m_debugComparison->loadToolpath(*toolpathStore->get());*/
        //m_debugComparison->loadToolpath()->loadToolpath("C:\\temp\\layer_debug\\toolpath.bin");

        domain::v1::Toolpath toolpath;
        if (domain::v1::loadToolpathBinary(toolpath, "C:\\temp\\layer_debug\\toolpath.bin"))
            m_debugComparison->loadToolpath(toolpath);
     

       m_eventBus->subscribe("debug.toggle.model", [this](const std::string& payload) {
            if (m_debugComparison) m_debugComparison->setShowModel(payload == "1");
            });

       m_eventBus->subscribe("debug.toggle.solidshader", [this](const std::string& payload) {
           if (m_debugComparison) m_debugComparison->setUseSolidShader(payload == "1");
           });

       m_eventBus->subscribe("debug.toggle.ribbon", [this](const std::string& payload) {
            if (m_debugComparison) m_debugComparison->setShowRibbon(payload == "1");
            });
    }

    void onUnload(UiHostServices& svc, domain::DataContext& dataContext) override
    {
        printf("[ToolpathVisualization] onUnload\n");

        if (svc.application)
        {
            auto* registry = svc.application->services().resolve<ports::IViewportRendererRegistry>();
            if (registry)
                registry->unregisterRenderer("toolpath");
        }

        m_testGrid.reset();
    }

    void render(UiHostServices& svc, domain::DataContext& dataContext) override
    {
        // Deliberately empty — see header comment.
    }

private:
    std::unique_ptr<TestGridGLRender> m_testGrid;
    std::unique_ptr<ToolpathRibbonGLRender> m_ribbon;
    std::unique_ptr<DebugComparisonGLRender>  m_debugComparison;

    ports::IEventBus* m_eventBus = nullptr;

};

// -----------------------------------------------------------------------
// Required exports
// -----------------------------------------------------------------------
IUiModule* pistachio_create_ui_module()
{
    return new ToolpathVisualizationPlugin();
}

void pistachio_destroy_ui_module(IUiModule* m) { delete m; }

static const UiPluginManifestV1 g_manifest = {
    sizeof(UiPluginManifestV1), 1,
    "pistachio.toolpath_visualization", "Toolpath Visualization", "0.1.0", "Slicer"
};

const UiPluginManifestV1* pistachio_get_ui_manifest() { return &g_manifest; }