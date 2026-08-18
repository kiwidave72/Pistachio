#include "adapters/plugins/ToolpathEnginePlugin/TopologyRepairPhase.h"
#include "domain/DiagnosticMessage.h"

#include <cstdio>
#include <cmath>

namespace kinetica {

    namespace {

        // Inlined here rather than pulled from a shared SlidingWindowAnomaly.h --
        // this is the same detection logic (flag a layer whose metric
        // diverges sharply from both neighbors' average, in EITHER
        // direction; only trust a repair when those neighbors also agree
        // with each other), just kept local to this phase for now rather
        // than wired in as a separate header.
        //
        // Two directions matter, not just one:
        //   - a DROP (area collapses) is a missing WALL -- the original
        //     motivating case, a chain that should have been an outer
        //     boundary never closed.
        //   - a RISE (area balloons) is a missing HOLE -- a chain that
        //     should have subtracted area (a screw hole, a cutout) never
        //     closed, so nothing gets carved out and that region prints
        //     solid where it should be a void. This is just as real a
        //     defect as a missing wall, but produces the OPPOSITE area
        //     signature, so a detector that only watches for drops is
        //     structurally blind to it -- confirmed on S6_bracket_set,
        //     where two hole boundaries failed to close and the resulting
        //     solid infill went completely unflagged until manually traced.
        // riseThreshold defaults to the reciprocal of dropThreshold (0.5 ->
        // 2.0) so both directions represent the same relative severity.

        struct AnomalyFlag
        {
            size_t layerArrayIndex = 0;
            float value = 0.0f;
            float neighborAverage = 0.0f;
            float ratio = 0.0f;
            bool isRise = false; // false = area dropped (missing wall), true = area rose (missing hole)
        };

        std::vector<AnomalyFlag> detectSlidingWindowAnomalies(
            const std::vector<float>& perLayerMetric,
            float dropThreshold = 0.5f, float riseThreshold = 2.0f)
        {
            std::vector<AnomalyFlag> flags;
            if (perLayerMetric.size() < 3) return flags;

            for (size_t i = 1; i + 1 < perLayerMetric.size(); ++i)
            {
                float prev = perLayerMetric[i - 1];
                float next = perLayerMetric[i + 1];
                float neighborAvg = (prev + next) * 0.5f;

                if (neighborAvg <= 1e-6f) continue; // neighbors themselves are ~empty, nothing meaningful to compare against

                float value = perLayerMetric[i];
                float ratio = value / neighborAvg;

                if (ratio < dropThreshold)
                    flags.push_back(AnomalyFlag{ i, value, neighborAvg, ratio, false });
                else if (ratio > riseThreshold)
                    flags.push_back(AnomalyFlag{ i, value, neighborAvg, ratio, true });
            }

            return flags;
        }

        // See TopologyRepairPhase.h for why this check exists: a genuine
        // geometric transition shows neighbors disagreeing with EACH OTHER,
        // not just with the flagged layer. Only repair when they agree.
        bool neighborsAreSelfConsistent(
            float prevMetric, float nextMetric, float similarityThreshold = 0.2f)
        {
            float avg = (prevMetric + nextMetric) * 0.5f;
            if (avg <= 1e-6f) return false;
            float diff = std::abs(prevMetric - nextMetric) / avg;
            return diff <= similarityThreshold;
        }

        float polygonArea(const std::vector<glm::vec3>& points)
        {
            double area = 0.0;
            size_t n = points.size();
            for (size_t i = 0; i < n; ++i)
            {
                const glm::vec3& p1 = points[i];
                const glm::vec3& p2 = points[(i + 1) % n];
                area += static_cast<double>(p1.x) * p2.y - static_cast<double>(p2.x) * p1.y;
            }
            return static_cast<float>(std::abs(area) * 0.5);
        }

        // Net solid area on this layer: outer contours contribute
        // positively, holes subtract. This is the per-layer health
        // metric fed into the sliding-window detector -- it drops
        // sharply and specifically when real wall material goes missing,
        // which is exactly the failure mode we're trying to catch.
        float netSolidArea(const domain::v1::TopologyLayer& layer)
        {
            double total = 0.0;
            for (auto& c : layer.contours)
            {
                float a = polygonArea(c.points);
                total += c.isOuter ? a : -a;
            }
            return static_cast<float>(total);
        }

        void repairInstance(domain::v1::AdvancedTopology& topology, RepairReport& report)
        {
            auto& layers = topology.layers;
            if (layers.size() < 3) return;

            std::vector<float> metric;
            metric.reserve(layers.size());
            for (auto& l : layers)
                metric.push_back(netSolidArea(l));

            auto flags = detectSlidingWindowAnomalies(metric);

            for (auto& flag : flags)
            {
                size_t i = flag.layerArrayIndex;
                float prevMetric = metric[i - 1];
                float nextMetric = metric[i + 1];

                // Capture this BEFORE appending our own diagnostic below,
                // so it reflects only what P4/P5 already knew, not our
                // own repair-phase message.
                int priorDiagnosticCount = static_cast<int>(layers[i].diagnostics.size());

                RepairReportEntry entry;
                entry.modelInstanceId = topology.modelInstanceId;
                entry.layerIndex = layers[i].layerIndex;
                entry.z = layers[i].z;
                entry.isRise = flag.isRise;
                entry.measuredArea = flag.value;
                entry.neighborAverageArea = flag.neighborAverage;
                entry.priorDiagnosticCount = priorDiagnosticCount;

                domain::v1::DiagnosticMessage diag;
                diag.phase = "P5R TopologyRepairPhase";
                diag.hasLocation = false;

                // Word the diagnostic differently per direction -- "wall"
                // vs "hole" is the actual actionable difference for anyone
                // triaging these later, not just a sign flip on a number.
                std::string kindDescription = flag.isRise
                    ? "unexpectedly larger (missing hole -- a void likely failed to close and will print solid)"
                    : "collapsed (missing wall)";

                bool safeToRepair = neighborsAreSelfConsistent(prevMetric, nextMetric);

                if (!safeToRepair)
                {
                    entry.wasRepaired = false;
                    report.totalFlaggedNotRepaired++;

                    diag.severity = domain::v1::DiagnosticSeverity::Warning;
                    diag.message = "layer " + std::to_string(layers[i].layerIndex)
                        + " net solid area (" + std::to_string(flag.value)
                        + ") is " + kindDescription + " relative to neighbor average ("
                        + std::to_string(flag.neighborAverage)
                        + "), but neighbors also disagree with each other -- likely a genuine "
                        + "geometric transition, not repaired";
                    layers[i].diagnostics.push_back(diag);

                    printf("[TopologyRepairPhase] layer %d FLAGGED (%s, not repaired, neighbors inconsistent): "
                        "area=%.3f vs neighbor avg=%.3f (ratio=%.2f)\n",
                        layers[i].layerIndex, flag.isRise ? "RISE/missing-hole" : "DROP/missing-wall",
                        flag.value, flag.neighborAverage, flag.ratio);

                    report.entries.push_back(std::move(entry));
                    continue;
                }

                // Neighbors agree with each other and this layer doesn't
                // agree with either -- safe to treat as damage. Repair by
                // copying the nearer neighbor's contours (by metric
                // closeness, a reasonable proxy for "more likely to be
                // representative of this exact Z", though with mutually
                // consistent neighbors either would do).
                bool copyFromPrev = std::abs(prevMetric - flag.value) <= std::abs(nextMetric - flag.value);
                const domain::v1::TopologyLayer& source = copyFromPrev ? layers[i - 1] : layers[i + 1];
                int sourceLayerIndex = copyFromPrev ? layers[i - 1].layerIndex : layers[i + 1].layerIndex;

                int originalLayerIndex = layers[i].layerIndex;
                float originalZ = layers[i].z;

                layers[i].contours = source.contours;
                layers[i].skippedOpenChains = source.skippedOpenChains;
                layers[i].layerIndex = originalLayerIndex; // keep this layer's own identity, only borrow its shape
                layers[i].z = originalZ;

                diag.severity = domain::v1::DiagnosticSeverity::Warning;
                diag.message = "layer " + std::to_string(originalLayerIndex)
                    + " net solid area (" + std::to_string(flag.value)
                    + ") is " + kindDescription + " relative to consistent neighbors (avg=" + std::to_string(flag.neighborAverage)
                    + "); repaired by copying layer " + std::to_string(sourceLayerIndex) + "'s contours";
                layers[i].diagnostics.push_back(diag);

                printf("[TopologyRepairPhase] layer %d REPAIRED (%s, copied from layer %d, %d prior diagnostics "
                    "on this layer before repair -- %s): area was %.3f, neighbor avg %.3f\n",
                    originalLayerIndex, flag.isRise ? "RISE/missing-hole" : "DROP/missing-wall",
                    sourceLayerIndex, priorDiagnosticCount,
                    priorDiagnosticCount == 0 ? "SILENT failure, likely upstream of P4" : "see prior diagnostics for phase attribution",
                    flag.value, flag.neighborAverage);

                entry.wasRepaired = true;
                report.totalRepaired++;
                report.entries.push_back(std::move(entry));
            }
        }

    } // anonymous namespace

    TopologyRepairResult TopologyRepairPhase::run(std::vector<TopologizedGeometry> input)
    {
        RepairReport report;

        for (auto& item : input)
            repairInstance(item.topology, report);

        printf("\n[TopologyRepairPhase] === Run summary: %d repaired, %d flagged-but-not-repaired ===\n",
            report.totalRepaired, report.totalFlaggedNotRepaired);

        if (!report.entries.empty())
        {
            int riseCount = 0;
            for (auto& e : report.entries)
                if (e.isRise) ++riseCount;
            if (riseCount > 0)
            {
                printf("[TopologyRepairPhase] %d of %d anomalies were RISES (missing holes -- solid infill "
                    "printed where a void should be), not drops. Just as real a defect as a missing wall.\n",
                    riseCount, (int)report.entries.size());
            }
        }

        if (report.totalRepaired > 0)
        {
            int silentCount = 0;
            for (auto& e : report.entries)
                if (e.wasRepaired && e.priorDiagnosticCount == 0)
                    ++silentCount;

            if (silentCount > 0)
            {
                printf("[TopologyRepairPhase] %d of %d repairs had ZERO prior diagnostics from P4/P5 -- "
                    "these are silent failures upstream of P4, not yet root-caused. Worth prioritizing.\n",
                    silentCount, report.totalRepaired);
            }
        }

        for (auto& e : report.entries)
        {
            printf("[TopologyRepairPhase]   instance=%s layer=%d z=%.3f %s [%s] (area %.1f vs neighbor avg %.1f, "
                "%d prior diagnostics)\n",
                e.modelInstanceId.c_str(), e.layerIndex, e.z,
                e.wasRepaired ? "REPAIRED" : "FLAGGED-ONLY",
                e.isRise ? "missing-hole" : "missing-wall",
                e.measuredArea, e.neighborAverageArea, e.priorDiagnosticCount);
        }

        return TopologyRepairResult{ std::move(input), std::move(report) };
    }

} // namespace kinetica