#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <GL/gl.h>

#include "adapters/ui/ImGuiAdapter.h"
#include "core/Application.h"

#include <imgui.h>
#include <imgui_internal.h>
#include <GLFW/glfw3.h>
#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3native.h>

#include <cstring>
#include <algorithm>

#include "../Roboto-Regular.embed"
#include "../../../Walnut-Icon.embed"
#include "../../../WindowImages.embed"

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h" // or wherever you include it (likely already in your project)

static GLuint CreateGLTextureRGBA_Minimal(const unsigned char* rgba, int w, int h)
{
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



namespace adapters {

    // ------------------------------------------------------------
    // ctor / dtor
    // ------------------------------------------------------------

    ImGuiAdapter::ImGuiAdapter(core::Application* app, GLFWwindow* hostWindow, IGuiHost* host)
        : m_app(app), m_window(hostWindow), m_host(host)
    {
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
            io.FontDefault = io.Fonts->AddFontFromMemoryTTF(
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
        printf("[PLUGIN] ctx=%p\n", (void*)ImGui::GetCurrentContext());

        // ⚠️ DO NOT touch ImGuiIO.Fonts here

        renderMainMenu();
        renderStatusBar();
        renderModelInfo();
        render3DView();
        renderSketchEditor();
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
        ImGui::Begin("File Operations");

        static char filePath[512] = {};
        ImGui::InputTextWithHint("##file", "STEP file path...", filePath, sizeof(filePath));

        if (ImGui::Button("Load STEP"))
        {
            if (m_app && filePath[0])
                m_app->loadFile(filePath);
        }

        ImGui::End();
    }

    void ImGuiAdapter::renderStatusBar()
    {
        ImGui::Begin("Status", nullptr, ImGuiWindowFlags_NoScrollbar);

        if (m_app)
            ImGui::Text("Status: %s", m_app->getStatus().c_str());

        ImGui::End();
    }

    void ImGuiAdapter::renderModelInfo()
    {
        ImGui::Begin("Model Info");

        if (!m_app)
        {
            ImGui::TextDisabled("No application");
            ImGui::End();
            return;
        }

        auto model = m_app->getCurrentModel();
        if (!model)
        {
            ImGui::TextDisabled("No model loaded");
            ImGui::End();
            return;
        }

        ImGui::Text("Model loaded");
        ImGui::End();
    }

    void ImGuiAdapter::render3DView()
    {
        ImGui::Begin("3D Viewport");

        if (m_app && m_app->getRenderer())
        {
            ImVec2 size = ImGui::GetContentRegionAvail();
            void* tex = m_app->getRenderer()->getFramebufferTexture();
            if (tex)
                ImGui::Image(tex, size);
        }
        else
        {
            ImGui::TextDisabled("No renderer");
        }

        ImGui::End();
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

void ImGuiAdapter::renderSketchEditor()
{
    ImGui::Begin("Sketch Editor");

    auto doc = m_app->getSketchDocument();
    static int s_lastSketchIdx = -1;
    if (!doc || doc->sketches.empty()) {
        ImGui::TextDisabled("No sketch document loaded");
        ImGui::End();
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

    const bool hovered = ImGui::IsItemHovered();

    // Update canvas mapping
    m_canvas2D.origin_screen = canvasPos;
    m_canvas2D.size = canvasSize;
    m_canvas2D.pan_screen = m_sketchPan;
    m_canvas2D.pixels_per_unit = m_sketchZoom;

    // === Hover picking (for delete + visual feedback) ===
    if (hovered)
    {
        const float tolW = 6.0f / std::max(m_canvas2D.pixels_per_unit, 1.0f);
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

        // Delete hovered entity (single entity) with undo/redo
        if (m_uiHoverId != 0
            && !ImGui::IsAnyItemActive()
            && !ImGui::GetIO().WantTextInput
            && (ImGui::IsKeyPressed(ImGuiKey_Delete) || ImGui::IsKeyPressed(ImGuiKey_Backspace)))
        {
            m_cmdHistory.Execute(std::make_unique<core::commands::DeleteEntityCommand>(sketch, m_uiHoverId));

            // Clear UI picks (so tools don't think the entity is still there)
            m_uiPickedIds.clear();
            m_uiHoverId = 0;

            // Force solve + redraw
            m_sketchNeedsSolve = true;
            ++m_sketchChangeSerial;
        }
    }
    else
    {
        // Not hovering canvas; clear hover id so highlights don't stick
        m_uiHoverId = 0;
    }

    // Pan with MMB drag
    if (hovered && ImGui::IsMouseDragging(ImGuiMouseButton_Middle, 0.0f)) {
        ImVec2 d = ImGui::GetIO().MouseDelta;
        m_sketchPan.x += d.x;
        m_sketchPan.y += d.y;
    }

    // Zoom with wheel (towards cursor)
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
}

} // namespace adapters