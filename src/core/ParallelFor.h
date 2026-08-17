#pragma once

#include "domain/PhaseProcessingTiming.h"

#include <vector>
#include <thread>
#include <functional>
#include <algorithm>

namespace core {

    // Splits [0, count) into chunks across a pooled set of worker
    // threads, blocks until all complete. Returns the TRUE wall-clock
    // time for the whole parallel section — not a sum of per-chunk
    // durations, which would measure total work done, not elapsed time
    // (the actual number worth comparing against a sequential baseline).
    inline domain::v1::PhaseProcessingTiming parallelFor(
        size_t count, const std::function<void(size_t start, size_t end)>& fn, size_t minChunkSize = 4)
    {
        domain::v1::PhaseProcessingTimer timer;

        if (count == 0) return timer.elapsed();

        size_t hwThreads = (std::max)(1u, std::thread::hardware_concurrency());
        size_t chunkCount = (std::min)(hwThreads, (std::max)((size_t)1, count / minChunkSize));
        size_t chunkSize = (count + chunkCount - 1) / chunkCount;

        std::vector<std::thread> threads;
        threads.reserve(chunkCount);

        for (size_t c = 0; c < chunkCount; ++c)
        {
            size_t start = c * chunkSize;
            size_t end = (std::min)(start + chunkSize, count);
            if (start >= end) break;

            threads.emplace_back([&fn, start, end]() { fn(start, end); });
        }

        for (auto& t : threads) t.join();

        return timer.elapsed();
    }

} // namespace core