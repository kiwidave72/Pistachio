#pragma once

#include <vector>
#include <thread>
#include <queue>
#include <functional>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <algorithm>

namespace core {

    // -----------------------------------------------------------------------
    // ThreadPool
    //
    // Persistent worker threads, created once and reused for the lifetime of
    // the process. Replaces the previous pattern of spawning + joining a
    // fresh std::thread per parallelFor() call, which paid real OS thread
    // creation/teardown cost on every call -- and in ToolpathEnginePlugin,
    // that meant once per model instance per phase (Wall Generation, Infill),
    // repeatedly, for the whole plate.
    //
    // Usage is via parallelFor(), which mirrors the old core::parallelFor
    // signature: splits [0, count) into chunks and blocks the calling thread
    // until all chunks complete. Safe to call from the UI/TaskRunner thread;
    // NOT safe to nest (calling parallelFor from inside a chunk running on a
    // pool worker will detect the nesting and run inline/sequentially on that
    // worker instead of touching the pool, to avoid deadlocking a
    // fixed-size pool on itself).
    // -----------------------------------------------------------------------
    class ThreadPool
    {
    public:
        explicit ThreadPool(size_t threadCount = 0)
        {
            size_t count = threadCount > 0
                ? threadCount
                : (std::max)(1u, std::thread::hardware_concurrency());

            m_workers.reserve(count);
            for (size_t i = 0; i < count; ++i)
                m_workers.emplace_back([this]() { workerLoop(); });
        }

        ~ThreadPool()
        {
            {
                std::lock_guard<std::mutex> lock(m_queueMutex);
                m_stop = true;
            }
            m_cv.notify_all();
            for (auto& t : m_workers)
                if (t.joinable()) t.join();
        }

        ThreadPool(const ThreadPool&) = delete;
        ThreadPool& operator=(const ThreadPool&) = delete;

        // Process-lifetime shared pool. Sized to hardware_concurrency() once,
        // on first use.
        static ThreadPool& shared()
        {
            static ThreadPool instance;
            return instance;
        }

        size_t threadCount() const { return m_workers.size(); }

        // Splits [0, count) into up to threadCount() chunks (each at least
        // minChunkSize elements, so tiny workloads don't pay dispatch
        // overhead for no benefit), submits them to the pool, and blocks
        // the calling thread until all chunks finish.
        void parallelFor(size_t count, const std::function<void(size_t start, size_t end)>& fn, size_t minChunkSize = 4)
        {
            if (count == 0) return;

            // Reentrancy guard: if we're already executing on a pool worker
            // (e.g. a chunk lambda itself called parallelFor), the fixed-size
            // pool can't service a nested wait without risking deadlock if
            // all workers are simultaneously blocked waiting on their own
            // nested calls. Fall back to running the whole range inline on
            // the calling thread in that case -- correct, just not parallel
            // for the nested portion.
            if (t_inWorker)
            {
                fn(0, count);
                return;
            }

            size_t poolSize = (std::max)((size_t)1, m_workers.size());
            size_t chunkCount = (std::min)(poolSize, (std::max)((size_t)1, count / minChunkSize));
            size_t chunkSize = (count + chunkCount - 1) / chunkCount;

            std::atomic<size_t> remaining{ 0 };
            std::mutex doneMutex;
            std::condition_variable doneCv;

            // Run all but the last chunk on the pool; run the last chunk on
            // the calling thread itself. This uses the calling thread as an
            // extra worker (avoids wasting it while it waits) and guarantees
            // forward progress even if the pool's queue is temporarily busy
            // with other work.
            size_t submitted = 0;
            size_t lastStart = 0, lastEnd = 0;
            bool haveLast = false;

            for (size_t c = 0; c < chunkCount; ++c)
            {
                size_t start = c * chunkSize;
                size_t end = (std::min)(start + chunkSize, count);
                if (start >= end) break;

                if (c == chunkCount - 1)
                {
                    lastStart = start;
                    lastEnd = end;
                    haveLast = true;
                    continue;
                }

                ++submitted;
                {
                    std::lock_guard<std::mutex> lock(m_queueMutex);
                    m_tasks.push([&fn, start, end, &remaining, &doneMutex, &doneCv]()
                        {
                            fn(start, end);
                            if (remaining.fetch_sub(1, std::memory_order_acq_rel) == 1)
                            {
                                std::lock_guard<std::mutex> lk(doneMutex);
                                doneCv.notify_all();
                            }
                        });
                }
            }

            remaining.store(submitted, std::memory_order_release);
            if (submitted > 0)
                m_cv.notify_all();

            if (haveLast)
                fn(lastStart, lastEnd);

            if (submitted > 0)
            {
                std::unique_lock<std::mutex> lock(doneMutex);
                doneCv.wait(lock, [&]() { return remaining.load(std::memory_order_acquire) == 0; });
            }
        }

    private:
        void workerLoop()
        {
            t_inWorker = true;
            while (true)
            {
                std::function<void()> task;
                {
                    std::unique_lock<std::mutex> lock(m_queueMutex);
                    m_cv.wait(lock, [this]() { return m_stop || !m_tasks.empty(); });
                    if (m_stop && m_tasks.empty()) return;
                    task = std::move(m_tasks.front());
                    m_tasks.pop();
                }
                task();
            }
        }

        std::vector<std::thread> m_workers;
        std::queue<std::function<void()>> m_tasks;
        std::mutex m_queueMutex;
        std::condition_variable m_cv;
        bool m_stop = false;

        static inline thread_local bool t_inWorker = false;
    };

} // namespace core