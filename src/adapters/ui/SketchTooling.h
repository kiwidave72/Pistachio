#pragma once

#include <cstdint>

#include <algorithm>
#include <cmath>
#include <memory>
#include <vector>
#include <cstdlib>

#include <imgui.h>

#include "ConstraintIcons.h"

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

    
    // --------------------------------------------------------------------
    // Picking / hit-testing helpers (screen -> world already handled by Canvas2D)
    // --------------------------------------------------------------------
    enum class PickType : int { None=0, Point, Line, Circle, Arc };

    struct PickResult
    {
        PickType type{ PickType::None };
        domain::sketch::EntityId id{ 0 };
        float distance{ 1e30f }; // world units
    };

    static inline float VLen2(ImVec2 v) { return v.x*v.x + v.y*v.y; }
    static inline float VLen(ImVec2 v) { return std::sqrt(VLen2(v)); }

    static inline float DistancePointToSegment(ImVec2 p, ImVec2 a, ImVec2 b)
    {
        ImVec2 ab = VSub(b, a);
        float ab2 = VLen2(ab);
        if (ab2 <= 1e-12f) return VLen(VSub(p, a));
        float t = ( (p.x - a.x)*ab.x + (p.y - a.y)*ab.y ) / ab2;
        t = std::clamp(t, 0.0f, 1.0f);
        ImVec2 q = ImVec2(a.x + ab.x*t, a.y + ab.y*t);
        return VLen(VSub(p, q));
    }

    static inline float DistancePointToCircle(ImVec2 p, ImVec2 c, float r)
    {
        float d = VLen(VSub(p, c));
        return std::fabs(d - r);
    }

    static inline PickResult PickGeometry(const domain::sketch::Sketch& sketch, ImVec2 mouseW, float tolW)
    {
        PickResult best;

        // points
        for (const auto& pt : sketch.entities.points())
        {
            ImVec2 pw = ImVec2((float)pt.p.x, (float)pt.p.y);
            float d = VLen(VSub(mouseW, pw));
            if (d <= tolW && d < best.distance)
                best = { PickType::Point, pt.h.id, d };
        }

        // lines
        for (const auto& ln : sketch.entities.lines())
        {
            ImVec2 a = ImVec2((float)ln.a.x, (float)ln.a.y);
            ImVec2 b = ImVec2((float)ln.b.x, (float)ln.b.y);
            float d = DistancePointToSegment(mouseW, a, b);
            if (d <= tolW && d < best.distance)
                best = { PickType::Line, ln.h.id, d };
        }

        // circles
        for (const auto& cc : sketch.entities.circles())
        {
            ImVec2 c = ImVec2((float)cc.center.x, (float)cc.center.y);
            float d = DistancePointToCircle(mouseW, c, (float)cc.radius);
            if (d <= tolW && d < best.distance)
                best = { PickType::Circle, cc.h.id, d };
        }

        // arcs (optional: treat as circle hit on radius around center; angle window ignored for now)
        for (const auto& ac : sketch.entities.arcs())
        {
            ImVec2 c = ImVec2((float)ac.center.x, (float)ac.center.y);
            float d = DistancePointToCircle(mouseW, c, (float)ac.radius);
            if (d <= tolW && d < best.distance)
                best = { PickType::Arc, ac.h.id, d };
        }

        return best;
    }

    // --------------------------------------------------------------------
    // Snap-to-endpoint support for line creation tools
    // --------------------------------------------------------------------
    
    struct SnapResult {
        bool hasSnap = false;
        ImVec2 snapPointWorld{0, 0};
        domain::sketch::EntityId snapToEntityId = 0;
        domain::sketch::EntityAnchor snapAnchor = domain::sketch::EntityAnchor::None;
        float distance = 1e30f;
    };

    // Find the nearest line endpoint or point to snap to
    static inline SnapResult FindSnapPoint(
        const domain::sketch::Sketch& sketch,
        ImVec2 mouseW,
        float snapTolW)
    {
        SnapResult result;
        
        // Check all line endpoints
        for (const auto& ln : sketch.entities.lines()) {
            // Check start point
            ImVec2 startW = ImVec2((float)ln.a.x, (float)ln.a.y);
            float distToStart = VLen(VSub(mouseW, startW));
            
            if (distToStart <= snapTolW && distToStart < result.distance) {
                result.hasSnap = true;
                result.snapPointWorld = startW;
                result.snapToEntityId = ln.h.id;
                result.snapAnchor = domain::sketch::EntityAnchor::LineStart;
                result.distance = distToStart;
            }
            
            // Check end point
            ImVec2 endW = ImVec2((float)ln.b.x, (float)ln.b.y);
            float distToEnd = VLen(VSub(mouseW, endW));
            
            if (distToEnd <= snapTolW && distToEnd < result.distance) {
                result.hasSnap = true;
                result.snapPointWorld = endW;
                result.snapToEntityId = ln.h.id;
                result.snapAnchor = domain::sketch::EntityAnchor::LineEnd;
                result.distance = distToEnd;
            }
        }
        
        // Also check standalone points
        for (const auto& pt : sketch.entities.points()) {
            ImVec2 ptW = ImVec2((float)pt.p.x, (float)pt.p.y);
            float dist = VLen(VSub(mouseW, ptW));
            
            if (dist <= snapTolW && dist < result.distance) {
                result.hasSnap = true;
                result.snapPointWorld = ptW;
                result.snapToEntityId = pt.h.id;
                result.snapAnchor = domain::sketch::EntityAnchor::None;
                result.distance = dist;
            }
        }
        
        return result;
    }

    // --------------------------------------------------------------------
    // Tool types and base classes
    // --------------------------------------------------------------------

enum class ToolKind { None, Line2Pt, CircleCenterRadius, Constraint };
    enum class DraftStage { Idle, PickingStart, PickingEnd, AdjustingValue };

    struct ToolContext {
        domain::sketch::Sketch& sketch;
        core::commands::CommandHistory& history;
        int* activeConstraintIcon = nullptr;
        bool* needsSolve = nullptr;
        uint64_t* changeSerial = nullptr;

        // UI-only selection feedback (no persistence in the sketch model)
        std::vector<domain::sketch::EntityId>* uiPickedIds = nullptr; // current pick sequence
        domain::sketch::EntityId* uiHoverId = nullptr;               // current hover (0 = none)

        void MarkDirty() {
            if (needsSolve) *needsSolve = true;
            if (changeSerial) ++(*changeSerial);
        }
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

        void UpdateAndDraw(ToolContext& ctx, const Canvas2D& canvas, ImDrawList* dl)
        {
            if (m_active && m_active->IsActive())
                m_active->UpdateAndDraw(ctx, canvas, dl);
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
        bool requestFocus,
        bool* outActive)
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

        // InputText returns true when Enter is used to validate with EnterReturnsTrue.
        const bool inputEnter = ImGui::InputText("##dim", buf, bufSize,
            ImGuiInputTextFlags_AutoSelectAll | ImGuiInputTextFlags_EnterReturnsTrue);

        const bool inputActive = ImGui::IsItemActive();
        // Some ImGui versions only reliably report deactivation (not "after edit"),
        // so we return active state for the caller to detect focus loss.
        const bool inputDeactivatedAfterEdit = ImGui::IsItemDeactivatedAfterEdit();

        ImGui::PopID();

        if (outActive) *outActive = inputActive;

        // Commit if InputText explicitly validated, or if ImGui reports deactivated-after-edit.
        return inputEnter || inputDeactivatedAfterEdit;
    }

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
                    m_len = (std::max)(0.0001, v);
                m_bW = VAdd(m_aW, VMul(m_dirW, (float)m_len));
            } break;

            default: break;
            }

            if (!m_hasStart) return;

            // preview line
            dl->AddLine(canvas.WorldToScreen(m_aW), canvas.WorldToScreen(m_bW), IM_COL32(255, 255, 0, 255), 2.0f);

            if (m_stage == DraftStage::AdjustingValue) {
                bool activeNow = false;
                bool enter = DrawDimensionEditBox(canvas, dl, "LineLen", m_aW, m_bW, m_buf, (int)sizeof(m_buf), m_focusEdit, &activeNow);
                if (!enter && m_editWasActive && !activeNow) enter = true;
                m_editWasActive = activeNow;
                m_focusEdit = false;
                if (enter) {
                    domain::sketch::Vec2 a{ (double)m_aW.x, (double)m_aW.y };
                    domain::sketch::Vec2 b{ (double)m_bW.x, (double)m_bW.y };
                    ctx.history.Execute(std::make_unique<core::commands::AddLine2DCommand>(ctx.sketch, a, b));
                    ctx.MarkDirty();

                    // Reset to start a new line (separate entity)
                    m_stage = DraftStage::PickingStart;
                    m_hasStart = false;
                    m_editWasActive = false;
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
        bool m_editWasActive = false;
        char m_buf[64]{};
    };

    // Circle: click center, click radius point, then inline edit radius (enter to commit).
    class CircleCenterRadiusTool final : public ISketchTool {
    public:
        ToolKind Kind() const override { return ToolKind::CircleCenterRadius; }
        bool IsActive() const override { return m_active; }

        void Begin(ToolContext&) override
        {
            m_active = true;
            m_stage = DraftStage::PickingStart; // center
            m_hasCenter = false;
            m_focusEdit = false;
            m_buf[0] = '\0';
            m_r = 1.0;
        }

        void Cancel(ToolContext&) override
        {
            m_active = false;
            m_stage = DraftStage::Idle;
            m_hasCenter = false;
        }

        void UpdateAndDraw(ToolContext& ctx, const Canvas2D& canvas, ImDrawList* dl) override
        {
            if (!m_active) return;

            const bool hovered = ImGui::IsWindowHovered(ImGuiHoveredFlags_AllowWhenBlockedByActiveItem);
            const ImVec2 mouseW = canvas.ScreenToWorld(ImGui::GetIO().MousePos);

            if (ImGui::IsKeyPressed(ImGuiKey_Escape)) {
                Cancel(ctx);
                return;
            }

            // Commit is driven by the inline edit box (see DrawDimensionEditBox). Do not rely on
            // raw Enter polling here because an active InputText can own the keyboard.

            // stage progression
            if (m_stage == DraftStage::PickingStart) {
                if (hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
                    m_centerW = mouseW;
                    m_hasCenter = true;
                    m_stage = DraftStage::PickingEnd;
                }
            }
            else if (m_stage == DraftStage::PickingEnd) {
                if (m_hasCenter) {
                    ImVec2 d = VSub(mouseW, m_centerW);
                    double rr = std::sqrt((double)d.x * d.x + (double)d.y * d.y);
                    if (rr < 1e-6) rr = 1.0;
                    m_r = rr;
                }
                if (hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
                    snprintf(m_buf, sizeof(m_buf), "%.3f", m_r);
                    m_focusEdit = true;
                    m_stage = DraftStage::AdjustingValue;
                }
            }
            else if (m_stage == DraftStage::AdjustingValue) {
                double v;
                if (ParseDouble(m_buf, v))
                    m_r = (std::max)(0.0001, v);
            }

            if (!m_hasCenter) return;

            // preview circle
            dl->AddCircle(canvas.WorldToScreen(m_centerW), (float)(m_r * canvas.pixels_per_unit), IM_COL32(255, 255, 0, 255), 0, 2.0f);

            // preview dimension as radius line and edit box at midpoint
            ImVec2 edgeW = VAdd(m_centerW, ImVec2((float)m_r, 0.0f));
            ImVec2 aS = canvas.WorldToScreen(m_centerW);
            ImVec2 bS = canvas.WorldToScreen(edgeW);
            dl->AddLine(aS, bS, IM_COL32(255, 255, 0, 255), 1.5f);

            char dimText[64];
            snprintf(dimText, sizeof(dimText), "R %.3f", m_r);
            ImVec2 midS = VMul(VAdd(aS, bS), 0.5f);
            ImVec2 sz = ImGui::CalcTextSize(dimText);
            dl->AddText(ImVec2(midS.x - sz.x * 0.5f, midS.y - sz.y - 6.0f), IM_COL32(255, 255, 0, 255), dimText);

            if (m_stage == DraftStage::AdjustingValue) {
                // If user clears, keep a value visible so it doesn't look broken.
                if (m_buf[0] == '\0')
                    snprintf(m_buf, sizeof(m_buf), "%.3f", m_r);

                bool activeNow = false;
                bool enter = DrawDimensionEditBox(canvas, dl, "CircleRad",
                    m_centerW, edgeW, m_buf, (int)sizeof(m_buf), m_focusEdit, &activeNow);
                m_focusEdit = false;
                if (!enter && m_editWasActive && !activeNow) enter = true;
                m_editWasActive = activeNow;

                if (enter) {
                    Commit(ctx);
                    m_editWasActive = false;
                    // Keep tool active for the next circle
                    Begin(ctx);
                    return;
                }
            }
        }

    private:
        void Commit(ToolContext& ctx)
        {
            domain::sketch::Vec2 c{ (double)m_centerW.x, (double)m_centerW.y };
            ctx.history.Execute(std::make_unique<core::commands::AddCircle2DCommand>(ctx.sketch, c, m_r));
                    ctx.MarkDirty();
        }

        bool m_active = false;
        DraftStage m_stage = DraftStage::Idle;
        bool m_hasCenter = false;

        ImVec2 m_centerW{};
        double m_r = 1.0;

        bool m_focusEdit = false;
        bool m_editWasActive = false;
        char m_buf[64]{};
    };

    // Placeholder constraint tool so clicking constraint icons can enter a mode without breaking sketch tools.
    
    class ConstraintTool final : public ISketchTool {
    public:
        ToolKind Kind() const override { return ToolKind::Constraint; }
        bool IsActive() const override { return m_active; }

        void Begin(ToolContext& ctx) override
        {
            m_active = true;
            m_picks.clear();
            if (ctx.uiPickedIds) ctx.uiPickedIds->clear();
            if (ctx.uiHoverId) *ctx.uiHoverId = (domain::sketch::EntityId)0;
        }

        void Cancel(ToolContext& ctx) override
        {
            // Do not "lock" input after cancel; just clear local state.
            m_active = false;
            m_picks.clear();
            if (ctx.uiPickedIds) ctx.uiPickedIds->clear();
            if (ctx.uiHoverId) *ctx.uiHoverId = (domain::sketch::EntityId)0;
        }

        void UpdateAndDraw(ToolContext& ctx, const Canvas2D& canvas, ImDrawList* dl) override
        {
            if (!m_active) return;

            // ESC clears current pick sequence (but tool stays active)
            if (ImGui::IsKeyPressed(ImGuiKey_Escape)) {
                m_picks.clear();
            }

            // Need the active constraint icon from the UI
            if (!ctx.activeConstraintIcon || *ctx.activeConstraintIcon < 0)
                return;

            ConstraintIcon icon = static_cast<ConstraintIcon>(*ctx.activeConstraintIcon);

            // Determine how many picks are required
            int required = RequiredPicks(icon);

            // Hover highlight
            const float tolW = 6.0f / (std::max)(canvas.pixels_per_unit, 1.0f);
            ImVec2 mouseW = canvas.ScreenToWorld(ImGui::GetMousePos());
            PickResult hover = PickGeometry(ctx.sketch, mouseW, tolW);

            if (dl && hover.type != PickType::None)
            {
                // simple highlight: small circle around hovered entity location estimate
                ImVec2 s = canvas.WorldToScreen(mouseW);
                dl->AddCircle(s, 10.0f, IM_COL32(255, 210, 0, 200), 16, 2.0f);
            }

            // Click to pick
            if (ImGui::IsMouseClicked(ImGuiMouseButton_Left))
            {
                // Only accept clicks when mouse is over canvas rect
                if (ImGui::IsMouseHoveringRect(canvas.rect().Min, canvas.rect().Max))
                {
                    if (hover.type != PickType::None)
                    {
                        // Filter pick types per constraint
                        if (IsPickAllowed(icon, hover))
                        {
                            m_picks.push_back(hover);

                            if ((int)m_picks.size() >= required)
                            {
                                Commit(ctx, icon);
                                ctx.MarkDirty();
                                m_picks.clear();
                            }
                        }
                    }
                }
            }

            // Push hover + picked ids out to the renderer so entities can be highlighted.
            if (ctx.uiHoverId)
                *ctx.uiHoverId = (hover.type != PickType::None) ? hover.id : (domain::sketch::EntityId)0;

            if (ctx.uiPickedIds) {
                ctx.uiPickedIds->clear();
                for (const auto& pk : m_picks) ctx.uiPickedIds->push_back(pk.id);
            }

            // Tiny on-canvas status (top-left)
            if (dl)
            {
                ImVec2 p = VAdd(canvas.origin_screen, ImVec2(8, 8));
                char buf[128];
                snprintf(buf, sizeof(buf), "Constraint: %s  (%d/%d)",
                         ConstraintIconName(icon), (int)m_picks.size(), required);
                dl->AddText(p, IM_COL32(200, 200, 200, 220), buf);
            }
        }

    private:
        bool m_active = false;
        std::vector<PickResult> m_picks;

        static const char* ConstraintIconName(ConstraintIcon ic)
        {
            switch (ic)
            {
            case ConstraintIcon::Fixed:        return "Fixed";
            case ConstraintIcon::Horizontal:   return "Horizontal";
            case ConstraintIcon::Vertical:     return "Vertical";
            case ConstraintIcon::Parallel:     return "Parallel";
            case ConstraintIcon::Perpendicular:return "Perpendicular";
            case ConstraintIcon::Tangent:      return "Tangent";
            case ConstraintIcon::Coincident:   return "Coincident";
            case ConstraintIcon::Midpoint:     return "Midpoint";
            case ConstraintIcon::Equal:        return "Equal";
            default: return "Constraint";
            }
        }

        static int RequiredPicks(ConstraintIcon ic)
        {
            switch (ic)
            {
            case ConstraintIcon::Fixed:
            case ConstraintIcon::Horizontal:
            case ConstraintIcon::Vertical:
                return 1;

            case ConstraintIcon::Coincident:
            case ConstraintIcon::Parallel:
            case ConstraintIcon::Perpendicular:
            case ConstraintIcon::Tangent:
            case ConstraintIcon::Midpoint:
            case ConstraintIcon::Equal:
            default:
                return 2;
            }
        }

        static bool IsPickAllowed(ConstraintIcon ic, const PickResult& p)
        {
            // Minimal filtering so "Horizontal" doesn't accept circles, etc.
            if (ic == ConstraintIcon::Horizontal || ic == ConstraintIcon::Vertical)
                return p.type == PickType::Line;

            // Coincident wants points ideally, but allow anything for now (anchors later)
            if (ic == ConstraintIcon::Coincident)
                return p.type != PickType::None;

            // Fixed: allow anything (it may become "lock entity" later)
            if (ic == ConstraintIcon::Fixed)
                return p.type != PickType::None;

            return p.type != PickType::None;
        }

        static domain::sketch::GeometricConstraintType MapToGeometricType(ConstraintIcon ic)
        {
            using domain::sketch::GeometricConstraintType;
            switch (ic)
            {
            case ConstraintIcon::Horizontal:    return GeometricConstraintType::Horizontal;
            case ConstraintIcon::Vertical:      return GeometricConstraintType::Vertical;
            case ConstraintIcon::Coincident:    return GeometricConstraintType::Coincident;
            case ConstraintIcon::Parallel:      return GeometricConstraintType::Parallel;
            case ConstraintIcon::Perpendicular: return GeometricConstraintType::Perpendicular;
            case ConstraintIcon::Tangent:       return GeometricConstraintType::Tangent;
            case ConstraintIcon::Midpoint:      return GeometricConstraintType::Midpoint;
            default:                            return GeometricConstraintType::Coincident;
            }
        }

        void Commit(ToolContext& ctx, ConstraintIcon icon)
        {
            // NOTE: "Fixed" isn't a domain geometric type in the current solver.
            // For now we treat Fixed as Coincident with a single ref (solver will ignore until implemented).
            domain::sketch::GeometricConstraint gc;
            gc.meta.id = ctx.sketch.nextConstraintId++;
            gc.meta.name = ConstraintIconName(icon);
            gc.type = MapToGeometricType(icon);

            gc.refs.clear();
            if (!m_picks.empty())
            {
                domain::sketch::EntityRef a{ m_picks[0].id, domain::sketch::EntityAnchor::None };
                gc.refs.push_back(a);
            }
            if (m_picks.size() >= 2)
            {
                domain::sketch::EntityRef b{ m_picks[1].id, domain::sketch::EntityAnchor::None };
                gc.refs.push_back(b);
            }

            ctx.history.Execute(std::make_unique<core::commands::AddGeometricConstraintCommand>(ctx.sketch, gc));
        }
    };

    // Minimal placeholder constraint tool: makes constraint selection "an active tool"

} // namespace adapters::sketchui