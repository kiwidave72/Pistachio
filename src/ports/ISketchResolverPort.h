#pragma once
#include <string>
#include "domain/SketchModel.h"

namespace ports {

    struct SolveReport {
        bool converged{ false };
        int iterations{ 0 };
        double maxDelta{ 0.0 };
        std::vector<std::string> warnings;
        std::vector<std::string> errors;
    };

    struct ResolvedSketch {
        domain::sketch::Sketch sketch;   // copy of sketch with resolved geometry
        SolveReport report;
    };

    class ISketchResolverPort {
    public:
        virtual ~ISketchResolverPort() = default;
        virtual ResolvedSketch solve(const domain::sketch::Sketch& input) = 0;
    };

}
