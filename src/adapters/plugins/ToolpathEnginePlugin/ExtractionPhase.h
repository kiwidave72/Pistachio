#pragma once

// -----------------------------------------------------------------------
// ExtractionPhase.h — P4
//
// Stitches P3's raw unordered segments into ordered SegmentChains by
// matching endpoints. Purely a connectivity problem — no new geometry
// computed. See design doc: chains that don't close (isClosed = false)
// are reported honestly, not repaired — this is where a genuinely
// non-manifold source mesh (per P1's findings) is expected to surface
// as a concrete open chain.
// -----------------------------------------------------------------------

#include "domain/SegmentChain.h"
#include "adapters/plugins/ToolpathEnginePlugin/SlicingPhase.h"

#include <vector>

namespace kinetica {

    struct ExtractedGeometry
    {
        std::string modelInstanceId;
        std::vector<domain::v1::ExtractedLayer> layers;
        ValidationReport report;   // carried forward from P1, diagnostic only
    };

    class ExtractionPhase
    {
    public:
        std::vector<ExtractedGeometry> run(std::vector<SlicedGeometry> input);
    };

} // namespace kinetica