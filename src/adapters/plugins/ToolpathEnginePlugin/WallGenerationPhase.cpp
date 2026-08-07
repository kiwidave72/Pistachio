#include "adapters/plugins/ToolpathEnginePlugin/WallGenerationPhase.h"

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

        std::vector<domain::v1::ToolpathSegment> contourToSegments(
            const std::vector<glm::vec3>& points,
            domain::v1::ToolpathMoveType moveType, float extrusionWidth)
        {
            std::vector<domain::v1::ToolpathSegment> segments;
            if (points.size() < 2) return segments;

            for (size_t i = 0; i < points.size(); ++i)
            {
                size_t next = (i + 1) % points.size();

                domain::v1::ToolpathSegment seg;
                seg.start.position = points[i];
                seg.end.position = points[next];
                seg.extrusionDelta = 1.0f;   // placeholder — real E-value resolution comes later
                seg.extrusionWidth = extrusionWidth;
                seg.moveType = moveType;

                segments.push_back(seg);
            }
            return segments;
        }

    } // anonymous namespace

    domain::v1::WallGenerationResult WallGenerationPhase::run(const domain::v1::TopologyLayer& layer)
    {
        domain::v1::WallGenerationResult result;

        int wallCount = ports::getConfig<int>(m_config, "slicer.Settings", "slicer.wallCount");
        float nozzleSize = ports::getConfig<float>(m_config, "slicer.toolheads.0.generalSettings", "nozzle.size");
        float extrusionWidthPct = ports::getConfig<float>(m_config, "slicer.toolheads.0.generalSettings", "extrusionWidth.percent");
        float extrusionWidth = nozzleSize * (extrusionWidthPct / 100.0f);

        int outerCount = 0, holeCount = 0;

        for (auto& contour : layer.contours)
        {

            (contour.isOuter ? outerCount : holeCount)++;

            printf("[WallGenerationPhase] layer z=%.2f: %d outer, %d hole contours\n", layer.z, outerCount, holeCount);

            // Outer walls shrink inward (negative offset); hole walls grow
            // outward into solid material (positive offset) — a hole needs
            // its own printed wall for clean, dimensionally-accurate edges.
            double offsetSign = contour.isOuter ? -1.0 : 1.0;

            auto basePath = contourToPath(contour.points);
            std::vector<glm::vec3> lastWallPoints;   // tracks the innermost wall as the loop progresses


            for (int i = 0; i < wallCount; ++i)
            {
                double offsetAmount = offsetSign * static_cast<double>(extrusionWidth) * (i + 0.5) * kClipperScale;

                Clipper2Lib::Paths64 offsetResult = Clipper2Lib::InflatePaths(
                    { basePath }, offsetAmount,
                    Clipper2Lib::JoinType::Round, Clipper2Lib::EndType::Polygon);

                if (offsetResult.empty())
                {
                    printf("[WallGenerationPhase] wall %d collapsed (contour too small), stopping at %d walls\n", i, i);
                    break;
                }


                for (auto& path : offsetResult)
                {
                    auto points = pathToContour(path, layer.z);
                    auto moveType = (i == 0) ? domain::v1::ToolpathMoveType::OuterWall
                        : domain::v1::ToolpathMoveType::InnerWall;

                    auto segments = contourToSegments(points, moveType, extrusionWidth);
                    result.segments.insert(result.segments.end(), segments.begin(), segments.end());

                    lastWallPoints = points;   // last one written wins — the innermost wall generated this contour
                }
            }

            if (!lastWallPoints.empty())
            {
                if (contour.isOuter)
                    result.innermostOuterBoundaries.push_back(lastWallPoints);
                else
                    result.innermostHoleBoundaries.push_back(lastWallPoints);
            }
        }

        return result;
    }

} // namespace kinetica