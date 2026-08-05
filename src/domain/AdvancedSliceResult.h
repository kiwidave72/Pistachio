#pragma once

// -----------------------------------------------------------------------
// AdvancedSliceResult.h — P3 output
//
// Raw, UNORDERED, disconnected line segments per layer — not closed
// contours yet. Turning this into ordered, connected loops is P4/P5's
// job, deliberately not P3's. See design doc: P3 cannot and does not
// guarantee watertightness; it only guarantees geometrically correct
// segments for genuine triangle-plane intersections, which is what
// makes watertightness ACHIEVABLE downstream (on geometry P1 already
// confirmed manifold).
// -----------------------------------------------------------------------

#include <glm/glm.hpp>
#include <string>
#include <vector>

namespace domain::v1 {

    struct SliceSegment
    {
        glm::vec3 start;
        glm::vec3 end;
    };

    struct SliceLayer
    {
        int layerIndex = 0;   // 0 = first layer
        float z = 0.0f;
        std::vector<SliceSegment> segments;
    };

    class AdvancedSliceResult
    {
    public:
        std::string modelInstanceId;
        std::vector<SliceLayer> layers;
    };

} // namespace domain::v1