#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

// IMPORTANT: we use glad as the OpenGL loader.
// Do NOT include <GL/gl.h> anywhere in the program, otherwise glad will
// trigger "OpenGL header already included".
#include <glad/glad.h>

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#include "adapters/ui/ImGuiAdapter.h"
#include "adapters/rendering/GlSketch3DViewRenderer.h"
#include "core/Application.h"

#include <imgui.h>
#include <imgui_internal.h>

#include <cstring>
#include <algorithm>
#include <cmath>
#include <functional>
#include <glm/glm.hpp>

#include <nlohmann/json.hpp>

#include "../Roboto-Regular.embed"
#include "../../../Walnut-Icon.embed"
#include "../../../WindowImages.embed"

// stb_image implementation must live in exactly one compilation unit.
// We keep the implementation in imgui/StbImageImpl.cpp (linked into imgui.dll).
#include "stb_image.h"

// -----------------------------------------------------------------------------
// OpenGL function loader for the UI plugin.
//
// IMPORTANT: The UI plugin is a separate DLL. If it links to glad, it has its own
// glad function-pointer table and MUST load them itself.
// We intentionally do NOT call any GLFW functions here (to avoid having a second
// copy of GLFW state inside the plugin DLL).
// -----------------------------------------------------------------------------
static void* GetOpenGLProcAddress(const char* name)
{
    // Try WGL first (requires a current context).
    void* p = (void*)wglGetProcAddress(name);

    // wglGetProcAddress returns small sentinel values on failure.
    if (p == nullptr || p == (void*)0x1 || p == (void*)0x2 || p == (void*)0x3 || p == (void*)-1)
    {
        static HMODULE s_opengl32 = ::GetModuleHandleA("opengl32.dll");
        if (!s_opengl32)
            s_opengl32 = ::LoadLibraryA("opengl32.dll");
        if (s_opengl32)
            p = (void*)::GetProcAddress(s_opengl32, name);
    }
    return p;
}

static bool EnsureGladLoaded()
{
    static bool s_loaded = false;
    if (s_loaded)
        return true;

    if (!gladLoadGLLoader((GLADloadproc)GetOpenGLProcAddress))
        return false;

    s_loaded = true;
    return true;
}

static GLuint CreateGLTextureRGBA_Minimal(const unsigned char* rgba, int w, int h)
{
    if (!EnsureGladLoaded())
        return 0;
    GLuint tex = 0;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    // Avoid enums missing in your build:
    // - no GL_TEXTURE_WRAP_S/T
    // - no GL_CLAMP_TO_EDGE
    // - no GL_UNPACK_ALIGNMENT
    // - no GL_RGBA8

    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, rgba);

    glBindTexture(GL_TEXTURE_2D, 0);
    return tex;
}



static bool CreateTextureFromEmbeddedPng(
    const unsigned char* bytes,
    int bytesSize,
    GLuint& outTex,
    ImTextureID& outId,
    ImVec2& outSize)
{
    int w = 0, h = 0, comp = 0;

    // Force RGBA output
    stbi_uc* data = stbi_load_from_memory(bytes, bytesSize, &w, &h, &comp, 4);
    if (!data || w <= 0 || h <= 0)
        return false;

    // Minimal upload (avoids GL_CLAMP_TO_EDGE / GL_RGBA8 / GL_UNPACK_ALIGNMENT)
    outTex = CreateGLTextureRGBA_Minimal(data, w, h);

    stbi_image_free(data);

    // ImGui OpenGL convention: ImTextureID is the GLuint cast to void*
    outId = (ImTextureID)(intptr_t)outTex;
    outSize = ImVec2((float)w, (float)h);
    return outTex != 0;
}




// --- small helpers (avoid Windows min/max macros) ---
static inline float maxf(float a, float b) { return (a > b) ? a : b; }
static inline float minf(float a, float b) { return (a < b) ? a : b; }
static inline float clampf(float v, float lo, float hi) { return (v < lo) ? lo : (v > hi) ? hi : v; }
namespace adapters {

    // ------------------------------------------------------------
    // ctor / dtor
    // ------------------------------------------------------------

    ImGuiAdapter::ImGuiAdapter(core::Application* app, GLFWwindow* hostWindow, IGuiHost* host, ports::IConfigPort* config)
        : m_app(app), m_window(hostWindow), m_host(host), m_config(config)
    {
        // Pull persisted view state from config (if available)
        auto readBool = [&](const char* ns, const char* key, bool defVal) -> bool {
            if (!m_config) return defVal;
            nlohmann::json v = m_config->get(ns, key);
            return v.is_boolean() ? v.get<bool>() : defVal;
            };

        m_viewFileOperations = readBool("pistachio.ui", "views.fileOperations", true);
        m_viewStatus = readBool("pistachio.ui", "views.status", true);
        m_viewModelInfo = readBool("pistachio.ui", "views.modelInfo", true);
        m_view3DViewport = readBool("pistachio.ui", "views.viewport3d", true);
        m_viewSketchEditor = readBool("pistachio.ui", "views.sketchEditor", true);
        m_viewSketch3DViewport = readBool("pistachio.ui", "views.sketch3dViewport", true);

        // Phase 2: dedicated renderer for the Sketch 3D View (kept separate from the app\'s primary renderer)
        m_sketch3dRenderer = std::make_unique<adapters::GlSketch3DViewRenderer>();
    }

    ImGuiAdapter::~ImGuiAdapter() = default;

    // ------------------------------------------------------------
    // SAFE ONE-TIME RESOURCE INIT (fonts etc)
    // ------------------------------------------------------------

    void ImGuiAdapter::initializeResources()
    {
        if (m_resourcesInitialized)
            return;

        ImGuiIO& io = ImGui::GetIO();

        ImFontConfig cfg;
        cfg.FontDataOwnedByAtlas = false;

        // Default font
        if (!io.FontDefault)
        {
            m_bodyFont = io.FontDefault = io.Fonts->AddFontFromMemoryTTF(
                (void*)g_RobotoRegular,
                sizeof(g_RobotoRegular),
                17.0f,
                &cfg
            );
        }

        // Small UI font
        if (!m_smallFont)
        {
            m_smallFont = io.Fonts->AddFontFromMemoryTTF(
                (void*)g_RobotoRegular,
                sizeof(g_RobotoRegular),
                14.0f,
                &cfg
            );
        }

        // Group header font (UE-style section header)
        if (!m_groupFont)
        {
            m_groupFont = io.Fonts->AddFontFromMemoryTTF(
                (void*)g_RobotoRegular,
                sizeof(g_RobotoRegular),
                19.0f,
                &cfg
            );
        }

        // Title font (UE-style page title)
        if (!m_titleFont)
        {
            m_titleFont = io.Fonts->AddFontFromMemoryTTF(
                (void*)g_RobotoRegular,
                sizeof(g_RobotoRegular),
                24.0f,
                &cfg
            );
        }



        ImVec2 szMin, szMax, szRes, szClose;

        bool ok1 = CreateTextureFromEmbeddedPng(g_WindowMinimizeIcon, (int)sizeof(g_WindowMinimizeIcon), m_glTexMinimize, m_iconMinimize, szMin);
        bool ok2 = CreateTextureFromEmbeddedPng(g_WindowMaximizeIcon, (int)sizeof(g_WindowMaximizeIcon), m_glTexMaximize, m_iconMaximize, szMax);
        bool ok3 = CreateTextureFromEmbeddedPng(g_WindowRestoreIcon, (int)sizeof(g_WindowRestoreIcon), m_glTexRestore, m_iconRestore, szRes);
        bool ok4 = CreateTextureFromEmbeddedPng(g_WindowCloseIcon, (int)sizeof(g_WindowCloseIcon), m_glTexClose, m_iconClose, szClose);

        if (!(ok1 && ok2 && ok3 && ok4))
            return;

        // Pick a consistent button icon size (you can scale in draw code too)
        ImVec2 m_iconSize = ImVec2(16, 16);

        // Push to host

        setWindowControlIcons(
            m_iconMinimize,
            m_iconMaximize,
            m_iconRestore,
            m_iconClose,
            m_iconSize
        );



        m_resourcesInitialized = true;

        // Register sketch tools (2D editor)
        if (!m_toolingInitialized) {
            m_toolManager.Register(std::make_unique<adapters::sketchui::Line2PtTool>());
            m_toolManager.Register(std::make_unique<adapters::sketchui::CircleCenterRadiusTool>());
            m_toolManager.Register(std::make_unique<adapters::sketchui::ConstraintTool>());
            m_toolingInitialized = true;
        }

    }


    void ImGuiAdapter::setWindowControlIcons(
        ImTextureID minimize,
        ImTextureID maximize,
        ImTextureID restore,
        ImTextureID close,
        ImVec2 size
    )
    {
        if (m_host) {
            m_host->setWindowControlIcons(minimize, maximize, restore, close, size);
        }
    }

    // ------------------------------------------------------------
    // FRAME RENDER (NO FONT MUTATION HERE)
    // ------------------------------------------------------------

    void ImGuiAdapter::render()
    {
        // ⚠️ DO NOT touch ImGuiIO.Fonts here

        // Global keyboard shortcuts
        ImGuiIO& io = ImGui::GetIO();
        if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_S)) {
            // Ctrl+S: Save sketch document
            if (m_app && m_app->getSketchDocument()) {
                m_app->saveSketchDocument("test.pistachio.json");
            }
        }

        // Sync view visibility every frame so the "Views" menu (which updates the
        // shared config store) can re-open windows after the user closes them via
        // the window close button (X).
        auto readBool = [&](const char* ns, const char* key, bool defVal) -> bool {
            if (!m_config) return defVal;
            nlohmann::json v = m_config->get(ns, key);
            return v.is_boolean() ? v.get<bool>() : defVal;
            };

        m_viewFileOperations = readBool("pistachio.ui", "views.fileOperations", m_viewFileOperations);
        m_viewStatus = readBool("pistachio.ui", "views.status", m_viewStatus);
        m_viewModelInfo = readBool("pistachio.ui", "views.modelInfo", m_viewModelInfo);
        m_view3DViewport = readBool("pistachio.ui", "views.viewport3d", m_view3DViewport);
        m_viewSketch3DViewport = readBool("pistachio.ui", "views.sketch3dViewport", m_viewSketch3DViewport);
        m_viewSketchEditor = readBool("pistachio.ui", "views.sketchEditor", m_viewSketchEditor);

        renderMainMenu();
        renderStatusBar();
        renderModelInfo();
        render3DView();
        renderSketch3DView();
        renderSketchEditor();
    }

    void ImGuiAdapter::renderSketch3DView()
    {
        if (!m_viewSketch3DViewport)
            return;

        bool wasOpen = m_viewSketch3DViewport;
        ImGui::Begin("Sketch 3D View", &m_viewSketch3DViewport, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);

        // Phase 3: choose which orthogonal plane is considered "active" for sketch overlays.
        {
            int plane = (int)m_sketch3dActivePlane;
            ImGui::TextUnformatted("Active sketch plane:");
            ImGui::SameLine();

            // Highlight the hovered plane button
            if (m_sketch3dHasHoverPlane && m_sketch3dHoverPlane == Sketch3DPlane::XY && plane != 0)
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.24f, 0.78f, 1.0f, 1.0f));
            if (ImGui::RadioButton("XY", plane == 0)) plane = 0;
            if (m_sketch3dHasHoverPlane && m_sketch3dHoverPlane == Sketch3DPlane::XY && plane != 0)
                ImGui::PopStyleColor();

            ImGui::SameLine();

            if (m_sketch3dHasHoverPlane && m_sketch3dHoverPlane == Sketch3DPlane::YZ && plane != 1)
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.71f, 0.24f, 1.0f));
            if (ImGui::RadioButton("YZ", plane == 1)) plane = 1;
            if (m_sketch3dHasHoverPlane && m_sketch3dHoverPlane == Sketch3DPlane::YZ && plane != 1)
                ImGui::PopStyleColor();

            ImGui::SameLine();

            if (m_sketch3dHasHoverPlane && m_sketch3dHoverPlane == Sketch3DPlane::XZ && plane != 2)
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.63f, 1.0f, 0.35f, 1.0f));
            if (ImGui::RadioButton("XZ", plane == 2)) plane = 2;
            if (m_sketch3dHasHoverPlane && m_sketch3dHoverPlane == Sketch3DPlane::XZ && plane != 2)
                ImGui::PopStyleColor();

            m_sketch3dActivePlane = (Sketch3DPlane)plane;

            ImGui::SameLine();
            ImGui::Dummy(ImVec2(12, 0));
            ImGui::SameLine();
            ImGui::Checkbox("Show other planes (faint)", &m_sketch3dShowOtherPlanes);

            // Show legend when hovering
            if (m_sketch3dHasHoverPlane && m_hasHoverRayData)
            {
                ImGui::SameLine();
                ImGui::Dummy(ImVec2(20, 0));
                ImGui::SameLine();
                ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.4f, 1.0f), "●");
                ImGui::SameLine();
                ImGui::TextDisabled("Hovering: Cube face + Ray + Hit marker + Detected line");
            }

            ImGui::Separator();
        }


        if (m_sketch3dRenderer)
        {
            ImVec2 size = ImGui::GetContentRegionAvail();

            if (size.x > 0 && size.y > 0)
            {
                const uint32_t w = (uint32_t)ImMax(1.0f, size.x);
                const uint32_t h = (uint32_t)ImMax(1.0f, size.y);

                // Phase 2: build a renderer-agnostic overlay scene from the current sketch document.
                // This does NOT affect the existing cube renderer/view.
                {
                    ports::RenderScene scene;

                    // Collect world-space segments for click picking (rebuilt every frame).
                    m_sketch3dSegments.clear();

                    auto packRgba = [](uint8_t r, uint8_t g, uint8_t b, uint8_t a) -> uint32_t {
                        return (uint32_t(r) << 24) | (uint32_t(g) << 16) | (uint32_t(b) << 8) | uint32_t(a);
                        };

                    auto addGrid = [&](const glm::vec3& origin,
                        const glm::vec3& u,
                        const glm::vec3& v,
                        float spacing,
                        int linesPerSide,
                        uint32_t color,
                        float depthBias,
                        const glm::vec3& normal)
                        {
                            ports::RenderPrimitive prim;
                            prim.kind = ports::RenderPrimitiveKind::LineList;
                            prim.rgba = color;
                            prim.size = 1.0f;
                            prim.depthTest = true;

                            const float half = float(linesPerSide) * spacing;
                            const glm::vec3 O = origin + normal * depthBias;

                            for (int i = -linesPerSide; i <= linesPerSide; ++i)
                            {
                                const float t = float(i) * spacing;

                                // Line parallel to u axis (varying v)
                                prim.positions.push_back(O + u * (-half) + v * t);
                                prim.positions.push_back(O + u * (half)+v * t);

                                // Line parallel to v axis (varying u)
                                prim.positions.push_back(O + u * t + v * (-half));
                                prim.positions.push_back(O + u * t + v * (half));
                            }

                            scene.primitives.push_back(std::move(prim));
                        };

                    // 3 work planes (XY, YZ, XZ) with gentle depth bias to avoid z-fighting.
                    const float spacing = 10.0f;
                    const int lines = 20;
                    const float bias = 0.25f;
                    addGrid(glm::vec3(0, 0, 0), glm::vec3(1, 0, 0), glm::vec3(0, 1, 0), spacing, lines, packRgba(90, 90, 95, 255), bias, glm::vec3(0, 0, 1)); // XY
                    addGrid(glm::vec3(0, 0, 0), glm::vec3(0, 1, 0), glm::vec3(0, 0, 1), spacing, lines, packRgba(75, 80, 90, 255), bias, glm::vec3(1, 0, 0)); // YZ
                    addGrid(glm::vec3(0, 0, 0), glm::vec3(1, 0, 0), glm::vec3(0, 0, 1), spacing, lines, packRgba(80, 75, 90, 255), bias, glm::vec3(0, 1, 0)); // XZ

                    // Helper to draw a bright plane outline rectangle for hover feedback
                    auto addPlaneOutline = [&](const glm::vec3& origin,
                        const glm::vec3& u,
                        const glm::vec3& v,
                        float size,
                        uint32_t color,
                        float depthBias,
                        const glm::vec3& normal)
                        {
                            // Draw outer glow (thicker, slightly transparent)
                            {
                                ports::RenderPrimitive glow;
                                glow.kind = ports::RenderPrimitiveKind::LineList;
                                // Extract RGB and reduce alpha for glow
                                uint8_t r = (color >> 24) & 0xFF;
                                uint8_t g = (color >> 16) & 0xFF;
                                uint8_t b = (color >> 8) & 0xFF;
                                glow.rgba = packRgba(r, g, b, 200);  // More opaque
                                glow.size = 15.0f;  // VERY thick outer glow (was 8px)
                                glow.depthTest = false;  // Draw on top so it's always visible

                                const float half = size;
                                const glm::vec3 O = origin + normal * (depthBias + 0.1f);

                                // Draw rectangle outline (4 edges)
                                glm::vec3 corner1 = O + u * (-half) + v * (-half);
                                glm::vec3 corner2 = O + u * (half)+v * (-half);
                                glm::vec3 corner3 = O + u * (half)+v * (half);
                                glm::vec3 corner4 = O + u * (-half) + v * (half);

                                glow.positions.push_back(corner1); glow.positions.push_back(corner2);
                                glow.positions.push_back(corner2); glow.positions.push_back(corner3);
                                glow.positions.push_back(corner3); glow.positions.push_back(corner4);
                                glow.positions.push_back(corner4); glow.positions.push_back(corner1);

                                scene.primitives.push_back(std::move(glow));
                            }

                            // Draw inner solid outline (bright and sharp)
                            {
                                ports::RenderPrimitive outline;
                                outline.kind = ports::RenderPrimitiveKind::LineList;
                                outline.rgba = color;
                                outline.size = 10.0f;  // VERY thick, ultra-visible outline (was 5px)
                                outline.depthTest = false;  // Draw on top so it's always visible

                                const float half = size;
                                const glm::vec3 O = origin + normal * (depthBias + 0.2f);

                                // Draw rectangle outline (4 edges)
                                glm::vec3 corner1 = O + u * (-half) + v * (-half);
                                glm::vec3 corner2 = O + u * (half)+v * (-half);
                                glm::vec3 corner3 = O + u * (half)+v * (half);
                                glm::vec3 corner4 = O + u * (-half) + v * (half);

                                outline.positions.push_back(corner1); outline.positions.push_back(corner2);
                                outline.positions.push_back(corner2); outline.positions.push_back(corner3);
                                outline.positions.push_back(corner3); outline.positions.push_back(corner4);
                                outline.positions.push_back(corner4); outline.positions.push_back(corner1);

                                scene.primitives.push_back(std::move(outline));
                            }
                        };

                    // Helper to draw a cube face perpendicular to the hovered plane
                    auto addCubeFaceOutline = [&](const glm::vec3& origin,  // NEW: add origin!
                        const glm::vec3& u,
                        const glm::vec3& v,
                        const glm::vec3& normal,
                        float cubeSize,
                        uint32_t color)
                        {
                            // Draw a bright highlighted square showing the cube face
                            // This face is perpendicular to the sketch plane

                            const float half = cubeSize / 2.0f;

                            // Calculate the center of the cube face on the positive side of the normal
                            glm::vec3 center = origin + normal * half;  // Use origin!

                            // Create the four corners of the cube face
                            glm::vec3 corner1 = center + u * (-half) + v * (-half);
                            glm::vec3 corner2 = center + u * (half)+v * (-half);
                            glm::vec3 corner3 = center + u * (half)+v * (half);
                            glm::vec3 corner4 = center + u * (-half) + v * (half);

                            // Draw outer glow
                            {
                                ports::RenderPrimitive glow;
                                glow.kind = ports::RenderPrimitiveKind::LineList;
                                uint8_t r = (color >> 24) & 0xFF;
                                uint8_t g = (color >> 16) & 0xFF;
                                uint8_t b = (color >> 8) & 0xFF;
                                glow.rgba = packRgba(r, g, b, 220);  // More opaque
                                glow.size = 12.0f;  // Very thick (was 6px)
                                glow.depthTest = false;

                                glow.positions.push_back(corner1); glow.positions.push_back(corner2);
                                glow.positions.push_back(corner2); glow.positions.push_back(corner3);
                                glow.positions.push_back(corner3); glow.positions.push_back(corner4);
                                glow.positions.push_back(corner4); glow.positions.push_back(corner1);

                                scene.primitives.push_back(std::move(glow));
                            }

                            // Draw solid outline
                            {
                                ports::RenderPrimitive outline;
                                outline.kind = ports::RenderPrimitiveKind::LineList;
                                outline.rgba = color;
                                outline.size = 8.0f;  // Thicker (was 4px)
                                outline.depthTest = false;

                                outline.positions.push_back(corner1); outline.positions.push_back(corner2);
                                outline.positions.push_back(corner2); outline.positions.push_back(corner3);
                                outline.positions.push_back(corner3); outline.positions.push_back(corner4);
                                outline.positions.push_back(corner4); outline.positions.push_back(corner1);

                                scene.primitives.push_back(std::move(outline));
                            }

                            // Draw diagonal to make it extra obvious it's a face
                            {
                                ports::RenderPrimitive diag;
                                diag.kind = ports::RenderPrimitiveKind::LineList;
                                uint8_t r = (color >> 24) & 0xFF;
                                uint8_t g = (color >> 16) & 0xFF;
                                uint8_t b = (color >> 8) & 0xFF;
                                diag.rgba = packRgba(r, g, b, 180);  // More opaque
                                diag.size = 5.0f;  // Thicker (was 2.5px)
                                diag.depthTest = false;

                                diag.positions.push_back(corner1); diag.positions.push_back(corner3);
                                diag.positions.push_back(corner2); diag.positions.push_back(corner4);

                                scene.primitives.push_back(std::move(diag));
                            }
                        };

                    // Sketch overlays: for now, we project ALL sketch line entities onto all three planes.
                    // NOTE: We intentionally do NOT gate on any 'visible' flags in Phase 2, because older
                    // documents / models may default these to false, which would look like "nothing renders".
                    int totalLineSegments = 0;
                    bool hasDoc = false;
                    if (m_app)
                    {
                        auto doc = m_app->getSketchDocument();
                        if (doc)
                        {
                            hasDoc = true;
                            auto addSketchLinesOnPlane = [&](Sketch3DPlane plane,
                                const glm::vec3& normal,
                                uint32_t color,
                                const std::function<glm::vec3(const domain::sketch::Vec2&)>& toWorld)
                                {
                                    ports::RenderPrimitive l;
                                    l.kind = ports::RenderPrimitiveKind::LineList;
                                    l.rgba = color;

                                    // Make hovered plane lines thicker and more visible
                                    if (m_sketch3dHasHoverPlane && plane == m_sketch3dHoverPlane && plane != m_sketch3dActivePlane)
                                        l.size = 3.5f;  // Thick lines for hover feedback
                                    else if (plane == m_sketch3dActivePlane)
                                        l.size = 2.5f;  // Medium-thick for active plane
                                    else
                                        l.size = 2.0f;  // Normal thickness

                                    l.depthTest = true;

                                    // Convert adapters::Sketch3DPlane to domain::sketch::SketchPlane
                                    domain::sketch::SketchPlane domainPlane = static_cast<domain::sketch::SketchPlane>(static_cast<int>(plane));

                                    for (const auto& sk : doc->sketches)
                                    {
                                        // ONLY render sketches that belong to this plane
                                        if (sk.plane != domainPlane)
                                            continue;

                                        for (const auto& ln : sk.entities.lines())
                                        {
                                            glm::vec3 A = toWorld(ln.a) + normal * 0.35f;
                                            glm::vec3 B = toWorld(ln.b) + normal * 0.35f;
                                            l.positions.push_back(A);
                                            l.positions.push_back(B);

                                            // Register ALL segments on their proper plane for picking
                                            m_sketch3dSegments.push_back(ImGuiAdapter::Sketch3DSegment{ plane, A, B });
                                            ++totalLineSegments;
                                        }
                                    }

                                    if (!l.positions.empty())
                                        scene.primitives.push_back(std::move(l));
                                };

                            // Phase 3: draw the sketch onto the active plane (optionally show the other planes faint).
                            const bool showOthers = m_sketch3dShowOtherPlanes;

                            auto alphaFor = [&](Sketch3DPlane p) -> uint8_t {
                                // Base: active plane fully visible. Other planes optionally visible.
                                int a = (p == m_sketch3dActivePlane) ? 255 : (showOthers ? 80 : 0);

                                // Hover feedback: if the mouse is near a segment on a plane, boost that plane's opacity
                                // and significantly fade the others so the user can clearly see which plane they're interacting with.
                                if (m_sketch3dHasHoverPlane && showOthers)
                                {
                                    if (p == m_sketch3dHoverPlane)
                                        a = (p == m_sketch3dActivePlane) ? 255 : 240;  // Very bright for hover
                                    else
                                        a = std::min(a, 40);  // Fade others more
                                }
                                return a;
                                };

                            // XY: (u,v)->(x,y)
                            if (uint8_t a = alphaFor(Sketch3DPlane::XY); a != 0)
                                addSketchLinesOnPlane(Sketch3DPlane::XY, glm::vec3(0, 0, 1), packRgba(60, 200, 255, a),
                                    [](const domain::sketch::Vec2& p) { return glm::vec3((float)p.x, (float)p.y, 0.0f); });

                            // YZ: (u,v)->(y,z)
                            if (uint8_t a = alphaFor(Sketch3DPlane::YZ); a != 0)
                                addSketchLinesOnPlane(Sketch3DPlane::YZ, glm::vec3(1, 0, 0), packRgba(255, 180, 60, a),
                                    [](const domain::sketch::Vec2& p) { return glm::vec3(0.0f, (float)p.x, (float)p.y); });

                            // XZ: (u,v)->(x,z)
                            if (uint8_t a = alphaFor(Sketch3DPlane::XZ); a != 0)
                                addSketchLinesOnPlane(Sketch3DPlane::XZ, glm::vec3(0, 1, 0), packRgba(160, 255, 90, a),
                                    [](const domain::sketch::Vec2& p) { return glm::vec3((float)p.x, 0.0f, (float)p.y); });

                            // Draw bright, fully visible plane outline and cube face when hovering ANY plane
                            if (m_sketch3dHasHoverPlane && m_hasHoverRayData)
                            {
                                const float outlineSize = spacing * lines;  // Same size as the grid (200 units)
                                const float outlineBias = 0.5f;  // Slightly forward of grid
                                const float cubeSize = 80.0f;  // Larger cube face for better visibility (was 30)

                                // Calculate the center of the detected segment
                                glm::vec3 segmentCenter = (m_hoverSegmentA + m_hoverSegmentB) * 0.5f;

                                switch (m_sketch3dHoverPlane)
                                {
                                case Sketch3DPlane::XY:
                                    // Draw VERY thick plane outline at segment position
                                    addPlaneOutline(segmentCenter, glm::vec3(1, 0, 0), glm::vec3(0, 1, 0),
                                        outlineSize, packRgba(60, 200, 255, 255), outlineBias, glm::vec3(0, 0, 1));
                                    // Add LARGE cube face perpendicular to XY plane (face on Z axis) at segment
                                    addCubeFaceOutline(segmentCenter, glm::vec3(1, 0, 0), glm::vec3(0, 1, 0), glm::vec3(0, 0, 1),
                                        cubeSize, packRgba(60, 200, 255, 255));
                                    // Add diagonal cross lines for extra visibility
                                    {
                                        ports::RenderPrimitive cross;
                                        cross.kind = ports::RenderPrimitiveKind::LineList;
                                        cross.rgba = packRgba(60, 200, 255, 180);
                                        cross.size = 3.0f;
                                        cross.depthTest = false;
                                        glm::vec3 offset = glm::vec3(0, 0, 1) * (outlineBias + 0.3f);
                                        cross.positions.push_back(glm::vec3(-outlineSize, -outlineSize, 0) + offset);
                                        cross.positions.push_back(glm::vec3(outlineSize, outlineSize, 0) + offset);
                                        cross.positions.push_back(glm::vec3(outlineSize, -outlineSize, 0) + offset);
                                        cross.positions.push_back(glm::vec3(-outlineSize, outlineSize, 0) + offset);
                                        scene.primitives.push_back(std::move(cross));
                                    }
                                    break;
                                case Sketch3DPlane::YZ:
                                    // Draw VERY thick plane outline at segment position
                                    addPlaneOutline(segmentCenter, glm::vec3(0, 1, 0), glm::vec3(0, 0, 1),
                                        outlineSize, packRgba(255, 180, 60, 255), outlineBias, glm::vec3(1, 0, 0));
                                    // Add LARGE cube face perpendicular to YZ plane (face on X axis) at segment
                                    addCubeFaceOutline(segmentCenter, glm::vec3(0, 1, 0), glm::vec3(0, 0, 1), glm::vec3(1, 0, 0),
                                        cubeSize, packRgba(255, 180, 60, 255));
                                    // Add diagonal cross lines
                                    {
                                        ports::RenderPrimitive cross;
                                        cross.kind = ports::RenderPrimitiveKind::LineList;
                                        cross.rgba = packRgba(255, 180, 60, 180);
                                        cross.size = 3.0f;
                                        cross.depthTest = false;
                                        glm::vec3 offset = glm::vec3(1, 0, 0) * (outlineBias + 0.3f);
                                        cross.positions.push_back(glm::vec3(0, -outlineSize, -outlineSize) + offset);
                                        cross.positions.push_back(glm::vec3(0, outlineSize, outlineSize) + offset);
                                        cross.positions.push_back(glm::vec3(0, outlineSize, -outlineSize) + offset);
                                        cross.positions.push_back(glm::vec3(0, -outlineSize, outlineSize) + offset);
                                        scene.primitives.push_back(std::move(cross));
                                    }
                                    break;
                                case Sketch3DPlane::XZ:
                                    // Draw VERY thick plane outline at segment position
                                    addPlaneOutline(segmentCenter, glm::vec3(1, 0, 0), glm::vec3(0, 0, 1),
                                        outlineSize, packRgba(160, 255, 90, 255), outlineBias, glm::vec3(0, 1, 0));
                                    // Add LARGE cube face perpendicular to XZ plane (face on Y axis) at segment
                                    addCubeFaceOutline(segmentCenter, glm::vec3(1, 0, 0), glm::vec3(0, 0, 1), glm::vec3(0, 1, 0),
                                        cubeSize, packRgba(160, 255, 90, 255));
                                    // Add diagonal cross lines
                                    {
                                        ports::RenderPrimitive cross;
                                        cross.kind = ports::RenderPrimitiveKind::LineList;
                                        cross.rgba = packRgba(160, 255, 90, 180);
                                        cross.size = 3.0f;
                                        cross.depthTest = false;
                                        glm::vec3 offset = glm::vec3(0, 1, 0) * (outlineBias + 0.3f);
                                        cross.positions.push_back(glm::vec3(-outlineSize, 0, -outlineSize) + offset);
                                        cross.positions.push_back(glm::vec3(outlineSize, 0, outlineSize) + offset);
                                        cross.positions.push_back(glm::vec3(outlineSize, 0, -outlineSize) + offset);
                                        cross.positions.push_back(glm::vec3(-outlineSize, 0, outlineSize) + offset);
                                        scene.primitives.push_back(std::move(cross));
                                    }
                                    break;
                                }
                            }
                        }
                    }

                    // Give the user a clear hint about why they might not see sketch geometry yet.
                    if (!hasDoc)
                        ImGui::TextDisabled("No sketch document loaded (showing grids only)");
                    else if (totalLineSegments == 0)
                        ImGui::TextDisabled("No sketch lines found (showing grids only)");

                    // Visualize the ray that selected the plane - shows WHY a plane was chosen
                    if (m_hasHoverRayData)
                    {
                        // Determine color based on which plane is hovered
                        uint32_t rayColor = packRgba(255, 255, 255, 255);
                        switch (m_sketch3dHoverPlane)
                        {
                        case Sketch3DPlane::XY: rayColor = packRgba(60, 200, 255, 255); break;
                        case Sketch3DPlane::YZ: rayColor = packRgba(255, 180, 60, 255); break;
                        case Sketch3DPlane::XZ: rayColor = packRgba(160, 255, 90, 255); break;
                        }

                        // 1. Draw the ray showing the path from behind camera through cursor to beyond hit point
                        {
                            ports::RenderPrimitive ray;
                            ray.kind = ports::RenderPrimitiveKind::LineList;
                            ray.rgba = rayColor;
                            ray.size = 2.5f;
                            ray.depthTest = false;  // Always visible

                            // Calculate ray direction from origin to hit point
                            glm::vec3 rayDir = glm::normalize(m_hoverRayHitPoint - m_hoverRayOrigin);

                            // Draw ray from near plane (close to cursor) extending to beyond the hit point
                            // This makes it clear the ray goes THROUGH the cursor position
                            glm::vec3 rayStart = m_hoverRayOrigin;  // Near plane point (at cursor in 3D)
                            glm::vec3 rayEnd = m_hoverRayHitPoint + rayDir * 20.0f;  // Extend beyond hit

                            ray.positions.push_back(rayStart);
                            ray.positions.push_back(rayEnd);

                            scene.primitives.push_back(std::move(ray));
                        }

                        // 2. Highlight the specific segment that was selected (thicker, pulsing)
                        {
                            ports::RenderPrimitive segment;
                            segment.kind = ports::RenderPrimitiveKind::LineList;
                            segment.rgba = rayColor;
                            segment.size = 6.0f;  // Extra thick to stand out
                            segment.depthTest = false;

                            segment.positions.push_back(m_hoverSegmentA);
                            segment.positions.push_back(m_hoverSegmentB);

                            scene.primitives.push_back(std::move(segment));
                        }

                        // 3. Draw a small sphere/cross at the hit point
                        {
                            ports::RenderPrimitive hitMarker;
                            hitMarker.kind = ports::RenderPrimitiveKind::LineList;
                            hitMarker.rgba = packRgba(255, 255, 0, 255);  // Yellow for high visibility
                            hitMarker.size = 3.0f;
                            hitMarker.depthTest = false;

                            const float markerSize = 0.5f;
                            // Draw a 3D cross at the hit point
                            hitMarker.positions.push_back(m_hoverRayHitPoint + glm::vec3(-markerSize, 0, 0));
                            hitMarker.positions.push_back(m_hoverRayHitPoint + glm::vec3(markerSize, 0, 0));
                            hitMarker.positions.push_back(m_hoverRayHitPoint + glm::vec3(0, -markerSize, 0));
                            hitMarker.positions.push_back(m_hoverRayHitPoint + glm::vec3(0, markerSize, 0));
                            hitMarker.positions.push_back(m_hoverRayHitPoint + glm::vec3(0, 0, -markerSize));
                            hitMarker.positions.push_back(m_hoverRayHitPoint + glm::vec3(0, 0, markerSize));

                            scene.primitives.push_back(std::move(hitMarker));
                        }

                        // 4. Draw connection line from hit point to segment (shortest distance indicator)
                        {
                            ports::RenderPrimitive connector;
                            connector.kind = ports::RenderPrimitiveKind::LineList;
                            connector.rgba = packRgba(255, 255, 0, 200);  // Yellow, slightly transparent
                            connector.size = 2.0f;
                            connector.depthTest = false;

                            // Find closest point on segment to the ray hit point
                            glm::vec3 ab = m_hoverSegmentB - m_hoverSegmentA;
                            glm::vec3 ap = m_hoverRayHitPoint - m_hoverSegmentA;
                            float t = glm::dot(ap, ab) / glm::dot(ab, ab);
                            t = clampf(t, 0.0f, 1.0f);
                            glm::vec3 closestOnSegment = m_hoverSegmentA + ab * t;

                            connector.positions.push_back(m_hoverRayHitPoint);
                            connector.positions.push_back(closestOnSegment);

                            scene.primitives.push_back(std::move(connector));
                        }
                    }

                    m_sketch3dRenderer->setScene(scene);
                }

                // Advance camera animation / easing.
                if (auto* gl = dynamic_cast<adapters::GlSketch3DViewRenderer*>(m_sketch3dRenderer.get())) {
                    gl->tick(ImGui::GetIO().DeltaTime);
                }

                m_sketch3dRenderer->renderToFramebuffer(m_window, w, h);

                void* tex = m_sketch3dRenderer->getFramebufferTexture();
                if (tex) {
                    ImVec2 imageMin = ImGui::GetCursorScreenPos();

                    ImGui::InvisibleButton("##viewport3d_sketch3d", size,
                        ImGuiButtonFlags_MouseButtonLeft |
                        ImGuiButtonFlags_MouseButtonRight |
                        ImGuiButtonFlags_MouseButtonMiddle);
                    ImGui::SetItemAllowOverlap();  // Allow other items to overlap and receive input

                    bool hovered = ImGui::IsItemHovered();
                    bool active = ImGui::IsItemActive();

                    ImVec2 imageMax = ImVec2(imageMin.x + size.x, imageMin.y + size.y);
                    ImGui::GetWindowDrawList()->AddImage(
                        tex,
                        imageMin,
                        imageMax,
                        ImVec2(0, 0),
                        ImVec2(1, 1)
                    );

                    if (active || ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows))
                        m_activeViewport = ActiveViewport::Sketch3D;

                    const bool isActiveViewport = (m_activeViewport == ActiveViewport::Sketch3D);


                    // Hover feedback: determine which plane the cursor is currently closest to (based on segments).
                    // This gives immediate visual confirmation without needing to click.
                    m_sketch3dHasHoverPlane = false;
                    if (isActiveViewport && hovered && !m_sketch3dSegments.empty())
                    {
                        // Convert mouse to NDC within the image
                        ImVec2 mp = ImGui::GetIO().MousePos;
                        const ImVec2 viewportSize = size;
                        const float vx = (mp.x - imageMin.x);
                        const float vy = (mp.y - imageMin.y);

                        if (vx >= 0.0f && vy >= 0.0f && vx <= viewportSize.x && vy <= viewportSize.y)
                        {
                            const float ndcX = (2.0f * (vx / maxf(1.0f, viewportSize.x))) - 1.0f;
                            const float ndcY = 1.0f - (2.0f * (vy / maxf(1.0f, viewportSize.y)));

                            // Build a ray in world space using the current camera matrices.
                            // We fetch them from the concrete GL renderer; the port is kept minimal.
                            const auto* gl = dynamic_cast<const adapters::GlSketch3DViewRenderer*>(m_sketch3dRenderer.get());
                            if (gl)
                            {
                                const glm::mat4& V = gl->getViewMatrix();
                                const glm::mat4& P = gl->getProjMatrix();
                                const glm::mat4 invVP = glm::inverse(P * V);
                                const glm::vec4 nearH = invVP * glm::vec4(ndcX, ndcY, -1.0f, 1.0f);
                                const glm::vec4 farH = invVP * glm::vec4(ndcX, ndcY, 1.0f, 1.0f);
                                const glm::vec3 nearP = glm::vec3(nearH) / nearH.w;
                                const glm::vec3 farP = glm::vec3(farH) / farH.w;

                                glm::vec3 ro = nearP;
                                glm::vec3 rd = glm::normalize(farP - nearP);

                                auto distanceRaySegment = [](const glm::vec3& ro, const glm::vec3& rd,
                                    const glm::vec3& a, const glm::vec3& b) -> float
                                    {
                                        // Ray-segment distance via closest points between a ray and a segment.
                                        // Based on standard geometric formulation.
                                        const glm::vec3 ab = b - a;
                                        const glm::vec3 ao = ro - a;

                                        const float ab2 = glm::dot(ab, ab);
                                        const float rd2 = glm::dot(rd, rd);
                                        const float rda = glm::dot(rd, ab);
                                        const float rdo = glm::dot(rd, ao);
                                        const float abo = glm::dot(ab, ao);

                                        const float denom = rd2 * ab2 - rda * rda;
                                        float t = 0.0f;
                                        float u = 0.0f;

                                        if (std::abs(denom) > 1e-6f)
                                        {
                                            t = (rda * abo - ab2 * rdo) / denom;
                                            u = (rd2 * abo - rda * rdo) / denom;
                                        }

                                        // Clamp to ray t>=0 and segment u in [0,1]
                                        t = maxf(0.0f, t);
                                        u = clampf(u, 0.0f, 1.0f);

                                        const glm::vec3 pRay = ro + rd * t;
                                        const glm::vec3 pSeg = a + ab * u;
                                        return glm::length(pRay - pSeg);
                                    };

                                float bestDist = 1e9f;
                                adapters::Sketch3DPlane bestPlane = m_sketch3dActivePlane;
                                glm::vec3 bestSegmentA{ 0,0,0 };
                                glm::vec3 bestSegmentB{ 0,0,0 };
                                glm::vec3 bestRayPoint{ 0,0,0 };
                                glm::vec3 bestSegPoint{ 0,0,0 };

                                for (const auto& seg : m_sketch3dSegments)
                                {
                                    // Distance from ray to segment (same helper as click-select)
                                    // We also need to compute the closest points for visualization
                                    const glm::vec3 ab = seg.b - seg.a;
                                    const glm::vec3 ao = ro - seg.a;

                                    const float ab2 = glm::dot(ab, ab);
                                    const float rd2 = glm::dot(rd, rd);
                                    const float rda = glm::dot(rd, ab);
                                    const float rdo = glm::dot(rd, ao);
                                    const float abo = glm::dot(ab, ao);

                                    const float denom = rd2 * ab2 - rda * rda;
                                    float t = 0.0f;
                                    float u = 0.0f;

                                    if (std::abs(denom) > 1e-6f)
                                    {
                                        t = (rda * abo - ab2 * rdo) / denom;
                                        u = (rd2 * abo - rda * rdo) / denom;
                                    }

                                    // Clamp to ray t>=0 and segment u in [0,1]
                                    t = maxf(0.0f, t);
                                    u = clampf(u, 0.0f, 1.0f);

                                    const glm::vec3 pRay = ro + rd * t;
                                    const glm::vec3 pSeg = seg.a + ab * u;
                                    float d = glm::length(pRay - pSeg);

                                    if (d < bestDist)
                                    {
                                        bestDist = d;
                                        bestPlane = seg.plane;
                                        bestSegmentA = seg.a;
                                        bestSegmentB = seg.b;
                                        bestRayPoint = pRay;
                                        bestSegPoint = pSeg;
                                    }
                                }

                                // Threshold greatly increased for huge hotspot - was 0.15f
                                if (bestDist < 5.0f)  // 5 units = huge detection area!
                                {
                                    m_sketch3dHasHoverPlane = true;
                                    m_sketch3dHoverPlane = bestPlane;

                                    // Store ray visualization data
                                    m_hasHoverRayData = true;
                                    m_hoverRayOrigin = ro;
                                    m_hoverRayHitPoint = bestSegPoint;
                                    m_hoverSegmentA = bestSegmentA;
                                    m_hoverSegmentB = bestSegmentB;
                                }
                                else
                                {
                                    m_hasHoverRayData = false;
                                }
                            }
                        }
                    }
                    else
                    {
                        m_hasHoverRayData = false;
                    }

                    // Click-select: pick the closest sketch segment under the cursor, then snap/animate
                                        // the camera to that segment's plane.
                    if (isActiveViewport && hovered && ImGui::IsMouseReleased(ImGuiMouseButton_Left)) {
                        ImVec2 mp = ImGui::GetIO().MousePos;
                        float localX = mp.x - imageMin.x;
                        float localY = mp.y - imageMin.y;

                        if (localX >= 0.0f && localY >= 0.0f && localX <= size.x && localY <= size.y) {
                            // Convert to NDC (-1..1)
                            float denomX = maxf(1.0f, size.x);
                            float denomY = maxf(1.0f, size.y);
                            float ndcX = (localX / denomX) * 2.0f - 1.0f;
                            float ndcY = 1.0f - (localY / denomY) * 2.0f;

                            if (auto* gl = dynamic_cast<adapters::GlSketch3DViewRenderer*>(m_sketch3dRenderer.get())) {
                                glm::mat4 V = gl->getViewMatrix();
                                glm::mat4 P = gl->getProjMatrix();
                                glm::mat4 invVP = glm::inverse(P * V);

                                auto unproject = [&](float zNdc) {
                                    glm::vec4 p = invVP * glm::vec4(ndcX, ndcY, zNdc, 1.0f);
                                    if (std::abs(p.w) < 1e-6f) return glm::vec3(0.0f);
                                    return glm::vec3(p) / p.w;
                                    };

                                glm::vec3 ro = unproject(-1.0f);
                                glm::vec3 rf = unproject(1.0f);
                                glm::vec3 rd = glm::normalize(rf - ro);

                                auto raySegmentDistance = [&](const glm::vec3& a, const glm::vec3& b) {
                                    // Closest distance between ray (ro + t*rd, t>=0) and segment [a,b].
                                    glm::vec3 v = b - a;
                                    glm::vec3 w0 = ro - a;

                                    float aDot = glm::dot(rd, rd); // = 1
                                    float bDot = glm::dot(rd, v);
                                    float cDot = glm::dot(v, v);
                                    float dDot = glm::dot(rd, w0);
                                    float eDot = glm::dot(v, w0);

                                    float denom = aDot * cDot - bDot * bDot;
                                    float t = 0.0f;
                                    float s = 0.0f;

                                    if (denom > 1e-8f) {
                                        t = (bDot * eDot - cDot * dDot) / denom;
                                        s = (aDot * eDot - bDot * dDot) / denom;
                                    }
                                    else {
                                        // Almost parallel
                                        t = 0.0f;
                                        s = eDot / maxf(1e-8f, cDot);
                                    }

                                    t = maxf(0.0f, t);
                                    s = clampf(s, 0.0f, 1.0f);

                                    glm::vec3 pr = ro + rd * t;
                                    glm::vec3 ps = a + v * s;
                                    return glm::length(pr - ps);
                                    };

                                float best = 1e9f;
                                Sketch3DPlane bestPlane = m_sketch3dActivePlane;

                                for (const auto& seg : m_sketch3dSegments) {
                                    float d = raySegmentDistance(seg.a, seg.b);
                                    if (d < best) {
                                        best = d;
                                        bestPlane = seg.plane;
                                    }
                                }

                                // Threshold is in world units - greatly increased for easier clicking
                                if (best < 5.0f) {  // Match hover threshold
                                    m_sketch3dActivePlane = bestPlane;
                                    gl->animateToPlane(bestPlane);
                                }
                            }
                        }
                    }

                    if (isActiveViewport && hovered && ImGui::IsMouseDragging(ImGuiMouseButton_Right, 0.0f)) {
                        ImVec2 delta = ImGui::GetIO().MouseDelta;
                        m_sketch3dRenderer->rotate(delta.x, delta.y);
                    }

                    if (isActiveViewport && hovered && ImGui::IsMouseDragging(ImGuiMouseButton_Middle, 0.0f)) {
                        ImVec2 delta = ImGui::GetIO().MouseDelta;
                        m_sketch3dRenderer->zoom(-delta.y * 0.05f);
                    }

                    // Visual hover feedback: show which plane the user is about to click
                    if (m_sketch3dHasHoverPlane && hovered && m_sketch3dHoverPlane != m_sketch3dActivePlane)
                    {
                        const char* planeName = "";
                        ImU32 planeColor = IM_COL32(255, 255, 255, 255);

                        switch (m_sketch3dHoverPlane)
                        {
                        case Sketch3DPlane::XY:
                            planeName = "XY Plane";
                            planeColor = IM_COL32(60, 200, 255, 255);
                            break;
                        case Sketch3DPlane::YZ:
                            planeName = "YZ Plane";
                            planeColor = IM_COL32(255, 180, 60, 255);
                            break;
                        case Sketch3DPlane::XZ:
                            planeName = "XZ Plane";
                            planeColor = IM_COL32(160, 255, 90, 255);
                            break;
                        }

                        // Draw hover indicator at mouse position
                        ImVec2 mousePos = ImGui::GetIO().MousePos;
                        ImDrawList* drawList = ImGui::GetWindowDrawList();

                        // Draw a crosshair at cursor to emphasize "ray starts here"
                        const float crossSize = 12.0f;
                        drawList->AddLine(
                            ImVec2(mousePos.x - crossSize, mousePos.y),
                            ImVec2(mousePos.x + crossSize, mousePos.y),
                            planeColor, 2.0f);
                        drawList->AddLine(
                            ImVec2(mousePos.x, mousePos.y - crossSize),
                            ImVec2(mousePos.x, mousePos.y + crossSize),
                            planeColor, 2.0f);

                        // Draw a small circle at cursor center
                        drawList->AddCircle(mousePos, 4.0f, planeColor, 12, 2.5f);

                        // Background rectangle for text - now with more explanation
                        char hoverText[128];
                        const char* planeExplanation = "";
                        switch (m_sketch3dHoverPlane) {
                        case Sketch3DPlane::XY:
                            planeExplanation = "Front/Back view (Z perpendicular)";
                            break;
                        case Sketch3DPlane::YZ:
                            planeExplanation = "Side view (X perpendicular)";
                            break;
                        case Sketch3DPlane::XZ:
                            planeExplanation = "Top/Bottom view (Y perpendicular)";
                            break;
                        }
                        snprintf(hoverText, sizeof(hoverText),
                            "Ray detected %s - %s", planeName, planeExplanation);
                        ImVec2 textSize = ImGui::CalcTextSize(hoverText);

                        // Position tooltip below cursor to not obscure the ray visualization
                        ImVec2 bgMin = ImVec2(mousePos.x + 15, mousePos.y + 20);
                        ImVec2 bgMax = ImVec2(bgMin.x + textSize.x + 16, bgMin.y + textSize.y + 8);

                        drawList->AddRectFilled(bgMin, bgMax, IM_COL32(30, 30, 35, 240), 4.0f);
                        drawList->AddRect(bgMin, bgMax, planeColor, 4.0f, 0, 2.5f);
                        drawList->AddText(ImVec2(bgMin.x + 8, bgMin.y + 4), planeColor, hoverText);

                        // Add a second line explaining the visualization
                        const char* helpText = "Cube face shows orientation - Click to select";
                        ImVec2 helpSize = ImGui::CalcTextSize(helpText);
                        ImVec2 helpMin = ImVec2(bgMin.x, bgMax.y + 4);
                        ImVec2 helpMax = ImVec2(helpMin.x + helpSize.x + 16, helpMin.y + helpSize.y + 8);

                        drawList->AddRectFilled(helpMin, helpMax, IM_COL32(30, 30, 35, 200), 4.0f);
                        drawList->AddRect(helpMin, helpMax, IM_COL32(255, 255, 0, 255), 4.0f, 0, 1.5f);
                        drawList->AddText(ImVec2(helpMin.x + 8, helpMin.y + 4),
                            IM_COL32(255, 255, 200, 255), helpText);

                        // Also draw a subtle cursor indicator
                        ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
                    }
                }
                else {
                    ImGui::TextColored(ImVec4(1, 0, 0, 1), "ERROR: No texture from Sketch3D renderer");
                }
            }
            else
            {
                ImGui::TextDisabled("Viewport too small");
            }
        }
        else
        {
            ImGui::TextDisabled("No Sketch3D renderer");
        }

        ImGui::End();

        if (m_config && wasOpen != m_viewSketch3DViewport)
            m_config->set("pistachio.ui", "views.sketch3dViewport", m_viewSketch3DViewport);
    }


    // ------------------------------------------------------------
    // UI SECTIONS (unchanged behavior)
    // ------------------------------------------------------------

    void ImGuiAdapter::setMenubarCallback(const std::function<void()>& cb)
    {
        m_MenubarCallback = cb;
    }

    void ImGuiAdapter::renderMainMenu()
    {
        if (!m_viewFileOperations)
            return;

        bool wasOpen = m_viewFileOperations;
        ImGui::Begin("File Operations", &m_viewFileOperations);

        static char filePath[512] = {};
        ImGui::InputTextWithHint("##file", "STEP file path...", filePath, sizeof(filePath));

        if (ImGui::Button("Load STEP"))
        {
            if (m_app && filePath[0])
                m_app->loadFile(filePath);
        }

        ImGui::End();

        // Persist close/open state
        if (m_config && wasOpen != m_viewFileOperations)
            m_config->set("pistachio.ui", "views.fileOperations", m_viewFileOperations);
    }

    void ImGuiAdapter::renderStatusBar()
    {
        if (!m_viewStatus)
            return;

        bool wasOpen = m_viewStatus;
        ImGui::Begin("Status", &m_viewStatus, ImGuiWindowFlags_NoScrollbar);

        if (m_app)
            ImGui::Text("Status: %s", m_app->getStatus().c_str());

        ImGui::End();

        if (m_config && wasOpen != m_viewStatus)
            m_config->set("pistachio.ui", "views.status", m_viewStatus);
    }

    void ImGuiAdapter::renderModelInfo()
    {
        if (!m_viewModelInfo)
            return;

        bool wasOpen = m_viewModelInfo;
        ImGui::Begin("Model Info", &m_viewModelInfo);

        if (!m_app)
        {
            ImGui::TextDisabled("No application");
            ImGui::End();
            if (m_config && wasOpen != m_viewModelInfo)
                m_config->set("pistachio.ui", "views.modelInfo", m_viewModelInfo);
            return;
        }

        auto model = m_app->getCurrentModel();
        if (!model)
        {
            ImGui::TextDisabled("No model loaded");
            ImGui::End();
            if (m_config && wasOpen != m_viewModelInfo)
                m_config->set("pistachio.ui", "views.modelInfo", m_viewModelInfo);
            return;
        }

        ImGui::Text("Model loaded");
        ImGui::End();
        if (m_config && wasOpen != m_viewModelInfo)
            m_config->set("pistachio.ui", "views.modelInfo", m_viewModelInfo);
    }


    void ImGuiAdapter::render3DView()
    {
        if (!m_view3DViewport)
            return;

        bool wasOpen = m_view3DViewport;
        ImGui::Begin("3D Viewport", &m_view3DViewport, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);

        if (m_app && m_app->getRenderer())
        {
            ImVec2 size = ImGui::GetContentRegionAvail();

            if (size.x > 0 && size.y > 0)
            {
                const uint32_t w = (uint32_t)ImMax(1.0f, size.x);
                const uint32_t h = (uint32_t)ImMax(1.0f, size.y);

                m_app->getRenderer()->renderToFramebuffer(m_window, w, h);

                void* tex = m_app->getRenderer()->getFramebufferTexture();
                if (tex) {
                    // Get position BEFORE drawing
                    ImVec2 imageMin = ImGui::GetCursorScreenPos();

                    // === CRITICAL: Use InvisibleButton to capture ALL input ===
                    ImGui::InvisibleButton("##viewport3d_cube", size,
                        ImGuiButtonFlags_MouseButtonLeft |
                        ImGuiButtonFlags_MouseButtonRight |
                        ImGuiButtonFlags_MouseButtonMiddle);
                    ImGui::SetItemAllowOverlap();  // Allow other items to overlap and receive input

                    bool hovered = ImGui::IsItemHovered();
                    bool active = ImGui::IsItemActive();


                    // Mark this viewport as active when focused or clicked
                    if (active || ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows))
                        m_activeViewport = ActiveViewport::Cube;

                    const bool isActiveViewport = (m_activeViewport == ActiveViewport::Cube);
                    // Draw the texture ON TOP using the drawlist
                    ImVec2 imageMax = ImVec2(imageMin.x + size.x, imageMin.y + size.y);
                    ImGui::GetWindowDrawList()->AddImage(
                        tex,
                        imageMin,
                        imageMax,
                        ImVec2(0, 0),
                        ImVec2(1, 1)
                    );

                    // Record for gizmo
                    m_viewportImageMin = imageMin;
                    m_viewportImageMax = imageMax;
                    m_viewportImageValid = true;

                    // === ROTATE: Right mouse button drag ===
                    if (isActiveViewport && hovered && ImGui::IsMouseDragging(ImGuiMouseButton_Right, 0.0f)) {
                        ImVec2 delta = ImGui::GetIO().MouseDelta;
                        m_app->getRenderer()->rotate(delta.x, delta.y);
                    }

                    //// === ZOOM: Mouse wheel - NOW IT WILL WORK! ===
                    //if (hovered) {
                    //    float wheel = ImGui::GetIO().MouseWheel;
                    //    if (wheel != 0.0f) {
                    //        m_app->getRenderer()->zoom(wheel);
                    //    }
                    //}

                   // Middle-drag = zoom (no modifier needed!)
                    if (isActiveViewport && hovered && ImGui::IsMouseDragging(ImGuiMouseButton_Middle, 0.0f)) {
                        ImVec2 delta = ImGui::GetIO().MouseDelta;
                        m_app->getRenderer()->zoom(-delta.y * 0.05f);
                    }

                    // Render camera gizmo overlay
                    if (isActiveViewport) renderCameraGizmo();

                }
                else {
                    m_viewportImageValid = false;
                    ImGui::TextColored(ImVec4(1, 0, 0, 1), "ERROR: No texture from renderer");
                }
            }
            else
            {
                m_viewportImageValid = false;
                ImGui::TextDisabled("Viewport too small");
            }
        }
        else
        {
            m_viewportImageValid = false;
            ImGui::TextDisabled("No renderer");
        }

        ImGui::End();

        if (m_config && wasOpen != m_view3DViewport)
            m_config->set("pistachio.ui", "views.viewport3d", m_view3DViewport);
    }
    void ImGuiAdapter::renderRibbonBar()
    {
        // Called from host via RibbonBar callback. No Begin/End here.
        if (!m_app) return;
        auto doc = m_app->getSketchDocument();
        if (!doc || doc->sketches.empty()) {
            ImGui::TextDisabled("No sketch");
            return;
        }

        // Ensure active sketch index valid
        if (m_activeSketchIndex < 0) m_activeSketchIndex = 0;
        if (m_activeSketchIndex >= (int)doc->sketches.size()) m_activeSketchIndex = (int)doc->sketches.size() - 1;

        auto& sketch = doc->sketches[(size_t)m_activeSketchIndex];

        auto makeCtx = [&]() -> adapters::sketchui::ToolContext {
            return adapters::sketchui::ToolContext{
                sketch,
                m_cmdHistory,
                &m_activeConstraintIcon,
                &m_sketchNeedsSolve,
                &m_sketchChangeSerial,
                &m_uiPickedIds,
                &m_uiHoverId
            };
            };

        // Tool buttons
        // Undo / Redo
        {
            auto ctx = makeCtx();
            const bool canUndo = m_cmdHistory.CanUndo();
            const bool canRedo = m_cmdHistory.CanRedo();
            if (!canUndo) ImGui::BeginDisabled();
            if (ImGui::Button("Undo")) { m_cmdHistory.Undo(); ctx.MarkDirty(); m_uiPickedIds.clear(); m_uiHoverId = 0; }
            if (!canUndo) ImGui::EndDisabled();
            ImGui::SameLine();
            if (!canRedo) ImGui::BeginDisabled();
            if (ImGui::Button("Redo")) { m_cmdHistory.Redo(); ctx.MarkDirty(); m_uiPickedIds.clear(); m_uiHoverId = 0; }
            if (!canRedo) ImGui::EndDisabled();
            ImGui::SameLine();
            ImGui::TextDisabled("|");
            ImGui::SameLine();
        }
        if (ImGui::Button("Select"))
        {
            auto ctx = makeCtx();
            m_toolManager.Deactivate(ctx);
            m_activeConstraintIcon = -1;
            m_skipToolUpdateOnce = true; // consume the toolbar click
        }
        ImGui::SameLine();

        if (ImGui::Button("Line"))
        {
            auto ctx = makeCtx();
            m_toolManager.Activate(adapters::sketchui::ToolKind::Line2Pt, ctx);
            m_activeConstraintIcon = -1;
            m_skipToolUpdateOnce = true; // consume the toolbar click
        }
        ImGui::SameLine();

        if (ImGui::Button("Circle"))
        {
            auto ctx = makeCtx();
            m_toolManager.Activate(adapters::sketchui::ToolKind::CircleCenterRadius, ctx);
            m_activeConstraintIcon = -1;
            m_skipToolUpdateOnce = true; // consume the toolbar click
        }
        ImGui::SameLine();

        if (ImGui::Button("Constraint"))
        {
            // Toggle into constraint tool; actual constraint icon selection happens in Sketch Editor panel.
            auto ctx = makeCtx();
            m_toolManager.Activate(adapters::sketchui::ToolKind::Constraint, ctx);
            m_skipToolUpdateOnce = true; // consume the toolbar click
        }

        ImGui::SameLine();
        ImGui::TextDisabled(" | Sketch tools");
    }


    void ImGuiAdapter::renderCameraGizmo()
    {
        if (!m_viewportImageValid || !m_app || !m_app->getRenderer())
            return;

        // Draw a small "view cube" in the top-right of the 3D viewport image.
        // Click faces to snap the camera to +/-X +/-Y +/-Z.
        // This is intentionally lightweight (no ImGuizmo dependency).
        ImDrawList* dl = ImGui::GetWindowDrawList();
        if (!dl) return;

        const float pad = 10.0f;
        const float size = 96.0f; // pixels
        const ImVec2 imgMin = m_viewportImageMin;
        const ImVec2 imgMax = m_viewportImageMax;

        // Clamp gizmo to image rect
        ImVec2 gmax(imgMax.x - pad, imgMin.y + pad + size);
        ImVec2 gmin(gmax.x - size, gmax.y - size);

        if (gmin.x < imgMin.x + pad) gmin.x = imgMin.x + pad;
        if (gmin.y < imgMin.y + pad) gmin.y = imgMin.y + pad;
        if (gmax.x > imgMax.x - pad) gmax.x = imgMax.x - pad;
        if (gmax.y > imgMax.y - pad) gmax.y = imgMax.y - pad;

        const ImVec2 center((gmin.x + gmax.x) * 0.5f, (gmin.y + gmax.y) * 0.5f);
        const float radius = (gmax.x - gmin.x) * 0.5f;

        // Background card
        dl->AddRectFilled(gmin, gmax, IM_COL32(25, 25, 28, 210), 10.0f);
        dl->AddRect(gmin, gmax, IM_COL32(255, 255, 255, 40), 10.0f);

        // Get current camera direction (from target to camera)
        ports::CameraState cam = m_app->getRenderer()->getCameraState();
        glm::vec3 fwd = glm::normalize(cam.target - cam.position); // camera forward (look direction)

        // Approximate yaw/pitch consistent with GlCubeViewRenderer orbit math
        // yaw rotates around +Y, pitch rotates around +X.
        float yaw = std::atan2(fwd.x, fwd.z);            // -pi..pi
        float pitch = std::asin(ImClamp(fwd.y, -1.0f, 1.0f)); // -pi/2..pi/2

        struct V3 { float x, y, z; };
        auto v3 = [](float x, float y, float z) { return V3{ x,y,z }; };
        auto add = [](V3 a, V3 b) { return V3{ a.x + b.x,a.y + b.y,a.z + b.z }; };
        auto sub = [](V3 a, V3 b) { return V3{ a.x - b.x,a.y - b.y,a.z - b.z }; };
        auto mul = [](V3 a, float s) { return V3{ a.x * s,a.y * s,a.z * s }; };
        auto dot = [](V3 a, V3 b) { return a.x * b.x + a.y * b.y + a.z * b.z; };
        auto cross = [](V3 a, V3 b) { return V3{ a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x }; };
        auto len = [&](V3 a) { return std::sqrt(dot(a, a)); };
        auto norm = [&](V3 a) { float l = len(a); return (l > 1e-6f) ? mul(a, 1.0f / l) : v3(0, 0, 0); };

        auto rotX = [](V3 p, float a) {
            float c = std::cos(a), s = std::sin(a);
            return V3{ p.x, c * p.y - s * p.z, s * p.y + c * p.z };
            };
        auto rotY = [](V3 p, float a) {
            float c = std::cos(a), s = std::sin(a);
            return V3{ c * p.x + s * p.z, p.y, -s * p.x + c * p.z };
            };

        // Cube vertices in object space
        V3 P[8] = {
            v3(-1,-1,-1), v3(1,-1,-1), v3(1, 1,-1), v3(-1, 1,-1),
            v3(-1,-1, 1), v3(1,-1, 1), v3(1, 1, 1), v3(-1, 1, 1)
        };

        // Rotate cube opposite the camera so it represents world axes relative to view
        V3 W[8];
        for (int i = 0;i < 8;i++) {
            V3 p = P[i];
            p = rotY(p, -yaw);
            p = rotX(p, -pitch);
            W[i] = p;
        }

        // Simple ortho projection for the widget
        auto project = [&](V3 p)->ImVec2 {
            // p in -1..1; map to widget space with a bit of perspective-ish scaling
            float z = p.z;
            float s = 0.75f + 0.25f * (z + 1.0f) * 0.5f; // nearer faces slightly larger
            float x = p.x * s;
            float y = p.y * s;
            return ImVec2(center.x + x * radius * 0.75f, center.y - y * radius * 0.75f);
            };

        // Faces, each as quad indices
        const int F[6][4] = {
            {0,1,2,3}, // -Z (back)
            {4,5,6,7}, // +Z (front)
            {0,1,5,4}, // -Y (bottom)
            {3,2,6,7}, // +Y (top)
            {1,2,6,5}, // +X (right)
            {0,3,7,4}  // -X (left)
        };

        struct FacePoly {
            ImVec2 p[4];
            float depth;
            int dir;
            ImU32 col;
            const char* label;
        };

        FacePoly faces[6];
        // Map face to GlCubeViewRenderer::setViewDirection indices
        // dir: 0 +X, 1 -X, 2 +Z, 3 -Z, 4 +Y, 5 -Y
        const int dirMap[6] = { 3, 2, 5, 4, 0, 1 };
        const ImU32 baseCol[6] = {
            IM_COL32(180,  80,  80, 255), // -Z
            IM_COL32(80, 170,  90, 255), // +Z
            IM_COL32(90, 130, 210, 255), // -Y
            IM_COL32(210, 190,  90, 255), // +Y
            IM_COL32(90, 190, 190, 255), // +X
            IM_COL32(190,  90, 200, 255)  // -X
        };
        const char* labels[6] = { "-Z","+Z","-Y","+Y","+X","-X" };

        // Build face polys with depth for painter's algorithm
        for (int fi = 0;fi < 6;fi++) {
            float dz = 0.0f;
            for (int k = 0;k < 4;k++) {
                V3 wp = W[F[fi][k]];
                dz += wp.z;
                faces[fi].p[k] = project(wp);
            }
            faces[fi].depth = dz / 4.0f;
            faces[fi].dir = dirMap[fi];
            faces[fi].col = baseCol[fi];
            faces[fi].label = labels[fi];
        }

        // Sort faces back-to-front (smaller depth first)
        int order[6] = { 0,1,2,3,4,5 };
        std::sort(order, order + 6, [&](int a, int b) { return faces[a].depth < faces[b].depth; });

        // Helper: point in triangle
        auto pointInTri = [&](ImVec2 p, ImVec2 a, ImVec2 b, ImVec2 c)->bool {
            auto sign = [](ImVec2 p1, ImVec2 p2, ImVec2 p3) {
                return (p1.x - p3.x) * (p2.y - p3.y) - (p2.x - p3.x) * (p1.y - p3.y);
                };
            float d1 = sign(p, a, b);
            float d2 = sign(p, b, c);
            float d3 = sign(p, c, a);
            bool has_neg = (d1 < 0) || (d2 < 0) || (d3 < 0);
            bool has_pos = (d1 > 0) || (d2 > 0) || (d3 > 0);
            return !(has_neg && has_pos);
            };

        ImVec2 mouse = ImGui::GetIO().MousePos;
        bool mouseOver = (mouse.x >= gmin.x && mouse.x <= gmax.x && mouse.y >= gmin.y && mouse.y <= gmax.y);

        // Determine clicked face: test from frontmost to backmost so the visible face wins.
        int hoveredDir = -1;
        if (mouseOver)
        {
            for (int oi = 5; oi >= 0; --oi) {
                int fi = order[oi];
                ImVec2 a = faces[fi].p[0];
                ImVec2 b = faces[fi].p[1];
                ImVec2 c = faces[fi].p[2];
                ImVec2 d = faces[fi].p[3];
                if (pointInTri(mouse, a, b, c) || pointInTri(mouse, a, c, d)) {
                    hoveredDir = faces[fi].dir;
                    break;
                }
            }
        }

        // Draw faces
        for (int oi = 0; oi < 6; ++oi)
        {
            int fi = order[oi];

            ImU32 col = faces[fi].col;
            if (faces[fi].dir == hoveredDir)
                col = IM_COL32(
                    ImMin(255, (int)((col >> IM_COL32_R_SHIFT) & 0xFF) + 25),
                    ImMin(255, (int)((col >> IM_COL32_G_SHIFT) & 0xFF) + 25),
                    ImMin(255, (int)((col >> IM_COL32_B_SHIFT) & 0xFF) + 25),
                    255);

            dl->AddConvexPolyFilled(faces[fi].p, 4, col);
            dl->AddPolyline(faces[fi].p, 4, IM_COL32(0, 0, 0, 120), true, 1.0f);
        }

        // Labels (only for the hovered face, to keep it clean)
        if (hoveredDir != -1)
        {
            const char* lbl = nullptr;
            switch (hoveredDir) {
            case 0: lbl = "+X"; break;
            case 1: lbl = "-X"; break;
            case 2: lbl = "+Z"; break;
            case 3: lbl = "-Z"; break;
            case 4: lbl = "+Y"; break;
            case 5: lbl = "-Y"; break;
            default: break;
            }
            if (lbl) {
                ImVec2 tp = ImVec2(gmin.x + 10.0f, gmin.y + 8.0f);
                dl->AddText(tp, IM_COL32(255, 255, 255, 220), lbl);
            }
        }

        // Click -> snap camera
        if (hoveredDir != -1 && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
        {
            m_app->getRenderer()->setViewDirection(hoveredDir);
        }

        if (mouseOver)
            ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
    }


    void ImGuiAdapter::renderSketchEditor()
    {
        if (!m_viewSketchEditor)
            return;

        bool wasOpen = m_viewSketchEditor;
        ImGui::Begin("Sketch Editor", &m_viewSketchEditor);

        auto doc = m_app->getSketchDocument();
        static int s_lastSketchIdx = -1;
        if (!doc || doc->sketches.empty()) {
            ImGui::TextDisabled("No sketch document loaded");
            ImGui::End();
            if (m_config && wasOpen != m_viewSketchEditor)
                m_config->set("pistachio.ui", "views.sketchEditor", m_viewSketchEditor);
            return;
        }

        // Choose active sketch (simple)
        if (m_activeSketchIndex < 0) m_activeSketchIndex = 0;
        if (m_activeSketchIndex >= (int)doc->sketches.size()) m_activeSketchIndex = (int)doc->sketches.size() - 1;

        if (doc->sketches.size() > 1) {
            ImGui::Text("Active sketch:");
            ImGui::SameLine();
            ImGui::SetNextItemWidth(200);
            ImGui::Combo("##ActiveSketch", &m_activeSketchIndex,
                [](void* data, int idx, const char** out_text) {
                    auto* d = reinterpret_cast<domain::sketch::Document*>(data);
                    if (idx < 0 || idx >= (int)d->sketches.size()) return false;
                    *out_text = d->sketches[(size_t)idx].name.c_str();
                    return true;
                },
                doc.get(),
                (int)doc->sketches.size());
        }

        // Solve constraints once when requested (e.g. after adding constraints)
        if (m_sketchNeedsSolve) {
            m_sketchNeedsSolve = false;
            if (m_app) m_app->runSolver();
            // document may have been updated by solver; continue with latest sketch reference
        }

        if (s_lastSketchIdx != m_activeSketchIndex) {
            s_lastSketchIdx = m_activeSketchIndex;
            m_sketchNeedsSolve = true;
        }

        auto& sketch = doc->sketches[(size_t)m_activeSketchIndex];

        // Tool selection status
        const auto _ak = m_toolManager.ActiveKind();
        const char* _toolName =
            (_ak == adapters::sketchui::ToolKind::Line2Pt) ? "Line (2pt)" :
            (_ak == adapters::sketchui::ToolKind::CircleCenterRadius) ? "Circle (center-radius)" :
            (_ak == adapters::sketchui::ToolKind::Constraint) ? "Constraint" :
            "None";
        ImGui::Text("Tool: %s", _toolName);
        ImGui::SameLine();
        ImGui::TextDisabled("(Click Line icon in toolbar)");
        ImGui::Separator();

        ImGui::TextUnformatted("Constraints:");
        ImGui::SameLine();
        {
            using adapters::sketchui::ConstraintIcon;
            auto iconBtn = [&](const char* id, ConstraintIcon ic, const char* tip) {
                bool sel = (m_activeConstraintIcon == (int)ic);
                if (adapters::sketchui::ConstraintIconButton(id, ic, sel)) {
                    m_activeConstraintIcon = sel ? -1 : (int)ic;

                    // Make constraint selection feel like an active tool (separate from sketch drawing tools).
                    if (auto doc2 = m_app->getSketchDocument(); doc2 && !doc2->sketches.empty()) {
                        if (m_activeSketchIndex < 0) m_activeSketchIndex = 0;
                        if (m_activeSketchIndex >= (int)doc2->sketches.size()) m_activeSketchIndex = (int)doc2->sketches.size() - 1;

                        adapters::sketchui::ToolContext tctx{
                            doc2->sketches[(size_t)m_activeSketchIndex],
                            m_cmdHistory,
                            &m_activeConstraintIcon,
                            &m_sketchNeedsSolve,
                            &m_sketchChangeSerial,
                            &m_uiPickedIds,
                            &m_uiHoverId
                        };

                        if (m_activeConstraintIcon >= 0)
                            m_toolManager.Activate(adapters::sketchui::ToolKind::Constraint, tctx);
                    }
                }
                if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", tip);
                ImGui::SameLine();
                };

            iconBtn("##c_fixed", ConstraintIcon::Fixed, "Fixed");
            iconBtn("##c_tangent", ConstraintIcon::Tangent, "Tangent");
            iconBtn("##c_h", ConstraintIcon::Horizontal, "Horizontal");
            iconBtn("##c_v", ConstraintIcon::Vertical, "Vertical");
            iconBtn("##c_parallel", ConstraintIcon::Parallel, "Parallel");
            iconBtn("##c_perp", ConstraintIcon::Perpendicular, "Perpendicular");
            iconBtn("##c_coin", ConstraintIcon::Coincident, "Coincident");
            iconBtn("##c_mid", ConstraintIcon::Midpoint, "Midpoint");
            iconBtn("##c_equal", ConstraintIcon::Equal, "Equal");
            ImGui::NewLine();
        }

        // Canvas region
        ImVec2 canvasPos = ImGui::GetCursorScreenPos();
        ImVec2 canvasSize = ImGui::GetContentRegionAvail();
        if (canvasSize.x < 100) canvasSize.x = 100;
        if (canvasSize.y < 100) canvasSize.y = 100;

        ImDrawList* dl = ImGui::GetWindowDrawList();
        dl->AddRectFilled(canvasPos, ImVec2(canvasPos.x + canvasSize.x, canvasPos.y + canvasSize.y), IM_COL32(20, 20, 20, 255));
        dl->AddRect(canvasPos, ImVec2(canvasPos.x + canvasSize.x, canvasPos.y + canvasSize.y), IM_COL32(80, 80, 80, 255));

        // Input capture for canvas
        ImGui::InvisibleButton("##SketchCanvas", canvasSize,
            ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_MouseButtonRight | ImGuiButtonFlags_MouseButtonMiddle);
        ImGui::SetItemAllowOverlap();  // Allow other items to overlap and receive input

        const bool hovered = ImGui::IsItemHovered();

        // Update canvas mapping
        m_canvas2D.origin_screen = canvasPos;
        m_canvas2D.size = canvasSize;
        m_canvas2D.pan_screen = m_sketchPan;
        m_canvas2D.pixels_per_unit = m_sketchZoom;

        // === Hover picking (for delete + visual feedback) ===
        if (hovered)
        {
            const float tolW = 6.0f / maxf(m_canvas2D.pixels_per_unit, 1.0f);
            ImVec2 mouseW = m_canvas2D.ScreenToWorld(ImGui::GetMousePos());
            adapters::sketchui::PickResult hp = adapters::sketchui::PickGeometry(sketch, mouseW, tolW);
            m_uiHoverId = (hp.type != adapters::sketchui::PickType::None) ? hp.id : (domain::sketch::EntityId)0;

            // Draw a yellow highlight over the hovered entity
            if (m_uiHoverId != 0 && sketch.entities.contains(m_uiHoverId))
            {
                auto WS = [&](const domain::sketch::Vec2& w) {
                    return m_canvas2D.WorldToScreen(ImVec2((float)w.x, (float)w.y));
                    };

                auto h = sketch.entities.getHandle(m_uiHoverId);
                const ImU32 col = IM_COL32(255, 210, 0, 220);

                switch (h.kind)
                {
                case domain::sketch::EntityKind::Point:
                {
                    const auto& pt = sketch.entities.point(h.index);
                    ImVec2 s = WS(pt.p);
                    dl->AddCircle(s, 6.0f, col, 16, 2.0f);
                } break;
                case domain::sketch::EntityKind::Line:
                {
                    const auto& ln = sketch.entities.line(h.index);
                    dl->AddLine(WS(ln.a), WS(ln.b), col, 3.0f);
                } break;
                case domain::sketch::EntityKind::Circle:
                {
                    const auto& cc = sketch.entities.circle(h.index);
                    ImVec2 c = WS(cc.center);
                    float r = (float)cc.radius * m_canvas2D.pixels_per_unit;
                    dl->AddCircle(c, r, col, 64, 3.0f);
                } break;
                default:
                    break; // no cursor drawing here
                }
            }
        }
        else
        {
            // Not hovering canvas; clear hover id so highlights don't stick
            m_uiHoverId = 0;
        }

        // Delete hovered entity (single entity) with undo/redo
        // Use direct GLFW key check to bypass potential ImGui navigation issues
        bool windowFocused = ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows);
        
        // Check delete key using both ImGui and GLFW
        bool deletePressed = false;
        if (m_window) {
            // GLFW key states: GLFW_PRESS or GLFW_REPEAT mean the key is down
            int deleteState = glfwGetKey(m_window, GLFW_KEY_DELETE);
            int backspaceState = glfwGetKey(m_window, GLFW_KEY_BACKSPACE);
            
            // Use static to track previous state for edge detection (only trigger on press, not hold)
            static int s_lastDeleteState = GLFW_RELEASE;
            static int s_lastBackspaceState = GLFW_RELEASE;
            
            bool deleteJustPressed = (deleteState == GLFW_PRESS && s_lastDeleteState == GLFW_RELEASE);
            bool backspaceJustPressed = (backspaceState == GLFW_PRESS && s_lastBackspaceState == GLFW_RELEASE);
            
            s_lastDeleteState = deleteState;
            s_lastBackspaceState = backspaceState;
            
            deletePressed = deleteJustPressed || backspaceJustPressed;
        }
        
        // Fallback to ImGui if GLFW isn't working
        if (!deletePressed) {
            deletePressed = ImGui::IsKeyPressed(ImGuiKey_Delete) || ImGui::IsKeyPressed(ImGuiKey_Backspace);
        }
        
        // Only show debug when delete is actually pressed
        if (deletePressed) {
            printf("[DELETE] Press detected - hovered=%d, id=%llu, focused=%d, WantText=%d\n",
                hovered, (unsigned long long)m_uiHoverId, windowFocused, ImGui::GetIO().WantTextInput);
        }
        
        if (hovered
            && m_uiHoverId != 0
            && windowFocused
            && !ImGui::GetIO().WantTextInput
            && deletePressed)
        {
            printf("[DELETE] SUCCESS - Deleting entity %llu\n", (unsigned long long)m_uiHoverId);
            m_cmdHistory.Execute(std::make_unique<core::commands::DeleteEntityCommand>(sketch, m_uiHoverId));

            // Clear UI picks (so tools don't think the entity is still there)
            m_uiPickedIds.clear();
            m_uiHoverId = 0;

            // Force solve + redraw
            m_sketchNeedsSolve = true;
            ++m_sketchChangeSerial;
        }

        // Zoom with MMB drag (vertical movement)
        // NOTE: The sketch editor's pan/zoom is independent of the 3D viewport "active renderer" concept.
        if (hovered && ImGui::IsMouseDragging(ImGuiMouseButton_Middle, 0.0f)) {
            ImVec2 d = ImGui::GetIO().MouseDelta;
            
            // Use vertical drag distance for zooming
            float zoomDelta = -d.y * 0.01f; // Negative so dragging up zooms in
            float old = m_sketchZoom;
            float next = std::clamp(old * (1.0f + zoomDelta), 5.0f, 400.0f);
            
            if (next != old) {
                ImVec2 mouseS = ImGui::GetIO().MousePos;
                ImVec2 beforeW = m_canvas2D.ScreenToWorld(mouseS);

                m_sketchZoom = next;
                m_canvas2D.pixels_per_unit = m_sketchZoom;

                // Adjust pan so zoom centers on cursor position
                ImVec2 afterS = m_canvas2D.WorldToScreen(beforeW);
                ImVec2 delta = ImVec2(mouseS.x - afterS.x, mouseS.y - afterS.y);
                m_sketchPan.x += delta.x;
                m_sketchPan.y += delta.y;
            }
        }

        // Pan with RMB drag
        if (hovered && ImGui::IsMouseDragging(ImGuiMouseButton_Right, 0.0f)) {
            ImVec2 d = ImGui::GetIO().MouseDelta;
            m_sketchPan.x += d.x;
            m_sketchPan.y += d.y;
        }

        // Zoom with wheel (towards cursor) - alternative zoom method
        if (hovered && ImGui::GetIO().MouseWheel != 0.0f && !ImGui::IsAnyItemActive()) {
            float old = m_sketchZoom;
            float next = std::clamp(old * (1.0f + ImGui::GetIO().MouseWheel * 0.10f), 5.0f, 400.0f);
            if (next != old) {
                ImVec2 mouseS = ImGui::GetIO().MousePos;
                ImVec2 beforeW = m_canvas2D.ScreenToWorld(mouseS);

                m_sketchZoom = next;
                m_canvas2D.pixels_per_unit = m_sketchZoom;

                ImVec2 afterS = m_canvas2D.WorldToScreen(beforeW);
                ImVec2 delta = ImVec2(mouseS.x - afterS.x, mouseS.y - afterS.y);
                m_sketchPan.x += delta.x;
                m_sketchPan.y += delta.y;
            }
        }

        // Draw grid
        {
            const float ppu = m_canvas2D.pixels_per_unit;
            const ImVec2 origin = canvasPos;
            const ImVec2 end = ImVec2(canvasPos.x + canvasSize.x, canvasPos.y + canvasSize.y);
            const ImVec2 center = ImVec2(
                canvasPos.x + canvasSize.x * 0.5f + m_sketchPan.x,
                canvasPos.y + canvasSize.y * 0.5f + m_sketchPan.y);

            ImVec2 w0 = m_canvas2D.ScreenToWorld(origin);
            ImVec2 w1 = m_canvas2D.ScreenToWorld(end);
            float xmin = std::floor((std::min)(w0.x, w1.x)) - 1.0f;
            float xmax = std::ceil((std::max)(w0.x, w1.x)) + 1.0f;
            float ymin = std::floor((std::min)(w0.y, w1.y)) - 1.0f;
            float ymax = std::ceil((std::max)(w0.y, w1.y)) + 1.0f;

            for (int x = (int)xmin; x <= (int)xmax; ++x) {
                ImVec2 a = ImVec2(center.x + x * ppu, origin.y);
                ImVec2 b = ImVec2(center.x + x * ppu, end.y);
                ImU32 col = (x == 0) ? IM_COL32(80, 120, 255, 255) : IM_COL32(40, 40, 40, 255);
                dl->AddLine(a, b, col, (x == 0) ? 2.0f : 1.0f);
            }

            for (int y = (int)ymin; y <= (int)ymax; ++y) {
                ImVec2 a = ImVec2(origin.x, center.y - y * ppu);
                ImVec2 b = ImVec2(end.x, center.y - y * ppu);
                ImU32 col = (y == 0) ? IM_COL32(255, 80, 80, 255) : IM_COL32(40, 40, 40, 255);
                dl->AddLine(a, b, col, (y == 0) ? 2.0f : 1.0f);
            }
        }

        // Render existing entities
        {
            auto isHi = [&](domain::sketch::EntityId id) -> bool {
                if (m_uiHoverId != 0 && id == m_uiHoverId) return true;
                for (auto pid : m_uiPickedIds) if (pid == id) return true;
                return false;
                };

            const ImU32 baseCol = IM_COL32(60, 90, 180, 255);
            const ImU32 hiCol = IM_COL32(255, 220, 0, 255);

            // Lines
            for (const auto& l : sketch.entities.lines()) {
                ImVec2 aW{ (float)l.a.x, (float)l.a.y };
                ImVec2 bW{ (float)l.b.x, (float)l.b.y };
                dl->AddLine(m_canvas2D.WorldToScreen(aW), m_canvas2D.WorldToScreen(bW), isHi(l.h.id) ? hiCol : baseCol, 2.0f);
            }

            // Circles
            for (const auto& c : sketch.entities.circles()) {
                ImVec2 ctrW{ (float)c.center.x, (float)c.center.y };
                float r = (float)c.radius;
                dl->AddCircle(m_canvas2D.WorldToScreen(ctrW), r * m_canvas2D.pixels_per_unit, isHi(c.h.id) ? hiCol : baseCol, 0, 2.0f);
            }

            // Points (optional)
            for (const auto& p : sketch.entities.points()) {
                ImVec2 pW{ (float)p.p.x, (float)p.p.y };
                dl->AddCircleFilled(m_canvas2D.WorldToScreen(pW), 3.0f, IM_COL32(200, 200, 200, 255));
            }

            // Constraint glyphs (geometric)
            {
                using namespace domain::sketch;

                auto findLine = [&](EntityId id) -> const Line2D* {
                    for (const auto& l : sketch.entities.lines()) if (l.h.id == id) return &l;
                    return nullptr;
                    };
                auto findCircle = [&](EntityId id) -> const Circle2D* {
                    for (const auto& c : sketch.entities.circles()) if (c.h.id == id) return &c;
                    return nullptr;
                    };

                auto norm2 = [](ImVec2 v) { return v.x * v.x + v.y * v.y; };
                auto len = [&](ImVec2 v) { return std::sqrt(norm2(v)); };
                auto norm = [&](ImVec2 v) {
                    float l = len(v);
                    return (l > 1e-6f) ? ImVec2(v.x / l, v.y / l) : ImVec2(1, 0);
                    };
                auto sub = [&](ImVec2 a, ImVec2 b) { return ImVec2(a.x - b.x, a.y - b.y); };
                auto add = [&](ImVec2 a, ImVec2 b) { return ImVec2(a.x + b.x, a.y + b.y); };
                auto mul = [&](ImVec2 a, float s) { return ImVec2(a.x * s, a.y * s); };
                auto dot = [&](ImVec2 a, ImVec2 b) { return a.x * b.x + a.y * b.y; };

                const ImU32 iconCol = IM_COL32(255, 255, 255, 220);
                const float iconSizePx = 18.0f;

                for (const auto& cvar : sketch.constraints) {
                    if (!std::holds_alternative<GeometricConstraint>(cvar))
                        continue;

                    const auto& gc = std::get<GeometricConstraint>(cvar);
                    if (!gc.meta.enabled || gc.meta.suppressed)
                        continue;
                    if (gc.type != GeometricConstraintType::Tangent)
                        continue;
                    if (gc.refs.size() < 2)
                        continue;

                    const EntityId aId = gc.refs[0].id;
                    const EntityId bId = gc.refs[1].id;

                    // Try Line-Circle first (either order)
                    const Line2D* line = findLine(aId);
                    const Circle2D* cir = findCircle(bId);
                    if (!line || !cir) {
                        line = findLine(bId);
                        cir = findCircle(aId);
                    }

                    bool drawn = false;

                    if (line && cir) {
                        ImVec2 A{ (float)line->a.x, (float)line->a.y };
                        ImVec2 B{ (float)line->b.x, (float)line->b.y };
                        ImVec2 C{ (float)cir->center.x, (float)cir->center.y };

                        ImVec2 d = sub(B, A);
                        float d2 = norm2(d);
                        if (d2 > 1e-8f) {
                            float t = dot(sub(C, A), d) / d2;
                            ImVec2 P = add(A, mul(d, t));

                            ImVec2 Ps = m_canvas2D.WorldToScreen(P);
                            ImVec2 Cs = m_canvas2D.WorldToScreen(C);
                            ImVec2 nS = norm(sub(Cs, Ps));
                            ImVec2 iconPos = add(Ps, mul(nS, 14.0f));

                            adapters::sketchui::DrawConstraintIcon(
                                dl, iconPos, iconSizePx, iconCol, adapters::sketchui::ConstraintIcon::Tangent);

                            drawn = true;
                        }
                    }

                    if (drawn) continue;

                    // Circle-Circle (external tangency)
                    const Circle2D* c1 = findCircle(aId);
                    const Circle2D* c2 = findCircle(bId);
                    if (c1 && c2) {
                        ImVec2 C1{ (float)c1->center.x, (float)c1->center.y };
                        ImVec2 C2{ (float)c2->center.x, (float)c2->center.y };
                        ImVec2 v = sub(C2, C1);
                        float L = len(v);
                        if (L > 1e-6f) {
                            ImVec2 dir = mul(v, 1.0f / L);
                            ImVec2 P = add(C1, mul(dir, (float)c1->radius));

                            ImVec2 Ps = m_canvas2D.WorldToScreen(P);
                            ImVec2 C1s = m_canvas2D.WorldToScreen(C1);
                            ImVec2 nS = norm(sub(Ps, C1s));
                            ImVec2 iconPos = add(Ps, mul(nS, 12.0f));

                            adapters::sketchui::DrawConstraintIcon(
                                dl, iconPos, iconSizePx, iconCol, adapters::sketchui::ConstraintIcon::Tangent);
                        }
                    }
                }
            }
        }

        // Tool update/draw (draft geometry + in-canvas dimension editing)
        {
            adapters::sketchui::ToolContext tctx{
                sketch,
                m_cmdHistory,
                &m_activeConstraintIcon,
                &m_sketchNeedsSolve,
                &m_sketchChangeSerial,
                &m_uiPickedIds,
                &m_uiHoverId
            };
            if (m_skipToolUpdateOnce)
            {
                // Prevent the same click that activated a tool from being processed
                // by the tool/canvas logic later in this frame.
                m_skipToolUpdateOnce = false;
            }
            else
            {
                m_toolManager.UpdateAndDraw(tctx, m_canvas2D, dl);
            }
        }

        // ---- Always-on sketch cursor (draw last so it's on top) ----
        if (hovered)
        {
            const ImVec2 p = ImGui::GetMousePos();
            const ImVec2 cmin = canvasPos;
            const ImVec2 cmax = ImVec2(canvasPos.x + canvasSize.x, canvasPos.y + canvasSize.y);
            const bool inside =
                (p.x >= cmin.x && p.x <= cmax.x && p.y >= cmin.y && p.y <= cmax.y);

            if (inside)
            {
                const ImU32 col = (m_uiHoverId != 0)
                    ? IM_COL32(255, 210, 0, 255)
                    : IM_COL32(220, 220, 220, 255);

                const float s = 7.0f;
                const float thickness = 2.0f;

                dl->AddLine(ImVec2(p.x - s, p.y - s), ImVec2(p.x + s, p.y + s), col, thickness);
                dl->AddLine(ImVec2(p.x - s, p.y + s), ImVec2(p.x + s, p.y - s), col, thickness);
            }
        }

        // (OCCT overlay sync disabled in plugin-hotload build)
        ImGui::End();

        if (m_config && wasOpen != m_viewSketchEditor)
            m_config->set("pistachio.ui", "views.sketchEditor", m_viewSketchEditor);
    }

} // namespace adapters

// -----------------------------------------------------------------------------
// Build integration helper
// -----------------------------------------------------------------------------
// Some Visual Studio project configurations do not automatically include newly
// added .cpp files in the pistachio_ui target. If GlSketch3DViewRenderer.cpp is
// not part of that target, you'll see linker errors for GlSketch3DViewRenderer
// symbols. Including the implementation here ensures the Sketch3D renderer is
// compiled into the UI plugin without requiring build-file changes.

#include "../rendering/GlSketch3DViewRenderer.cpp"
#include "../rendering/FramebufferManager.cpp"