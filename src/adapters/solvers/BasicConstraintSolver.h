#pragma once
#include "ports/ISketchResolverPort.h"

namespace adapters::solver {

    class BasicConstraintSolver final : public ports::ISketchResolverPort {
    public:
        ports::ResolvedSketch solve(const domain::sketch::Sketch& input) override;
    };

}
