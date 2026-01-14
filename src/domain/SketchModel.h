#pragma once

#include <string>
#include <vector>

#include "SketchConstraints.h"
#include "SketchEntities.h"
#include "SketchIds.h"

namespace domain::sketch {

    struct Sketch {
        SketchId id{};
        std::string name;
        bool visible{ true };

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
