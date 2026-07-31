#pragma once

// -----------------------------------------------------------------------
// IContributionRegistry.h
//
// Pure interface for the registration half of adapters::ContributionRegistry
// (contribute*/remove* — no ImGui). adapters::ContributionRegistry itself
// is NOT changed to implement this yet — that header intentionally still
// includes <imgui.h>/<imgui_internal.h> directly, and is documented as
// "Include in ImGuiHost.cpp only".
//
// This interface exists so a future service plugin can register menu/
// ribbon/drag-drop contributions without linking ImGui at all. Wiring
// ContributionRegistry to implement it, and registering it into the
// service registry, is deferred follow-up work — not part of this pass.
// -----------------------------------------------------------------------

#include "ports/Contributions.h"
#include <string>

namespace ports {

    class IContributionRegistry
    {
    public:
        virtual ~IContributionRegistry() = default;

        virtual MenuContribution* contributeMenu(
            const std::string& pluginId,
            const std::string& menuName,
            int menuPriority = 100) = 0;

        virtual RibbonContribution* contributeRibbon(
            const std::string& pluginId,
            const std::string& groupName,
            int groupPriority = 100) = 0;

        virtual DragDropContribution* contributeDragDrop(
            const std::string& pluginId,
            const std::string& payloadType,
            int priority = 100) = 0;

        virtual void removeMenuContributions(const std::string& pluginId) = 0;
        virtual void removeRibbonContributions(const std::string& pluginId) = 0;
        virtual void removeDragDropContributions(const std::string& pluginId) = 0;
        virtual void removeAllContributions(const std::string& pluginId) = 0;
    };

} // namespace ports