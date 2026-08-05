#include "adapters/plugins/ToolpathEnginePlugin/ValidationPhase.h"

#include <glm/glm.hpp>
#include <unordered_map>
#include <cstdio>
#include <cmath>

namespace kinetica {

    namespace {

        int countDegenerateTriangles(const domain::v1::UnifiedGeometry& geom)
        {
            int count = 0;
            for (size_t i = 0; i + 2 < geom.indices.size(); i += 3)
            {
                const glm::vec3& v0 = geom.vertices[geom.indices[i]].position;
                const glm::vec3& v1 = geom.vertices[geom.indices[i + 1]].position;
                const glm::vec3& v2 = geom.vertices[geom.indices[i + 2]].position;

                glm::vec3 cross = glm::cross(v1 - v0, v2 - v0);
                float area2 = glm::length(cross);

                if (area2 < 1e-8f)
                    ++count;
            }
            return count;
        }

        // Quantize a position to an integer grid so nearly-identical
        // floating point positions (from independently-stored, non-
        // deduplicated triangle corners) hash identically.
        int64_t quantize(float f)
        {
            return static_cast<int64_t>(std::round(f * 10000.0f));   // 0.0001 unit tolerance
        }

        uint64_t positionKey(const glm::vec3& p)
        {
            int64_t x = quantize(p.x), y = quantize(p.y), z = quantize(p.z);
            // Combine three quantized coordinates into one hash — fine for
            // this diagnostic use, doesn't need to be collision-proof.
            uint64_t h = 1469598103934665603ull;
            auto mix = [&](int64_t v) {
                h ^= (uint64_t)v;
                h *= 1099511628211ull;
                };
            mix(x); mix(y); mix(z);
            return h;
        }

        // Edge key from two positions, order-independent (a,b) == (b,a).
        std::pair<uint64_t, uint64_t> edgeKey(const glm::vec3& a, const glm::vec3& b)
        {
            uint64_t ka = positionKey(a);
            uint64_t kb = positionKey(b);
            if (ka > kb) std::swap(ka, kb);
            return { ka, kb };
        }

        struct PairHash
        {
            size_t operator()(const std::pair<uint64_t, uint64_t>& p) const
            {
                return std::hash<uint64_t>()(p.first) ^ (std::hash<uint64_t>()(p.second) << 1);
            }
        };

        int countNonManifoldEdges(const domain::v1::UnifiedGeometry& geom)
        {
            std::unordered_map<std::pair<uint64_t, uint64_t>, int, PairHash> edgeCounts;

            for (size_t i = 0; i + 2 < geom.indices.size(); i += 3)
            {
                const glm::vec3& a = geom.vertices[geom.indices[i]].position;
                const glm::vec3& b = geom.vertices[geom.indices[i + 1]].position;
                const glm::vec3& c = geom.vertices[geom.indices[i + 2]].position;

                ++edgeCounts[edgeKey(a, b)];
                ++edgeCounts[edgeKey(b, c)];
                ++edgeCounts[edgeKey(c, a)];
            }

            int count = 0;
            for (auto& [key, n] : edgeCounts)
                if (n != 2) ++count;

            return count;
        }

    } // anonymous namespace

    std::vector<ValidatedGeometry> ValidationPhase::run(std::vector<domain::v1::UnifiedGeometry> input)
    {
        std::vector<ValidatedGeometry> result;
        result.reserve(input.size());

        for (auto& geom : input)
        {
            ValidationReport report;
            report.modelInstanceId = geom.modelInstanceId;
            report.degenerateTriangleCount = countDegenerateTriangles(geom);
            report.nonManifoldEdgeCount = countNonManifoldEdges(geom);

            if (!report.isClean())
                printf("[ValidationPhase] WARNING: instance %s - %d degenerate triangles, %d non-manifold edges\n",
                    report.modelInstanceId.c_str(), report.degenerateTriangleCount, report.nonManifoldEdgeCount);
            else
                printf("[ValidationPhase] instance %s: clean\n", report.modelInstanceId.c_str());

            result.push_back(ValidatedGeometry{ std::move(geom), report });
        }

        return result;
    }

} // namespace kinetica