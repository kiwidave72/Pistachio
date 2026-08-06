#pragma once

// -----------------------------------------------------------------------
// TopologyPhase.h — P5
//
// Determines outer-vs-hole nesting for P4's closed chains via even-odd
// point-in-polygon containment counting. O(n^2) per layer in chain
// count — fine at current scale, a real parallelFor/acceleration
// candidate later if chain counts grow (see design doc's CPU/GPU
// parallelism section, not yet connected to the pipeline).
// -----------------------------------------------------------------------

#include "domain/AdvancedTopology.h"
#include "adapters/plugins/ToolpathEnginePlugin/ExtractionPhase.h"

#include <vector>

namespace kinetica {

    struct TopologizedGeometry
    {
        domain::v1::AdvancedTopology topology;
        ValidationReport report;   // carried forward from P1, diagnostic only
    };

    class TopologyPhase
    {
    public:
        std::vector<TopologizedGeometry> run(std::vector<ExtractedGeometry> input);
    };

} // namespace kinetica