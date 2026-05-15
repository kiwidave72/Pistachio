#pragma once

// Centralized theme + colors used by ImGuiHost and other UI code.
// This header intentionally provides the UI::Colors namespace that the host code expects.

#include "imgui.h"

namespace UI {

namespace Colors {

    // Utility: multiply RGB by a scalar (alpha preserved)
    inline ImU32 ColorWithMultipliedValue(ImU32 color, float multiplier)
    {
        ImVec4 c = ImGui::ColorConvertU32ToFloat4(color);
        c.x *= multiplier;
        c.y *= multiplier;
        c.z *= multiplier;
        return ImGui::ColorConvertFloat4ToU32(c);
    }

    struct Theme
    {
        // NOTE: These are the exact names referenced by ImGuiHost.cpp and ImGuiTheme.cpp.
        static ImU32 titlebar;
        static ImU32 text;
        static ImU32 textDarker;
        static ImU32 invalidPrefab;

        // Used by ImGuiTheme.cpp (and may be useful elsewhere)
        static ImU32 groupHeader;
        static ImU32 propertyField;
    };

} // namespace Colors

// Apply the app theme to ImGui::GetStyle().Colors, etc.
void SetChocolateOverlayTheme();

} // namespace UI
