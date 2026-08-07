#include "adapters/plugins/ToolpathEnginePlugin/InfillRegionPhase.h"

#include <clipper2/clipper.h>

#include <cstdio>

namespace kinetica {

    namespace {

        constexpr double kClipperScale = 1000.0;

        Clipper2Lib::Path64 contourToPath(const std::vector<glm::vec3>& points)
        {
            Clipper2Lib::Path64 path;
            path.reserve(points.size());
            for (auto& p : points)
                path.push_back(Clipper2Lib::Point64(
                    static_cast<int64_t>(p.x * kClipperScale),
                    static_cast<int64_t>(p.y * kClipperScale)));
            return path;
        }

        std::vector<glm::vec3> pathToContour(const Clipper2Lib::Path64& path, float z)
        {
            std::vector<glm::vec3> points;
            points.reserve(path.size());
            for (auto& pt : path)
                points.push_back(glm::vec3(
                    static_cast<float>(pt.x / kClipperScale),
                    static_cast<float>(pt.y / kClipperScale),
                    z));
            return points;
        }

        // Shoelace formula — real area in mm^2, used purely for the
        // spurious-hole size filter, not for any geometric computation.
        double contourArea(const std::vector<glm::vec3>& points)
        {
            double area = 0.0;
            for (size_t i = 0; i < points.size(); ++i)
            {
                size_t j = (i + 1) % points.size();
                area += (double)points[i].x * points[j].y - (double)points[j].x * points[i].y;
            }
            return std::abs(area) * 0.5;
        }

    } // anonymous namespace

    domain::v1::InfillRegion InfillRegionPhase::run(
        const domain::v1::WallGenerationResult& wallResult,
        float minHoleArea)
    {
        domain::v1::InfillRegion result;

        if (wallResult.innermostOuterBoundaries.empty())
            return result;   // no outer wall at all this layer — nothing to fill

        Clipper2Lib::Paths64 outerPaths;
        for (auto& boundary : wallResult.innermostOuterBoundaries)
            outerPaths.push_back(contourToPath(boundary));

        Clipper2Lib::Paths64 holePaths;
        for (auto& boundary : wallResult.innermostHoleBoundaries)
        {
            double area = contourArea(boundary);
            if (area < minHoleArea)
            {
                ++result.holesFilteredAsSpurious;
                continue;   // excluded — treated as tessellation-seam noise, not a real hole
            }
            holePaths.push_back(contourToPath(boundary));
        }

        Clipper2Lib::Paths64 diffResult = Clipper2Lib::Difference(
            outerPaths, holePaths, Clipper2Lib::FillRule::NonZero);

        // z is the same across every boundary on this layer — take it
        // from the first outer boundary's first point.
        float z = wallResult.innermostOuterBoundaries[0].empty()
            ? 0.0f : wallResult.innermostOuterBoundaries[0][0].z;

        for (auto& path : diffResult)
            result.polygons.push_back(pathToContour(path, z));

        if (result.holesFilteredAsSpurious > 0)
            printf("[InfillRegionPhase] filtered %d hole(s) as spurious (area < %.2f mm^2)\n",
                result.holesFilteredAsSpurious, minHoleArea);

        printf("[InfillRegionPhase] outerBoundaries=%zu holeBoundaries=%zu -> region.polygons=%zu\n",
            wallResult.innermostOuterBoundaries.size(), wallResult.innermostHoleBoundaries.size(), result.polygons.size());

        return result;
    }

} // namespace kinetica