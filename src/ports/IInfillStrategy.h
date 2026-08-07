#pragma once

// -----------------------------------------------------------------------
// IInfillStrategy.h
//
// Self-describing infill plugin interface, per the design doc. Each
// implementation names itself (id()) and declares its own settings
// schema — the host never hardcodes a fixed infill parameter shape,
// same principle as ContributionRegistry not knowing in advance what
// menu items a UI plugin contributes.
//
// Single default (RectilinearInfillStrategy) registered for now, per
// the "single infill pattern for A/B testing" scope decision — the
// plugin boundary exists so a different algorithm can be dropped in
// later without touching the pipeline, not because multiple run
// simultaneously today.
// -----------------------------------------------------------------------

#include "domain/InfillRegion.h"
#include "domain/Toolpath.h"
#include "ports/IConfigPort.h"
#include "adapters/plugins/ToolpathEnginePlugin/WallGenerationPhase.h"

#include <string>
#include <vector>

namespace ports {

    class IInfillStrategy
    {
    public:
        virtual ~IInfillStrategy() = default;

        // Identity — becomes the config namespace key: slicer.infill.<id()>.settings
        virtual std::string id() const = 0;

        // Each plugin describes its own settings via ports::SettingInfo —
        // the real, already-existing config type, not a bespoke schema.
        virtual std::vector<ports::SettingInfo> getSettingsSchema() const = 0;

        virtual std::vector<domain::v1::ToolpathSegment> generate(
            const domain::v1::InfillRegion& region,
            const domain::v1::WallGenerationResult& wallResult,
            float z,
            const ports::IConfigPort& config) = 0;
    };

} // namespace ports