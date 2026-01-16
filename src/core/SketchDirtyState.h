#pragma once

#include <cstdint>

namespace core {

// Simple state holder used by UI/controllers to decide when to re-run the constraint solver.
struct SketchDirtyState {
    bool needsSolve = false;
    std::uint64_t changeSerial = 0;

    void MarkDirty() {
        needsSolve = true;
        ++changeSerial;
    }

    void Clear() {
        needsSolve = false;
    }
};

} // namespace core
