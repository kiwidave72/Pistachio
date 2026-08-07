#ifndef DOMAIN_V1_INFILL_REGION_H
#define DOMAIN_V1_INFILL_REGION_H

// -----------------------------------------------------------------------
// InfillRegion.h — P6, infill region computation output
//
// The actual fillable area per layer: innermost outer wall boundary,
// minus hole interiors — computed via Clipper2 boolean Difference,
// reusing WallGenerationResult's boundaries rather than recomputing
// offsets a second time.
//
// Spurious tiny hole contours (tessellation-seam noise, see design doc)
// are filtered by minimum area BEFORE the Difference operation — a
// real hole should be many mm^2; a noise artifact is sub-mm^2.
// -----------------------------------------------------------------------

#include <glm/glm.hpp>
#include <vector>

namespace domain::v1 {

    struct InfillRegion
    {
        // One or more disjoint polygons — Difference can split a region
        // into multiple pieces (e.g. a wide hole splitting one region
        // into two separate infill areas either side of it).
        std::vector<std::vector<glm::vec3>> polygons;

        int holesFilteredAsSpurious = 0;   // reported, not hidden — see run.json extension
    };

} // namespace domain::v1

#endif // DOMAIN_V1_INFILL_REGION_H