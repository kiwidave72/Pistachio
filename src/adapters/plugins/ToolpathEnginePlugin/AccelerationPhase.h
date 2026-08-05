#pragma once

// -----------------------------------------------------------------------
// AccelerationPhase.h — P2
//
// Builds a Z-bucket spatial index per instance, so P3 can test only
// nearby triangles per layer instead of every triangle in the mesh.
// Operates on ValidatedGeometry (P1's output) — geometry passes through
// unchanged; this phase only builds an index alongside it.
// -----------------------------------------------------------------------

#include "domain/UnifiedAcceleration.h"
#include "adapters/plugins/ToolpathEnginePlugin/ValidationPhase.h"
#include "ports/IConfigPort.h"

#include <vector>

namespace kinetica {

    struct AcceleratedGeometry
    {
        domain::v1::UnifiedGeometry geometry;        // unchanged from P0/P1
        ValidationReport report;                      // carried through from P1
        domain::v1::UnifiedAcceleration acceleration;  // new, this phase's output
    };

    class AccelerationPhase
    {
    public:
        explicit AccelerationPhase(ports::IConfigPort& config) : m_config(config) {}

        std::vector<AcceleratedGeometry> run(std::vector<ValidatedGeometry> input);

    private:
        ports::IConfigPort& m_config;
    };

} // namespace kinetica