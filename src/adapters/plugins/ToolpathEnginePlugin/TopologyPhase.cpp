#include "adapters/plugins/ToolpathEnginePlugin/TopologyPhase.h"
#include "domain/DiagnosticMessage.h"

#include <cstdio>

namespace kinetica {

    namespace {

        // Standard 2D point-in-polygon test (ray casting), X/Y only —
        // every chain in a layer shares the same Z by construction.
        bool pointInPolygon(const glm::vec3& point, const std::vector<glm::vec3>& polygon)
        {
            bool inside = false;
            size_t n = polygon.size();
            for (size_t i = 0, j = n - 1; i < n; j = i++)
            {
                float xi = polygon[i].x, yi = polygon[i].y;
                float xj = polygon[j].x, yj = polygon[j].y;

                bool intersects = ((yi > point.y) != (yj > point.y)) &&
                    (point.x < (xj - xi) * (point.y - yi) / (yj - yi) + xi);

                if (intersects) inside = !inside;
            }
            return inside;
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

            for (size_t i = 0; i < closedChains.size(); ++i)
            {
                int depth = 0;
                const glm::vec3& testPoint = closedChains[i]->points[0];

                for (size_t j = 0; j < closedChains.size(); ++j)
                {
                    if (i == j) continue;
                    if (pointInPolygon(testPoint, closedChains[j]->points))
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
                // NOTE: dumpTopologyLayer() removed from here — was called
                // AFTER std::move(topoLayer) above (use-after-move bug),
                // and duplicated what onRunPipeline()'s dumpAllTopologyLayers
                // already does correctly, once, after this phase completes.
            }

            printf("[TopologyPhase] instance %s: %d outer contours, %d holes, %d chains skipped (unclosed)\n",
                topology.modelInstanceId.c_str(), totalOuter, totalHoles, totalSkipped);

            result.push_back(TopologizedGeometry{ std::move(topology), extracted.report });
        }

        return result;
    }

} // namespace kinetica