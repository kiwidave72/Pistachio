#include "adapters/plugins/ToolpathEnginePlugin/AccelerationPhase.h"

#include <cstdio>
#include <cmath>

namespace kinetica {

    namespace {

        domain::v1::UnifiedAcceleration buildAcceleration(
            const domain::v1::UnifiedGeometry& geom, float layerHeight)
        {
            domain::v1::UnifiedAcceleration accel;
            accel.modelInstanceId = geom.modelInstanceId;
            accel.bucketHeight = layerHeight;
            accel.minZ = geom.bounds.min.z;

            float zRange = geom.bounds.max.z - geom.bounds.min.z;
            int bucketCount = (layerHeight > 0.0f)
                ? static_cast<int>(std::ceil(zRange / layerHeight)) + 1
                : 1;
            if (bucketCount < 1) bucketCount = 1;

            accel.buckets.resize(bucketCount);

            uint32_t triangleCount = (uint32_t)(geom.indices.size() / 3);
            for (uint32_t tri = 0; tri < triangleCount; ++tri)
            {
                const glm::vec3& v0 = geom.vertices[geom.indices[tri * 3 + 0]].position;
                const glm::vec3& v1 = geom.vertices[geom.indices[tri * 3 + 1]].position;
                const glm::vec3& v2 = geom.vertices[geom.indices[tri * 3 + 2]].position;

                float triMinZ = std::min({ v0.z, v1.z, v2.z });
                float triMaxZ = std::max({ v0.z, v1.z, v2.z });

                int startBucket = accel.bucketIndexForZ(triMinZ);
                int endBucket = accel.bucketIndexForZ(triMaxZ);

                // Both can legitimately be -1 only if triMinZ/triMaxZ fall
                // outside [minZ, minZ + bucketCount*bucketHeight) — shouldn't
                // happen given bucketCount was sized from this same geometry's
                // own bounds, but clamp defensively rather than skip silently.
                if (startBucket < 0) startBucket = 0;
                if (endBucket < 0) endBucket = bucketCount - 1;
                if (endBucket >= bucketCount) endBucket = bucketCount - 1;

                for (int b = startBucket; b <= endBucket; ++b)
                    accel.buckets[b].triangleIndices.push_back(tri);
            }

            return accel;
        }

    } // anonymous namespace

    std::vector<AcceleratedGeometry> AccelerationPhase::run(std::vector<ValidatedGeometry> input)
    {
        std::vector<AcceleratedGeometry> result;
        result.reserve(input.size());

        float layerHeight = ports::getConfig<float>(m_config, "slicer.Settings", "slicer.layerHeight");

        for (auto& v : input)
        {
            auto accel = buildAcceleration(v.geometry, layerHeight);

            printf("[AccelerationPhase] instance %s: %d buckets, %zu triangles\n",
                accel.modelInstanceId.c_str(), (int)accel.buckets.size(),
                v.geometry.indices.size() / 3);

            result.push_back(AcceleratedGeometry{
                std::move(v.geometry), v.report, std::move(accel)
                });
        }

        return result;
    }

} // namespace kinetica