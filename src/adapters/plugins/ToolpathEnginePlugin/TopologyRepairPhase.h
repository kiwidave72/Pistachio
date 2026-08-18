#pragma once

// -----------------------------------------------------------------------
// TopologyRepairPhase.h -- P5R (runs immediately after P5 TopologyPhase)
//
// Sliding-window consistency check across each instance's layers, using
// total net solid area (sum of outer-contour areas minus sum of
// hole-contour areas) as the per-layer health metric. A layer whose net
// area collapses relative to BOTH neighbors, while those neighbors agree
// with EACH OTHER, is treated as a pipeline failure and repaired by
// copying the nearest neighbor's contours -- not a true geometric
// interpolation (correspondence between two possibly-different-length
// contours is a much harder problem), but a deliberately simple,
// auditable first pass. Every repair is logged explicitly, same
// convention as P4's existing gap-snap repair diagnostic -- this phase
// never silently substitutes geometry.
//
// If the neighbors DISAGREE with each other, the flagged layer is left
// alone and only warned about: that pattern means the part's shape was
// already changing across this span, and blindly copying a neighbor
// would erase a real, intended feature rather than fix a bug.
// -----------------------------------------------------------------------

#include "domain/AdvancedTopology.h"
#include "adapters/plugins/ToolpathEnginePlugin/TopologyPhase.h"

#include <vector>
#include <string>

namespace kinetica {

    // One entry per layer that the sliding-window check flagged, whether
    // or not it ended up being repaired. This is the record that actually
    // answers "where should we spend engineering effort" -- a pile of
    // individual per-layer diagnostics buried inside each instance's
    // layer list doesn't scale to reviewing a full print job; this does.
    struct RepairReportEntry
    {
        std::string modelInstanceId;
        int layerIndex = 0;
        float z = 0.0f;
        bool wasRepaired = false;      // false = flagged but left alone (neighbors disagreed)
        bool isRise = false;           // false = area dropped (missing wall), true = area rose (missing hole)
        float measuredArea = 0.0f;
        float neighborAverageArea = 0.0f;

        // How many diagnostics P4/P5 had ALREADY logged for this layer
        // before repair ran. This is a real attribution clue: our own
        // investigation found the P4 stitching bug always left a trail
        // (dead-end / excluded-chain diagnostics), while the still-
        // unexplained P3-level drop left NONE. Zero here points upstream
        // of P4; non-zero points at whatever those diagnostics already
        // named.
        int priorDiagnosticCount = 0;
    };

    struct RepairReport
    {
        int totalRepaired = 0;
        int totalFlaggedNotRepaired = 0;
        std::vector<RepairReportEntry> entries;
    };

    struct TopologyRepairResult
    {
        std::vector<TopologizedGeometry> geometry;
        RepairReport report;
    };

    class TopologyRepairPhase
    {
    public:
        TopologyRepairResult run(std::vector<TopologizedGeometry> input);
    };

} // namespace kinetica