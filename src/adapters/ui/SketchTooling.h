#pragma once

#include <algorithm>
#include <cmath>
#include <memory>
#include <vector>
#include <cstdlib>
#include <cstdio>

#include <imgui.h>
#include <imgui_internal.h>

#include "core/commands/CommandHistory.h"
#include "core/commands/SketchCommands.h"
#include "domain/SketchModel.h"

namespace adapters::sketchui {

    // ImVec2 has no operator overloads in Dear ImGui. Keep math explicit.
    inline ImVec2 VAdd(const ImVec2& a, const ImVec2& b) { return ImVec2(a.x + b.x, a.y + b.y); }
    inline ImVec2 VSub(const ImVec2& a, const ImVec2& b) { return ImVec2(a.x - b.x, a.y - b.y); }
    inline ImVec2 VMul(const ImVec2& a, float s) { return ImVec2(a.x * s, a.y * s); }
    inline ImVec2 VDiv(const ImVec2& a, float s) { return ImVec2(a.x / s, a.y / s); }



    // ImGui's ImRect lives in imgui_internal.h; keep a tiny local rect to avoid that include.
    struct Rect2 {
        ImVec2 Min{ 0,0 };
        ImVec2 Max{ 0,0 };
    };

    // Simple 2D canvas mapping (world <-> screen).
    struct Canvas2D {
        ImVec2 origin_screen{ 0,0 }; // top-left of canvas in screen coords
        ImVec2 size{ 0,0 };

        // world-space origin mapped to screen center + pan
        ImVec2 pan_screen{ 0,0 };
        float pixels_per_unit = 40.0f;

        Rect2 rect() const { return Rect2{ origin_screen, VAdd(origin_screen, size) }; }

        ImVec2 WorldToScreen(const ImVec2& w) const
        {
            ImVec2 center = VAdd(VAdd(origin_screen, ImVec2(size.x * 0.5f, size.y * 0.5f)), pan_screen);
            // +Y up in world, -Y down on screen
            return ImVec2(center.x + w.x * pixels_per_unit,
                          center.y - w.y * pixels_per_unit);
        }

        ImVec2 ScreenToWorld(const ImVec2& s) const
        {
            ImVec2 center = VAdd(VAdd(origin_screen, ImVec2(size.x * 0.5f, size.y * 0.5f)), pan_screen);
            return ImVec2((s.x - center.x) / pixels_per_unit,
                          -(s.y - center.y) / pixels_per_unit);
        }
    };

    enum class ToolKind { None, Select, Line2Pt };
    enum class DraftStage { Idle, PickingStart, PickingEnd, AdjustingValue };

    struct ToolContext {
        domain::sketch::Sketch& sketch;
        core::commands::CommandHistory& history;
    };

    class ISketchTool {
    public:
        virtual ~ISketchTool() = default;
        virtual ToolKind Kind() const = 0;
        virtual void Begin(ToolContext&) {}
        virtual void Cancel(ToolContext&) {}
        virtual bool IsActive() const = 0;
        virtual void UpdateAndDraw(ToolContext&, const Canvas2D&, ImDrawList*) = 0;
    };

    class ToolManager {
    public:
        void Register(std::unique_ptr<ISketchTool> t) { m_tools.push_back(std::move(t)); }

        void Activate(ToolKind k, ToolContext& ctx)
        {
            if (m_active && m_active->Kind() == k) return;
            if (m_active) m_active->Cancel(ctx);
            m_active = Find(k);
            if (m_active) m_active->Begin(ctx);
        }

        void Deactivate(ToolContext& ctx)
        {
            if (m_active) m_active->Cancel(ctx);
            m_active = nullptr;
        }

        ToolKind ActiveKind() const { return m_active ? m_active->Kind() : ToolKind::None; }

        void UpdateAndDraw(ToolContext& ctx, const Canvas2D& c, ImDrawList* dl)
        {
            if (!m_active) return;

            if (!m_active->IsActive())
            {
                m_active = nullptr;
                return;
            }

            m_active->UpdateAndDraw(ctx, c, dl);

            // If tool became inactive during UpdateAndDraw (Esc/cancel), clear it
            if (m_active && !m_active->IsActive())
                m_active = nullptr;
        }

    private:
        ISketchTool* Find(ToolKind k)
        {
            for (auto& t : m_tools)
                if (t->Kind() == k) return t.get();
            return nullptr;
        }

        std::vector<std::unique_ptr<ISketchTool>> m_tools;
        ISketchTool* m_active = nullptr;
    };

    // --- Helpers ---
    inline bool ParseDouble(const char* s, double& out)
    {
        if (!s || !*s) return false;
        char* end = nullptr;
        double v = std::strtod(s, &end);
        if (end == s) return false;
        out = v;
        return true;
    }

    inline bool DrawDimensionEditBox(
        const Canvas2D& canvas, ImDrawList* dl, const char* id,
        ImVec2 aW, ImVec2 bW,
        char* buf, int bufSize,
        bool requestFocus)
    {
        ImVec2 aS = canvas.WorldToScreen(aW);
        ImVec2 bS = canvas.WorldToScreen(bW);

        dl->AddLine(aS, bS, IM_COL32(255, 220, 120, 255), 1.5f);

        ImVec2 d = VSub(bS, aS);
        float len = std::sqrt(d.x * d.x + d.y * d.y);
        ImVec2 n = (len > 1e-6f) ? ImVec2(-d.y / len, d.x / len) : ImVec2(0, -1);

        ImVec2 mid = VMul(VAdd(aS, bS), 0.5f);
        ImVec2 pos = VAdd(mid, VMul(n, 14.0f));

        float w = 72.0f;
        ImGui::SetCursorScreenPos(ImVec2(pos.x - w * 0.5f, pos.y - ImGui::GetFrameHeight() * 0.5f));

        ImGui::PushID(id);
        ImGui::SetNextItemWidth(w);
        if (requestFocus) ImGui::SetKeyboardFocusHere();
        bool enter = ImGui::InputText("##dim", buf, bufSize,
            ImGuiInputTextFlags_AutoSelectAll | ImGuiInputTextFlags_EnterReturnsTrue);
        ImGui::PopID();

        return enter;
    }

    inline void DrawDimensionLabel(
        const Canvas2D& canvas,
        ImDrawList* dl,
        const ImVec2& aW,
        const ImVec2& bW,
        const char* units = "mm"
    )
    {
        ImVec2 midW{ (aW.x + bW.x) * 0.5f, (aW.y + bW.y) * 0.5f };
        ImVec2 midS = canvas.WorldToScreen(midW);
        const float dx = bW.x - aW.x;
        const float dy = bW.y - aW.y;
        const double len = std::sqrt((double)dx * (double)dx + (double)dy * (double)dy);

        char buf[64];
        std::snprintf(buf, sizeof(buf), "%.2f %s", len, units);

        ImVec2 textSize = ImGui::CalcTextSize(buf);
        ImVec2 pad{ 6.0f, 3.0f };
        ImRect r(ImVec2(midS.x - textSize.x * 0.5f - pad.x, midS.y - textSize.y * 0.5f - pad.y),
                 ImVec2(midS.x + textSize.x * 0.5f + pad.x, midS.y + textSize.y * 0.5f + pad.y));
        dl->AddRectFilled(r.Min, r.Max, IM_COL32(10, 10, 10, 220), 4.0f);
        dl->AddRect(r.Min, r.Max, IM_COL32(180, 180, 180, 180), 4.0f);
        dl->AddText(ImVec2(r.Min.x + pad.x, r.Min.y + pad.y), IM_COL32(230, 230, 230, 255), buf);
    }

    inline float DistPointToSegment(const ImVec2& p, const ImVec2& a, const ImVec2& b)
    {
        const float vx = b.x - a.x;
        const float vy = b.y - a.y;
        const float wx = p.x - a.x;
        const float wy = p.y - a.y;
        const float vv = vx * vx + vy * vy;
        float t = 0.0f;
        if (vv > 1e-12f) {
            t = (wx * vx + wy * vy) / vv;
            t = std::max(0.0f, std::min(1.0f, t));
        }
        const float px = a.x + t * vx;
        const float py = a.y + t * vy;
        const float dx = p.x - px;
        const float dy = p.y - py;
        return std::sqrt(dx * dx + dy * dy);
    }

    inline bool IsSelected(const domain::sketch::Sketch& sk, domain::sketch::EntityId id)
    {
        const auto& v = sk.selectedEntities;
        return std::find(v.begin(), v.end(), id) != v.end();
    }

    inline void ClearSelection(domain::sketch::Sketch& sk)
    {
        sk.selectedEntities.clear();
    }

    inline void ToggleSelected(domain::sketch::Sketch& sk, domain::sketch::EntityId id)
    {
        auto& v = sk.selectedEntities;
        auto it = std::find(v.begin(), v.end(), id);
        if (it == v.end()) v.push_back(id);
        else v.erase(it);
    }

    inline void SetSingleSelection(domain::sketch::Sketch& sk, domain::sketch::EntityId id)
    {
        sk.selectedEntities.clear();
        sk.selectedEntities.push_back(id);
    }

    inline domain::sketch::EntityId HitTestEntity(const domain::sketch::Sketch& sk, const ImVec2& mouseW, float tolW)
    {
        // Points first
        for (auto const& p : sk.entities.points()) {
            if (!p.h.visible || !p.h.selectable) continue;
            const float dx = (float)p.p.x - mouseW.x;
            const float dy = (float)p.p.y - mouseW.y;
            if (dx * dx + dy * dy <= tolW * tolW)
                return p.h.id;
        }

        // Lines
        for (auto const& l : sk.entities.lines()) {
            if (!l.h.visible || !l.h.selectable) continue;
            ImVec2 a{ (float)l.a.x, (float)l.a.y };
            ImVec2 b{ (float)l.b.x, (float)l.b.y };
            if (DistPointToSegment(mouseW, a, b) <= tolW)
                return l.h.id;
        }

        return 0; // 0 = none (EntityId starts at 1)
    }

    // Select tool: click to select entities (Shift=toggle). Escape / click empty clears.
    class SelectTool final : public ISketchTool {
    public:
        ToolKind Kind() const override { return ToolKind::Select; }
        bool IsActive() const override { return m_active; }

        void Begin(ToolContext&) override { m_active = true; }
        void Cancel(ToolContext& ctx) override { m_active = false; /* keep selection */ }

        void UpdateAndDraw(ToolContext& ctx, const Canvas2D& canvas, ImDrawList*) override
        {
            if (!m_active) return;

            const Rect2 r = canvas.rect();
            const ImVec2 mouseS = ImGui::GetMousePos();
            const bool inCanvas = (mouseS.x >= r.Min.x && mouseS.x <= r.Max.x && mouseS.y >= r.Min.y && mouseS.y <= r.Max.y);

            // Escape clears
            if (ImGui::IsKeyPressed(ImGuiKey_Escape)) {
                ClearSelection(ctx.sketch);
            }

            if (inCanvas && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
                const ImVec2 mouseW = canvas.ScreenToWorld(mouseS);
                const float tolW = 6.0f / canvas.pixels_per_unit; // ~6px

                const auto hit = HitTestEntity(ctx.sketch, mouseW, tolW);
                if (hit != 0) {
                    const bool shift = ImGui::GetIO().KeyShift;
                    if (shift) ToggleSelected(ctx.sketch, hit);
                    else SetSingleSelection(ctx.sketch, hit);
                }
                else {
                    ClearSelection(ctx.sketch);
                }
            }
        }

    private:
        bool m_active = false;
    };

    // Line: click start, click end, then inline edit length (enter to commit).
    class Line2PtTool final : public ISketchTool {
    public:
        ToolKind Kind() const override { return ToolKind::Line2Pt; }
        bool IsActive() const override { return m_active; }

        void Begin(ToolContext&) override
        {
            m_active = true;
            m_stage = DraftStage::PickingStart;
            m_hasStart = false;
            m_focusEdit = false;
            m_buf[0] = '\0';
        }

        void Cancel(ToolContext&) override
        {
            m_active = false;
            m_stage = DraftStage::Idle;
            m_hasStart = false;
            m_focusEdit = false;
        }

        void UpdateAndDraw(ToolContext& ctx, const Canvas2D& canvas, ImDrawList* dl) override
        {
            const Rect2 r = canvas.rect();
            const bool hovered =
                ImGui::IsWindowHovered(ImGuiHoveredFlags_AllowWhenBlockedByActiveItem) &&
                ImGui::IsMouseHoveringRect(r.Min, r.Max);

            if (ImGui::IsKeyPressed(ImGuiKey_Escape) ||
                (hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Right))) {
                Cancel(ctx);
                return;
            }

            ImVec2 mouseW = canvas.ScreenToWorld(ImGui::GetIO().MousePos);

            switch (m_stage) {
            case DraftStage::PickingStart:
                if (hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
                    m_aW = mouseW;
                    m_bW = mouseW;
                    m_hasStart = true;
                    m_stage = DraftStage::PickingEnd;
                }
                break;

            case DraftStage::PickingEnd:
                if (m_hasStart) m_bW = mouseW;
                if (hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
                    ImVec2 d = VSub(m_bW, m_aW);
                    double L = std::sqrt((double)d.x * d.x + (double)d.y * d.y);
                    if (L < 1e-9) L = 1.0;

                    m_dirW = ImVec2((float)(d.x / (float)L), (float)(d.y / (float)L));
                    m_len = L;

                    snprintf(m_buf, sizeof(m_buf), "%.3f", m_len);
                    m_focusEdit = true;
                    m_stage = DraftStage::AdjustingValue;
                }
                break;

            case DraftStage::AdjustingValue: {
                double v;
                if (ParseDouble(m_buf, v))
                    m_len = std::max(0.0001, v);
                m_bW = VAdd(m_aW, VMul(m_dirW, (float)m_len));
            } break;

            default: break;
            }

            if (!m_hasStart) return;

            // preview line
            dl->AddLine(canvas.WorldToScreen(m_aW), canvas.WorldToScreen(m_bW), IM_COL32(255, 255, 0, 255), 2.0f);

            if (m_stage == DraftStage::AdjustingValue) {
                bool enter = DrawDimensionEditBox(canvas, dl, "LineLen", m_aW, m_bW, m_buf, (int)sizeof(m_buf), m_focusEdit);
                m_focusEdit = false;
                if (enter) {
                    domain::sketch::Vec2 a{ (double)m_aW.x, (double)m_aW.y };
                    domain::sketch::Vec2 b{ (double)m_bW.x, (double)m_bW.y };
                    ctx.history.Execute(std::make_unique<core::commands::AddLine2DCommand>(ctx.sketch, a, b));

                    // keep tool active for next segment
                    m_aW = m_bW;
                    ImGui::ClearActiveID();           // releases InputText active state
                    m_stage = DraftStage::PickingStart; // or PickingEnd if you want chaining
                    m_hasStart = false;
                    m_focusEdit = false;
                    m_buf[0] = '\0';
                }
            }
        }

    private:
        bool m_active = false;
        DraftStage m_stage = DraftStage::Idle;
        bool m_hasStart = false;

        ImVec2 m_aW{}, m_bW{};
        ImVec2 m_dirW{ 1,0 };
        double m_len = 1.0;

        bool m_focusEdit = false;
        char m_buf[64]{};
    };

} // namespace adapters::sketchui
