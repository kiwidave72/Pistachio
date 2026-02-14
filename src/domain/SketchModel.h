#pragma once

#include <string>
#include <vector>

#include "SketchConstraints.h"
#include "SketchEntities.h"
#include "SketchIds.h"

namespace domain::sketch {

    // Orthogonal plane that the sketch is drawn on
    enum class SketchPlane {
        XY = 0,  // Front/Back view (sketching in XY, perpendicular to Z)
        YZ = 1,  // Side view (sketching in YZ, perpendicular to X)
        XZ = 2   // Top/Bottom view (sketching in XZ, perpendicular to Y)
    };

    struct Sketch {
        SketchId id{};
        std::string name;
        bool visible{ true };
        SketchPlane plane{ SketchPlane::XY };  // Default to XY plane

        // Stable ID generators (required for undo/redo commands).
        EntityId nextEntityId{ 1 };
        ConstraintId nextConstraintId{ 1 };

        EntityStore entities;
        std::vector<Constraint> constraints;
    };

    struct Document {
        DocumentId id{};
        std::string name;
        std::vector<Sketch> sketches;
    };

} // namespace domain::sketch
