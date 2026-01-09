#pragma once
#include "ports/IRendererPort.h"
#include <memory>

// OpenCASCADE includes - need full definitions in header for handle types
#include <Standard_Handle.hxx>
#include <V3d_View.hxx>
#include <V3d_Viewer.hxx>
#include <AIS_InteractiveContext.hxx>
#include <AIS_Shape.hxx>
#include <AIS_ViewCube.hxx>
#include <AIS_Trihedron.hxx>

namespace adapters {

    class OcctRenderer : public ports::IRendererPort {
    public:
        OcctRenderer();
        ~OcctRenderer() override;

        bool initialize() override;
        void shutdown() override;
        void render() override;
        void setModel(std::shared_ptr<domain::Model> model) override;
        void fitAll() override;
        void* getFramebufferTexture() override;
        void resize(int width, int height) override;

        // Camera controls
        void rotate(float dx, float dy);
        void pan(float dx, float dy);
        void zoom(float delta);
        void setViewDirection(int direction);

    private:
        std::shared_ptr<domain::Model> m_currentModel;

        opencascade::handle<V3d_Viewer> m_viewer;
        opencascade::handle<V3d_View> m_view;
        opencascade::handle<AIS_InteractiveContext> m_context;
        opencascade::handle<AIS_Shape> m_currentShape;
        opencascade::handle<AIS_ViewCube> m_viewCube;
        opencascade::handle<AIS_Trihedron> m_trihedron;

        int m_width;
        int m_height;
        bool m_initialized;

        void createExampleCube();
        void setupViewCube();
    };

} // namespace adapters