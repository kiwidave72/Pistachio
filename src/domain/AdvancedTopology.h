#pragma once

// -----------------------------------------------------------------------
// AdvancedTopology.h — P5 output
//
// Gives P4's flat, unrelated closed chains actual geometric meaning:
// which are solid outer boundaries, which are holes nested inside them.
// Determined via even-odd point-in-polygon nesting depth (2D, X/Y only
// — every chain in a layer shares the same Z by construction).
//
// Flat list, not a hierarchical tree — isOuter/nestingDepth is enough
// for P6 to do correct even-odd infill without needing an explicit
// parent-child structure. Revisit only if P6 concretely needs it.
//
// Only processes chains where isClosed == true (natural or repaired —
// P5 doesn't distinguish, per P4's design). Chains P4 couldn't close
// are skipped here, counted, not silently dropped without a trace.
// -----------------------------------------------------------------------
#pragma once

#include <glm/glm.hpp>
#include <string>
#include <vector>

#include "domain/DiagnosticMessage.h"

namespace domain::v1 {

    struct Contour
    {
        std::vector<glm::vec3> points;   // from the source SegmentChain, unchanged
        bool isOuter = true;              // even nesting depth = solid, odd = hole
        int nestingDepth = 0;
    };

    struct TopologyLayer
    {
        int layerIndex = 0;
        float z = 0.0f;
        std::vector<Contour> contours;
        int skippedOpenChains = 0;
        std::vector<DiagnosticMessage> diagnostics;
    };

    class AdvancedTopology
    {
    public:
        std::string modelInstanceId;
        std::vector<TopologyLayer> layers;
    };

} // namespace domain::v1