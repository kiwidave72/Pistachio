#pragma once

// -----------------------------------------------------------------------
// SegmentChain.h — P4 output
//
// Ordered polylines, stitched from P3's raw unordered segments by
// matching endpoints. Purely a connectivity problem — no new geometry
// computed, only reordering/linking existing points.
//
// isClosed reports honestly whether a chain's start and end points
// matched up (a real closed loop) or not (an open polyline — a genuine
// gap in the source geometry, not a bug to silently paper over). See
// design doc: this is where P1's non-manifold warning for a specific
// instance is expected to concretely surface as an open chain, not
// something P4 repairs.
// -----------------------------------------------------------------------

#include <glm/glm.hpp>
#include <vector>

namespace domain::v1 {

    struct SegmentChain
    {
        std::vector<glm::vec3> points;   // ordered — points[i] connects to points[i+1]
        bool isClosed = false;
    };

    struct ExtractedLayer
    {
        int layerIndex = 0;
        float z = 0.0f;
        std::vector<SegmentChain> chains;
    };

} // namespace domain::v1