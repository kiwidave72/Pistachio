#include "TaskRunner.h"

#include <sstream>
#include <algorithm>
#include <cmath>
#include <cassert>

// ================================================================
//  TaskGroup helpers
// ================================================================
float TaskGroup::aggregateFraction() const
{
    if (steps.empty()) return 1.f;

    float sum = 0.f;
    int   count = 0;
    for (auto& s : steps)
    {
        if (!s.progress) continue;
        float f = s.progress->fraction.load();
        if (f >= 0.f) { sum += f; ++count; }
    }
    if (count == 0) return -1.f; // all indeterminate
    // Include not-yet-started steps as 0 contribution
    return sum / static_cast<float>(steps.size());
}

int TaskGroup::finishedCount() const
{
    int n = 0;
    for (auto& s : steps)
        if (s.progress && (s.progress->done.load() || s.progress->failed.load()))
            ++n;
    return n;
}

// ================================================================
//  TaskBuilder
// ================================================================
TaskBuilder::TaskBuilder(TaskRunner& runner, std::string groupLabel)
    : runner_(runner)
{
    group_.label = std::move(groupLabel);
}

TaskBuilder& TaskBuilder::sequential() { group_.kind = GroupKind::Sequential; return *this; }
TaskBuilder& TaskBuilder::parallel() { group_.kind = GroupKind::Parallel;   return *this; }

TaskBuilder& TaskBuilder::stopOnFailure(bool v) { group_.stopOnFailure = v; return *this; }
TaskBuilder& TaskBuilder::showSubSteps(bool v) { group_.showSubSteps = v; return *this; }
TaskBuilder& TaskBuilder::showProgressBar(bool v) { group_.showProgressBar = v; return *this; }
TaskBuilder& TaskBuilder::showTimeElapsed(bool v) { group_.showTimeElapsed = v; return *this; }
TaskBuilder& TaskBuilder::showTimeEstimate(bool v) { group_.showTimeEstimate = v; return *this; }

TaskBuilder& TaskBuilder::step(
    const std::string& label,
    TaskStep::Worker   worker,
    bool showBar,
    bool showElapsed,
    bool showEta)
{
    TaskStep s;
    s.label = label;
    s.worker = std::move(worker);
    s.showProgressBar = showBar;
    s.showTimeElapsed = showElapsed;
    s.showTimeEstimate = showEta;
    group_.steps.push_back(std::move(s));
    return *this;
}

std::shared_ptr<TaskGroup> TaskBuilder::submit()
{
    return runner_.submitGroup(std::move(group_));
}

TaskBuilder& TaskBuilder::completed(std::function<void(bool)> cb)
{
    group_.onCompleted = std::move(cb);
    return *this;
}

// ================================================================
//  TaskRunner
// ================================================================
TaskRunner::~TaskRunner()
{
    waitAll();
}

// ---- public API ------------------------------------------------

TaskBuilder TaskRunner::group(const std::string& label)
{
    return TaskBuilder(*this, label);
}

std::shared_ptr<TaskGroup> TaskRunner::submit(
    TaskStep::Worker       worker,
    const std::string& label,
    bool                   showProgressBar,
    bool                   showTimeElapsed,
    bool                   showTimeEstimate)
{
    TaskGroup g;
    g.label = label;
    g.kind = GroupKind::Sequential;
    g.showProgressBar = showProgressBar;
    g.showTimeElapsed = showTimeElapsed;
    g.showTimeEstimate = showTimeEstimate;
    g.showSubSteps = false; // single task – no sub-row

    TaskStep s;
    s.label = label;
    s.worker = std::move(worker);
    s.showProgressBar = showProgressBar;
    s.showTimeElapsed = showTimeElapsed;
    s.showTimeEstimate = showTimeEstimate;
    g.steps.push_back(std::move(s));

    return submitGroup(std::move(g));
}

std::shared_ptr<TaskGroup> TaskRunner::submitGroup(TaskGroup g)
{
    auto ptr = std::make_shared<TaskGroup>(std::move(g));

    // Allocate a TaskProgress for every step up-front
    for (auto& s : ptr->steps)
        s.progress = std::make_shared<TaskProgress>();

    {
        std::lock_guard<std::mutex> lk(mtx_);
        entries_.push_back({ ptr, {} });
    }

    launchGroup(ptr);
    return ptr;
}

bool TaskRunner::anyRunning() const
{
    std::lock_guard<std::mutex> lk(mtx_);
    for (auto& e : entries_)
        if (e.group->isRunning())
            return true;
    return false;
}

void TaskRunner::waitAll()
{
    // Snapshot to avoid holding lock while joining
    std::vector<std::shared_ptr<TaskGroup>> groups;
    {
        std::lock_guard<std::mutex> lk(mtx_);
        for (auto& e : entries_)
            groups.push_back(e.group);
    }
    for (auto& g : groups)
        for (auto& s : g->steps)
            if (s.thread_.joinable())
                s.thread_.join();
}

void TaskRunner::pruneFinished(float retainSeconds)
{
    auto now = std::chrono::steady_clock::now();
    std::lock_guard<std::mutex> lk(mtx_);

    entries_.erase(
        std::remove_if(entries_.begin(), entries_.end(),
            [&](Entry& e) -> bool
            {
                auto& g = e.group;
                if (!g->isDone() && !g->isFailed()) return false;

                if (e.pruneTime.time_since_epoch().count() == 0)
                {
                    // First time we see it finished – join all threads
                    for (auto& s : g->steps)
                        if (s.thread_.joinable())
                            s.thread_.join();
                    e.pruneTime = now;
                }

                float age = std::chrono::duration<float>(now - e.pruneTime).count();
                return age > retainSeconds;
            }),
        entries_.end());
}

void TaskRunner::queueCallback(std::function<void()> cb)
{
    std::lock_guard<std::mutex> lk(cbMtx_);
    pendingCallbacks_.push_back(std::move(cb));
}

void TaskRunner::pumpCallbacks()
{
    // Drain and run any per-group onCompleted callbacks first.
    std::vector<std::function<void()>> toRun;
    {
        std::lock_guard<std::mutex> lk(cbMtx_);
        toRun.swap(pendingCallbacks_);
    }
    for (auto& cb : toRun)
        cb();

    // Then check for the running -> idle edge for onAllCompleted.
    bool running = anyRunning();
    bool wasRunning = wasAnyRunning_.exchange(running);
    if (wasRunning && !running && onAllCompleted)
        onAllCompleted();
}

std::vector<std::shared_ptr<TaskGroup>> TaskRunner::snapshotGroups() const
{
    std::lock_guard<std::mutex> lk(mtx_);
    std::vector<std::shared_ptr<TaskGroup>> out;
    out.reserve(entries_.size());
    for (auto& e : entries_)
        out.push_back(e.group);
    return out;
}

// ---- Internal launch -------------------------------------------

void TaskRunner::launchGroup(std::shared_ptr<TaskGroup> g)
{
    g->started = true;
    g->startTime = std::chrono::steady_clock::now();

    if (g->kind == GroupKind::Sequential)
        launchSequential(g);
    else
        launchParallel(g);
}

void TaskRunner::launchSequential(std::shared_ptr<TaskGroup> g)
{
    // Spin up ONE orchestrator thread that runs steps one after another.
    // The orchestrator thread itself is stored in steps[0].thread_ as a
    // sentinel; individual step threads run inline inside it.
    // (Simpler: just run steps serially on the orchestrator thread.)

    std::thread orchestrator([this, g]()
        {
            for (auto& s : g->steps)
            {
                // Check if a prior step failed and we should abort
                if (g->stopOnFailure && g->failed.load())
                    break;

                s.startTime = std::chrono::steady_clock::now();

                try
                {
                    s.worker(s.progress);
                }
                catch (...)
                {
                    s.progress->failed = true;
                }

                s.progress->done = true;
                s.finishTime = std::chrono::steady_clock::now();

                if (s.progress->failed.load())
                {
                    g->failed = true;
                    if (g->stopOnFailure)
                        break;
                }
            }

            g->finishTime = std::chrono::steady_clock::now();
            g->done = true;

            if (g->onCompleted)
            {
                bool success = !g->failed.load();
                queueCallback([g, success]() { g->onCompleted(success); });
            }
        });

    // Park the orchestrator thread in the first step slot so waitAll/prune can join it
    if (!g->steps.empty())
        g->steps[0].thread_ = std::move(orchestrator);
    else
    {
        // Edge case: empty group
        orchestrator.detach();
        g->done = true;
    }
}

void TaskRunner::launchParallel(std::shared_ptr<TaskGroup> g)
{
    // Launch each step on its own thread.
    // One additional watcher thread marks the group done when all finish.
    for (auto& s : g->steps)
    {
        auto prog = s.progress;
        auto worker = s.worker;
        s.startTime = std::chrono::steady_clock::now();

        s.thread_ = std::thread([prog, worker, &s, g]()
            {
                try
                {
                    worker(prog);
                }
                catch (...)
                {
                    prog->failed = true;
                }
                prog->done = true;
                s.finishTime = std::chrono::steady_clock::now();

                if (prog->failed.load())
                    g->failed = true;
            });
    }

    // Watcher thread
    std::thread watcher([this, g]()
        {
            for (auto& s : g->steps)
                if (s.thread_.joinable())
                    s.thread_.join();

            g->finishTime = std::chrono::steady_clock::now();
            g->done = true;

            if (g->onCompleted)
            {
                bool success = !g->failed.load();
                queueCallback([g, success]() { g->onCompleted(success); });
            }
        });
    watcher.detach(); // watcher manages its own lifetime
}

// ================================================================
//  Formatting helpers (no ImGui dependency — shared by any reporter)
// ================================================================
std::string TaskRunner::formatDuration(float seconds)
{
    if (seconds < 0.f) return "--";
    if (seconds < 1.f) return "< 1s";
    int secs = static_cast<int>(seconds);
    int mins = secs / 60;  int hrs = mins / 60;
    secs %= 60; mins %= 60;
    std::ostringstream ss;
    if (hrs > 0) ss << hrs << "h ";
    if (mins > 0) ss << mins << "m ";
    ss << secs << "s";
    return ss.str();
}

std::string TaskRunner::badgeText(bool running, bool done, bool failed, bool pending)
{
    if (failed)  return "FAILED";
    if (done)    return "DONE";
    if (running) return "RUNNING";
    return "PENDING";
}