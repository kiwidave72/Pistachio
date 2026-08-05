#include "adapters/plugins/ToolpathEnginePlugin/SlicingPhase.h"

#include <cstdio>
#include <cmath>

namespace kinetica {

    namespace {

        // Two states only, never three — see design doc P3 open question #3.
        // Bias "on-plane" consistently to one side so the messy 3-vertex
        // classification cases (1-on/1-above/1-below, etc.) structurally
        // cannot occur.
        enum class Side { Below, Above };

        Side classify(float z, float planeZ, float epsilon = 1e-5f)
        {
            return (z - planeZ) < epsilon ? Side::Below : Side::Above;
        }

        glm::vec3 lerpToPlane(const glm::vec3& a, const glm::vec3& b, float planeZ)
        {
            float t = (planeZ - a.z) / (b.z - a.z);
            return a + t * (b - a);
        }

        // Returns true and fills outStart/outEnd if this triangle genuinely
        // crosses the plane (a real 1-vs-2 split). Returns false otherwise
        // (all three vertices on the same side — no intersection).
        bool intersectTriangle(
            const glm::vec3& v0, const glm::vec3& v1, const glm::vec3& v2,
            float planeZ, glm::vec3& outStart, glm::vec3& outEnd)
        {
            Side s0 = classify(v0.z, planeZ);
            Side s1 = classify(v1.z, planeZ);
            Side s2 = classify(v2.z, planeZ);

            if (s0 == s1 && s1 == s2)
                return false;   // no intersection, whole triangle on one side

            // Find the one vertex that's alone on its side — the other two
            // share a side. Exactly one of these three cases is true.
            glm::vec3 lone, a, b;
            if (s0 != s1 && s0 != s2) { lone = v0; a = v1; b = v2; }
            else if (s1 != s0 && s1 != s2) { lone = v1; a = v0; b = v2; }
            else { lone = v2; a = v0; b = v1; }

            outStart = lerpToPlane(lone, a, planeZ);
            outEnd = lerpToPlane(lone, b, planeZ);

            // Filter degenerate near-zero-length segments from the epsilon
            // boundary case — see design doc, not prevented at classification,
            // filtered here instead.
            if (glm::length(outEnd - outStart) < 1e-6f)
                return false;

            return true;
        }

        domain::v1::AdvancedSliceResult sliceInstance(
            const AcceleratedGeometry& accGeom, float firstLayerHeight, float layerHeight)
        {
            domain::v1::AdvancedSliceResult result;
            result.modelInstanceId = accGeom.geometry.modelInstanceId;

            const auto& geom = accGeom.geometry;
            const auto& accel = accGeom.acceleration;

            float minZ = geom.bounds.min.z;
            float maxZ = geom.bounds.max.z;

            // Top-of-band Z sequence — see design doc:
            //   layer 0: sliceZ = minZ + firstLayerHeight
            //   layer N (N>=1): sliceZ = minZ + firstLayerHeight + N*layerHeight
            int layerIndex = 0;
            float sliceZ = minZ + firstLayerHeight;

            while (sliceZ <= maxZ)
            {
                domain::v1::SliceLayer layer;
                layer.layerIndex = layerIndex;
                layer.z = sliceZ;

                int bucket = accel.bucketIndexForZ(sliceZ);
                if (bucket >= 0)
                {
                    for (uint32_t tri : accel.buckets[bucket].triangleIndices)
                    {
                        const glm::vec3& v0 = geom.vertices[geom.indices[tri * 3 + 0]].position;
                        const glm::vec3& v1 = geom.vertices[geom.indices[tri * 3 + 1]].position;
                        const glm::vec3& v2 = geom.vertices[geom.indices[tri * 3 + 2]].position;

                        glm::vec3 segStart, segEnd;
                        if (intersectTriangle(v0, v1, v2, sliceZ, segStart, segEnd))
                            layer.segments.push_back({ segStart, segEnd });
                    }
                }

                result.layers.push_back(std::move(layer));

                ++layerIndex;
                sliceZ = minZ + firstLayerHeight + layerIndex * layerHeight;
            }

            return result;
        }

    } // anonymous namespace

    std::vector<SlicedGeometry> SlicingPhase::run(std::vector<AcceleratedGeometry> input)
    {
        std::vector<SlicedGeometry> result;
        result.reserve(input.size());

        float firstLayerHeight = ports::getConfig<float>(m_config, "slicer.Settings", "slicer.firstLayerHeight");
        float layerHeight = ports::getConfig<float>(m_config, "slicer.Settings", "slicer.layerHeight");

        for (auto& accGeom : input)
        {
            auto sliceResult = sliceInstance(accGeom, firstLayerHeight, layerHeight);

            size_t totalSegments = 0;
            for (auto& l : sliceResult.layers)
                totalSegments += l.segments.size();

            printf("[SlicingPhase] instance %s: %zu layers, %zu total segments\n",
                sliceResult.modelInstanceId.c_str(), sliceResult.layers.size(), totalSegments);

            result.push_back(SlicedGeometry{ std::move(sliceResult), accGeom.report });
        }

        return result;
    }

} // namespace kinetica