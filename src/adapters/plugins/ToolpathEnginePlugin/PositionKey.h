#pragma once

// -----------------------------------------------------------------------
// PositionKey.h
//
// Shared position-matching utility. NOT a simple grid-hash-equality
// check — that has a real boundary-straddling flaw: two genuinely
// coincident points can land in different grid cells if they straddle
// a cell boundary, producing false negatives at tight tolerances. Fixed
// here by treating the grid purely as a fast CANDIDATE filter (checking
// a point's cell + all 26 neighbors), with the actual accept/reject
// decision made by real Euclidean distance — not grid-key equality.
// This lets a small, geometrically honest tolerance work correctly,
// without needing to loosen it to something coarse enough to risk
// incorrectly merging genuinely distinct nearby vertices.
// -----------------------------------------------------------------------

#include <glm/glm.hpp>
#include <cstdint>
#include <cmath>
#include <vector>

namespace kinetica {

    constexpr float kDefaultPositionTolerance = 0.05f;

    inline int64_t quantizeCoord(float f, float cellSize = kDefaultPositionTolerance)
    {
        return static_cast<int64_t>(std::floor(f / cellSize));
    }

    inline uint64_t cellKey(int64_t cx, int64_t cy, int64_t cz)
    {
        uint64_t h = 1469598103934665603ull;
        auto mix = [&](int64_t v) { h ^= (uint64_t)v; h *= 1099511628211ull; };
        mix(cx); mix(cy); mix(cz);
        return h;
    }

    inline uint64_t positionKey(const glm::vec3& p, float cellSize = kDefaultPositionTolerance)
    {
        return cellKey(quantizeCoord(p.x, cellSize), quantizeCoord(p.y, cellSize), quantizeCoord(p.z, cellSize));
    }

    // Returns the point's own cell key plus all 26 neighbors (27 total) —
    // use this when SEARCHING for a match, not just the point's own cell.
    inline std::vector<uint64_t> neighborCellKeys(const glm::vec3& p, float cellSize = kDefaultPositionTolerance)
    {
        int64_t cx = quantizeCoord(p.x, cellSize);
        int64_t cy = quantizeCoord(p.y, cellSize);
        int64_t cz = quantizeCoord(p.z, cellSize);

        std::vector<uint64_t> keys;
        keys.reserve(27);
        for (int64_t dx = -1; dx <= 1; ++dx)
            for (int64_t dy = -1; dy <= 1; ++dy)
                for (int64_t dz = -1; dz <= 1; ++dz)
                    keys.push_back(cellKey(cx + dx, cy + dy, cz + dz));
        return keys;
    }

    inline bool positionsEqual(const glm::vec3& a, const glm::vec3& b, float tolerance = kDefaultPositionTolerance)
    {
        return glm::length(a - b) <= tolerance;   // real distance check, not grid equality
    }

} // namespace kinetica