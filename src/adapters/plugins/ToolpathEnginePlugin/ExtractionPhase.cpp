#include "adapters/plugins/ToolpathEnginePlugin/ExtractionPhase.h"
#include "adapters/plugins/ToolpathEnginePlugin/PositionKey.h"
#include "domain/DiagnosticMessage.h"

#include <unordered_map>
#include <deque>
#include <algorithm>
#include <cstdio>

namespace kinetica {

    namespace {

        // Hardcoded for now, per explicit decision not to add a config
        // setting yet. Move to slicer.maxGapRepairDistance later if this
        // needs to become user-tunable.
        constexpr float kRepairThreshold = 0.1f;

        std::vector<domain::v1::SegmentChain> chainSegments(
            const std::vector<domain::v1::SliceSegment>& segments,
            std::vector<domain::v1::DiagnosticMessage>& outDiagnostics)
        {
            std::vector<domain::v1::SegmentChain> chains;
            if (segments.empty()) return chains;

            std::unordered_map<uint64_t, std::vector<std::pair<size_t, int>>> adjacency;
            for (size_t i = 0; i < segments.size(); ++i)
            {
                adjacency[positionKey(segments[i].start)].push_back({ i, 0 });
                adjacency[positionKey(segments[i].end)].push_back({ i, 1 });
            }

            std::vector<bool> used(segments.size(), false);

            auto removeFromAdjacency = [&](size_t segIdx)
                {
                    auto removeOne = [&](const glm::vec3& p)
                        {
                            auto it = adjacency.find(positionKey(p));
                            if (it == adjacency.end()) return;
                            auto& vec = it->second;
                            vec.erase(std::remove_if(vec.begin(), vec.end(),
                                [&](const std::pair<size_t, int>& pr) { return pr.first == segIdx; }),
                                vec.end());
                        };
                    removeOne(segments[segIdx].start);
                    removeOne(segments[segIdx].end);
                };

            auto popMatch = [&](const glm::vec3& point) -> std::pair<size_t, int>
                {
                    for (uint64_t key : neighborCellKeys(point))
                    {
                        auto it = adjacency.find(key);
                        if (it == adjacency.end()) continue;
                        for (auto& pr : it->second)
                        {
                            if (used[pr.first]) continue;
                            const glm::vec3& candidatePos = (pr.second == 0)
                                ? segments[pr.first].start : segments[pr.first].end;
                            if (positionsEqual(point, candidatePos))
                                return pr;
                        }
                    }
                    return { (size_t)-1, -1 };
                };

            auto consume = [&](size_t segIdx)
                {
                    used[segIdx] = true;
                    removeFromAdjacency(segIdx);
                };

            for (size_t startSeg = 0; startSeg < segments.size(); ++startSeg)
            {
                if (used[startSeg]) continue;

                consume(startSeg);

                std::deque<glm::vec3> points;
                points.push_back(segments[startSeg].start);
                points.push_back(segments[startSeg].end);

                bool closed = false;

                // Extend forward from the end
                glm::vec3 current = points.back();
                while (true)
                {
                    if (positionsEqual(current, points.front()))
                    {
                        closed = true;
                        break;
                    }

                    auto [nextSeg, whichEnd] = popMatch(current);
                    if (nextSeg == (size_t)-1)
                    {
                        printf("[ExtractionPhase]   dead-end (forward) at (%.4f, %.4f, %.4f) - chain has %zu points so far\n",
                            current.x, current.y, current.z, points.size());

                        domain::v1::DiagnosticMessage diag;
                        diag.severity = domain::v1::DiagnosticSeverity::Warning;
                        diag.phase = "P4 ExtractionPhase";
                        diag.message = "dead-end (forward), chain had " + std::to_string(points.size()) + " points";
                        diag.hasLocation = true;
                        diag.location = current;
                        outDiagnostics.push_back(diag);
                        break;
                    }

                    glm::vec3 next = (whichEnd == 0) ? segments[nextSeg].end : segments[nextSeg].start;
                    consume(nextSeg);
                    points.push_back(next);
                    current = next;
                }

                // If it didn't close, extend BACKWARD from the original
                // start too — a segment landing mid-chain must be able to
                // grow in both directions, not just forward.
                if (!closed)
                {
                    glm::vec3 currentBack = points.front();
                    while (true)
                    {
                        auto [prevSeg, whichEnd] = popMatch(currentBack);
                        if (prevSeg == (size_t)-1)
                        {
                            printf("[ExtractionPhase]   dead-end (backward) at (%.4f, %.4f, %.4f) - chain has %zu points so far\n",
                                currentBack.x, currentBack.y, currentBack.z, points.size());

                            domain::v1::DiagnosticMessage diag;
                            diag.severity = domain::v1::DiagnosticSeverity::Warning;
                            diag.phase = "P4 ExtractionPhase";
                            diag.message = "dead-end (backward), chain had " + std::to_string(points.size()) + " points";
                            diag.hasLocation = true;
                            diag.location = currentBack;
                            outDiagnostics.push_back(diag);
                            break;
                        }

                        glm::vec3 prev = (whichEnd == 0) ? segments[prevSeg].end : segments[prevSeg].start;
                        consume(prevSeg);
                        points.push_front(prev);
                        currentBack = prev;
                    }
                }

                bool wasRepaired = false;

                // Repair: if the chain still isn't closed but its two loose
                // ends are close, snap-bridge them rather than leave a gap.
                if (!closed && points.size() >= 3)
                {
                    float gap = glm::length(points.front() - points.back());
                    if (gap <= kRepairThreshold)
                    {
                        glm::vec3 midpoint = (points.front() + points.back()) * 0.5f;
                        points.front() = midpoint;
                        points.back() = midpoint;
                        closed = true;
                        wasRepaired = true;

                        domain::v1::DiagnosticMessage diag;
                        diag.severity = domain::v1::DiagnosticSeverity::Info;
                        diag.phase = "P4 ExtractionPhase";
                        diag.message = "chain repaired via gap snap, distance=" + std::to_string(gap);
                        diag.hasLocation = true;
                        diag.location = midpoint;
                        outDiagnostics.push_back(diag);
                    }
                }

                // A 2-point "loop" isn't a valid polygon.
                if (closed && points.size() < 3)
                {
                    closed = false;
                }

                domain::v1::SegmentChain chain;
                chain.points.assign(points.begin(), points.end());
                chain.isClosed = closed;
                chain.wasRepaired = wasRepaired;
                chains.push_back(std::move(chain));
            }

            return chains;
        }

    } // anonymous namespace

    std::vector<ExtractedGeometry> ExtractionPhase::run(std::vector<SlicedGeometry> input)
    {
        std::vector<ExtractedGeometry> result;
        result.reserve(input.size());

        for (auto& sliced : input)
        {
            ExtractedGeometry extracted;
            extracted.modelInstanceId = sliced.sliceResult.modelInstanceId;
            extracted.report = sliced.report;

            int closedChains = 0, openChains = 0, repairedChains = 0;

            for (auto& layer : sliced.sliceResult.layers)
            {
                domain::v1::ExtractedLayer extractedLayer;
                extractedLayer.layerIndex = layer.layerIndex;
                extractedLayer.z = layer.z;
                extractedLayer.chains = chainSegments(layer.segments, extractedLayer.diagnostics);

                for (auto& c : extractedLayer.chains)
                {
                    (c.isClosed ? closedChains : openChains)++;
                    if (c.wasRepaired) ++repairedChains;
                }

                extracted.layers.push_back(std::move(extractedLayer));
            }

            printf("[ExtractionPhase] instance %s: %d closed chains (%d repaired), %d open chains\n",
                extracted.modelInstanceId.c_str(), closedChains, repairedChains, openChains);

            if (openChains > 0)
            {
                printf("[ExtractionPhase] WARNING: instance %s has %d chains that could not be closed "
                    "even after repair (gap exceeded %.3f)\n",
                    extracted.modelInstanceId.c_str(), openChains, kRepairThreshold);
            }

            result.push_back(std::move(extracted));
        }

        return result;
    }

} // namespace kinetica