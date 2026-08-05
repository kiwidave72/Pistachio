#pragma once

// -----------------------------------------------------------------------
// UnifiedGeometry.h
//
// P0's output. Mesh-only for now — B-Rep fields deliberately omitted,
// not stubbed, per the "mesh path first, B-Rep deferred" scope decision
// in the design doc.
//
// *** COORDINATE CONVENTION — READ BEFORE TOUCHING THIS TYPE ***
// Vertices/indices are in BED SPACE, ORIGIN AT THE BED CORNER — matching
// gcode's own coordinate convention, NOT this app's scene/rendering
// convention (where the bed is centered at origin).
//
// P0 (ImportPhase) performs the conversion exactly once:
// ModelInstance::transform is applied first (scene space, bed-center
// origin), then a single translation of (+bed.size.X/2, +bed.size.Y/2)
// shifts everything into bed-corner-origin space. Every downstream
// phase (P1-P6) works entirely in this bed-space convention and never
// needs to think about the scene/rendering convention again, and never
// re-applies a per-instance transform.
//
// This is a temporary, deliberate simplification: the app's own
// scene/rendering code still uses bed-center origin everywhere else
// (unchanged). Converting the whole app to bed-corner origin throughout
// is a real future option, not done now — this single translation in
// ImportPhase is the cheaper fix for today's actual need. Expected to
// be deleted once ModelInstance/the app's scene convention migrates to
// bed-corner-origin natively — don't build further logic that depends
// on this conversion happening in ImportPhase specifically.
//
// No toolheadId here — that's a P3 concern (toolhead assignment),
// belongs on P3's own output type once designed, not retrofitted onto
// P0's output ahead of need.
// -----------------------------------------------------------------------

#include "domain/ModelCache.h"   // for Vertex, BoundingBox

#include <string>
#include <vector>

namespace domain::v1 {

    class UnifiedGeometry
    {
    public:
        std::string modelInstanceId;    // ModelInstance::id this came from

        std::vector<Vertex> vertices;   // BED SPACE, corner origin — see header comment above
        std::vector<uint32_t> indices;

        BoundingBox bounds;             // same space as vertices
    };

} // namespace domain::v1