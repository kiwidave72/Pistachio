#pragma once

// -----------------------------------------------------------------------
// UnifiedAcceleration.h — P2 output
//
// Z-bucket spatial index. Divides an instance's Z range into buckets
// (sized to slicer.layerHeight, the resolution P3 will actually query
// at) and records which triangles overlap each bucket — so P3 can test
// only the handful of relevant triangles per layer instead of every
// triangle in the mesh.
//
// A triangle spanning multiple buckets (common for near-vertical walls)
// is added to every bucket it overlaps, not just one.
// -----------------------------------------------------------------------

#include <cstdint>
#include <string>
#include <vector>

namespace domain::v1 {

    struct AccelerationBucket
    {
        // Triangle index, not vertex index — triangle N occupies
        // geometry.indices[N*3], [N*3+1], [N*3+2].
        std::vector<uint32_t> triangleIndices;
    };

    class UnifiedAcceleration
    {
    public:
        std::string modelInstanceId;
        float minZ = 0.0f;
        float bucketHeight = 0.0f;
        std::vector<AccelerationBucket> buckets;

        int bucketIndexForZ(float z) const
        {
            if (bucketHeight <= 0.0f || buckets.empty()) return -1;
            int idx = static_cast<int>((z - minZ) / bucketHeight);
            return (idx >= 0 && idx < (int)buckets.size()) ? idx : -1;
        }
    };

} // namespace domain::v1