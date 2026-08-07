#pragma once

// -----------------------------------------------------------------------
// WallGenerationPhase.h — P6, part 1
//
// Generates slicer.wallCount concentric offset copies inward from each
// outer contour, using Clipper2's InflatePaths (negative offset =
// shrink inward). Already-linked project dependency — deliberately not
// hand-rolled, given robust 2D polygon offsetting (self-intersection
// handling, sharp concave corners) is exactly what it's built for.
//
// Clipper2 (not Clipper2Z) — no per-point Z needed, each TopologyLayer
// already carries its own z.
//
// Output feeds directly into ToolpathSegment generation — walls are
// the first move-type category P6 produces; infill/skin follow in
// later steps.
// -----------------------------------------------------------------------

#include "domain/AdvancedTopology.h"
#include "domain/Toolpath.h"
#include "ports/IConfigPort.h"
#include "domain/WallGenerationResult.h"

#include <vector>

namespace kinetica {

    class WallGenerationPhase
    {
    public:
        explicit WallGenerationPhase(ports::IConfigPort& config) : m_config(config) {}

         
        domain::v1::WallGenerationResult run(const domain::v1::TopologyLayer& layer);


    private:
        ports::IConfigPort& m_config;
    };

} // namespace kinetica