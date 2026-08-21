#include "adapters/plugins/ToolpathEnginePlugin/RectilinearInfillStrategy.h"

#include <clipper2/clipper.h>

#include <algorithm>
#include <cfloat>
#include <cmath>
#include <cstdio>
#include <limits>

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

        // Returns true if segments (p1,p2) and (p3,p4) properly intersect
        // (strict interior crossing; shared endpoints/collinear overlap don't count).
        bool segmentsIntersect(const glm::vec2& p1, const glm::vec2& p2,
            const glm::vec2& p3, const glm::vec2& p4)
        {
            auto cross = [](const glm::vec2& a, const glm::vec2& b) { return a.x * b.y - a.y * b.x; };

            glm::vec2 r = p2 - p1;
            glm::vec2 s = p4 - p3;
            float rxs = cross(r, s);
            if (std::abs(rxs) < 1e-9f) return false; // parallel/collinear, ignore for this purpose

            glm::vec2 qp = p3 - p1;
            float t = cross(qp, s) / rxs;
            float u = cross(qp, r) / rxs;

            return t > 1e-6f && t < 1.0f - 1e-6f && u > 1e-6f && u < 1.0f - 1e-6f;
        }

        // Checks whether the straight connector (a,b) crosses any edge of any region
        // boundary (outer contour or hole). This is what catches connectors that dip
        // outside the region and back in around concave features (e.g. circular holes)
        // that a pure midpoint-containment test would miss.
        bool connectorCrossesAnyEdge(
            const glm::vec2& a, const glm::vec2& b,
            const Clipper2Lib::Paths64& regionPaths)
        {
            for (auto& path : regionPaths)
            {
                size_t n = path.size();
                for (size_t i = 0; i < n; ++i)
                {
                    glm::vec2 e0(path[i].x / kClipperScale, path[i].y / kClipperScale);
                    glm::vec2 e1(path[(i + 1) % n].x / kClipperScale, path[(i + 1) % n].y / kClipperScale);
                    if (segmentsIntersect(a, b, e0, e1))
                        return true;
                }
            }
            return false;
        }

        // Was: full Clipper64 boolean intersection (AddOpenSubject + AddClip + Execute)
        // for every connector test. That builds a whole clipping engine (edge lists,
        // sorting, local minima) just to answer "is this short segment inside the region?"
        //
        // Now: reject if the connector crosses any boundary edge (outer wall or hole),
        // then confirm containment via point-in-polygon on the midpoint. The edge-crossing
        // check is required on non-convex regions (e.g. circular holes) — a midpoint-only
        // test can pass even when the segment dips outside the region and back in, which
        // is what caused infill to cross walls around holes. Still O(V) with no clipper
        // engine allocation, just a linear scan over edges with a closed-form line test.
        bool connectorStaysInsideRegion(
            const glm::vec2& a, const glm::vec2& b,
            const Clipper2Lib::Paths64& regionPaths,
            float wallTolerance)
        {
            // Cheap early-out: most connectors between adjacent scan-line pieces
            // are tiny gaps well within tolerance.
            if (glm::length(b - a) < wallTolerance)
                return true;

            if (connectorCrossesAnyEdge(a, b, regionPaths))
                return false;

            glm::vec2 mid = (a + b) * 0.5f;
            Clipper2Lib::Point64 pt(
                static_cast<int64_t>(mid.x * kClipperScale),
                static_cast<int64_t>(mid.y * kClipperScale));

            int insideCount = 0;
            for (auto& path : regionPaths)
            {
                auto pip = Clipper2Lib::PointInPolygon(pt, path);
                if (pip != Clipper2Lib::PointInPolygonResult::IsOutside)
                    insideCount++;
            }
            return (insideCount % 2) == 1;
        }

        struct WallProjection
        {
            int boundaryIndex = -1;
            size_t nearestVertexIndex = 0;
        };

        struct BoundaryBounds
        {
            glm::vec2 min{ FLT_MAX, FLT_MAX };
            glm::vec2 max{ -FLT_MAX, -FLT_MAX };
        };

        BoundaryBounds computeBounds(const std::vector<glm::vec3>& boundary)
        {
            BoundaryBounds b;
            for (auto& p : boundary)
            {
                glm::vec2 v(p);
                b.min = glm::min(b.min, v);
                b.max = glm::max(b.max, v);
            }
            return b;
        }

        // Precomputed cumulative arc-length table for a boundary, so walkBoundaryBounded
        // doesn't have to walk the whole ring (forward AND backward) just to decide
        // whether a candidate join is short enough to take.
        struct BoundaryArcInfo
        {
            std::vector<float> cumDist; // cumDist[k] = forward distance from vertex 0 to vertex k, size n+1
            float totalLen = 0.0f;
        };

        BoundaryArcInfo buildArcInfo(const std::vector<glm::vec3>& boundary)
        {
            BoundaryArcInfo info;
            size_t n = boundary.size();
            info.cumDist.resize(n + 1, 0.0f);
            for (size_t i = 0; i < n; ++i)
            {
                glm::vec2 a(boundary[i]);
                glm::vec2 c(boundary[(i + 1) % n]);
                info.cumDist[i + 1] = info.cumDist[i] + glm::length(c - a);
            }
            info.totalLen = info.cumDist[n];
            return info;
        }

        // O(1) forward arc length from vertex index 'from' to 'to', using the prefix sums.
        float forwardArcLength(const BoundaryArcInfo& info, size_t from, size_t to)
        {
            if (from == to) return 0.0f;
            return (to > from)
                ? (info.cumDist[to] - info.cumDist[from])
                : (info.totalLen - info.cumDist[from] + info.cumDist[to]);
        }

        WallProjection projectOntoWalls(
            const glm::vec2& point,
            const std::vector<std::vector<glm::vec3>>& allBoundaries,
            const std::vector<BoundaryBounds>& allBounds,
            float tolerance)
        {
            WallProjection best;
            float bestDist = tolerance;

            for (size_t b = 0; b < allBoundaries.size(); ++b)
            {
                // AABB reject: skip boundaries that can't possibly be within tolerance,
                // avoiding the O(n) vertex walk entirely for most boundaries.
                const auto& bounds = allBounds[b];
                if (point.x < bounds.min.x - tolerance || point.x > bounds.max.x + tolerance ||
                    point.y < bounds.min.y - tolerance || point.y > bounds.max.y + tolerance)
                    continue;

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
            const std::vector<glm::vec3>& boundary,
            const BoundaryArcInfo& arcInfo,
            size_t from, size_t to, float maxArcLength)
        {
            size_t n = boundary.size();

            float fwd = forwardArcLength(arcInfo, from, to);
            float bwd = arcInfo.totalLen - fwd;
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
        regionPaths.reserve(region.polygons.size());
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
        int startIndex = -lineCount / 2;

        std::vector<std::vector<glm::vec3>> allBoundaries;
        allBoundaries.reserve(wallResult.innermostOuterBoundaries.size() + wallResult.innermostHoleBoundaries.size());
        allBoundaries.insert(allBoundaries.end(), wallResult.innermostOuterBoundaries.begin(), wallResult.innermostOuterBoundaries.end());
        allBoundaries.insert(allBoundaries.end(), wallResult.innermostHoleBoundaries.begin(), wallResult.innermostHoleBoundaries.end());

        // Precompute arc-length tables and AABBs once, instead of re-walking boundaries
        // and re-scanning all vertices on every fallback connector.
        std::vector<BoundaryArcInfo> allArcInfo;
        std::vector<BoundaryBounds> allBounds;
        allArcInfo.reserve(allBoundaries.size());
        allBounds.reserve(allBoundaries.size());
        for (auto& boundary : allBoundaries)
        {
            allArcInfo.push_back(buildArcInfo(boundary));
            allBounds.push_back(computeBounds(boundary));
        }

        //printf("[RectilinearInfill] allBoundaries.size()=%zu (outer=%zu hole=%zu)\n",
        //    allBoundaries.size(), wallResult.innermostOuterBoundaries.size(), wallResult.innermostHoleBoundaries.size());

        // --- Scan-line generation ---
        // Was: one Clipper64 (AddOpenSubject + AddClip(regionPaths) + Execute) PER scan
        // line, rebuilding the whole clipping engine (edge lists, sorting of regionPaths)
        // lineCount times. Now: batch every scan line into a single open-subject set and
        // clip against regionPaths once. Since Clipper2 doesn't tag output paths with
        // which input line they came from, we recover the source line index geometrically
        // (all points on line i satisfy dot(p - center, normal) == i * lineSpacing), then
        // sort into the same raster order the original nested loop produced.
        std::vector<ScanPiece> pieces;
        {
            Clipper2Lib::Paths64 allScanLines;
            allScanLines.reserve(lineCount + 1);

            for (int i = startIndex; i <= lineCount / 2; ++i)
            {
                glm::vec2 lineOffset = center + normal * (i * lineSpacing);
                glm::vec2 lineStart = lineOffset - dir * (float)(diagonal / kClipperScale);
                glm::vec2 lineEnd = lineOffset + dir * (float)(diagonal / kClipperScale);

                Clipper2Lib::Path64 scanLine;
                scanLine.push_back(Clipper2Lib::Point64((int64_t)(lineStart.x * kClipperScale), (int64_t)(lineStart.y * kClipperScale)));
                scanLine.push_back(Clipper2Lib::Point64((int64_t)(lineEnd.x * kClipperScale), (int64_t)(lineEnd.y * kClipperScale)));
                allScanLines.push_back(std::move(scanLine));
            }

            Clipper2Lib::Clipper64 clipper;
            clipper.AddOpenSubject(allScanLines);
            clipper.AddClip(regionPaths);

            Clipper2Lib::Paths64 closedSolution, openSolution;
            clipper.Execute(Clipper2Lib::ClipType::Intersection, Clipper2Lib::FillRule::NonZero, closedSolution, openSolution);

            struct RawPiece
            {
                int lineIndex;
                double tAlongLine;
                std::vector<glm::vec2> points;
            };
            std::vector<RawPiece> raw;
            raw.reserve(openSolution.size());

            for (auto& clipped : openSolution)
            {
                if (clipped.size() < 2) continue;

                std::vector<glm::vec2> pts;
                pts.reserve(clipped.size());
                for (auto& pt : clipped)
                    pts.push_back(glm::vec2(pt.x / kClipperScale, pt.y / kClipperScale));

                glm::vec2 mid = (pts.front() + pts.back()) * 0.5f;
                double offsetAlongNormal = glm::dot(mid - center, normal);
                int lineIndex = (int)std::lround(offsetAlongNormal / (double)lineSpacing);
                double tAlongLine = glm::dot(pts.front() - center, dir);

                raw.push_back(RawPiece{ lineIndex, tAlongLine, std::move(pts) });
            }

            std::sort(raw.begin(), raw.end(), [](const RawPiece& a, const RawPiece& b)
                {
                    if (a.lineIndex != b.lineIndex) return a.lineIndex < b.lineIndex;
                    return a.tAlongLine < b.tAlongLine;
                });

            pieces.reserve(raw.size());
            for (auto& rp : raw)
            {
                // Matches original: reverseNext toggled once per loop iteration i,
                // unconditionally, starting false at i == startIndex.
                bool reverse = ((rp.lineIndex - startIndex) % 2) != 0;

                ScanPiece piece;
                piece.points = std::move(rp.points);
                if (reverse) std::reverse(piece.points.begin(), piece.points.end());
                pieces.push_back(std::move(piece));
            }
        }

        //printf("[RectilinearInfill] total pieces=%zu\n", pieces.size());

        std::vector<glm::vec2> currentRun;
        currentRun.reserve(64);
        float maxWallArc = lineSpacing * 20.0f;
        float wallTolerance = lineSpacing * 0.5f;

        result.reserve(pieces.size() * 2);

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

            if (connectorStaysInsideRegion(lastPoint, nextStart, regionPaths, wallTolerance))
            {
                //printf("[RectilinearInfill] connector OK: (%.3f,%.3f) -> (%.3f,%.3f)\n",
                //    lastPoint.x, lastPoint.y, nextStart.x, nextStart.y);
                currentRun.insert(currentRun.end(), piece.points.begin(), piece.points.end());
                continue;
            }

            WallProjection projA = projectOntoWalls(lastPoint, allBoundaries, allBounds, wallTolerance);
            WallProjection projB = projectOntoWalls(nextStart, allBoundaries, allBounds, wallTolerance);

            //printf("[RectilinearInfill] fallback: (%.3f,%.3f)->(%.3f,%.3f) projA.boundary=%d projB.boundary=%d wallTolerance=%.4f\n",
            //    lastPoint.x, lastPoint.y, nextStart.x, nextStart.y, projA.boundaryIndex, projB.boundaryIndex, wallTolerance);

            if (projA.boundaryIndex != -1 && projA.boundaryIndex == projB.boundaryIndex)
            {
                auto wallPoints = walkBoundaryBounded(
                    allBoundaries[projA.boundaryIndex],
                    allArcInfo[projA.boundaryIndex],
                    projA.nearestVertexIndex, projB.nearestVertexIndex, maxWallArc);

                //printf("[RectilinearInfill]   wallPoints.size()=%zu maxWallArc=%.4f\n",
                //    wallPoints.size(), maxWallArc);

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