#pragma once

// -----------------------------------------------------------------------
// SlicingPhase.h — P3
//
// Toolhead-agnostic, deliberately. No assignment logic here at all —
// see design doc: assignment is a separate labeling/routing concern
// layered on top of geometry, not a prerequisite for slicing it. No
// purge-tower or support logic either — both belong downstream (P6-
// adjacent tool-change sequencing, and P5 respectively), not here.
//
// Consumes AcceleratedGeometry (P2's output). ValidationReport carries
// forward for diagnostic use; the mesh (UnifiedGeometry) and the
// Z-bucket index (UnifiedAcceleration) are both dropped after this
// phase — P4 onward works purely on 2D segment/contour data and needs
// neither.
// -----------------------------------------------------------------------

#include "domain/AdvancedSliceResult.h"
#include "adapters/plugins/ToolpathEnginePlugin/AccelerationPhase.h"
#include "ports/IConfigPort.h"

#include <vector>

namespace kinetica {

    struct SlicedGeometry
    {
        domain::v1::AdvancedSliceResult sliceResult;
        ValidationReport report;   // carried forward from P1, diagnostic only
    };

    class SlicingPhase
    {
    public:
        explicit SlicingPhase(ports::IConfigPort& config) : m_config(config) {}

        std::vector<SlicedGeometry> run(std::vector<AcceleratedGeometry> input);

    private:
        ports::IConfigPort& m_config;
    };

} // namespace kinetica