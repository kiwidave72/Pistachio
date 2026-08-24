#pragma once

// Narrow host-facing UI interface.
//
// Intent:
// - Host owns the window + ImGui context/backends.
// - UI adapter/plugin may supply GPU texture handles for window chrome icons.
// - Keep this interface free of Walnut types so it can be used across hot-loaded modules.

#include <imgui.h>

struct IGuiHost
{
    virtual ~IGuiHost() = default;

    virtual void* nativeWindowHandle() const = 0;

    virtual void setWindowControlIcons(
        ImTextureID minimize,
        ImTextureID maximize,
        ImTextureID restore,
        ImTextureID close,
        ImVec2 size
    ) = 0;
};
