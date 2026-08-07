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

        bool connectorStaysInsideRegion(
            const glm::vec2& a, const glm::vec2& b,
            const Clipper2Lib::Paths64& regionPaths)
        {
            Clipper2Lib::Path64 connector;
            connector.push_back(Clipper2Lib::Point64((int64_t)(a.x * kClipperScale), (int64_t)(a.y * kClipperScale)));
            connector.push_back(Clipper2Lib::Point64((int64_t)(b.x * kClipperScale), (int64_t)(b.y * kClipperScale)));

            Clipper2Lib::Clipper64 clipper;
            clipper.AddOpenSubject({ connector });
            clipper.AddClip(regionPaths);

            Clipper2Lib::Paths64 closedSolution, openSolution;
            clipper.Execute(Clipper2Lib::ClipType::Intersection, Clipper2Lib::FillRule::NonZero, closedSolution, openSolution);

            if (openSolution.size() != 1) return false;
            if (openSolution[0].size() != connector.size()) return false;

            return openSolution[0].front() == connector.front() && openSolution[0].back() == connector.back();
        }

        struct WallProjection
        {
            int boundaryIndex = -1;
            size_t nearestVertexIndex = 0;
        };

        WallProjection projectOntoWalls(
            const glm::vec2& point,
            const std::vector<std::vector<glm::vec3>>& allBoundaries,
            float tolerance)
        {
            WallProjection best;
            float bestDist = tolerance;

            for (size_t b = 0; b < allBoundaries.size(); ++b)
            {
                const auto& boundary = allBoundaries[b];
                size_t n = boundary.size();

                for (size_t v = 0; v < n; ++v)
                {
                    glm::vec2 a(boundary[v]);
                    glm::vec2 c(boundary[(v + 1) % n]);
                    glm::vec2 ac = c - a;
                    float lenSq = glm::dot(ac, ac);

                    float t = lenSq > 1e-12f
                        ? glm::clamp(glm::dot(point - a, ac) / lenSq, 0.0f, 1.0f)
                        : 0.0f;
                    glm::vec2 closest = a + ac * t;
                    float d = glm::length(point - closest);

                    if (d < bestDist)
                    {
                        bestDist = d;
                        best.boundaryIndex = (int)b;
                        best.nearestVertexIndex = (t < 0.5f) ? v : (v + 1) % n;
                    }
                }
            }
            return best;
        }

        std::vector<glm::vec2> walkBoundaryBounded(
            const std::vector<glm::vec3>& boundary, size_t from, size_t to, float maxArcLength)
        {
            size_t n = boundary.size();

            auto arcLength = [&](bool forward) -> float
                {
                    float len = 0.0f;
                    size_t i = from;
                    while (i != to)
                    {
                        size_t next = forward ? (i + 1) % n : (i == 0 ? n - 1 : i - 1);
                        len += glm::length(glm::vec2(boundary[next]) - glm::vec2(boundary[i]));
                        i = next;
                    }
                    return len;
                };

            float fwd = arcLength(true);
            float bwd = arcLength(false);
            bool goForward = fwd <= bwd;
            float chosen = goForward ? fwd : bwd;

            if (chosen > maxArcLength) return {};

            std::vector<glm::vec2> result;
            size_t i = from;
            while (true)
            {
                result.push_back(glm::vec2(boundary[i]));
                if (i == to) break;
                i = goForward ? (i + 1) % n : (i == 0 ? n - 1 : i - 1);
            }
            return result;
        }

        struct ScanPiece
        {
            std::vector<glm::vec2> points;
        };

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
        const domain::v1::WallGenerationResult& wallResult,
        float z,
        const ports::IConfigPort& config)
    {
        std::vector<domain::v1::ToolpathSegment> result;
        if (region.polygons.empty()) return result;

        float density = ports::getConfig<float>(config, "slicer.infill.rectilinear.settings", "density");
        float angleDeg = ports::getConfig<float>(config, "slicer.infill.rectilinear.settings", "angle");
        float nozzleSize = ports::getConfig<float>(config, "slicer.toolheads.0.generalSettings", "nozzle.size");
        float extrusionWidthPct = ports::getConfig<float>(config, "slicer.toolheads.0.generalSettings", "extrusionWidth.percent");
        float extrusionWidth = nozzleSize * (extrusionWidthPct / 100.0f);

        if (density <= 0.0f) return result;

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

        std::vector<std::vector<glm::vec3>> allBoundaries;
        allBoundaries.insert(allBoundaries.end(), wallResult.innermostOuterBoundaries.begin(), wallResult.innermostOuterBoundaries.end());
        allBoundaries.insert(allBoundaries.end(), wallResult.innermostHoleBoundaries.begin(), wallResult.innermostHoleBoundaries.end());

        printf("[RectilinearInfill] allBoundaries.size()=%zu (outer=%zu hole=%zu)\n",
            allBoundaries.size(), wallResult.innermostOuterBoundaries.size(), wallResult.innermostHoleBoundaries.size());

        std::vector<ScanPiece> pieces;
        bool reverseNext = false;

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

            Clipper2Lib::Paths64 closedSolution, openSolution;
            clipper.Execute(Clipper2Lib::ClipType::Intersection, Clipper2Lib::FillRule::NonZero, closedSolution, openSolution);

            for (auto& clipped : openSolution)
            {
                if (clipped.size() < 2) continue;

                ScanPiece piece;
                for (auto& pt : clipped)
                    piece.points.push_back(glm::vec2(pt.x / kClipperScale, pt.y / kClipperScale));

                if (reverseNext) std::reverse(piece.points.begin(), piece.points.end());
                pieces.push_back(std::move(piece));
            }

            reverseNext = !reverseNext;
        }

        printf("[RectilinearInfill] total pieces=%zu\n", pieces.size());

        std::vector<glm::vec2> currentRun;
        float maxWallArc = lineSpacing * 20.0f;
        float wallTolerance = lineSpacing * 0.5f;

        auto flushRun = [&]()
            {
                for (size_t p = 0; p + 1 < currentRun.size(); ++p)
                {
                    domain::v1::ToolpathSegment seg;
                    seg.start.position = glm::vec3(currentRun[p], z);
                    seg.end.position = glm::vec3(currentRun[p + 1], z);
                    seg.extrusionDelta = 1.0f;
                    seg.extrusionWidth = extrusionWidth;
                    seg.moveType = domain::v1::ToolpathMoveType::Infill;
                    result.push_back(seg);
                }
                currentRun.clear();
            };

        for (auto& piece : pieces)
        {
            if (piece.points.size() < 2) continue;

            if (currentRun.empty())
            {
                currentRun = piece.points;
                continue;
            }

            glm::vec2 lastPoint = currentRun.back();
            glm::vec2 nextStart = piece.points.front();

            if (connectorStaysInsideRegion(lastPoint, nextStart, regionPaths))
            {
                printf("[RectilinearInfill] connector OK: (%.3f,%.3f) -> (%.3f,%.3f)\n",
                    lastPoint.x, lastPoint.y, nextStart.x, nextStart.y);
                currentRun.insert(currentRun.end(), piece.points.begin(), piece.points.end());
                continue;
            }

            WallProjection projA = projectOntoWalls(lastPoint, allBoundaries, wallTolerance);
            WallProjection projB = projectOntoWalls(nextStart, allBoundaries, wallTolerance);

            printf("[RectilinearInfill] fallback: (%.3f,%.3f)->(%.3f,%.3f) projA.boundary=%d projB.boundary=%d wallTolerance=%.4f\n",
                lastPoint.x, lastPoint.y, nextStart.x, nextStart.y, projA.boundaryIndex, projB.boundaryIndex, wallTolerance);

            if (projA.boundaryIndex != -1 && projA.boundaryIndex == projB.boundaryIndex)
            {
                auto wallPoints = walkBoundaryBounded(
                    allBoundaries[projA.boundaryIndex],
                    projA.nearestVertexIndex, projB.nearestVertexIndex, maxWallArc);

                printf("[RectilinearInfill]   wallPoints.size()=%zu maxWallArc=%.4f\n",
                    wallPoints.size(), maxWallArc);

                if (!wallPoints.empty() && wallPoints.size() > 1)   // guard against degenerate single-point results
                {
                    currentRun.insert(currentRun.end(), wallPoints.begin(), wallPoints.end());
                    currentRun.insert(currentRun.end(), piece.points.begin(), piece.points.end());
                    continue;
                }
            }

            flushRun();
            currentRun = piece.points;
        }

        flushRun();

        return result;
    }

} // namespace kinetica