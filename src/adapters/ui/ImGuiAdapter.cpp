#include "adapters/ui/ImGuiAdapter.h"
#include "core/Application.h"
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>
#include "imgui_internal.h"
#include <GLFW/glfw3.h>
#include <cstring>

#include "../Roboto-Regular.embed"


namespace adapters {

ImGuiAdapter::ImGuiAdapter(core::Application* app)
    : m_window(nullptr), m_app(app), m_isRotating(false), m_isPanning(false) {
    std::memset(m_filePathBuffer, 0, sizeof(m_filePathBuffer));
    std::memset(m_exportPathBuffer, 0, sizeof(m_exportPathBuffer));
    m_lastMousePos = ImVec2(0, 0);
}

ImGuiAdapter::~ImGuiAdapter() {
    shutdown();
}

bool ImGuiAdapter::initialize() {
    if (!glfwInit()) return false;
    
    const char* glsl_version = "#version 130";
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    
    m_window = glfwCreateWindow(1800, 1000, "Pistachio - CAD Converter", nullptr, nullptr);
    if (!m_window) {
        glfwTerminate();
        return false;
    }
    
    glfwMakeContextCurrent(m_window);
    glfwSwapInterval(1);
    
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    
    // Load embedded Roboto font
    ImFontConfig fontConfig;
    fontConfig.FontDataOwnedByAtlas = false;
    ImFont* robotoFont = io.Fonts->AddFontFromMemoryTTF((void*)g_RobotoRegular, sizeof(g_RobotoRegular), 20.0f, &fontConfig);
    io.FontDefault = robotoFont;


    ImGui::StyleColorsDark();
    
    ImGui_ImplGlfw_InitForOpenGL(m_window, true);
    ImGui_ImplOpenGL3_Init(glsl_version);
    
    return true;
}

void ImGuiAdapter::shutdown() {
    if (m_window) {
        ImGui_ImplOpenGL3_Shutdown();
        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext();
        
        glfwDestroyWindow(m_window);
        glfwTerminate();
        m_window = nullptr;
    }
}

bool ImGuiAdapter::shouldClose() {
    return m_window && glfwWindowShouldClose(m_window);
}

void ImGuiAdapter::beginFrame() {
    glfwPollEvents();
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();
    
    ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->WorkPos);
    ImGui::SetNextWindowSize(viewport->WorkSize);
    ImGui::SetNextWindowViewport(viewport->ID);
    
    ImGuiWindowFlags window_flags = ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoDocking;
    window_flags |= ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse;
    window_flags |= ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove;
    window_flags |= ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus;
    
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
    
    ImGui::Begin("DockSpace", nullptr, window_flags);
    ImGui::PopStyleVar(3);
    
    ImGuiID dockspace_id = ImGui::GetID("MainDockspace");
    ImGui::DockSpace(dockspace_id, ImVec2(0.0f, 0.0f));
    
    ImGui::End();
}

void ImGuiAdapter::endFrame() {
    ImGui::Render();
    int display_w, display_h;
    glfwGetFramebufferSize(m_window, &display_w, &display_h);
    glViewport(0, 0, display_w, display_h);
    glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
    glfwSwapBuffers(m_window);
}

void ImGuiAdapter::render() {
    renderMainMenu();
    renderModelInfo();
    render3DView();
    renderStatusBar();
}

void ImGuiAdapter::renderMainMenu() {



    ImGuiViewportP* vp = (ImGuiViewportP*)ImGui::GetMainViewport();
    const float toolbar_h = 48.0f * 5.0f;

    ImGuiWindowFlags tb_flags =
        ImGuiWindowFlags_NoTitleBar |
        ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoScrollbar |
        ImGuiWindowFlags_NoSavedSettings |
        ImGuiWindowFlags_NoDocking;

    // This creates a top "bar" and SHRINKS vp->WorkPos/WorkSize for the rest of the frame.
    if (ImGui::BeginViewportSideBar("##MainToolbar", vp, ImGuiDir_Up, toolbar_h, tb_flags))
    {
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0,0));
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 0));

        if (ImGui::Button("Load", ImVec2(120, 36))) {}
        ImGui::SameLine();
        if (ImGui::Button("Save", ImVec2(120, 36))) {}
        ImGui::SameLine();
        if (ImGui::Button("Sketch", ImVec2(120, 36))) {}
        ImGui::SameLine();
        if (ImGui::Button("Line", ImVec2(120, 36))) {}
        ImGui::SameLine();
        if (ImGui::Button("Rectangle", ImVec2(120, 36))) {}
        ImGui::SameLine();
        if (ImGui::Button("Circle", ImVec2(120, 36))) {}
        ImGui::SameLine();
        if (ImGui::Button("Square", ImVec2(120, 36))) {}
        ImGui::SameLine();
        if (ImGui::Button("Dimension", ImVec2(120, 36))) {}
        ImGui::SameLine();
        if (ImGui::Button("Constraint", ImVec2(120, 36))) {}
        ImGui::SameLine();
        //if (HexButtonTrueHit("Line", 48.0f, true)) { /* tool = Line */ }
        //ImGui::SameLine();
        //if (HexButtonTrueHit("Circle", 48.0f, false)) { /* tool = Circle */ }
        //ImGui::SameLine();
        //if (HexButtonTrueHit("Rectangle", 48.0f, true)) { /* pointy-top variant */ }

        
        //ImGui::SameLine();
        ////ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 0));
        //if (ParallelogramButtonTrueHit("Line 2", ImVec2(120, 36), 20.0f)) { /* tool = line */ }
        //ImGui::SameLine();
        //if (TrapeziumButtonTrueHit("Dim", ImVec2(120, 36), 18.0f)) { /* tool = dimension */ }
        //ImGui::SameLine();
        //if (ParallelogramButtonTrueHit("Line 3", ImVec2(120, 36), -20.0f)) { /* tool = line */ }



        //ImGui::SameLine();
         
        // position the cool command buttons
        // should be a function that sets the cursor position
        ImVec2 pos = vp->Size;
        pos.y = ImGui::GetCursorScreenPos().y;
        pos.x = pos.x / 2;
        pos.x = pos.x - (120 * 3);
        pos.x = pos.x + 18;
        //if (ParallelogramButtonTrueHit("Line 4", pos , ImVec2(120, 36), 18.0f)) { /* tool = line */ }
        pos.x = pos.x + 120;
        //ImGui::SameLine(120,0);
        if (TrapeziumButtonTrueHit("Home",pos, ImVec2(120, 36), 18.0f,false)) { /* tool = dimension */ }
        pos.x = pos.x + 120;
        //ImGui::SameLine(120,0);
        //if (ParallelogramButtonTrueHit("Line 5",pos,  ImVec2(120, 36), -18.0f)) { /* tool = line */ }

        
        //pos = vp->Size;
        pos.y = pos.y+36;
        pos.x = vp->Size.x / 2;
        pos.x = pos.x - (120 * 4);
        pos.x = pos.x + 36 ;// 18;
        if (TrapeziumButtonTrueHit("Reset", pos, ImVec2(120, 36), 18.0f, false)) { /* tool = dimension */ }
        pos.x = pos.x + 120;
        if (ParallelogramButtonTrueHit("Move", pos, ImVec2(120, 36), -18.0f)) { /* tool = line */ }
        pos.x = pos.x + 120-18;
        //ImGui::SameLine(120,0);
        if (TrapeziumButtonTrueHit("Scale", pos, ImVec2(120, 36), 18.0f,true)) { /* tool = dimension */ }
        pos.x = pos.x + 120-18;
        //ImGui::SameLine(120,0);
        if (ParallelogramButtonTrueHit("Undo", pos, ImVec2(120, 36), 18.0f)) { /* tool = line */ }
        pos.x = pos.x + 120 ;
        if (TrapeziumButtonTrueHit("Redo", pos, ImVec2(120, 36), 18.0f, false)) { /* tool = dimension */ }


        //ImGui::PopStyleVar(3);
        //ImGui::Dummy(ImVec2(120, 36));
        //ImGui::SameLine();
        //if (ImGui::Button("Where is this", ImVec2(120, 36))) {}

        ImGui::PopStyleVar(2);
        ImGui::End();
    }
    ImGui::Begin("File Operations");
    
    ImGui::SeparatorText("Load File");
    ImGui::InputTextWithHint("##filepath", "Enter STEP file path...", m_filePathBuffer, sizeof(m_filePathBuffer));
    
    bool isLoading = m_app && m_app->isLoading();
    if (isLoading) {
        ImGui::BeginDisabled();
    }

    if (ImGui::Button("Load STEP File", ImVec2(150, 0))) {
        if (m_app && std::strlen(m_filePathBuffer) > 0) {
            m_app->loadFile(m_filePathBuffer);
        }
    }
    if (isLoading) {
        ImGui::EndDisabled();
        ImGui::SameLine();
        ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.0f, 1.0f), "Loading...");
    }

    ImGui::TextDisabled("Supported: .step, .stp");
    
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();
    
    ImGui::SeparatorText("Export File");
    ImGui::InputTextWithHint("##exportpath", "Enter export path...", m_exportPathBuffer, sizeof(m_exportPathBuffer));
    if (ImGui::Button("Export OBJ", ImVec2(120, 0))) {
        if (m_app && std::strlen(m_exportPathBuffer) > 0) {
            m_app->exportFile(m_exportPathBuffer, "obj");
        }
    }
    ImGui::SameLine();
    if (ImGui::Button("Export STL", ImVec2(120, 0))) {
        if (m_app && std::strlen(m_exportPathBuffer) > 0) {
            m_app->exportFile(m_exportPathBuffer, "stl");
        }
    }
    
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();
    
    if (ImGui::Button("Fit All", ImVec2(150, 0))) {
        if (m_app && m_app->getRenderer()) {
            m_app->getRenderer()->fitAll();
        }
    }
    
    ImGui::End();
}

void ImGuiAdapter::renderStatusBar() {
    ImGui::Begin("Status", nullptr, ImGuiWindowFlags_NoScrollbar);

    if (m_app) {
        ImGui::Text("Status: %s", m_app->getStatus().c_str());

        // Show progress bar when loading
        if (m_app->isLoading()) {
            ImGui::SameLine();
            float progress = m_app->getLoadingProgress() / 100.0f;
            ImGui::ProgressBar(progress, ImVec2(200, 0));
        }
    }

    ImGui::End();
}

void ImGuiAdapter::renderModelInfo() {
    ImGui::Begin("Model Information");
    
    if (m_app) {
        auto model = m_app->getCurrentModel();
        if (model && !model->isEmpty()) {
            ImGui::Text("Model Name:");
            ImGui::SameLine();
            ImGui::TextColored(ImVec4(0.4f, 1.0f, 0.4f, 1.0f), "%s", model->getName().c_str());
            
            ImGui::Separator();
            
            if (model->hasOcctShape()) {
                ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f), "OpenCASCADE Shape Loaded");
                ImGui::Text("Type: TopoDS_Shape");
            }
            
            if (model->getGeometryCount() > 0) {
                ImGui::Text("Geometries: %zu", model->getGeometryCount());
                
                size_t totalVertices = 0;
                size_t totalTriangles = 0;
                for (const auto& geom : model->getGeometries()) {
                    totalVertices += geom->getVertices().size();
                    totalTriangles += geom->getTriangles().size();
                }
                ImGui::Text("Total Vertices: %zu", totalVertices);
                ImGui::Text("Total Triangles: %zu", totalTriangles);
            }
        } else {
            ImGui::TextDisabled("No model loaded");
            ImGui::Spacing();
            ImGui::TextWrapped("Load a STEP file to begin working with CAD models");
        }
    }
    
    ImGui::End();
}

void ImGuiAdapter::render3DView() {
    ImGui::Begin("3D Viewport");

    ImVec2 viewportSize = ImGui::GetContentRegionAvail();

    if (m_app && m_app->getRenderer()) {
        static ImVec2 lastSize = ImVec2(0, 0);
        if (viewportSize.x != lastSize.x || viewportSize.y != lastSize.y) {
            m_app->getRenderer()->resize((int)viewportSize.x, (int)viewportSize.y);
            lastSize = viewportSize;
        }

        void* texture = m_app->getRenderer()->getFramebufferTexture();
        if (texture) {
            ImGui::Image(texture, viewportSize);
        }
        else {
            // Placeholder - OpenCASCADE renders to its own window
            ImGui::TextWrapped("3D View Active");
            ImGui::TextDisabled("OpenCASCADE rendering to native window");

            // Mouse interaction for camera control
            if (ImGui::IsWindowHovered()) {
                ImGuiIO& io = ImGui::GetIO();

                // Right mouse button - rotate
                if (ImGui::IsMouseDown(ImGuiMouseButton_Right)) {
                    ImVec2 mousePos = ImGui::GetMousePos();
                    if (m_isRotating) {
                        float dx = (mousePos.x - m_lastMousePos.x) * 0.01f;
                        float dy = (mousePos.y - m_lastMousePos.y) * 0.01f;
                        m_app->getRenderer()->rotate(dx, dy);
                    }
                    m_isRotating = true;
                    m_lastMousePos = mousePos;
                }
                else {
                    m_isRotating = false;
                }

                // Middle mouse button - pan
                if (ImGui::IsMouseDown(ImGuiMouseButton_Middle)) {
                    ImVec2 mousePos = ImGui::GetMousePos();
                    if (m_isPanning) {
                        float dx = mousePos.x - m_lastMousePos.x;
                        float dy = mousePos.y - m_lastMousePos.y;
                        m_app->getRenderer()->pan(dx, dy);
                    }
                    m_isPanning = true;
                    m_lastMousePos = mousePos;
                }
                else {
                    m_isPanning = false;
                }

                // Mouse wheel - zoom
                if (io.MouseWheel != 0.0f) {
                    m_app->getRenderer()->zoom(io.MouseWheel * 0.1f);
                }
            }
        }

        // Camera gizmo overlay
        renderCameraGizmo();
    }
    else {
        ImGui::TextWrapped("No renderer available");
    }

    ImGui::End();
}

void ImGuiAdapter::renderCameraGizmo() {
    if (!m_app || !m_app->getRenderer()) return;

    // Position in top-right corner of viewport
    ImVec2 windowPos = ImGui::GetWindowPos();
    ImVec2 windowSize = ImGui::GetWindowSize();

    ImGui::SetNextWindowPos(ImVec2(windowPos.x + windowSize.x - 120, windowPos.y + 30));
    ImGui::SetNextWindowSize(ImVec2(110, 200));

    ImGui::Begin("Camera", nullptr,
        ImGuiWindowFlags_NoTitleBar |
        ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoScrollbar |
        ImGuiWindowFlags_NoCollapse);

    ImGui::Text("View");
    ImGui::Separator();

    if (ImGui::Button("Front", ImVec2(90, 0))) {
        m_app->getRenderer()->setViewDirection(0);
    }
    if (ImGui::Button("Back", ImVec2(90, 0))) {
        m_app->getRenderer()->setViewDirection(1);
    }
    if (ImGui::Button("Top", ImVec2(90, 0))) {
        m_app->getRenderer()->setViewDirection(2);
    }
    if (ImGui::Button("Bottom", ImVec2(90, 0))) {
        m_app->getRenderer()->setViewDirection(3);
    }
    if (ImGui::Button("Left", ImVec2(90, 0))) {
        m_app->getRenderer()->setViewDirection(4);
    }
    if (ImGui::Button("Right", ImVec2(90, 0))) {
        m_app->getRenderer()->setViewDirection(5);
    }

    ImGui::Separator();
    if (ImGui::Button("Fit All", ImVec2(90, 0))) {
        m_app->getRenderer()->fitAll();
    }

    ImGui::End();
}


static bool PointInConvexPoly(const ImVec2* pts, int count, ImVec2 p)
{
    // Convex polygon point test using cross-product sign consistency.
    // Works for clockwise or counter-clockwise point order.
    float prev = 0.0f;
    for (int i = 0; i < count; ++i)
    {
        const ImVec2 a = pts[i];
        const ImVec2 b = pts[(i + 1) % count];
        const ImVec2 ab(b.x - a.x, b.y - a.y);
        const ImVec2 ap(p.x - a.x, p.y - a.y);
        const float cross = ab.x * ap.y - ab.y * ap.x;

        if (cross != 0.0f)
        {
            if (prev != 0.0f && (cross > 0.0f) != (prev > 0.0f))
                return false;
            prev = cross;
        }
    }
    return true;
}

 bool ImGuiAdapter::HexButtonTrueHit(const char* label, float radius, bool pointy_top)
{
    ImGuiWindow* window = ImGui::GetCurrentWindow();
    if (window->SkipItems) return false;

    ImGuiContext& g = *GImGui;
    const ImGuiID id = window->GetID(label);

    // Hex bounds
    //const float sqrt3 = 1.7320508f;
    const float sqrt3 = 1.8f;

    const float w = sqrt3 * radius;
    const float h = sqrt3 * radius;

    const ImVec2 pos = window->DC.CursorPos;           // local (screen) item pos
    const ImRect bb(pos, ImVec2(pos.x + w, pos.y + h));
    ImGui::ItemSize(bb);
    if (!ImGui::ItemAdd(bb, id)) return false;

    ImGui::PushItemFlag(ImGuiItemFlags_AllowOverlap, true);
    // Build hex points (screen space)
    ImVec2 c((bb.Min.x + bb.Max.x) * 0.5f, (bb.Min.y + bb.Max.y) * 0.5f);
    ImVec2 pts[6];

    const float rot = pointy_top ? (IM_PI / 6.0f) : 0.0f; // 30° rotation for pointy-top
    for (int i = 0; i < 6; ++i)
    {
        float a = rot + (float)i * (IM_PI / 3.0f);
        pts[i] = ImVec2(c.x + cosf(a) * radius, c.y + sinf(a) * radius);
    }

    // True hit test: mouse must be inside polygon
    const ImVec2 mp = g.IO.MousePos;
    const bool inside = bb.Contains(mp) && PointInConvexPoly(pts, 6, mp);

    // Use normal ImGui behavior, but only allow hover/press if inside.
    // Allow continuing an active hold even if mouse drifts slightly.
    bool hovered = false, held = false;
    bool pressed = false;

    if (inside || g.ActiveId == id)
    {
        pressed = ImGui::ButtonBehavior(bb, id, &hovered, &held);
        // Mask hover/press if not actually inside the hex
        if (!inside && g.ActiveId != id)
        {
            hovered = false;
            pressed = false;
            held = false;
        }
        // If pressed happens when mouse is outside polygon (rare edge), cancel it.
        if (pressed && !inside) pressed = false;
        // If held and mouse is outside, you can choose to cancel hold:
        if (held && !inside && g.ActiveId == id) held = false;
    }
    ImGui::PopItemFlag();
    // Colors from current style
    const ImU32 col_fill = ImGui::GetColorU32(
        held ? ImGuiCol_ButtonActive :
        hovered ? ImGuiCol_ButtonHovered :
        ImGuiCol_Button);
    
    const ImU32 col_border = ImGui::GetColorU32(ImGuiCol_Border);
    const ImU32 col_text = ImGui::GetColorU32(ImGuiCol_Text);

    

    // Draw
    ImDrawList* dl = ImGui::GetWindowDrawList();
    dl->AddConvexPolyFilled(pts, 6, col_fill);
    dl->AddPolyline(pts, 6, col_border, ImDrawFlags_Closed, 1.0f);

    // Center text
    ImVec2 ts = ImGui::CalcTextSize(label);
    dl->AddText(ImVec2(c.x - ts.x * 0.5f, c.y - ts.y * 0.5f), col_text, label);

    return pressed;
}
  

 // Shared polygon button behavior + drawing
 static bool PolyButtonTrueHit(const char* label, const ImRect& bb, const ImVec2* pts, int pt_count)
 {
     ImGuiWindow* window = ImGui::GetCurrentWindow();
     if (window->SkipItems) return false;

     ImGuiContext& g = *GImGui;
     const ImGuiID id = window->GetID(label);

     ImGui::ItemSize(bb);
     if (!ImGui::ItemAdd(bb, id)) return false;

     const ImVec2 mp = g.IO.MousePos;
     const bool inside = bb.Contains(mp) && PointInConvexPoly(pts, pt_count, mp);

     bool hovered = false, held = false;
     bool pressed = false;

     if (inside || g.ActiveId == id)
     {
         pressed = ImGui::ButtonBehavior(bb, id, &hovered, &held);

         // Mask hover/press if outside polygon (but allow active-id continuity)
         if (!inside && g.ActiveId != id)
         {
             hovered = false;
             pressed = false;
             held = false;
         }
         if (pressed && !inside) pressed = false;
     }

     // Colors based on ImGui style (theme-friendly)
     const ImU32 col_fill = ImGui::GetColorU32(
         held ? ImGuiCol_ButtonActive :
         hovered ? ImGuiCol_ButtonHovered :
         ImGuiCol_Button);

     const ImU32 col_border = ImGui::GetColorU32(ImGuiCol_Border);
     const ImU32 col_text = ImGui::GetColorU32(ImGuiCol_Text);

     ImDrawList* dl = ImGui::GetWindowDrawList();
     dl->AddConvexPolyFilled(pts, pt_count, col_fill);
     dl->AddPolyline(pts, pt_count, col_border, ImDrawFlags_Closed, 1.0f);

     // Center text using polygon bounding box
     const float cx = (bb.Min.x + bb.Max.x) * 0.5f;
     const float cy = (bb.Min.y + bb.Max.y) * 0.5f;
     ImVec2 ts = ImGui::CalcTextSize(label);
     dl->AddText(ImVec2(cx - ts.x * 0.5f, cy - ts.y * 0.5f), col_text, label);

     return pressed;
 }
 static ImRect PolyAabb(const ImVec2* pts, int count)
 {
     ImVec2 mn = pts[0];
     ImVec2 mx = pts[0];
     for (int i = 1; i < count; ++i)
     {
         mn.x = (pts[i].x < mn.x) ? pts[i].x : mn.x;
         mn.y = (pts[i].y < mn.y) ? pts[i].y : mn.y;
         mx.x = (pts[i].x > mx.x) ? pts[i].x : mx.x;
         mx.y = (pts[i].y > mx.y) ? pts[i].y : mx.y;
     }
     return ImRect(mn, mx);
 }
 bool ImGuiAdapter::ParallelogramButtonTrueHit(const char* label, ImVec2 size, float skew_x)
 {
     ImVec2 pos = ImGui::GetCursorScreenPos();
     return ImGuiAdapter::ParallelogramButtonTrueHit(label, pos,size, skew_x);

 }
 bool ImGuiAdapter::ParallelogramButtonTrueHit(const char* label, ImVec2 pos, ImVec2 size, float skew_x)
 {
     ImGuiWindow* window = ImGui::GetCurrentWindow();
     if (window->SkipItems) return false;

     // Define a "nominal" rect just to position the shape
     ImRect nominal(pos, ImVec2(pos.x + size.x, pos.y + size.y));

     float s = ImClamp(skew_x, -size.x * 0.49f, size.x * 0.49f);

     ImVec2 pts[4];
     pts[0] = ImVec2(nominal.Min.x, nominal.Max.y);
     pts[1] = ImVec2(nominal.Max.x, nominal.Max.y);
     pts[2] = ImVec2(nominal.Max.x + s, nominal.Min.y);
     pts[3] = ImVec2(nominal.Min.x + s, nominal.Min.y);

     // Tight bounding box around the actual polygon (THIS reduces spacing)
     ImRect bb = PolyAabb(pts, 4);

     return PolyButtonTrueHit(label, bb, pts, 4);
 }



 bool ImGuiAdapter::TrapeziumButtonTrueHit(
     const char* label,
     ImVec2 pos,
     ImVec2 size,
     float inset_x,
     bool short_edge_on_bottom // false = short top, true = short bottom
 )

 {
     ImGuiWindow* window = ImGui::GetCurrentWindow();
     if (window->SkipItems) return false;

     ImRect nominal(pos, ImVec2(pos.x + size.x, pos.y + size.y));

     // inset_x is how much to trim EACH SIDE of the short edge (so width reduces by 2*inset)
     float inset = ImClamp(inset_x, 0.0f, size.x * 0.49f);

     ImVec2 pts[4];

     if (!short_edge_on_bottom)
     {
         // Normal: short TOP, long BOTTOM
         // bottom edge full width
         pts[0] = ImVec2(nominal.Min.x, nominal.Max.y); // bottom-left
         pts[1] = ImVec2(nominal.Max.x, nominal.Max.y); // bottom-right
         // top edge trimmed equally => centered
         pts[2] = ImVec2(nominal.Max.x - inset, nominal.Min.y); // top-right
         pts[3] = ImVec2(nominal.Min.x + inset, nominal.Min.y); // top-left
     }
     else
     {
         // Inverted: long TOP, short BOTTOM
         // bottom edge trimmed equally => centered
         pts[0] = ImVec2(nominal.Min.x + inset, nominal.Max.y); // bottom-left
         pts[1] = ImVec2(nominal.Max.x - inset, nominal.Max.y); // bottom-right
         // top edge full width
         pts[2] = ImVec2(nominal.Max.x, nominal.Min.y); // top-right
         pts[3] = ImVec2(nominal.Min.x, nominal.Min.y); // top-left
     }

     ImRect bb = PolyAabb(pts, 4);
     return PolyButtonTrueHit(label, bb, pts, 4);
 }


 bool ImGuiAdapter::TrapeziumButtonTrueHit(const char* label, ImVec2 size, float top_inset_x)
 {
     ImVec2 pos = ImGui::GetCursorScreenPos();
     return ImGuiAdapter::TrapeziumButtonTrueHit(label, pos, size, top_inset_x,false);
 }

 bool ImGuiAdapter::TrapeziumButtonTrueHit(const char* label, ImVec2 pos, ImVec2 size, float top_inset_x)
 {
     ImGuiWindow* window = ImGui::GetCurrentWindow();
     if (window->SkipItems) return false;

     ImRect nominal(pos, ImVec2(pos.x + size.x, pos.y + size.y));

     // Allow both orientations:
     //  +inset => narrow top (normal)
     //  -inset => wide top (inverted)
     float inset = ImClamp(top_inset_x, -size.x * 0.49f, size.x * 0.49f);

     ImVec2 pts[4];

     // Bottom edge is always the nominal full width
     pts[0] = ImVec2(nominal.Min.x, nominal.Max.y); // bottom-left
     pts[1] = ImVec2(nominal.Max.x, nominal.Max.y); // bottom-right

     // Top edge: inset can be negative (expands outward)
     pts[2] = ImVec2(nominal.Max.x - inset, nominal.Min.y); // top-right
     pts[3] = ImVec2(nominal.Min.x + inset, nominal.Min.y); // top-left

     ImRect bb = PolyAabb(pts, 4);
     return PolyButtonTrueHit(label, bb, pts, 4);
 }



} // namespace adapters