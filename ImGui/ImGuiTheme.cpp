#include "ImGuiTheme.h"

#include <cmath>
#include "imgui_internal.h"

// Provide defaults so linking always succeeds even if a caller forgets to call SetChocolateOverlayTheme().
ImU32 UI::Colors::Theme::titlebar      = IM_COL32(30, 24, 20, 255);
ImU32 UI::Colors::Theme::text          = IM_COL32(230, 220, 210, 255);
ImU32 UI::Colors::Theme::textDarker    = IM_COL32(190, 175, 160, 255);
ImU32 UI::Colors::Theme::invalidPrefab = IM_COL32(255, 0, 255, 255);

ImU32 UI::Colors::Theme::groupHeader   = IM_COL32(45, 35, 28, 255);
ImU32 UI::Colors::Theme::propertyField = IM_COL32(35, 28, 22, 255);

static float PowF(float a, float b)
{
    // MSVC doesn't reliably expose std::powf in all modes; use the global C function.
    return ::powf(a, b);
}

void UI::SetChocolateOverlayTheme()
{
    ImGuiStyle& style = ImGui::GetStyle();
    ImVec4* colors = style.Colors;

    colors[ImGuiCol_Text]                  = ImGui::ColorConvertU32ToFloat4(Colors::Theme::text);
    colors[ImGuiCol_TextDisabled]          = ImVec4(0.55f, 0.52f, 0.48f, 1.00f);

    colors[ImGuiCol_WindowBg]              = ImVec4(0.10f, 0.08f, 0.07f, 1.00f);
    colors[ImGuiCol_ChildBg]               = ImVec4(0.10f, 0.08f, 0.07f, 1.00f);
    colors[ImGuiCol_PopupBg]               = ImVec4(0.12f, 0.10f, 0.09f, 1.00f);

    colors[ImGuiCol_Border]                = ImVec4(0.20f, 0.17f, 0.14f, 1.00f);
    colors[ImGuiCol_BorderShadow]          = ImVec4(0, 0, 0, 0);

    colors[ImGuiCol_FrameBg]               = ImGui::ColorConvertU32ToFloat4(Colors::Theme::propertyField);
    colors[ImGuiCol_FrameBgHovered]        = ImVec4(0.22f, 0.18f, 0.14f, 1.00f);
    colors[ImGuiCol_FrameBgActive]         = ImVec4(0.26f, 0.21f, 0.16f, 1.00f);

    colors[ImGuiCol_TitleBg]               = ImGui::ColorConvertU32ToFloat4(Colors::Theme::titlebar);
    colors[ImGuiCol_TitleBgActive]         = ImGui::ColorConvertU32ToFloat4(Colors::Theme::titlebar);
    colors[ImGuiCol_TitleBgCollapsed]      = ImVec4(0.10f, 0.08f, 0.07f, 1.00f);

    colors[ImGuiCol_MenuBarBg]             = ImVec4(0.12f, 0.10f, 0.09f, 1.00f);

    colors[ImGuiCol_ScrollbarBg]           = ImVec4(0.09f, 0.07f, 0.06f, 1.00f);
    colors[ImGuiCol_ScrollbarGrab]         = ImVec4(0.22f, 0.18f, 0.14f, 1.00f);
    colors[ImGuiCol_ScrollbarGrabHovered]  = ImVec4(0.26f, 0.21f, 0.16f, 1.00f);
    colors[ImGuiCol_ScrollbarGrabActive]   = ImVec4(0.30f, 0.24f, 0.18f, 1.00f);

    colors[ImGuiCol_CheckMark]             = ImVec4(0.82f, 0.68f, 0.52f, 1.00f);
    colors[ImGuiCol_SliderGrab]            = ImVec4(0.65f, 0.54f, 0.42f, 1.00f);
    colors[ImGuiCol_SliderGrabActive]      = ImVec4(0.78f, 0.64f, 0.48f, 1.00f);

    colors[ImGuiCol_Button]                = ImVec4(0.18f, 0.14f, 0.11f, 1.00f);
    colors[ImGuiCol_ButtonHovered]         = ImVec4(0.24f, 0.18f, 0.14f, 1.00f);
    colors[ImGuiCol_ButtonActive]          = ImVec4(0.28f, 0.21f, 0.16f, 1.00f);

    colors[ImGuiCol_Header]                = ImGui::ColorConvertU32ToFloat4(Colors::Theme::groupHeader);
    colors[ImGuiCol_HeaderHovered]         = ImVec4(0.22f, 0.18f, 0.14f, 1.00f);
    colors[ImGuiCol_HeaderActive]          = ImVec4(0.26f, 0.21f, 0.16f, 1.00f);

    colors[ImGuiCol_Tab]                   = ImGui::ColorConvertU32ToFloat4(Colors::Theme::titlebar);
    colors[ImGuiCol_TabHovered]            = ImVec4(0.24f, 0.18f, 0.14f, 1.00f);
    colors[ImGuiCol_TabActive]             = ImVec4(0.20f, 0.16f, 0.12f, 1.00f);
    colors[ImGuiCol_TabUnfocused]          = ImGui::ColorConvertU32ToFloat4(Colors::Theme::titlebar);
    colors[ImGuiCol_TabUnfocusedActive]    = ImVec4(0.18f, 0.14f, 0.11f, 1.00f);

    // Some mild rounding
    style.WindowRounding = PowF(2.0f, 1.0f);
    style.FrameRounding  = 4.0f;
    style.GrabRounding   = 4.0f;
    style.TabRounding    = 4.0f;
}
