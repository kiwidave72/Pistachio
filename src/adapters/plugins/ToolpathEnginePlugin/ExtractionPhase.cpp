#include "adapters/plugins/ToolpathEnginePlugin/ExtractionPhase.h"
#include "adapters/plugins/ToolpathEnginePlugin/PositionKey.h"

#include <unordered_map>
#include <deque>
#include <algorithm>
#include <cstdio>

namespace kinetica {

    namespace {

        std::vector<domain::v1::SegmentChain> chainSegments(
            const std::vector<domain::v1::SliceSegment>& segments)
        {
            constexpr float kTolerance = 0.05f;   // small, honest — matches real floating-point drift, not a compromise value

            std::vector<domain::v1::SegmentChain> chains;
            if (segments.empty()) return chains;

            // position key -> list of (segment index, which end: 0=start, 1=end)
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
                    for (uint64_t key : neighborCellKeys(point, kTolerance))
                    {
                        auto it = adjacency.find(key);
                        if (it == adjacency.end()) continue;

                        for (auto& pr : it->second)
                        {
                            if (used[pr.first]) continue;

                            const glm::vec3& candidatePos = (pr.second == 0)
                                ? segments[pr.first].start : segments[pr.first].end;

                            if (positionsEqual(point, candidatePos, kTolerance))
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
                    //if (nextSeg == (size_t)-1) break;   // dead end, genuinely open on this side
                    // In chainSegments(), replace the two "break; // dead end" comments with:

                    // Forward extension dead end:
                    if (nextSeg == (size_t)-1)
                    {
                        printf("[ExtractionPhase]   dead-end (forward) at (%.4f, %.4f, %.4f) — chain has %zu points so far\n",
                            current.x, current.y, current.z, points.size());
                        break;
                    }


                    glm::vec3 next = (whichEnd == 0) ? segments[nextSeg].end : segments[nextSeg].start;
                    consume(nextSeg);
                    points.push_back(next);
                    current = next;
                }

                // If it didn't close, also extend BACKWARD from the original start —
                // fixes the fragmentation bug: a segment landing mid-chain must be
                // able to grow in both directions, not just forward.
                if (!closed)
                {
                    glm::vec3 currentBack = points.front();
                    while (true)
                    {
                        auto [prevSeg, whichEnd] = popMatch(currentBack);
                        // Backward extension dead end (in the !closed block):
                        if (prevSeg == (size_t)-1)
                        {
                            printf("[ExtractionPhase]   dead-end (backward) at (%.4f, %.4f, %.4f) — chain has %zu points so far\n",
                                currentBack.x, currentBack.y, currentBack.z, points.size());
                            break;
                        }

                        glm::vec3 prev = (whichEnd == 0) ? segments[prevSeg].end : segments[prevSeg].start;
                        consume(prevSeg);
                        points.push_front(prev);
                        currentBack = prev;
                    }
                }

                domain::v1::SegmentChain chain;
                chain.points.assign(points.begin(), points.end());
                chain.isClosed = closed;
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

            int closedChains = 0, openChains = 0;

            for (auto& layer : sliced.sliceResult.layers)
            {
                domain::v1::ExtractedLayer extractedLayer;
                extractedLayer.layerIndex = layer.layerIndex;
                extractedLayer.z = layer.z;
                extractedLayer.chains = chainSegments(layer.segments);

                for (auto& c : extractedLayer.chains)
                    (c.isClosed ? closedChains : openChains)++;

                extracted.layers.push_back(std::move(extractedLayer));
            }

            printf("[ExtractionPhase] instance %s: %d closed chains, %d open chains\n",
                extracted.modelInstanceId.c_str(), closedChains, openChains);

            if (openChains > 0)
            {
                printf("[ExtractionPhase] WARNING: instance %s has %d open (non-closed) chains\n",
                    extracted.modelInstanceId.c_str(), openChains);
            }

            result.push_back(std::move(extracted));
        }

        return result;
    }

} // namespace kinetica