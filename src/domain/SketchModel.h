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

        EntityStore entities;
        std::vector<Constraint> constraints;
    };

    struct Document {
        DocumentId id{};
        std::string name;
        std::vector<Sketch> sketches;
    };

} // namespace domain::sketch
