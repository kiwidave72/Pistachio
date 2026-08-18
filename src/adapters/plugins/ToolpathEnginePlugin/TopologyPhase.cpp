#include "adapters/plugins/ToolpathEnginePlugin/TopologyPhase.h"
#include "domain/DiagnosticMessage.h"

#include <algorithm>
#include <cstdio>

namespace kinetica {

    namespace {

        // Values within this distance of the test point's Y are treated as
        // level with it, not above or below. This matters because rotating
        // a shape can turn an exact Y-equality (which the raw > comparison
        // already handled safely, since yi==yj==point.y short-circuits to
        // "no crossing") into a near-equality off by a few ULPs of float32
        // rounding -- and which side of that near-tie the rounding lands on
        // is essentially random per-rotation. 1e-4 (0.1 micron) is far
        // below any real printable feature, so this only affects genuine
        // near-ties, not normal containment decisions.
        constexpr double kContainmentEpsilon = 1e-4;

        // Standard 2D point-in-polygon test (ray casting), X/Y only —
        // every chain in a layer shares the same Z by construction.
        bool pointInPolygon(const glm::vec3& point, const std::vector<glm::vec3>& polygon)
        {
            bool inside = false;
            size_t n = polygon.size();
            for (size_t i = 0, j = n - 1; i < n; j = i++)
            {
                // Promote to double for the crossing arithmetic. The input
                // points are still float32, but doing the comparison and
                // intersection math in double keeps this test from adding
                // its own extra rounding error on top of whatever the
                // upstream transform (e.g. rotation) already introduced.
                double xi = polygon[i].x, yi = polygon[i].y;
                double xj = polygon[j].x, yj = polygon[j].y;
                double px = point.x, py = point.y;

                bool yiAbove = (yi - py) > kContainmentEpsilon;
                bool yjAbove = (yj - py) > kContainmentEpsilon;

                // Both endpoints on the same side (or within epsilon of
                // level with the test point) -- this edge can't cross the
                // ray, so skip it. This also makes the function safe
                // against a division by (yj - yi) == 0 below: that can
                // only happen when yi and yj are equal, which always
                // means yiAbove == yjAbove, so we never reach the divide.
                if (yiAbove == yjAbove) continue;

                double xIntersect = xi + (py - yi) * (xj - xi) / (yj - yi);
                if (px < xIntersect) inside = !inside;
            }
            return inside;
        }

        // Signed area (shoelace formula) -- positive means the contour
        // winds counter-clockwise, negative means clockwise. Needed to
        // know which side of an edge is "inward" for the test point below.
        double signedArea(const std::vector<glm::vec3>& polygon)
        {
            double area = 0.0;
            size_t n = polygon.size();
            for (size_t i = 0; i < n; ++i)
            {
                const glm::vec3& p1 = polygon[i];
                const glm::vec3& p2 = polygon[(i + 1) % n];
                area += static_cast<double>(p1.x) * p2.y - static_cast<double>(p2.x) * p1.y;
            }
            return area * 0.5;
        }

        // Single-edge sampling is fragile: if the ONE edge picked happens
        // to be the edge where this contour touches another (e.g. a hole
        // tangent to its outer boundary), the inward-offset point can
        // land inside that other contour instead of staying clear of it,
        // no matter how the offset is tuned. Sampling several edges
        // spread around the contour and taking a majority vote per other
        // contour means only edges actually touching something else can
        // misfire, and they're outvoted by the rest. kSampleCount is a
        // small constant, not proportional to vertex count -- this is a
        // sampling strategy, not a precision knob, so more points past a
        // handful buys negligible extra robustness for real added cost.
        constexpr int kSampleCount = 5;

        std::vector<glm::vec3> inwardTestPoints(const std::vector<glm::vec3>& polygon)
        {
            std::vector<glm::vec3> points;
            size_t n = polygon.size();
            bool ccw = signedArea(polygon) > 0.0;

            size_t sampleN = static_cast<size_t>(std::min<int>(kSampleCount, static_cast<int>(n)));
            points.reserve(sampleN);

            for (size_t s = 0; s < sampleN; ++s)
            {
                size_t edgeIdx = (s * n) / sampleN;
                const glm::vec3& p0 = polygon[edgeIdx];
                const glm::vec3& p1 = polygon[(edgeIdx + 1) % n];
                glm::vec3 mid = (p0 + p1) * 0.5f;

                glm::vec2 edge(p1.x - p0.x, p1.y - p0.y);
                float edgeLen = glm::length(edge);
                if (edgeLen < 1e-8f)
                {
                    points.push_back(mid); // degenerate zero-length edge, fall back to the midpoint itself
                    continue;
                }

                glm::vec2 normal = ccw ? glm::vec2(-edge.y, edge.x) : glm::vec2(edge.y, -edge.x);
                normal = glm::normalize(normal);

                float offset = std::max(edgeLen * 0.01f, 1e-6f);
                points.push_back(glm::vec3(mid.x + normal.x * offset, mid.y + normal.y * offset, mid.z));
            }

            return points;
        }

        // Whether `testPolygon` is nested inside `otherPolygon`, decided by
        // majority vote across testPolygon's sampled inward points rather
        // than trusting any single one -- see inwardTestPoints above for why.
        bool isNestedInside(const std::vector<glm::vec3>& testSamplePoints, const std::vector<glm::vec3>& otherPolygon)
        {
            int votes = 0;
            for (auto& p : testSamplePoints)
                if (pointInPolygon(p, otherPolygon))
                    ++votes;
            return votes * 2 > static_cast<int>(testSamplePoints.size()); // strict majority
        }

        domain::v1::TopologyLayer buildTopologyLayer(const domain::v1::ExtractedLayer& extracted)
        {
            domain::v1::TopologyLayer topoLayer;
            topoLayer.layerIndex = extracted.layerIndex;
            topoLayer.z = extracted.z;

            // Only closed chains participate — natural or repaired, P5
            // doesn't distinguish. Every excluded chain gets a diagnostic,
            // not just a silent count increment.
            std::vector<const domain::v1::SegmentChain*> closedChains;
            for (auto& chain : extracted.chains)
            {
                if (chain.isClosed && chain.points.size() >= 3)
                {
                    closedChains.push_back(&chain);
                    continue;
                }

                ++topoLayer.skippedOpenChains;

                domain::v1::DiagnosticMessage diag;
                diag.severity = domain::v1::DiagnosticSeverity::Info;
                diag.phase = "P5 TopologyPhase";
                diag.message = "chain excluded from topology (not closed, "
                    + std::to_string(chain.points.size()) + " points)";
                diag.hasLocation = !chain.points.empty();
                if (diag.hasLocation) diag.location = chain.points[0];
                topoLayer.diagnostics.push_back(diag);
            }

            // Pre-compute each contour's sample points once, up front --
            // otherwise they'd be recomputed O(n) times inside the O(n^2)
            // depth loop below for no benefit, since they don't depend on
            // which other contour is being tested against.
            std::vector<std::vector<glm::vec3>> sampleCache;
            sampleCache.reserve(closedChains.size());
            for (auto* chain : closedChains)
                sampleCache.push_back(inwardTestPoints(chain->points));

            for (size_t i = 0; i < closedChains.size(); ++i)
            {
                int depth = 0;

                for (size_t j = 0; j < closedChains.size(); ++j)
                {
                    if (i == j) continue;
                    if (isNestedInside(sampleCache[i], closedChains[j]->points))
                        ++depth;
                }

                domain::v1::Contour contour;
                contour.points = closedChains[i]->points;
                contour.nestingDepth = depth;
                contour.isOuter = (depth % 2 == 0);

                topoLayer.contours.push_back(std::move(contour));
            }

            return topoLayer;
        }

    } // anonymous namespace

    std::vector<TopologizedGeometry> TopologyPhase::run(const std::vector<ExtractedGeometry>& input)
    {
        std::vector<TopologizedGeometry> result;
        result.reserve(input.size());


        printf("[TopologyPhase] input has %zu instances\n", input.size());

        for (auto& extracted : input)
        {
            domain::v1::AdvancedTopology topology;
            topology.modelInstanceId = extracted.modelInstanceId;

            int totalOuter = 0, totalHoles = 0, totalSkipped = 0;

            for (auto& layer : extracted.layers)
            {
                auto topoLayer = buildTopologyLayer(layer);

                for (auto& c : topoLayer.contours)
                    (c.isOuter ? totalOuter : totalHoles)++;
                totalSkipped += topoLayer.skippedOpenChains;

                topology.layers.push_back(std::move(topoLayer));
            }

            printf("[TopologyPhase] instance %s: %d outer contours, %d holes, %d chains skipped (unclosed)\n",
                topology.modelInstanceId.c_str(), totalOuter, totalHoles, totalSkipped);

            result.push_back(TopologizedGeometry{ std::move(topology), extracted.report });
        }

        return result;
    }

} // namespace kinetica