#include "adapters/ui/ImGuiAdapter.h"
#include "core/Application.h"

#include "../Roboto-Regular.embed"
#include "../ImGui/ImGuiTheme.h"
#include "../ImGui/Image.h"


#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>
#include "imgui_internal.h"
#include <GLFW/glfw3.h>
#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3native.h>

#include <cstring>
 
#include "stb_image.h"


#include "UI.h"

#include "core/rendering/SketchRenderBuilder.h"
#include "adapters/rendering/OcctRenderer.h"
#include "adapters/rendering/RendererRouter.h"

#include "../../../Walnut-Icon.embed"
#include "../../../WindowImages.embed"

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
    glfwWindowHint(GLFW_TITLEBAR, false);



    m_window = glfwCreateWindow(1200, 800, "Pistachio - CAD Converter", nullptr, nullptr);
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

    {
        uint32_t w, h;
        void* data = Walnut::Image::Decode(g_WalnutIcon, sizeof(g_WalnutIcon), w, h);
        if (data) {
            m_AppHeaderIcon = std::make_shared<Walnut::Image>(w, h, Walnut::ImageFormat::RGBA, data);
            // Free the decoded data (Image has copied it)
            stbi_image_free(data);
        }
    }
    {
        uint32_t w, h;
        void* data = Walnut::Image::Decode(g_WindowMinimizeIcon, sizeof(g_WindowMinimizeIcon), w, h);
        if (data) {
            m_IconMinimize = std::make_shared<Walnut::Image>(w, h, Walnut::ImageFormat::RGBA, data);
            // Free the decoded data (Image has copied it)
            stbi_image_free(data);
        }
    }

    {
        uint32_t w, h;
        void* data = Walnut::Image::Decode(g_WindowMaximizeIcon, sizeof(g_WindowMaximizeIcon), w, h);
        if (data) {
            m_IconMaximize = std::make_shared<Walnut::Image>(w, h, Walnut::ImageFormat::RGBA, data);
            // Free the decoded data (Image has copied it)
            stbi_image_free(data);
        }
    }
    {
        uint32_t w, h;
        void* data = Walnut::Image::Decode(g_WindowRestoreIcon, sizeof(g_WindowRestoreIcon), w, h);
        if (data) {
            m_IconRestore = std::make_shared<Walnut::Image>(w, h, Walnut::ImageFormat::RGBA, data);
            // Free the decoded data (Image has copied it)
            stbi_image_free(data);
        }
    }

    {
        uint32_t w, h;
        void* data = Walnut::Image::Decode(g_WindowCloseIcon, sizeof(g_WindowCloseIcon), w, h);
        if (data) {
            m_IconClose = std::make_shared<Walnut::Image>(w, h, Walnut::ImageFormat::RGBA, data);
            // Free the decoded data (Image has copied it)
            stbi_image_free(data);
        }
    }

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
    glfwMakeContextCurrent(m_window);
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();
    float titlebarHeight = 50.0f;
    // ====================================
 // 1. Draw the Titlebar as a separate window FIRST
 // ====================================
    {
        ImGuiWindowFlags titlebar_flags =
            ImGuiWindowFlags_NoTitleBar |
            ImGuiWindowFlags_NoResize |
            ImGuiWindowFlags_NoMove |
            ImGuiWindowFlags_NoScrollbar |
            ImGuiWindowFlags_NoScrollWithMouse |
            ImGuiWindowFlags_NoDocking;

        ImGuiViewport* viewport = ImGui::GetMainViewport();

       

        // Position at top of viewport
        ImGui::SetNextWindowPos(viewport->Pos);
        ImGui::SetNextWindowSize(ImVec2(viewport->Size.x, titlebarHeight)); // Adjust height as needed
        ImGui::SetNextWindowViewport(viewport->ID);

        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));

        ImGui::Begin("##Titlebar", nullptr, titlebar_flags);
        {
            UI_DrawTitlebar(titlebarHeight);

        }
        ImGui::End();

        ImGui::PopStyleVar(3);
    }

    // ====================================
    // 2. Draw the DockSpace below the titlebar
    // ====================================
    {
        ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoDocking;
        ImGuiViewport* viewport = ImGui::GetMainViewport();

        //float titlebarHeight = 130.0f; // Should match the titlebar window height
        ImVec2 size=  viewport->Size;

        // Position BELOW the titlebar
        ImGui::SetNextWindowPos(ImVec2(viewport->Pos.x, viewport->Pos.y + titlebarHeight));
        ImGui::SetNextWindowSize(ImVec2(viewport->Size.x, viewport->Size.y - titlebarHeight));
        ImGui::SetNextWindowViewport(viewport->ID);

        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);

        window_flags |= ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse |
            ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove;
        window_flags |= ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus;

        const bool isMaximized = IsMaximized();
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, isMaximized ? ImVec2(6.0f, 6.0f) : ImVec2(1.0f, 1.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 3.0f);
        ImGui::PushStyleColor(ImGuiCol_MenuBarBg, ImVec4{ 0.0f, 0.0f, 0.0f, 0.0f });

        ImGui::Begin("DockSpaceWindow", nullptr, window_flags);
        ImGui::PopStyleColor(); // MenuBarBg
        ImGui::PopStyleVar(4);

        {
            ImGui::PushStyleColor(ImGuiCol_Border, IM_COL32(50, 50, 50, 255));
            // Draw window border if needed
            ImGui::PopStyleColor(); // ImGuiCol_Border
        }

        // Create the docking space
        ImGuiID dockspace_id = ImGui::GetID("MyDockSpace");
        ImGui::DockSpace(dockspace_id, ImVec2(0.0f, 0.0f), ImGuiDockNodeFlags_PassthruCentralNode);

        ImGui::End();
    }

   ///* ImGuiViewportP* vp = (ImGuiViewportP*)ImGui::GetMainViewport();
   // const float toolbar_h = 48.0f * 5.0f;

   // ImGuiWindowFlags tb_flags =
   //     ImGuiWindowFlags_NoTitleBar |
   //     ImGuiWindowFlags_NoResize |
   //     ImGuiWindowFlags_NoMove |
   //     ImGuiWindowFlags_NoScrollbar |
   //     ImGuiWindowFlags_NoSavedSettings |
   //     ImGuiWindowFlags_NoDocking;*/

   // // This creates a top "bar" and SHRINKS vp->WorkPos/WorkSize for the rest of the frame.
   // //if (ImGui::BeginViewportSideBar("##MainToolbar", vp, ImGuiDir_Up, toolbar_h, tb_flags))
   //// {
   //     ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0,0));
   //     ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 0));

   //     if (ImGui::Button("Load", ImVec2(120, 36))) {}
   //     ImGui::SameLine();
   //     if (ImGui::Button("Save", ImVec2(120, 36))) {}
   //     ImGui::SameLine();
   //     if (ImGui::Button("Sketch", ImVec2(120, 36))) {}
   //     ImGui::SameLine();
   //     if (ImGui::Button("Line", ImVec2(120, 36))) {}
   //     ImGui::SameLine();
   //     if (ImGui::Button("Rectangle", ImVec2(120, 36))) {}
   //     ImGui::SameLine();
   //     if (ImGui::Button("Circle", ImVec2(120, 36))) {}
   //     ImGui::SameLine();
   //     if (ImGui::Button("Square", ImVec2(120, 36))) {}
   //     ImGui::SameLine();
   //     if (ImGui::Button("Dimension", ImVec2(120, 36))) {}
   //     ImGui::SameLine();
   //     if (ImGui::Button("Constraint", ImVec2(120, 36))) {}
   //     ImGui::SameLine();
   //     //if (HexButtonTrueHit("Line", 48.0f, true)) { /* tool = Line */ }
   //     //ImGui::SameLine();
   //     //if (HexButtonTrueHit("Circle", 48.0f, false)) { /* tool = Circle */ }
   //     //ImGui::SameLine();
   //     //if (HexButtonTrueHit("Rectangle", 48.0f, true)) { /* pointy-top variant */ }

   //     
   //     //ImGui::SameLine();
   //     ////ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 0));
   //     //if (ParallelogramButtonTrueHit("Line 2", ImVec2(120, 36), 20.0f)) { /* tool = line */ }
   //     //ImGui::SameLine();
   //     //if (TrapeziumButtonTrueHit("Dim", ImVec2(120, 36), 18.0f)) { /* tool = dimension */ }
   //     //ImGui::SameLine();
   //     //if (ParallelogramButtonTrueHit("Line 3", ImVec2(120, 36), -20.0f)) { /* tool = line */ }



   //     //ImGui::SameLine();
   //      
   //     // position the cool command buttons
   //     // should be a function that sets the cursor position
   //     ImVec2 pos = viewport->Size;
   //     pos.y = ImGui::GetCursorScreenPos().y;
   //     pos.x = pos.x / 2;
   //     pos.x = pos.x - (120 * 3);
   //     pos.x = pos.x + 18;
   //     //if (ParallelogramButtonTrueHit("Line 4", pos , ImVec2(120, 36), 18.0f)) { /* tool = line */ }
   //     pos.x = pos.x + 120;
   //     //ImGui::SameLine(120,0);
   //     if (TrapeziumButtonTrueHit("Home",pos, ImVec2(120, 36), 18.0f,false)) { /* tool = dimension */ }
   //     pos.x = pos.x + 120;
   //     //ImGui::SameLine(120,0);
   //     //if (ParallelogramButtonTrueHit("Line 5",pos,  ImVec2(120, 36), -18.0f)) { /* tool = line */ }

   //     
   //     //pos = vp->Size;
   //     pos.y = pos.y+36;
   //     pos.x = viewport->Size.x / 2;
   //     pos.x = pos.x - (120 * 4);
   //     pos.x = pos.x + 36 ;// 18;
   //     if (TrapeziumButtonTrueHit("Reset", pos, ImVec2(120, 36), 18.0f, false)) { /* tool = dimension */ }
   //     pos.x = pos.x + 120;
   //     if (ParallelogramButtonTrueHit("Move", pos, ImVec2(120, 36), -18.0f)) { /* tool = line */ }
   //     pos.x = pos.x + 120-18;
   //     //ImGui::SameLine(120,0);
   //     if (TrapeziumButtonTrueHit("Scale", pos, ImVec2(120, 36), 18.0f,true)) { /* tool = dimension */ }
   //     pos.x = pos.x + 120-18;
   //     //ImGui::SameLine(120,0);
   //     if (ParallelogramButtonTrueHit("Undo", pos, ImVec2(120, 36), 18.0f)) { /* tool = line */ }
   //     pos.x = pos.x + 120 ;
   //     if (TrapeziumButtonTrueHit("Redo", pos, ImVec2(120, 36), 18.0f, false)) { /* tool = dimension */ }


   //     //ImGui::PopStyleVar(3);
   //     //ImGui::Dummy(ImVec2(120, 36));
   //     //ImGui::SameLine();
   //     //if (ImGui::Button("Where is this", ImVec2(120, 36))) {}

   //     ImGui::PopStyleVar(2);
   //     ImGui::End();
   //// }


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

//void ImGuiAdapter::beginFrame() {
//    
//    glfwPollEvents();
//    
//    glfwMakeContextCurrent(m_window);
//
//    ImGui_ImplOpenGL3_NewFrame();
//    ImGui_ImplGlfw_NewFrame();
//    ImGui::NewFrame();
//
//    // Simple dockspace setup
//    ImGuiViewport* viewport = ImGui::GetMainViewport();
//    ImGui::SetNextWindowPos(viewport->WorkPos);
//    ImGui::SetNextWindowSize(viewport->WorkSize);
//    ImGui::SetNextWindowViewport(viewport->ID);
//
//    ImGuiWindowFlags window_flags = ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoDocking;
//    window_flags |= ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse;
//    window_flags |= ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove;
//    window_flags |= ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus;
//
//    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
//    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
//    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
//
//    ImGui::Begin("DockSpace", nullptr, window_flags);
//    ImGui::PopStyleVar(3);
//
//    // Menu bar
//    if (ImGui::BeginMenuBar()) {
//        if (ImGui::BeginMenu("File")) {
//            if (ImGui::MenuItem("Exit")) {
//                glfwSetWindowShouldClose(m_window, GLFW_TRUE);
//            }
//            ImGui::EndMenu();
//        }
//        ImGui::EndMenuBar();
//    }
//
//    ImGuiID dockspace_id = ImGui::GetID("MainDockspace");
//    ImGui::DockSpace(dockspace_id, ImVec2(0.0f, 0.0f));
//
//    ImGui::End();
//
//    
//}

void ImGuiAdapter::endFrame() {
    ImGui::Render();
    int display_w, display_h;
    glfwGetFramebufferSize(m_window, &display_w, &display_h);
    glViewport(0, 0, display_w, display_h);
    glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
    glfwSwapBuffers(m_window);

    glfwMakeContextCurrent(m_window);
}

void ImGuiAdapter::render() {
    
    //ImGui::ShowDemoWindow();

    //renderMainMenu();
    //renderModelInfo();  
    render3DView();
    //renderStatusBar();
    DrawViewport();
   
}

void ImGuiAdapter::DrawViewport()
{
    ImGui::Begin("Viewport");

    // === DIAGNOSTICS ===
    ImGui::Separator();
    ImGui::Text("RENDERER STATUS");
    ImGui::Separator();

    auto* renderer = m_app->getRenderer();
    if (!renderer) {
        ImGui::TextColored(ImVec4(1, 0, 0, 1), "[X] NO RENDERER!");
        ImGui::End();
        return;
    }

    // Direct cast (no router)
    auto* occt = dynamic_cast<adapters::OcctRenderer*>(renderer);
    if (occt) {
        ImGui::TextColored(ImVec4(0, 1, 0, 1), "[OK] OcctRenderer active");
    }
    else {
        ImGui::TextColored(ImVec4(1, 0, 0, 1), "[X] Not OcctRenderer!");
        ImGui::End();
        return;
    }

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Text("SKETCH DATA");
    ImGui::Separator();

    // === SKETCH SECTION ===
    auto doc = m_app->getSketchDocument();
    if (!doc) {
        ImGui::TextColored(ImVec4(1, 1, 0, 1), "[!] No sketch document loaded");
        ImGui::TextWrapped("Sketch should have been loaded in main.cpp");
        ImGui::End();
        return;
    }

    ImGui::TextColored(ImVec4(0, 1, 0, 1), "[OK] Sketch document loaded");
    ImGui::Text("Sketches: %zu", doc->sketches.size());

    if (doc->sketches.empty()) {
        ImGui::TextColored(ImVec4(1, 1, 0, 1), "[!] Document has no sketches");
        ImGui::End();
        return;
    }

    // Get first sketch
    auto& sketch = doc->sketches[0];

    ImGui::Text("Sketch 0 entities:");
    ImGui::Indent();
    size_t pointCount = sketch.entities.points().size();
    size_t lineCount = sketch.entities.lines().size();
    size_t circleCount = sketch.entities.circles().size();
    size_t arcCount = sketch.entities.arcs().size();
    size_t ellipseCount = sketch.entities.ellipses().size();
    size_t curveCount = sketch.entities.curves().size();

    ImGui::Text("Points: %zu", pointCount);
    ImGui::Text("Lines: %zu", lineCount);
    ImGui::Text("Circles: %zu", circleCount);
    ImGui::Text("Arcs: %zu", arcCount);
    ImGui::Text("Ellipses: %zu", ellipseCount);
    ImGui::Text("Curves: %zu", curveCount);
    ImGui::Unindent();

    size_t totalEntities = pointCount + lineCount + circleCount + arcCount + ellipseCount + curveCount;

    if (totalEntities == 0) {
        ImGui::TextColored(ImVec4(1, 1, 0, 1), "[!] Sketch has no entities!");
        ImGui::End();
        return;
    }

    // Build render scene
    auto scene = core::rendering::BuildRenderSceneFromSketch(sketch);

    ImGui::Spacing();
    ImGui::Text("Render scene:");
    ImGui::Indent();
    ImGui::Text("Lines: %zu", scene.lines.size());
    ImGui::Text("Circles: %zu", scene.circles.size());
    ImGui::Text("Arcs: %zu", scene.arcs.size());
    ImGui::Text("Ellipses: %zu", scene.ellipses.size());
    ImGui::Text("Polylines: %zu", scene.polylines.size());
    ImGui::Text("Points: %zu", scene.points.size());
    ImGui::Unindent();

    // Show sample data
    if (!scene.lines.empty()) {
        ImGui::Spacing();
        ImGui::Text("Sample line 0:");
        ImGui::Indent();
        auto& line = scene.lines[0];
        ImGui::Text("From: (%.2f, %.2f, %.2f)", line.a.x, line.a.y, line.a.z);
        ImGui::Text("  To: (%.2f, %.2f, %.2f)", line.b.x, line.b.y, line.b.z);
        float length = glm::length(line.b - line.a);
        ImGui::Text("Length: %.2f", length);
        ImGui::Unindent();
    }

    if (!scene.circles.empty()) {
        ImGui::Spacing();
        ImGui::Text("Sample circle 0:");
        ImGui::Indent();
        auto& circle = scene.circles[0];
        ImGui::Text("Center: (%.2f, %.2f, %.2f)", circle.center.x, circle.center.y, circle.center.z);
        ImGui::Text("Radius: %.2f", circle.radius);
        ImGui::Unindent();
    }

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Text("ACTIONS");
    ImGui::Separator();

    // Set overlay
    occt->setSketchOverlay(scene);
    ImGui::TextColored(ImVec4(0, 1, 0, 1), "[OK] Overlay set");

    if (ImGui::Button("Force Render & Update View", ImVec2(200, 0))) {
        std::cout << "\n>>> MANUAL RENDER TRIGGERED <<<\n" << std::endl;
        occt->render(m_window);
        occt->fitAll();
    }

    ImGui::SameLine();
    if (ImGui::Button("Fit All", ImVec2(100, 0))) {
        occt->fitAll();
    }

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Text("AUTO RENDER");
    ImGui::Separator();

    // Automatic render call
    occt->render(m_window);
    ImGui::Text("[OK] Render called automatically");

    ImGui::End();
}

void ImGuiAdapter::renderMainMenu() {
    
//    ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoDocking;
//
//    ImGuiViewport* viewport = ImGui::GetMainViewport();
//    ImGui::SetNextWindowPos(viewport->Pos);
//    ImGui::SetNextWindowSize(viewport->Size);
//    ImGui::SetNextWindowViewport(viewport->ID);
//    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
//    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
//    window_flags |= ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove;
//    window_flags |= ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus;
////    if (!m_Specification.CustomTitlebar && m_MenubarCallback)
////        window_flags |= ImGuiWindowFlags_MenuBar;
//
//    const bool isMaximized = IsMaximized();
//
//    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, isMaximized ? ImVec2(6.0f, 6.0f) : ImVec2(1.0f, 1.0f));
//    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 3.0f);
//
//    ImGui::PushStyleColor(ImGuiCol_MenuBarBg, ImVec4{ 0.0f, 0.0f, 0.0f, 0.0f });
//    ImGui::Begin("DockSpaceWindow", nullptr, window_flags);
//    ImGui::PopStyleColor(); // MenuBarBg
//    ImGui::PopStyleVar(2);
//
//    ImGui::PopStyleVar(2);
//
//    {
//        ImGui::PushStyleColor(ImGuiCol_Border, IM_COL32(50, 50, 50, 255));
//        // Draw window border if the window is not maximized
//        //if (!isMaximized)
//        //    UI::RenderWindowOuterBorders(ImGui::GetCurrentWindow());
//
//        ImGui::PopStyleColor(); // ImGuiCol_Border
//    }
//
//    //if (m_Specification.CustomTitlebar)
//    //{
//        float titleBarHeight;
//        UI_DrawTitlebar(titleBarHeight);
//        ImGui::SetCursorPosY(titleBarHeight);
//
//    //}
//   
//    ImGui::End();
    
    ImGui::Begin("File Operations");
    
    //ImGui::SeparatorText("Load File");
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
    
    //ImGui::SeparatorText("Export File");
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

    //ImGui::PushItemFlag(ImGuiItemFlags_AllowOverlap, true);
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

 bool ImGuiAdapter::IsMaximized() const
 {
     return (bool)glfwGetWindowAttrib(m_window, GLFW_MAXIMIZED);
 }

 void ImGuiAdapter::UI_DrawTitlebar(float& outTitlebarHeight)
 {
     const float titlebarHeight = outTitlebarHeight;// 60.0f;
     const bool isMaximized = IsMaximized();
     float titlebarVerticalOffset = isMaximized ? -6.0f : 0.0f;
     const ImVec2 windowPadding = ImGui::GetCurrentWindow()->WindowPadding;

     ImGui::SetCursorPos(ImVec2(windowPadding.x, windowPadding.y + titlebarVerticalOffset));
     const ImVec2 titlebarMin = ImGui::GetCursorScreenPos();
     const ImVec2 titlebarMax = { ImGui::GetCursorScreenPos().x + ImGui::GetWindowWidth() - windowPadding.y * 2.0f,
                                  ImGui::GetCursorScreenPos().y + titlebarHeight };
     auto* bgDrawList = ImGui::GetBackgroundDrawList();
     auto* fgDrawList = ImGui::GetForegroundDrawList();
     bgDrawList->AddRectFilled(titlebarMin, titlebarMax, UI::Colors::Theme::titlebar);
     // DEBUG TITLEBAR BOUNDS
     //fgDrawList->AddRect(titlebarMin, titlebarMax, UI::Colors::Theme::invalidPrefab);

     // Logo
     {
         const int logoWidth =  48;// m_LogoTex->GetWidth();
         const int logoHeight = 48;// m_LogoTex->GetHeight();
         const ImVec2 logoOffset(16.0f + windowPadding.x, 5.0f + windowPadding.y + titlebarVerticalOffset);
         const ImVec2 logoRectStart = { ImGui::GetItemRectMin().x + logoOffset.x, ImGui::GetItemRectMin().y + logoOffset.y };
         const ImVec2 logoRectMax = { logoRectStart.x + logoWidth, logoRectStart.y + logoHeight };
         
         fgDrawList->AddImage(m_AppHeaderIcon->GetDescriptorSet(), logoRectStart, logoRectMax);
     }

     ImGui::BeginHorizontal("Titlebar", { ImGui::GetWindowWidth() - windowPadding.y * 2.0f, ImGui::GetFrameHeightWithSpacing() });

     static float moveOffsetX;
     static float moveOffsetY;
     const float w = ImGui::GetContentRegionAvail().x;
     const float buttonsAreaWidth = 94;

     // Title bar drag area
     // On Windows we hook into the GLFW win32 window internals
     ImGui::SetCursorPos(ImVec2(windowPadding.x, windowPadding.y + titlebarVerticalOffset)); // Reset cursor pos
     // DEBUG DRAG BOUNDS
     //fgDrawList->AddRect(ImGui::GetCursorScreenPos(), ImVec2(ImGui::GetCursorScreenPos().x + w - buttonsAreaWidth, ImGui::GetCursorScreenPos().y + titlebarHeight), UI::Colors::Theme::invalidPrefab);
     ImGui::InvisibleButton("##titleBarDragZone", ImVec2(w - buttonsAreaWidth, titlebarHeight));
    
     m_TitleBarHovered = ImGui::IsItemHovered();

     const bool dragZoneHovered = ImGui::IsItemHovered(ImGuiHoveredFlags_RectOnly);
     const bool dragZoneClicked = ImGui::IsItemClicked(ImGuiMouseButton_Left); // first press
     // or: const bool dragZoneActive = ImGui::IsItemActive();

#ifdef _WIN32
     if (dragZoneClicked)  // only begin a drag when click begins in the zone
     {
         HWND hwnd = glfwGetWin32Window(m_window);

         // Optional: ignore double-click if you use it for maximize/restore
         // if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) { ... }

         ReleaseCapture();
         SendMessage(hwnd, WM_NCLBUTTONDOWN, HTCAPTION, 0);
     }
#endif


     if (isMaximized)
     {
         float windowMousePosY = ImGui::GetMousePos().y - ImGui::GetCursorScreenPos().y;
         if (windowMousePosY >= 0.0f && windowMousePosY <= 5.0f)
             m_TitleBarHovered = true; // Account for the top-most pixels which don't register
     }

     // Draw Menubar
     // TODO DN menu callbacks
     /*if (m_MenubarCallback)
     {
         ImGui::SuspendLayout();
         {
             ImGui::SetItemAllowOverlap();
             const float logoHorizontalOffset = 16.0f * 2.0f + 48.0f + windowPadding.x;
             ImGui::SetCursorPos(ImVec2(logoHorizontalOffset, 6.0f + titlebarVerticalOffset));
             UI_DrawMenubar();

             if (ImGui::IsItemHovered())
                 m_TitleBarHovered = false;
         }

         ImGui::ResumeLayout();
     }*/

     {
         // Centered Window title
         ImVec2 currentCursorPos = ImGui::GetCursorPos();
         ImVec2 textSize = ImGui::CalcTextSize("m_Specification");
         ImGui::SetCursorPos(ImVec2(ImGui::GetWindowWidth() * 0.5f - textSize.x * 0.5f, 2.0f + windowPadding.y + 6.0f));
         ImGui::Text("%s", "m_Specification"); // Draw title
         ImGui::SetCursorPos(currentCursorPos);
     }

    


     // Window buttons
     const ImU32 buttonColN = UI::Colors::ColorWithMultipliedValue(UI::Colors::Theme::text, 0.9f);
     const ImU32 buttonColH = UI::Colors::ColorWithMultipliedValue(UI::Colors::Theme::text, 1.2f);
     const ImU32 buttonColP = UI::Colors::Theme::textDarker;
     const float buttonWidth = 14.0f;
     const float buttonHeight = 14.0f;

     //// Minimize Button

     ImGui::Spring();
     Walnut::UI::ShiftCursorY(8.0f);
     {
         const int iconWidth = m_IconMinimize->GetWidth();
         const int iconHeight = m_IconMinimize->GetHeight();
         const float padY = (buttonHeight - (float)iconHeight) / 2.0f;
         if (ImGui::InvisibleButton("Minimize", ImVec2(buttonWidth, buttonHeight)))
         {
             // TODO: move this stuff to a better place, like Window class
             if (m_window)
             {
                 glfwIconifyWindow(m_window);
                 // we need to send the event so that Application knows its minimizing.
                 //   // Application::Get().QueueEvent([windowHandle = m_Window]() { glfwIconifyWindow(windowHandle); });
             }
         }

         Walnut::UI::DrawButtonImage(m_IconMinimize, buttonColN, buttonColH, buttonColP, Walnut::UI::RectExpanded(Walnut::UI::GetItemRect(), 0.0f, -padY));
     }


     //// Maximize Button
     ImGui::Spring(-1.0f, 17.0f);
     Walnut::UI::ShiftCursorY(8.0f);
     {
         const int iconWidth = m_IconMaximize->GetWidth();
         const int iconHeight = m_IconMaximize->GetHeight();

         const bool isMaximized = IsMaximized();

         if (ImGui::InvisibleButton("Maximize", ImVec2(buttonWidth, buttonHeight)))
         {

             if (isMaximized)
                 glfwRestoreWindow(m_window);
             else
                 glfwMaximizeWindow(m_window);

             // TOO DN add event queue
            /* Application::Get().QueueEvent([isMaximized, windowHandle = m_WindowHandle]()
                 {
                     if (isMaximized)
                         glfwRestoreWindow(windowHandle);
                     else
                         glfwMaximizeWindow(windowHandle);
                 });*/
         }

         Walnut::UI::DrawButtonImage(isMaximized ? m_IconRestore : m_IconMaximize, buttonColN, buttonColH, buttonColP);
     }

     // Close Button
     ImGui::Spring(-1.0f, 15.0f);
     Walnut::UI::ShiftCursorY(8.0f);
     {
         const int iconWidth = m_IconClose->GetWidth();
         const int iconHeight = m_IconClose->GetHeight();
         if (ImGui::InvisibleButton("Close", ImVec2(buttonWidth, buttonHeight)))
         {
             glfwSetWindowShouldClose(m_window, GLFW_TRUE);
             // TODO DN send the event to the application
            //Application::Get().Close();
         }
         Walnut::UI::DrawButtonImage(m_IconClose, UI::Colors::Theme::text, UI::Colors::ColorWithMultipliedValue(UI::Colors::Theme::text, 1.4f), buttonColP);
     }

     ImGui::Spring(-1.0f, 18.0f);
     ImGui::EndHorizontal();

     {
        // after you've computed titlebarMin/titlebarMax, w, buttonsAreaWidth etc.
        
        const float buttonsAreaWidth = 94.0f; //titlebarMin/titlebarMax/titlebarClose
        const float w = ImGui::GetWindowWidth() - windowPadding.y * 2.0f;
        const float dragWidth = w-buttonsAreaWidth;

        const float panelHeight = buttonHeight+buttonHeight + 90.0f;
        ImVec2 panelPos = ImVec2(titlebarMin.x, titlebarMax.y);
        ImVec2 panelSize = ImVec2(w, panelHeight);

        //Make TitlebarToolsOverlay use this size and pos
        ImGui::SetNextWindowPos(panelPos);
        ImGui::SetNextWindowSize(panelSize);

        // optional: keep it above other stuff
        ImGui::SetNextWindowViewport(ImGui::GetWindowViewport()->ID);

        ImGuiWindowFlags flags =
            ImGuiWindowFlags_NoDecoration |
            ImGuiWindowFlags_NoDocking |
            ImGuiWindowFlags_NoMove |
            ImGuiWindowFlags_NoSavedSettings |
            ImGuiWindowFlags_NoScrollbar |
            ImGuiWindowFlags_NoScrollWithMouse |
            ImGuiWindowFlags_NoFocusOnAppearing |
            ImGuiWindowFlags_NoNav;

        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(10, 10));
        // if you want it to match titlebar:
        ImGui::PushStyleColor(ImGuiCol_WindowBg, UI::Colors::Theme::titlebar);

        ImGui::Begin("##TitlebarToolsOverlay", nullptr, flags);
               
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


        const float buttonWidth = 120.0f;
        const float buttonHeight = 36.0f;

        ImVec2 pos;
        pos.y = ImGui::GetCursorScreenPos().y;

        // Center horizontally
        pos.x = (ImGui::GetWindowWidth() - buttonWidth) * 0.5f;
        if (TrapeziumButtonTrueHit("Home", pos, ImVec2(120, 36), 18.0f, false)) { /* tool = dimension */ }
       
        int count = 5;
        float totalWidth = count * buttonWidth;
        float startX = (ImGui::GetWindowWidth() - totalWidth) * 0.5f + 18;
        pos.y = ImGui::GetCursorScreenPos().y; // Cursor changed as we have aded a button
        pos.x = startX;

        if (TrapeziumButtonTrueHit("Reset", pos, ImVec2(120, 36), 18.0f, false)) { /* tool = dimension */ }
        pos.x += buttonWidth;
        if (ParallelogramButtonTrueHit("Move", pos, ImVec2(120, 36), -18.0f)) { /* tool = line */ }
        pos.x += buttonWidth - 18;
        //ImGui::SameLine(120,0);
        if (TrapeziumButtonTrueHit("Scale", pos, ImVec2(120, 36), 18.0f, true)) { /* tool = dimension */ }
        pos.x += buttonWidth - 18;
        //ImGui::SameLine(120,0);
        if (ParallelogramButtonTrueHit("Undo", pos, ImVec2(120, 36), 18.0f)) { /* tool = line */ }
        pos.x += buttonWidth;;
        if (TrapeziumButtonTrueHit("Redo", pos, ImVec2(120, 36), 18.0f, false)) { /* tool = dimension */ }

        ImGui::End();

        ImGui::PopStyleColor();
        ImGui::PopStyleVar(2);

        outTitlebarHeight = titlebarHeight + panelHeight;
     }
    
 }

} // namespace adapters