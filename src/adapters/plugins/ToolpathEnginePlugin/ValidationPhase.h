#pragma once

// -----------------------------------------------------------------------
// ValidationPhase.h — P1
//
// Detect-only, no repair. Confirms whether input geometry is actually
// clean rather than assuming it — degenerate triangles, non-manifold
// edges. Geometry passes through UNCHANGED; this phase never modifies
// vertices/indices.
//
// ValidationReport is a real result object, not just console output —
// bundled with its UnifiedGeometry (not a parallel vector, to avoid
// index-desync risk) so it can be consumed by later pipeline phases,
// or rendered in the GUI later, not just printed. Console printing is
// today's ONE consumer of it, not something baked into this phase.
// -----------------------------------------------------------------------

#include "domain/UnifiedGeometry.h"

#include <string>
#include <vector>

namespace kinetica {

    struct ValidationReport
    {
        std::string modelInstanceId;
        int degenerateTriangleCount = 0;
        int nonManifoldEdgeCount = 0;

        bool isClean() const { return degenerateTriangleCount == 0 && nonManifoldEdgeCount == 0; }
    };

    struct ValidatedGeometry
    {
        domain::v1::UnifiedGeometry geometry;   // unchanged from P0's output
        ValidationReport report;
    };

    class ValidationPhase
    {
    public:
        std::vector<ValidatedGeometry> run(std::vector<domain::v1::UnifiedGeometry> input);
    };

} // namespace kinetica