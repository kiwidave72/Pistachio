#pragma once

#include <functional>
#include <string>
#include <vector>
#include <thread>
#include <atomic>
#include <mutex>
#include <chrono>
#include <memory>
#include <optional>

// Include imgui.h before calling renderUI().
struct ImVec4;

// ================================================================
//  TaskProgress
//  Written by a worker lambda, read by the UI thread.
// ================================================================
struct TaskProgress
{
    std::atomic<float> fraction{ -1.f }; // 0..1  or  <0 = indeterminate
    std::atomic<bool>  done{ false };
    std::atomic<bool>  failed{ false };

    void setMessage(std::string msg)
    {
        std::lock_guard<std::mutex> lk(msgMtx_);
        message_ = std::move(msg);
    }
    std::string getMessage() const
    {
        std::lock_guard<std::mutex> lk(msgMtx_);
        return message_;
    }

private:
    mutable std::mutex msgMtx_;
    std::string        message_;
};

// ================================================================
//  TaskStep
//  One unit of work inside a TaskGroup.
//  Workers receive a shared_ptr<TaskProgress> and update it freely.
// ================================================================
struct TaskStep
{
    using Worker = std::function<void(std::shared_ptr<TaskProgress>)>;

    std::string                   label;
    Worker                        worker;
    bool                          showProgressBar = true;
    bool                          showTimeElapsed = true;
    bool                          showTimeEstimate = true;

    // --- set by TaskRunner, do not touch ---
    std::shared_ptr<TaskProgress>          progress;
    std::chrono::steady_clock::time_point  startTime{};
    std::chrono::steady_clock::time_point  finishTime{};
    std::thread                            thread_;

    // std::thread is move-only; provide explicit move so vector can grow.
    TaskStep() = default;

    TaskStep(TaskStep&& o) noexcept
        : label(std::move(o.label))
        , worker(std::move(o.worker))
        , showProgressBar(o.showProgressBar)
        , showTimeElapsed(o.showTimeElapsed)
        , showTimeEstimate(o.showTimeEstimate)
        , progress(std::move(o.progress))
        , startTime(o.startTime)
        , finishTime(o.finishTime)
        , thread_(std::move(o.thread_))
    {
    }

    TaskStep& operator=(TaskStep&& o) noexcept
    {
        if (this != &o)
        {
            label = std::move(o.label);
            worker = std::move(o.worker);
            showProgressBar = o.showProgressBar;
            showTimeElapsed = o.showTimeElapsed;
            showTimeEstimate = o.showTimeEstimate;
            progress = std::move(o.progress);
            startTime = o.startTime;
            finishTime = o.finishTime;
            thread_ = std::move(o.thread_);
        }
        return *this;
    }

    TaskStep(const TaskStep&) = delete;
    TaskStep& operator=(const TaskStep&) = delete;
};

// ================================================================
//  TaskGroup
//  A named collection of steps executed sequentially or in parallel.
//  The group itself carries aggregate progress visible in the UI.
// ================================================================
enum class GroupKind { Sequential, Parallel };

struct TaskGroup
{
    std::string             label;
    GroupKind               kind = GroupKind::Sequential;
    bool                    stopOnFailure = true;  // sequential only
    bool                    showSubSteps = true;  // expand sub-rows in UI
    bool                    showProgressBar = true;
    bool                    showTimeElapsed = true;
    bool                    showTimeEstimate = true;

    std::vector<TaskStep>   steps;

    // Fires once when this group finishes. success == !failed.
    // Dispatched on the main thread via TaskRunner::pumpCallbacks(),
    // never called directly from a worker/orchestrator thread.
    std::function<void(bool success)> onCompleted;

    // --- aggregate state (computed by TaskRunner) ---
    std::atomic<bool>       started{ false };
    std::atomic<bool>       done{ false };
    std::atomic<bool>       failed{ false };
    std::chrono::steady_clock::time_point startTime{};
    std::chrono::steady_clock::time_point finishTime{};

    // std::atomic is not copyable; explicitly provide move semantics and
    // delete the copy constructor so misuse is caught at compile time.
    TaskGroup() = default;

    TaskGroup(TaskGroup&& o) noexcept
        : label(std::move(o.label))
        , kind(o.kind)
        , stopOnFailure(o.stopOnFailure)
        , showSubSteps(o.showSubSteps)
        , showProgressBar(o.showProgressBar)
        , showTimeElapsed(o.showTimeElapsed)
        , showTimeEstimate(o.showTimeEstimate)
        , steps(std::move(o.steps))
        , onCompleted(std::move(o.onCompleted))
        , startTime(o.startTime)
        , finishTime(o.finishTime)
    {
        started.store(o.started.load());
        done.store(o.done.load());
        failed.store(o.failed.load());
    }

    TaskGroup& operator=(TaskGroup&& o) noexcept
    {
        if (this != &o)
        {
            label = std::move(o.label);
            kind = o.kind;
            stopOnFailure = o.stopOnFailure;
            showSubSteps = o.showSubSteps;
            showProgressBar = o.showProgressBar;
            showTimeElapsed = o.showTimeElapsed;
            showTimeEstimate = o.showTimeEstimate;
            steps = std::move(o.steps);
            onCompleted = std::move(o.onCompleted);
            startTime = o.startTime;
            finishTime = o.finishTime;
            started.store(o.started.load());
            done.store(o.done.load());
            failed.store(o.failed.load());
        }
        return *this;
    }

    TaskGroup(const TaskGroup&) = delete;
    TaskGroup& operator=(const TaskGroup&) = delete;

    // Derived helpers (thread-safe reads)
    bool isRunning()   const { return started.load() && !done.load() && !failed.load(); }
    bool isDone()      const { return done.load(); }
    bool isFailed()    const { return failed.load(); }

    // Aggregate fraction: average of all step fractions (skip <0)
    float aggregateFraction() const;

    // Number of steps that have finished (done or failed)
    int finishedCount() const;
};

// ================================================================
//  TaskBuilder  – fluent API for constructing a TaskGroup
//
//  Usage:
//    runner.group("Export")
//          .sequential()              // or .parallel()
//          .step("Load",  loadFn)
//          .step("Slice", sliceFn)
//          .step("Save",  saveFn)
//          .submit();
// ================================================================
class TaskRunner;

class TaskBuilder
{
public:
    TaskBuilder(TaskRunner& runner, std::string groupLabel);

    // Execution mode
    TaskBuilder& sequential();
    TaskBuilder& parallel();

    // Group-level display options
    TaskBuilder& stopOnFailure(bool v = true);
    TaskBuilder& showSubSteps(bool v = true);
    TaskBuilder& showProgressBar(bool v = true);
    TaskBuilder& showTimeElapsed(bool v = true);
    TaskBuilder& showTimeEstimate(bool v = true);

    // Add a step
    TaskBuilder& step(
        const std::string& label,
        TaskStep::Worker   worker,
        bool showBar = true,
        bool showElapsed = true,
        bool showEta = true
    );

    // Called once this group finishes (success == !failed)
    TaskBuilder& completed(std::function<void(bool success)> cb);

    // Finalise and hand off to TaskRunner
    std::shared_ptr<TaskGroup> submit();

private:
    TaskRunner& runner_;
    TaskGroup   group_;
};

// ================================================================
//  TaskRunner
// ================================================================
class TaskRunner
{
public:
    TaskRunner() = default;
    ~TaskRunner();

    TaskRunner(const TaskRunner&) = delete;
    TaskRunner& operator=(const TaskRunner&) = delete;
    TaskRunner(TaskRunner&&) = delete;
    TaskRunner& operator=(TaskRunner&&) = delete;

    // ---- Fluent group API (preferred) -------------------------
    //   runner.group("My Pipeline").sequential()
    //         .step("A", fnA).step("B", fnB).submit();
    TaskBuilder group(const std::string& label);

    // ---- Single-task shorthand (backwards compatible) ---------
    std::shared_ptr<TaskGroup> submit(
        TaskStep::Worker       worker,
        const std::string& label = "Task",
        bool                   showProgressBar = true,
        bool                   showTimeElapsed = true,
        bool                   showTimeEstimate = true
    );

    // ---- Submit a fully-built group ---------------------------
    std::shared_ptr<TaskGroup> submitGroup(TaskGroup g);

    // ---- Frame render -----------------------------------------
    void renderUI(
        const char* windowTitle = "Background Tasks",
        bool* p_open = nullptr,
        float       windowWidth = 460.f
    );

    // ---- Lifecycle --------------------------------------------
    void pruneFinished(float retainSeconds = 5.f);
    bool anyRunning() const;
    void waitAll();

    // Dispatches queued per-group onCompleted callbacks, then fires
    // onAllCompleted if the runner just transitioned from running to
    // idle. Call this once per frame on the main thread (renderUI()
    // already does this internally) -- or call it manually if you're
    // not rendering the UI every frame.
    void pumpCallbacks();

    // Fires once when every submitted group has finished and no new
    // group has started -- i.e. the whole queue went idle. Assign
    // directly: runner.onAllCompleted = [](){ ... };
    std::function<void()> onAllCompleted;

    // ---- Style ------------------------------------------------
    struct Style
    {
        ImVec4* colorRunning = nullptr;
        ImVec4* colorPending = nullptr;
        ImVec4* colorDone = nullptr;
        ImVec4* colorFailed = nullptr;
        float   barHeight = 6.f;
        float   subIndent = 16.f;
        float   subBarHeight = 4.f;
        bool    showDividers = true;
    } style;

private:
    // Internal entry tracks prune timer
    struct Entry
    {
        std::shared_ptr<TaskGroup>             group;
        std::chrono::steady_clock::time_point  pruneTime{};
    };

    mutable std::mutex     mtx_;
    std::vector<Entry>     entries_;

    // Pending onCompleted callbacks, queued from worker threads and
    // drained on the main thread inside pumpCallbacks().
    std::mutex                          cbMtx_;
    std::vector<std::function<void()>>  pendingCallbacks_;
    std::atomic<bool>                   wasAnyRunning_{ false };
    void queueCallback(std::function<void()> cb);

    void launchGroup(std::shared_ptr<TaskGroup> g);
    void launchSequential(std::shared_ptr<TaskGroup> g);
    void launchParallel(std::shared_ptr<TaskGroup> g);

    void renderGroup(const TaskGroup& g, size_t idx,
        const ImVec4& colRunning,
        const ImVec4& colPending,
        const ImVec4& colDone,
        const ImVec4& colFailed);

    void renderStep(const TaskStep& s, size_t stepIdx,
        bool isLast,
        const ImVec4& colRunning,
        const ImVec4& colPending,
        const ImVec4& colDone,
        const ImVec4& colFailed);

    void renderBar(float fraction, bool running, bool done, bool failed,
        float barH,
        const ImVec4& colRunning,
        const ImVec4& colDone,
        const ImVec4& colFailed);

    static std::string formatDuration(float seconds);
    static std::string badgeText(bool running, bool done, bool failed, bool pending);
    static ImVec4      badgeColor(bool running, bool done, bool failed, bool pending,
        const ImVec4& cr, const ImVec4& cd,
        const ImVec4& cf, const ImVec4& cp);
};