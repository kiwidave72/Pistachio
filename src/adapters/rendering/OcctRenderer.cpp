#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#undef WIN32_LEAN_AND_MEAN
#endif

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
#include <Geom_Circle.hxx>
#include <Geom_Ellipse.hxx>
#include <Geom_TrimmedCurve.hxx>
#include <BRep_Builder.hxx>
#include <BRepBuilderAPI_MakeEdge.hxx>
#include <TopoDS_Compound.hxx>
#include <ElCLib.hxx>
#include <TopExp_Explorer.hxx>
#include <AIS_DisplayMode.hxx>
#include <AIS_InteractiveContext.hxx>

#include <Prs3d_Drawer.hxx>
#include <Prs3d_LineAspect.hxx>
#include <AIS_Shape.hxx>

#include <WNT_Window.hxx>
#include "adapters/rendering/RendererRouter.h"

 
namespace adapters {

    OcctRenderer::OcctRenderer()
        : m_width(800), m_height(600), m_initialized(false) {
    }

    OcctRenderer::~OcctRenderer() {
        shutdown();
    }
    bool OcctRenderer::initialize() {
        try {
            std::cout << "\n=== OCCT RENDERER INITIALIZATION ===" << std::endl;

            // Create display connection
            opencascade::handle<Aspect_DisplayConnection> displayConnection = new Aspect_DisplayConnection();
            std::cout << "Display connection created" << std::endl;

            // Create OpenGL driver
            opencascade::handle<OpenGl_GraphicDriver> graphicDriver = new OpenGl_GraphicDriver(displayConnection, false);
            std::cout << "Graphics driver created" << std::endl;

            // Create 3D viewer
            m_viewer = new V3d_Viewer(graphicDriver);
            m_viewer->SetDefaultLights();
            m_viewer->SetLightOn();
            std::cout << "Viewer created" << std::endl;

            // Create 3D view
            m_view = m_viewer->CreateView();
            m_view->SetBackgroundColor(Quantity_NOC_GRAY30);
            std::cout << "View created" << std::endl;

            // CRITICAL: Create a separate window for the view
#ifdef _WIN32
        // Register window class once
            static bool classRegistered = false;
            if (!classRegistered) {
                WNDCLASSW wc = { 0 };
                wc.lpfnWndProc = DefWindowProcW;
                wc.hInstance = GetModuleHandle(NULL);
                wc.lpszClassName = L"OcctWindowClass";
                wc.style = CS_OWNDC;
                RegisterClassW(&wc);
                classRegistered = true;
            }

            // Create a normal overlapped window (NOT popup)
            HWND hwnd = CreateWindowExW(
                0,  // No extended style
                L"OcctWindowClass",
                L"OCCT 3D View - Sketch Geometry",
                WS_OVERLAPPEDWINDOW,  // Normal window with titlebar
                100, 100,  // Position
                800, 600,  // Size
                NULL,      // No parent
                NULL,      // No menu
                GetModuleHandle(NULL),
                NULL
            );

            if (!hwnd) {
                DWORD err = GetLastError();
                std::cout << "ERROR: Failed to create native window! Error code: " << err << std::endl;
                return false;
            }

            std::cout << "Native window created: HWND = " << hwnd << std::endl;

            // Show the window AFTER creation
            ShowWindow(hwnd, SW_SHOWNORMAL);
            UpdateWindow(hwnd);

            // Give it a moment to appear
            Sleep(100);

            // Create OCCT window wrapper
            Handle(WNT_Window) wntWindow = new WNT_Window(hwnd);
            m_view->SetWindow(wntWindow);

            if (!m_view->Window().IsNull()) {
                std::cout << "OCCT window set successfully!" << std::endl;
                Standard_Integer w, h;
                m_view->Window()->Size(w, h);
                std::cout << "Window size: " << w << "x" << h << std::endl;
            }
            else {
                std::cout << "ERROR: Failed to set OCCT window!" << std::endl;
            }

            // Map the window
            if (!m_view->Window()->IsMapped()) {
                m_view->Window()->Map();
                std::cout << "Window mapped" << std::endl;
            }
#else
            std::cout << "WARNING: Non-Windows platform - window creation not implemented!" << std::endl;
#endif

            m_view->MustBeResized();
            m_view->TriedronDisplay(Aspect_TOTP_LEFT_LOWER, Quantity_NOC_WHITE, 0.08);

            // Create AIS context
            m_context = new AIS_InteractiveContext(m_viewer);
            m_context->SetDisplayMode(AIS_Shaded, Standard_True);
            std::cout << "AIS context created" << std::endl;

            // Setup lighting
            m_view->SetLightOn();

            // Create example cube
            createExampleCube();

            // Setup view cube
            setupViewCube();

            m_view->SetProj(V3d_XposYposZneg);
            m_view->FitAll();
            m_view->ZFitAll();

            // Make sure the view updates immediately
            m_view->SetImmediateUpdate(Standard_True);

            // Force initial render
            m_view->Redraw();

            m_initialized = true;

            std::cout << "OCCT Renderer initialized successfully" << std::endl;
            std::cout << "*** You should see TWO windows: ***" << std::endl;
            std::cout << "  1. Main ImGui window (Pistachio - CAD Converter)" << std::endl;
            std::cout << "  2. OCCT 3D View window (showing cube + sketch)" << std::endl;
            std::cout << "====================================\n" << std::endl;

            return true;

        }
        catch (const Standard_Failure& e) {
            std::cout << "OCCT initialization failed: " << e.GetMessageString() << std::endl;
            m_initialized = false;
            return false;
        }
        catch (const std::exception& e) {
            std::cout << "OCCT initialization failed: " << e.what() << std::endl;
            m_initialized = false;
            return false;
        }
    }
    //bool OcctRenderer::initialize() {
    //    try {
    //        // Create display connection
    //        opencascade::handle<Aspect_DisplayConnection> displayConnection = new Aspect_DisplayConnection();

    //        // Create OpenGL driver
    //        opencascade::handle<OpenGl_GraphicDriver> graphicDriver = new OpenGl_GraphicDriver(displayConnection, false);

    //        // Create 3D viewer
    //        m_viewer = new V3d_Viewer(graphicDriver);
    //        m_viewer->SetDefaultLights();
    //        m_viewer->SetLightOn();

    //        // Create 3D view
    //        m_view = m_viewer->CreateView();
    //        m_view->SetBackgroundColor(Quantity_NOC_GRAY30);
    //        m_view->MustBeResized();
    //        m_view->TriedronDisplay(Aspect_TOTP_LEFT_LOWER, Quantity_NOC_WHITE, 0.08);

    //        // Create AIS context
    //        m_context = new AIS_InteractiveContext(m_viewer);
    //        m_context->SetDisplayMode(AIS_Shaded, Standard_True);

    //        // Setup lighting
    //        m_view->SetLightOn();

    //        // Create example cube
    //        createExampleCube();

    //        // Setup view cube
    //        setupViewCube();

    //        m_view->FitAll();
    //        m_view->ZFitAll();

    //        m_initialized = true;
    //        return true;

    //    }
    //    catch (const std::exception& e) {
    //        m_initialized = false;
    //        return false;
    //    }
    //}
    //bool OcctRenderer::initialize() {
    //    try {
    //        // Create display connection
    //        opencascade::handle<Aspect_DisplayConnection> displayConnection = new Aspect_DisplayConnection();

    //        // Create OpenGL driver
    //        opencascade::handle<OpenGl_GraphicDriver> graphicDriver = new OpenGl_GraphicDriver(displayConnection, false);

    //        // Create 3D viewer
    //        m_viewer = new V3d_Viewer(graphicDriver);
    //        m_viewer->SetDefaultLights();
    //        m_viewer->SetLightOn();

    //        // Create 3D view
    //        m_view = m_viewer->CreateView();
    //        m_view->SetBackgroundColor(Quantity_NOC_GRAY30);

    //        // CRITICAL: Set the window size before MustBeResized
    //        m_view->SetWindow(new Aspect_Window_GLFW(m_width, m_height)); // You'll need to implement this

    //        m_view->MustBeResized();
    //        m_view->TriedronDisplay(Aspect_TOTP_LEFT_LOWER, Quantity_NOC_WHITE, 0.08);

    //        // Create AIS context
    //        m_context = new AIS_InteractiveContext(m_viewer);
    //        m_context->SetDisplayMode(AIS_Shaded, Standard_True);

    //        // Setup lighting
    //        m_view->SetLightOn();

    //        // Create example cube
    //        createExampleCube();

    //        // Setup view cube
    //        setupViewCube();

    //        m_view->FitAll();
    //        m_view->ZFitAll();

    //        // CRITICAL: Make sure the view is set to immediate update mode
    //        m_view->SetImmediateUpdate(Standard_True);

    //        m_initialized = true;

    //        std::cout << "OCCT Renderer initialized successfully" << std::endl;
    //        std::cout << "  View size: " << m_width << "x" << m_height << std::endl;

    //        return true;

    //    }
    //    catch (const std::exception& e) {
    //        std::cout << "OCCT initialization failed: " << e.what() << std::endl;
    //        m_initialized = false;
    //        return false;
    //    }
    //}
    //static TopoDS_Shape BuildSketchOverlayShape(const core::rendering::RenderScene& s)
    //{
    //    BRep_Builder builder;
    //    TopoDS_Compound comp;
    //    builder.MakeCompound(comp);

    //    // ---- Lines ----
    //    for (auto const& ln : s.lines)
    //    {
    //        gp_Pnt a(ln.a.x, ln.a.y, ln.a.z);
    //        gp_Pnt b(ln.b.x, ln.b.y, ln.b.z);
    //        if (a.Distance(b) < 1e-9) continue;

    //        builder.Add(comp, BRepBuilderAPI_MakeEdge(a, b));
    //    }

    //    // ---- Polylines (curves temp / debug) ----
    //    for (auto const& pl : s.polylines)
    //    {
    //        if (pl.points.size() < 2) continue;
    //        for (size_t i = 1; i < pl.points.size(); ++i)
    //        {
    //            const auto& p0 = pl.points[i - 1];
    //            const auto& p1 = pl.points[i];
    //            gp_Pnt a(p0.x, p0.y, p0.z);
    //            gp_Pnt b(p1.x, p1.y, p1.z);
    //            if (a.Distance(b) < 1e-9) continue;

    //            builder.Add(comp, BRepBuilderAPI_MakeEdge(a, b));
    //        }
    //    }

    //    // ---- Circles ----
    //    for (auto const& c : s.circles)
    //    {
    //        gp_Pnt center(c.center.x, c.center.y, c.center.z);
    //        gp_Dir normal(c.normal.x, c.normal.y, c.normal.z);
    //        gp_Ax2 ax2(center, normal);

    //        Handle(Geom_Circle) gc = new Geom_Circle(ax2, c.radius);
    //        builder.Add(comp, BRepBuilderAPI_MakeEdge(gc));
    //    }

    //    // ---- Arcs ----
    //    for (auto const& a : s.arcs)
    //    {
    //        gp_Pnt center(a.center.x, a.center.y, a.center.z);
    //        gp_Dir normal(a.normal.x, a.normal.y, a.normal.z);
    //        gp_Ax2 ax2(center, normal);

    //        Handle(Geom_Circle) gc = new Geom_Circle(ax2, a.radius);

    //        gp_Pnt p0(a.start.x, a.start.y, a.start.z);
    //        gp_Pnt p1(a.end.x, a.end.y, a.end.z);

    //        Standard_Real u0 = ElCLib::Parameter(gc->Circ(), p0);
    //        Standard_Real u1 = ElCLib::Parameter(gc->Circ(), p1);

    //        // If you want CW, swap.
    //        if (!a.ccw) std::swap(u0, u1);

    //        Handle(Geom_TrimmedCurve) arc = new Geom_TrimmedCurve(gc, u0, u1, Standard_True);
    //        builder.Add(comp, BRepBuilderAPI_MakeEdge(arc));
    //    }

    //    // ---- Ellipses ----
    //    for (auto const& e : s.ellipses)
    //    {
    //        gp_Pnt center(e.center.x, e.center.y, e.center.z);
    //        gp_Dir normal(e.normal.x, e.normal.y, e.normal.z);
    //        gp_Ax2 ax2(center, normal);

    //        // rotate the in-plane axes by rotationRad
    //        const double cr = std::cos((double)e.rotationRad);
    //        const double sr = std::sin((double)e.rotationRad);

    //        gp_Dir xdir = ax2.XDirection();
    //        gp_Dir ydir = ax2.YDirection();

    //        gp_Vec xv(xdir.X(), xdir.Y(), xdir.Z());
    //        gp_Vec yv(ydir.X(), ydir.Y(), ydir.Z());

    //        gp_Vec xr = xv * cr + yv * sr;
    //        gp_Vec yr = yv * cr - xv * sr;

    //        ax2.SetXDirection(gp_Dir(xr));
    //        ax2.SetYDirection(gp_Dir(yr));

    //        Handle(Geom_Ellipse) ge = new Geom_Ellipse(ax2, e.rx, e.ry);
    //        builder.Add(comp, BRepBuilderAPI_MakeEdge(ge));
    //    }

    //    return comp;
    //}
    static TopoDS_Shape BuildSketchOverlayShape(const core::rendering::RenderScene& s)
    {
        BRep_Builder builder;
        TopoDS_Compound comp;
        builder.MakeCompound(comp);

        bool hasGeometry = false;

        // ---- Lines ----
        for (auto const& ln : s.lines)
        {
            //if (!ln.h.visible) continue;

            gp_Pnt a(ln.a.x, ln.a.y, ln.a.z);
            gp_Pnt b(ln.b.x, ln.b.y, ln.b.z);
            if (a.Distance(b) < 1e-9) continue;

            builder.Add(comp, BRepBuilderAPI_MakeEdge(a, b));
            hasGeometry = true;
        }

        // ---- Polylines (curves temp / debug) ----
        for (auto const& pl : s.polylines)
        {
            if (pl.points.size() < 2) continue;
            for (size_t i = 1; i < pl.points.size(); ++i)
            {
                const auto& p0 = pl.points[i - 1];
                const auto& p1 = pl.points[i];
                gp_Pnt a(p0.x, p0.y, p0.z);
                gp_Pnt b(p1.x, p1.y, p1.z);
                if (a.Distance(b) < 1e-9) continue;

                builder.Add(comp, BRepBuilderAPI_MakeEdge(a, b));
                hasGeometry = true;
            }
        }

        // ---- Circles ----
        for (auto const& c : s.circles)
        {
            try {
                gp_Pnt center(c.center.x, c.center.y, c.center.z);
                gp_Dir normal(c.normal.x, c.normal.y, c.normal.z);
                gp_Ax2 ax2(center, normal);

                Handle(Geom_Circle) gc = new Geom_Circle(ax2, c.radius);
                builder.Add(comp, BRepBuilderAPI_MakeEdge(gc));
                hasGeometry = true;
            }
            catch (...) {
                // Skip invalid circles
            }
        }

        // ---- Arcs ----
        for (auto const& a : s.arcs)
        {
            try {
                gp_Pnt center(a.center.x, a.center.y, a.center.z);
                gp_Dir normal(a.normal.x, a.normal.y, a.normal.z);
                gp_Ax2 ax2(center, normal);

                Handle(Geom_Circle) gc = new Geom_Circle(ax2, a.radius);

                gp_Pnt p0(a.start.x, a.start.y, a.start.z);
                gp_Pnt p1(a.end.x, a.end.y, a.end.z);

                Standard_Real u0 = ElCLib::Parameter(gc->Circ(), p0);
                Standard_Real u1 = ElCLib::Parameter(gc->Circ(), p1);

                // If you want CW, swap.
                if (!a.ccw) std::swap(u0, u1);

                Handle(Geom_TrimmedCurve) arc = new Geom_TrimmedCurve(gc, u0, u1, Standard_True);
                builder.Add(comp, BRepBuilderAPI_MakeEdge(arc));
                hasGeometry = true;
            }
            catch (...) {
                // Skip invalid arcs
            }
        }

        // ---- Ellipses ----
        for (auto const& e : s.ellipses)
        {
            try {
                gp_Pnt center(e.center.x, e.center.y, e.center.z);
                gp_Dir normal(e.normal.x, e.normal.y, e.normal.z);
                gp_Ax2 ax2(center, normal);

                // rotate the in-plane axes by rotationRad
                const double cr = std::cos((double)e.rotationRad);
                const double sr = std::sin((double)e.rotationRad);

                gp_Dir xdir = ax2.XDirection();
                gp_Dir ydir = ax2.YDirection();

                gp_Vec xv(xdir.X(), xdir.Y(), xdir.Z());
                gp_Vec yv(ydir.X(), ydir.Y(), ydir.Z());

                gp_Vec xr = xv * cr + yv * sr;
                gp_Vec yr = yv * cr - xv * sr;

                ax2.SetXDirection(gp_Dir(xr));
                ax2.SetYDirection(gp_Dir(yr));

                Handle(Geom_Ellipse) ge = new Geom_Ellipse(ax2, e.rx, e.ry);
                builder.Add(comp, BRepBuilderAPI_MakeEdge(ge));
                hasGeometry = true;
            }
            catch (...) {
                // Skip invalid ellipses
            }
        }

        // Return empty compound if no geometry was added
        return comp;
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

        Handle(Prs3d_Drawer) d = aisBox->Attributes();
        d->SetFaceBoundaryDraw(Standard_True);
        d->SetFaceBoundaryAspect(new Prs3d_LineAspect(Quantity_NOC_BLACK, Aspect_TOL_SOLID, 2.0));
        
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

    //void OcctRenderer::render()
    //{
    //    if (!m_initialized || m_view.IsNull()) return;

    //    try
    //    {
    //        // Update sketch overlay (only when changed)
    //        if (m_sketchOverlayDirty && !m_context.IsNull())
    //        {
    //            TopoDS_Shape overlay = BuildSketchOverlayShape(m_sketchOverlay);

    //            if (m_sketchOverlayAis.IsNull())
    //            {
    //                m_sketchOverlayAis = new AIS_Shape(overlay);
    //                m_context->Display(m_sketchOverlayAis, Standard_False);
    //            }
    //            else
    //            {
    //                m_sketchOverlayAis->SetShape(overlay);
    //                m_context->Redisplay(m_sketchOverlayAis, Standard_False);
    //            }

    //            m_sketchOverlayDirty = false;
    //        }

    //        m_view->Redraw();
    //    }
    //    catch (...)
    //    {
    //        // Handle rendering errors silently
    //    }
    //}
    //void OcctRenderer::render()
    //{
    //    if (!m_initialized || m_view.IsNull()) return;

    //    try
    //    {
    //        // Update sketch overlay (only when changed)
    //        if (m_sketchOverlayDirty && !m_context.IsNull())
    //        {
    //            TopoDS_Shape overlay = BuildSketchOverlayShape(m_sketchOverlay);

    //            if (m_sketchOverlayAis.IsNull())
    //            {
    //                m_sketchOverlayAis = new AIS_Shape(overlay);

    //                // Set appearance for sketch overlay
    //                m_sketchOverlayAis->SetColor(Quantity_NOC_WHITE);
    //                m_sketchOverlayAis->SetWidth(2.0);
    //                m_sketchOverlayAis->SetDisplayMode(AIS_WireFrame);
    //                m_sketchOverlayAis->SetZLayer(Graphic3d_ZLayerId_Top); // Draw on top

    //                m_context->Display(m_sketchOverlayAis, Standard_True);
    //            }
    //            else
    //            {
    //                m_sketchOverlayAis->SetShape(overlay);
    //                m_context->Redisplay(m_sketchOverlayAis, Standard_True);
    //            }

    //            m_sketchOverlayDirty = false;
    //        }

    //        // Force view update
    //        m_view->Redraw();
    //        m_view->Update();
    //    }
    //    catch (const Standard_Failure& e)
    //    {
    //        // Log error if needed
    //        std::string msg = e.GetMessageString();
    //    }
    //    catch (...)
    //    {
    //        // Handle rendering errors silently
    //    }
    //}
    void OcctRenderer::render(GLFWwindow* m_window)
    {
        if (!m_initialized) {
            std::cout << "ERROR: Renderer not initialized!" << std::endl;
            return;
        }

        if (m_view.IsNull()) {
            std::cout << "ERROR: View is null!" << std::endl;
            return;
        }

        if (m_context.IsNull()) {
            std::cout << "ERROR: Context is null!" << std::endl;
            return;
        }

        std::cout << "\n=== OCCT RENDER START ===" << std::endl;

        try
        {
            // Check view window
            if (!m_view->Window().IsNull()) {
                Standard_Integer width, height;
                m_view->Window()->Size(width, height);
                std::cout << "Window size: " << width << "x" << height << std::endl;
            }
            else {
                std::cout << "View has window: NO (THIS IS A PROBLEM!)" << std::endl;
            }


            // Count objects in context
            AIS_ListOfInteractive allObjects;
            m_context->DisplayedObjects(allObjects);
            std::cout << "Objects in context: " << allObjects.Size() << std::endl;

            // Update sketch overlay (only when changed)
            if (m_sketchOverlayDirty && !m_context.IsNull())
            {
                std::cout << "\n--- Building sketch overlay ---" << std::endl;
                std::cout << "Scene has:" << std::endl;
                std::cout << "  Lines: " << m_sketchOverlay.lines.size() << std::endl;
                std::cout << "  Circles: " << m_sketchOverlay.circles.size() << std::endl;
                std::cout << "  Arcs: " << m_sketchOverlay.arcs.size() << std::endl;
                std::cout << "  Ellipses: " << m_sketchOverlay.ellipses.size() << std::endl;
                std::cout << "  Polylines: " << m_sketchOverlay.polylines.size() << std::endl;

                TopoDS_Shape overlay = BuildSketchOverlayShape(m_sketchOverlay);

                if (overlay.IsNull()) {
                    std::cout << "WARNING: Built overlay shape is NULL!" << std::endl;
                }
                else {
                    std::cout << "Overlay shape built: NOT NULL" << std::endl;

                    // Check if compound has any content
                    TopExp_Explorer exp(overlay, TopAbs_EDGE);
                    int edgeCount = 0;
                    for (; exp.More(); exp.Next()) edgeCount++;
                    std::cout << "Edges in overlay shape: " << edgeCount << std::endl;
                }

                if (m_sketchOverlayAis.IsNull())
                {
                    std::cout << "Creating NEW AIS_Shape for overlay" << std::endl;
                    m_sketchOverlayAis = new AIS_Shape(overlay);

                    // Set appearance - make it very visible
                    m_sketchOverlayAis->SetColor(Quantity_NOC_RED);  // Bright red for testing
                    m_sketchOverlayAis->SetWidth(5.0);  // Very thick
                    m_sketchOverlayAis->SetDisplayMode(AIS_WireFrame);
                    m_sketchOverlayAis->SetZLayer(Graphic3d_ZLayerId_Top);

                    std::cout << "Calling m_context->Display()..." << std::endl;
                    m_context->Display(m_sketchOverlayAis, Standard_True);
                    std::cout << "Display complete" << std::endl;

                    // Check if it's actually displayed
                    if (m_context->IsDisplayed(m_sketchOverlayAis)) {
                        std::cout << "[OK] Overlay IS displayed in context" << std::endl;
                    }
                    else {
                        std::cout << "[X] Overlay NOT displayed in context!" << std::endl;
                    }
                }
                else
                {
                    std::cout << "UPDATING existing AIS_Shape overlay" << std::endl;
                    m_sketchOverlayAis->SetShape(overlay);
                    m_context->Redisplay(m_sketchOverlayAis, Standard_True);
                    std::cout << "Redisplay complete" << std::endl;
                }

                m_sketchOverlayDirty = false;
            }

            std::cout << "\nCalling m_view->Redraw()..." << std::endl;
            m_view->Redraw();
            std::cout << "Calling m_view->Update()..." << std::endl;
            m_view->Update();
            std::cout << "View update complete" << std::endl;

            // Count objects again after render
            allObjects.Clear();
            m_context->DisplayedObjects(allObjects);
            std::cout << "Objects in context after render: " << allObjects.Size() << std::endl;

            
            if (m_window)
                glfwMakeContextCurrent(m_window);
        }
        catch (const Standard_Failure& e)
        {
            std::cout << "OCCT Exception: " << e.GetMessageString() << std::endl;
        }
        catch (const std::exception& e)
        {
            std::cout << "Standard Exception: " << e.what() << std::endl;
        }
        catch (...)
        {
            std::cout << "Unknown exception in render()" << std::endl;
        }

        std::cout << "=== OCCT RENDER END ===\n" << std::endl;
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

    void OcctRenderer::setScene(const ports::RenderScene& scene) {
        // For now we keep the scene for future sketch overlay rendering.
        // OCCT renderer currently renders via AIS and ignores the generic primitives.
        m_scene = scene;
    }

    void OcctRenderer::setCameraState(const ports::CameraState& camera) {
        // We store the camera state, and will later map it onto V3d_View camera.
        m_camera = camera;
    }
    void adapters::OcctRenderer::setSketchOverlay(const core::rendering::RenderScene& scene)
    {
        m_sketchOverlay = scene;
        m_sketchOverlayDirty = true;
    }

    ports::CameraState OcctRenderer::getCameraState() const {
        return m_camera;
    }

    ports::RendererCapabilities OcctRenderer::getCapabilities() const {
        ports::RendererCapabilities caps;
        caps.supportsFramebufferTexture = false;
        caps.supportsNativeBRep = true;
        caps.supportsSectionPlane = true;
        return caps;
    }

    ports::RendererBackend OcctRenderer::getBackend() const {
        return ports::RendererBackend::Occt;
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