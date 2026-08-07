#include "adapters/plugins/ToolpathEnginePlugin/RectilinearInfillStrategy.h"

#include <clipper2/clipper.h>

#include <cmath>
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

    } // anonymous namespace

    std::vector<ports::SettingInfo> RectilinearInfillStrategy::getSettingsSchema() const
    {
        using ports::SettingInfo;
        using ports::SettingType;

        return {
            SettingInfo("slicer.infill.rectilinear.settings", "density", "Infill Density",
                "Percentage fill (0-100)", "Infill", SettingType::Float, 20.0, "", false),
            SettingInfo("slicer.infill.rectilinear.settings", "angle", "Infill Angle",
                "Degrees, alternates 90 per layer for cross-hatching", "Infill", SettingType::Float, 45.0, "", false),
        };
    }

    std::vector<domain::v1::ToolpathSegment> RectilinearInfillStrategy::generate(
        const domain::v1::InfillRegion& region,
        float z,
        const ports::IConfigPort& config)
    {
        std::vector<domain::v1::ToolpathSegment> result;

        printf("[RectilinearInfill] region.polygons=%zu\n", region.polygons.size());
        if (region.polygons.empty())
        {
            printf("[RectilinearInfill] EARLY OUT: region.polygons is empty\n");
            return result;
        }

        float density = ports::getConfig<float>(config, "slicer.infill.rectilinear.settings", "density");
        float angleDeg = ports::getConfig<float>(config, "slicer.infill.rectilinear.settings", "angle");
        float nozzleSize = ports::getConfig<float>(config, "slicer.toolheads.0.generalSettings", "nozzle.size");
        float extrusionWidthPct = ports::getConfig<float>(config, "slicer.toolheads.0.generalSettings", "extrusionWidth.percent");
        float extrusionWidth = nozzleSize * (extrusionWidthPct / 100.0f);

        printf("[RectilinearInfill] density=%.2f angle=%.2f extrusionWidth=%.4f\n", density, angleDeg, extrusionWidth);

        if (density <= 0.0f)
        {
            printf("[RectilinearInfill] EARLY OUT: density <= 0\n");
            return result;
        }

        float lineSpacing = extrusionWidth / (density / 100.0f);

        Clipper2Lib::Paths64 regionPaths;
        for (auto& poly : region.polygons)
            regionPaths.push_back(contourToPath(poly));

        Clipper2Lib::Rect64 bounds = Clipper2Lib::GetBounds(regionPaths);

        float angleRad = glm::radians(angleDeg);
        glm::vec2 dir(std::cos(angleRad), std::sin(angleRad));
        glm::vec2 normal(-dir.y, dir.x);

        double diagonal = std::sqrt(
            std::pow((double)(bounds.right - bounds.left), 2) +
            std::pow((double)(bounds.top - bounds.bottom), 2));

        glm::vec2 center(
            (bounds.left + bounds.right) / 2.0 / kClipperScale,
            (bounds.bottom + bounds.top) / 2.0 / kClipperScale);

        int lineCount = static_cast<int>(diagonal / kClipperScale / lineSpacing) + 2;

        printf("[RectilinearInfill] lineSpacing=%.4f diagonal=%.2f lineCount=%d\n", lineSpacing, diagonal, lineCount);

        for (int i = -lineCount / 2; i <= lineCount / 2; ++i)
        {
            glm::vec2 lineOffset = center + normal * (i * lineSpacing);
            glm::vec2 lineStart = lineOffset - dir * (float)(diagonal / kClipperScale);
            glm::vec2 lineEnd = lineOffset + dir * (float)(diagonal / kClipperScale);

            Clipper2Lib::Path64 scanLine;
            scanLine.push_back(Clipper2Lib::Point64((int64_t)(lineStart.x * kClipperScale), (int64_t)(lineStart.y * kClipperScale)));
            scanLine.push_back(Clipper2Lib::Point64((int64_t)(lineEnd.x * kClipperScale), (int64_t)(lineEnd.y * kClipperScale)));

            Clipper2Lib::Clipper64 clipper;
            clipper.AddOpenSubject({ scanLine });
            clipper.AddClip(regionPaths);

            Clipper2Lib::Paths64 closedSolution;
            Clipper2Lib::Paths64 openSolution;
            clipper.Execute(Clipper2Lib::ClipType::Intersection, Clipper2Lib::FillRule::NonZero, closedSolution, openSolution);

            for (auto& clipped : openSolution)
            {
                for (size_t p = 0; p + 1 < clipped.size(); ++p)
                {
                    domain::v1::ToolpathSegment seg;
                    seg.start.position = glm::vec3(clipped[p].x / kClipperScale, clipped[p].y / kClipperScale, z);
                    seg.end.position = glm::vec3(clipped[p + 1].x / kClipperScale, clipped[p + 1].y / kClipperScale, z);
                    seg.extrusionDelta = 1.0f;
                    seg.extrusionWidth = extrusionWidth;
                    seg.moveType = domain::v1::ToolpathMoveType::Infill;
                    result.push_back(seg);
                }
            }
        }

        printf("[RectilinearInfill] segments generated=%zu\n", result.size());
        return result;
    }

} // namespace kinetica