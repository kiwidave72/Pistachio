#include "../ImGui/ImGuiTheme.h"

// This translation unit provides storage for UI::Colors::Theme static members.
// They are declared in ImGuiTheme.h and used by ImGuiHost.cpp.
//
// If you later centralize theme configuration, you can update these values
// or expose setters.

namespace UI {
namespace Colors {

// Fallback colors (ARGB / ImU32-compatible). Chosen to match a dark UI.
unsigned int Theme::titlebar   = 0xFF1E1E1E; // dark gray
unsigned int Theme::text       = 0xFFE6E6E6; // near-white
unsigned int Theme::textDarker = 0xFFB0B0B0; // dim text

} // namespace Colors
} // namespace UI
