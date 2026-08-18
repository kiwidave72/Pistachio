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

            // Scans all 27 neighbor cells and returns the CLOSEST unused
            // candidate within tolerance, not the first one encountered.
            // First-found is a real bug in dense/fine mesh regions: when
            // real, topologically distinct vertices sit closer together
            // than kDefaultPositionTolerance (a rounded corner or small
            // fillet routinely produces this), a query point can match a
            // nearby-but-wrong candidate before ever reaching its true
            // topological neighbor, silently routing the chain through
            // the wrong vertex and orphaning the correct one instead.
            // Nearest-match doesn't make ambiguity impossible in a truly
            // pathological case, but it resolves the common one: prefer
            // whichever candidate is actually closest, not whichever the
            // hash map happened to enumerate first.
            auto popMatch = [&](const glm::vec3& point) -> std::pair<size_t, int>
                {
                    std::pair<size_t, int> best = { (size_t)-1, -1 };
                    float bestDist = kDefaultPositionTolerance;

                    for (uint64_t key : neighborCellKeys(point))
                    {
                        auto it = adjacency.find(key);
                        if (it == adjacency.end()) continue;
                        for (auto& pr : it->second)
                        {
                            if (used[pr.first]) continue;
                            const glm::vec3& candidatePos = (pr.second == 0)
                                ? segments[pr.first].start : segments[pr.first].end;
                            float dist = glm::length(point - candidatePos);
                            if (dist <= bestDist)
                            {
                                bestDist = dist;
                                best = pr;
                            }
                        }
                    }
                    return best;
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
                    // points.size() > 2 guard: a chain can't legitimately
                    // close on its very first segment alone (a polygon
                    // needs at least 3 distinct vertices). Without this
                    // guard, a single short segment whose own start and
                    // end happen to fall within tolerance of each other --
                    // routine in a densely-tessellated mesh region, not a
                    // sign of degenerate geometry -- self-closes here on
                    // the first check, before popMatch is ever tried. It
                    // then gets silently un-closed by the size<3 check
                    // further down with no diagnostic explaining why, and
                    // worse, it's already been removed from the adjacency
                    // map by consume() above, so its real neighboring
                    // segment can never find it again -- turning one
                    // short segment into a permanent gap in what should
                    // have been a continuous chain.
                    if (points.size() > 2 && positionsEqual(current, points.front()))
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
                // start too � a segment landing mid-chain must be able to
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
                // Requires >=3 points because a 2-point "loop" can never be
                // a valid polygon (needs at least 3 distinct vertices to
                // enclose an area) -- that's handled as a separate,
                // distinct case below, not as a "repair".
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

                // A chain that never grew past its own starting segment
                // (exactly 2 points -- both forward and backward dead-
                // ended immediately) AND whose own two endpoints are
                // already within normal position-matching tolerance of
                // each other isn't a meaningful boundary fragment worth
                // investigating as a defect. It can't be "repaired" into
                // a closed loop (2 points can't enclose an area), but it
                // also isn't the same kind of problem as a genuine
                // unresolved dead-end -- a real gap (see the >=3-point
                // case above, and the ~0.18mm gaps found investigating
                // S6_bracket_set) is typically much larger than this
                // tolerance. This is far more likely a near-zero-length
                // sliver triangle, the kind fine circular tessellation
                // routinely produces, showing up as slicing noise. Flag
                // it explicitly as discarded so it reads differently in
                // diagnostics than a real, unresolved dead-end -- one
                // needs investigation, this doesn't.
                if (!closed && points.size() == 2)
                {
                    float selfGap = glm::length(points.front() - points.back());
                    if (selfGap <= kDefaultPositionTolerance)
                    {
                        domain::v1::DiagnosticMessage diag;
                        diag.severity = domain::v1::DiagnosticSeverity::Info;
                        diag.phase = "P4 ExtractionPhase";
                        diag.message = "chain discarded as degenerate sliver (2 points, self-gap="
                            + std::to_string(selfGap) + ", within position tolerance) -- "
                            "likely fine-tessellation noise, not a boundary defect";
                        diag.hasLocation = true;
                        diag.location = (points.front() + points.back()) * 0.5f;
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