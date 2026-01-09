#include "adapters/rendering/OcctRenderer.h"
#include "domain/Model.h"

// OpenCASCADE includes
#include <OpenGl_GraphicDriver.hxx>
#include <Aspect_DisplayConnection.hxx>
#include <Geom_Axis2Placement.hxx>
#include <BRepPrimAPI_MakeBox.hxx>
#include <TopoDS_Shape.hxx>
#include <Quantity_Color.hxx>
#include <Graphic3d_Camera.hxx>
#include <gp_Pnt.hxx>
#include <gp_Dir.hxx>
#include <gp_Ax2.hxx>

namespace adapters {

    OcctRenderer::OcctRenderer()
        : m_width(800), m_height(600), m_initialized(false) {
    }

    OcctRenderer::~OcctRenderer() {
        shutdown();
    }

    bool OcctRenderer::initialize() {
        try {
            // Create display connection
            opencascade::handle<Aspect_DisplayConnection> displayConnection = new Aspect_DisplayConnection();

            // Create OpenGL driver
            opencascade::handle<OpenGl_GraphicDriver> graphicDriver = new OpenGl_GraphicDriver(displayConnection, false);

            // Create 3D viewer
            m_viewer = new V3d_Viewer(graphicDriver);
            m_viewer->SetDefaultLights();
            m_viewer->SetLightOn();

            // Create 3D view
            m_view = m_viewer->CreateView();
            m_view->SetBackgroundColor(Quantity_NOC_GRAY30);
            m_view->MustBeResized();
            m_view->TriedronDisplay(Aspect_TOTP_LEFT_LOWER, Quantity_NOC_WHITE, 0.08);

            // Create AIS context
            m_context = new AIS_InteractiveContext(m_viewer);
            m_context->SetDisplayMode(AIS_Shaded, Standard_True);

            // Setup lighting
            m_view->SetLightOn();

            // Create example cube
            createExampleCube();

            // Setup view cube
            setupViewCube();

            m_view->FitAll();
            m_view->ZFitAll();

            m_initialized = true;
            return true;

        }
        catch (const std::exception& e) {
            m_initialized = false;
            return false;
        }
    }

    void OcctRenderer::createExampleCube() {
        if (m_context.IsNull()) return;

        // Create a simple box
        gp_Pnt corner(0, 0, 0);
        BRepPrimAPI_MakeBox boxMaker(corner, 100, 100, 100);
        TopoDS_Shape box = boxMaker.Shape();

        // Display the box
        opencascade::handle<AIS_Shape> aisBox = new AIS_Shape(box);
        aisBox->SetColor(Quantity_NOC_ORANGE);
        aisBox->SetMaterial(Graphic3d_NOM_PLASTIC);

        m_context->Display(aisBox, Standard_True);
    }

    void OcctRenderer::setupViewCube() {
        if (m_context.IsNull()) return;

        // Create view cube for camera orientation
        m_viewCube = new AIS_ViewCube();
        m_viewCube->SetSize(60);
        m_viewCube->SetFontHeight(12);
        m_viewCube->SetAxesLabels("X", "Y", "Z");
        m_viewCube->SetDrawAxes(Standard_True);

        // Position in corner
        m_viewCube->SetFixedAnimationLoop(false);
        m_viewCube->SetAutoStartAnimation(true);

        m_context->Display(m_viewCube, Standard_False);
    }

    void OcctRenderer::shutdown() {
        if (!m_context.IsNull()) {
            m_context->RemoveAll(Standard_False);
            m_context.Nullify();
        }

        if (!m_viewCube.IsNull()) {
            m_viewCube.Nullify();
        }

        if (!m_trihedron.IsNull()) {
            m_trihedron.Nullify();
        }

        if (!m_view.IsNull()) {
            m_view.Nullify();
        }

        if (!m_viewer.IsNull()) {
            m_viewer.Nullify();
        }

        m_currentShape.Nullify();
        m_initialized = false;
    }

    void OcctRenderer::render() {
        if (!m_initialized || m_view.IsNull()) return;

        try {
            m_view->Redraw();
        }
        catch (...) {
            // Handle rendering errors silently
        }
    }

    void OcctRenderer::setModel(std::shared_ptr<domain::Model> model) {
        m_currentModel = model;

        if (!m_initialized || m_context.IsNull() || !model) return;

        // Remove previous shape
        if (!m_currentShape.IsNull()) {
            m_context->Remove(m_currentShape, Standard_False);
            m_currentShape.Nullify();
        }

        // Display new shape if available
        if (model->hasOcctShape()) {
            auto occtShape = model->getOcctShape();
            if (occtShape && !occtShape->IsNull()) {
                m_currentShape = new AIS_Shape(*occtShape);
                m_currentShape->SetColor(Quantity_NOC_CYAN1);
                m_currentShape->SetMaterial(Graphic3d_NOM_PLASTIC);
                m_context->Display(m_currentShape, Standard_True);
            }
        }
    }

    void OcctRenderer::fitAll() {
        if (!m_initialized || m_view.IsNull()) return;

        try {
            m_view->FitAll();
            m_view->ZFitAll();
        }
        catch (...) {
            // Handle errors silently
        }
    }

    void* OcctRenderer::getFramebufferTexture() {
        // TODO: Implement framebuffer texture export for ImGui integration
        return nullptr;
    }

    void OcctRenderer::resize(int width, int height) {
        m_width = width;
        m_height = height;

        if (!m_initialized || m_view.IsNull()) return;

        try {
            m_view->MustBeResized();
            m_view->Invalidate();
        }
        catch (...) {
            // Handle errors silently
        }
    }

    void OcctRenderer::rotate(float dx, float dy) {
        if (!m_initialized || m_view.IsNull()) return;

        m_view->Turn(dx, dy, 0);
        m_view->Invalidate();
    }

    void OcctRenderer::pan(float dx, float dy) {
        if (!m_initialized || m_view.IsNull()) return;

        m_view->Pan(static_cast<int>(dx), static_cast<int>(dy));
        m_view->Invalidate();
    }

    void OcctRenderer::zoom(float delta) {
        if (!m_initialized || m_view.IsNull()) return;

        m_view->SetZoom(delta);
        m_view->Invalidate();
    }

    void OcctRenderer::setViewDirection(int direction) {
        if (!m_initialized || m_view.IsNull()) return;

        switch (direction) {
        case 0: // Front
            m_view->SetProj(V3d_Yneg);
            break;
        case 1: // Back
            m_view->SetProj(V3d_Ypos);
            break;
        case 2: // Top
            m_view->SetProj(V3d_Zpos);
            break;
        case 3: // Bottom
            m_view->SetProj(V3d_Zneg);
            break;
        case 4: // Left
            m_view->SetProj(V3d_Xneg);
            break;
        case 5: // Right
            m_view->SetProj(V3d_Xpos);
            break;
        }

        m_view->FitAll();
        m_view->Invalidate();
    }

} // namespace adapters