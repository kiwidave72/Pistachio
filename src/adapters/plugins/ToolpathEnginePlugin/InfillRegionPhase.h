#pragma once

#include "domain/InfillRegion.h"
#include "domain/WallGenerationResult.h"

namespace kinetica {

    class InfillRegionPhase
    {
    public:
        // minHoleArea in mm^2 — hole boundaries smaller than this are
        // treated as spurious noise (tessellation-seam artifacts) and
        // excluded from the boolean subtraction, not walled, not
        // reported as real holes downstream.
        static domain::v1::InfillRegion run(
            const domain::v1::WallGenerationResult& wallResult,
            float minHoleArea = 1.0f);
    };

} // namespace kinetica