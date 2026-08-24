#pragma once

#include "domain/PhaseProcessingTiming.h"
#include "core/ThreadPool.h"

#include <functional>

namespace core {

    // Splits [0, count) into chunks and runs them across the shared,
    // persistent ThreadPool, blocking until all complete. Returns the TRUE
    // wall-clock time for the whole parallel section -- not a sum of
    // per-chunk durations, which would measure total work done, not elapsed
    // time (the actual number worth comparing against a sequential baseline).
    //
    // Signature is unchanged from the previous std::thread-per-call
    // implementation, so existing call sites don't need to change -- this
    // now dispatches onto ThreadPool::shared() instead of spawning and
    // joining fresh OS threads on every call. That matters here because this
    // is called once per model instance per phase in ToolpathEnginePlugin;
    // with N instances that's N thread spin-up/tear-down cycles per phase
    // under the old implementation, now zero (pool threads are created once,
    // at first use, for the process lifetime).
    inline domain::v1::PhaseProcessingTiming parallelFor(
        size_t count, const std::function<void(size_t start, size_t end)>& fn, size_t minChunkSize = 4)
    {
        domain::v1::PhaseProcessingTimer timer;
        ThreadPool::shared().parallelFor(count, fn, minChunkSize);
        return timer.elapsed();
    }

} // namespace core