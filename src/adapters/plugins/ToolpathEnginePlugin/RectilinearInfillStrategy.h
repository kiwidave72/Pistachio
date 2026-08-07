#pragma once

#include "ports/IInfillStrategy.h"

namespace kinetica {

    class RectilinearInfillStrategy final : public ports::IInfillStrategy
    {
    public:
        std::string id() const override { return "rectilinear"; }

        std::vector<ports::SettingInfo> getSettingsSchema() const override;

        std::vector<domain::v1::ToolpathSegment> generate(
            const domain::v1::InfillRegion& region,
            const domain::v1::WallGenerationResult& wallResult,
            float z,
            const ports::IConfigPort& config) override;
    };

} // namespace kinetica