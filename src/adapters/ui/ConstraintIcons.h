#pragma once
#include <imgui.h>
#include <cmath>

namespace adapters::sketchui
{
    // ImVec2 helper math (ImGui has no operators)
    static inline ImVec2 V(float x, float y) { return ImVec2(x, y); }
    static inline ImVec2 Add(ImVec2 a, ImVec2 b) { return ImVec2(a.x + b.x, a.y + b.y); }
    static inline ImVec2 Sub(ImVec2 a, ImVec2 b) { return ImVec2(a.x - b.x, a.y - b.y); }
    static inline ImVec2 Mul(ImVec2 a, float s)  { return ImVec2(a.x * s, a.y * s); }
    static inline float  Len(ImVec2 v) { return std::sqrt(v.x*v.x + v.y*v.y); }
    static inline ImVec2 Norm(ImVec2 v) { float l = Len(v); return (l > 1e-6f) ? ImVec2(v.x/l, v.y/l) : ImVec2(1,0); }
    static inline ImVec2 Perp(ImVec2 v) { return ImVec2(-v.y, v.x); }

    enum class ConstraintIcon
    {
        Fixed,
        Horizontal,
        Vertical,
        Parallel,
        Perpendicular,
        Tangent,
        Coincident,
        Midpoint,
        Equal,
        Distance,
        Angle,
        Radius,
        Diameter
    };

    static inline void Dot(ImDrawList* dl, ImVec2 p, float r, ImU32 col, float thick=1.8f)
    {
        dl->AddCircle(p, r, col, 12, thick);
    }

    static inline void Line(ImDrawList* dl, ImVec2 a, ImVec2 b, ImU32 col, float thick=1.8f)
    {
        dl->AddLine(a, b, col, thick);
    }

    static inline void Arrow(ImDrawList* dl, ImVec2 a, ImVec2 b, ImU32 col, float thick=1.6f, float head=5.0f)
    {
        Line(dl, a, b, col, thick);
        ImVec2 d = Norm(Sub(a, b));
        ImVec2 n = Perp(d);
        ImVec2 p1 = Add(b, Add(Mul(d, head), Mul(n, head*0.6f)));
        ImVec2 p2 = Add(b, Add(Mul(d, head), Mul(n, -head*0.6f)));
        Line(dl, b, p1, col, thick);
        Line(dl, b, p2, col, thick);
    }

    static inline void DrawConstraintIcon(ImDrawList* dl, ImVec2 p, float s, ImU32 col, ConstraintIcon icon)
    {
        const float t = 1.8f;
        const float r = s * 0.07f;
        auto P = [&](float x, float y){ return Add(p, V(x*s, y*s)); };

        switch (icon)
        {
        case ConstraintIcon::Fixed:
            Dot(dl, P(0.50f,0.25f), r*1.2f, col, t);
            Line(dl, P(0.50f,0.33f), P(0.50f,0.60f), col, t);
            Line(dl, P(0.22f,0.60f), P(0.78f,0.60f), col, t);
            Line(dl, P(0.34f,0.60f), P(0.42f,0.82f), col, t);
            Line(dl, P(0.66f,0.60f), P(0.58f,0.82f), col, t);
            break;

        case ConstraintIcon::Horizontal:
            Dot(dl, P(0.25f,0.50f), r*1.2f, col, t);
            Dot(dl, P(0.75f,0.50f), r*1.2f, col, t);
            Line(dl, P(0.32f,0.50f), P(0.68f,0.50f), col, t);
            break;

        case ConstraintIcon::Vertical:
            Dot(dl, P(0.50f,0.25f), r*1.2f, col, t);
            Dot(dl, P(0.50f,0.75f), r*1.2f, col, t);
            Line(dl, P(0.50f,0.32f), P(0.50f,0.68f), col, t);
            break;

        case ConstraintIcon::Parallel:
            Line(dl, P(0.25f,0.70f), P(0.60f,0.30f), col, t);
            Line(dl, P(0.40f,0.75f), P(0.75f,0.35f), col, t);
            break;

        case ConstraintIcon::Perpendicular:
            Line(dl, P(0.30f,0.70f), P(0.30f,0.30f), col, t);
            Line(dl, P(0.30f,0.70f), P(0.70f,0.70f), col, t);
            Line(dl, P(0.30f,0.62f), P(0.38f,0.62f), col, t);
            Line(dl, P(0.38f,0.62f), P(0.38f,0.70f), col, t);
            break;

        case ConstraintIcon::Tangent:
            dl->AddCircle(P(0.42f,0.55f), s*0.18f, col, 20, t);
            Line(dl, P(0.62f,0.22f), P(0.86f,0.86f), col, t);
            break;

        case ConstraintIcon::Coincident:
            Dot(dl, P(0.50f,0.50f), r*2.0f, col, t);
            dl->AddCircleFilled(P(0.50f,0.50f), r*0.9f, col);
            break;

        case ConstraintIcon::Midpoint:
            Dot(dl, P(0.25f,0.55f), r*1.1f, col, t);
            Dot(dl, P(0.75f,0.55f), r*1.1f, col, t);
            Line(dl, P(0.32f,0.55f), P(0.68f,0.55f), col, t);
            Line(dl, P(0.50f,0.40f), P(0.50f,0.70f), col, t);
            break;

        case ConstraintIcon::Equal:
            Line(dl, P(0.22f,0.35f), P(0.55f,0.35f), col, t);
            Line(dl, P(0.30f,0.72f), P(0.62f,0.52f), col, t);
            Line(dl, P(0.62f,0.43f), P(0.80f,0.43f), col, t);
            Line(dl, P(0.62f,0.56f), P(0.80f,0.56f), col, t);
            break;

        case ConstraintIcon::Distance:
            Arrow(dl, P(0.25f,0.55f), P(0.75f,0.55f), col, 1.6f, s*0.10f);
            Line(dl, P(0.25f,0.35f), P(0.25f,0.75f), col, 1.4f);
            Line(dl, P(0.75f,0.35f), P(0.75f,0.75f), col, 1.4f);
            break;

        case ConstraintIcon::Angle:
        {
            ImVec2 c = P(0.35f, 0.70f);
            Line(dl, c, P(0.75f, 0.70f), col, t);
            Line(dl, c, P(0.55f, 0.35f), col, t);

            float rad = s*0.18f;
            dl->PathClear();
            const int seg = 16;
            for (int i=0;i<=seg;i++)
            {
                float a = (float)(-0.2 * 3.14159) + (float)i/seg * (float)(0.55 * 3.14159);
                dl->PathLineTo(Add(c, V(std::cos(a)*rad, std::sin(a)*rad)));
            }
            dl->PathStroke(col, 0, 1.6f);
        } break;

        case ConstraintIcon::Radius:
        {
            ImVec2 c = P(0.45f, 0.55f);
            float cr = s*0.18f;
            dl->AddCircle(c, cr, col, 20, t);
            Line(dl, c, P(0.82f, 0.30f), col, t);
            Dot(dl, c, r*1.0f, col, t);
        } break;

        case ConstraintIcon::Diameter:
        {
            ImVec2 c = P(0.50f, 0.55f);
            float cr = s*0.18f;
            dl->AddCircle(c, cr, col, 20, t);
            Line(dl, Sub(c, V(cr, 0)), Add(c, V(cr, 0)), col, t);
        } break;
        }
    }

    static inline bool ConstraintIconButton(const char* id, ConstraintIcon icon, bool selected,
                                            ImVec2 size = ImVec2(26,26))
    {
        // Use only public ImGui API (no imgui_internal)
        ImVec2 p = ImGui::GetCursorScreenPos();
        bool pressed = ImGui::InvisibleButton(id, size);
        bool hovered = ImGui::IsItemHovered();
        bool held = ImGui::IsItemActive();
        ImDrawList* dl = ImGui::GetWindowDrawList();
        ImVec2 p2 = ImVec2(p.x + size.x, p.y + size.y);
        ImU32 bg = held ? ImGui::GetColorU32(ImGuiCol_ButtonActive) :
                 hovered ? ImGui::GetColorU32(ImGuiCol_ButtonHovered) :
                          ImGui::GetColorU32(ImGuiCol_Button);
        if (selected) bg = ImGui::GetColorU32(ImGuiCol_TabActive);
        dl->AddRectFilled(p, p2, bg, 4.0f);
        dl->AddRect(p, p2, ImGui::GetColorU32(ImGuiCol_Border), 4.0f);
        ImU32 col = ImGui::GetColorU32(ImGuiCol_Text);
        float s = (size.x < size.y ? size.x : size.y);
        float iconS = s * 0.85f;
        ImVec2 iconP = ImVec2(p.x + (size.x - iconS) * 0.5f, p.y + (size.y - iconS) * 0.5f);
        DrawConstraintIcon(dl, iconP, iconS, col, icon);
        return pressed;
    }
}