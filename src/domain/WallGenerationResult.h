#ifndef DOMAIN_V1_WALL_GENERATION_RESULT_H
#define DOMAIN_V1_WALL_GENERATION_RESULT_H

// -----------------------------------------------------------------------
// WallGenerationResult.h — P6, wall generation output
//
// Segments (for the final Toolpath) PLUS the innermost wall boundary per
// contour — the exact geometry infill region computation needs as input.
// Deliberately handed back here rather than recomputed by a separate
// infill phase, to avoid the two offsets silently drifting apart.
//
// innermostOuterBoundaries: infill fills INSIDE these.
// innermostHoleBoundaries: infill must stay OUTSIDE these (subtraction).
// -----------------------------------------------------------------------

#include "domain/Toolpath.h"

#include <glm/glm.hpp>
#include <vector>

namespace domain::v1 {

    struct WallGenerationResult
    {
        std::vector<ToolpathSegment> segments;
        std::vector<std::vector<glm::vec3>> innermostOuterBoundaries;
        std::vector<std::vector<glm::vec3>> innermostHoleBoundaries;
    };

} // namespace domain::v1

#endif // DOMAIN_V1_WALL_GENERATION_RESULT_H