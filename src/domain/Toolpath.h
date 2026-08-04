#pragma once

// -----------------------------------------------------------------------
// Toolpath.h
//
// The shared geometric core for toolpath data — deliberately lean
// (geometry/motion only, no print settings, no start/end gcode). This is
// what makes it genuinely two-directional:
//   - GCodeToolpathLoader parses gcode text -> Toolpath (visualizer path)
//   - ToolpathEnginePlugin's P6 produces this same type as its output
//     (Kinetica pipeline path)
// Both feed the identical visualization pipeline (ribbon mesh, layer
// scrubber, camera) unchanged.
//
// No toolhead identity on ToolpathSegment/Toolpath itself — see
// ToolheadToolpath below for how multi-toolhead composes with this.
// -----------------------------------------------------------------------

#include "domain/ModelCache.h"    

#include <glm/glm.hpp>
#include <string>
#include <vector>

namespace domain::v1 {

    enum class ToolpathMoveType
    {
        Unknown,
        Perimeter,
        InnerWall,
        OuterWall,
        Infill,
        Skin,
        Support,
        Skirt,
        Travel
    };

    struct ToolpathVertex
    {
        glm::vec3 position{ 0.0f, 0.0f, 0.0f };
        float feedRate = 0.0f;   // resolved, absolute mm/s
    };

    struct ToolpathSegment
    {
        ToolpathVertex start;
        ToolpathVertex end;
        float extrusionDelta = 0.0f;   // 0 = travel, >0 = actual E delta
        float extrusionWidth = 0.0f;   // resolved, absolute mm
        ToolpathMoveType moveType = ToolpathMoveType::Unknown;

        bool isExtruding() const { return extrusionDelta > 0.0f; }
    };

    struct ToolpathLayer
    {
        float z = 0.0f;
        std::vector<ToolpathSegment> segments;
        std::vector<std::string> comments;   // raw, unrecognized gcode comments
    };

    class Toolpath
    {
    public:
        std::vector<ToolpathLayer> layers;
        BoundingBox bounds;

        int layerCount() const { return (int)layers.size(); }
    };

    // One Toolpath per toolhead, combined here — not a toolheadId field
    // on ToolpathSegment. Segments belonging to different toolheads
    // aren't really one continuous path; they're separate paths
    // interleaved with tool changes.
    struct ToolheadToolpath
    {
        std::string toolheadId;   // matches slicer.toolheads.N's namespace index, as a string
        Toolpath toolpath;
        float estimatedTime = 0.0f;
        float materialUsed = 0.0f;
    };

    // NOTE: MultiToolheadResult intentionally NOT included here yet —
    // per the design doc's explicit "REVISIT WHEN CODING P6" marker,
    // default to P6 returning std::vector<ToolheadToolpath> directly
    // unless a concrete need for the wrapper struct shows up.

} // namespace domain::v1