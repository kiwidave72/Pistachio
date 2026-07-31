#include "TaskRunner.h"
#include "imgui.h"

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
//  Rendering helpers
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

ImVec4 TaskRunner::badgeColor(bool running, bool done, bool failed, bool pending,
    const ImVec4& cr, const ImVec4& cd,
    const ImVec4& cf, const ImVec4& cp)
{
    if (failed)  return cf;
    if (done)    return cd;
    if (running) return cr;
    return cp;
}

void TaskRunner::renderBar(float fraction, bool running, bool done, bool failed,
    float barH,
    const ImVec4& colRunning,
    const ImVec4& colDone,
    const ImVec4& colFailed)
{
    ImVec2 barSize(ImGui::GetContentRegionAvail().x, barH);
    ImVec2 pos = ImGui::GetCursorScreenPos();
    auto* dl = ImGui::GetWindowDrawList();
    float  rnd = barH * 0.5f;

    ImU32 bg = ImGui::ColorConvertFloat4ToU32(ImVec4(0.18f, 0.18f, 0.18f, 1.f));
    ImU32 fg = ImGui::ColorConvertFloat4ToU32(
        done ? colDone : (failed ? colFailed : colRunning));

    dl->AddRectFilled(pos, ImVec2(pos.x + barSize.x, pos.y + barH), bg, rnd);

    if (done || failed)
    {
        dl->AddRectFilled(pos, ImVec2(pos.x + barSize.x, pos.y + barH), fg, rnd);
    }
    else if (fraction >= 0.f)
    {
        float fillW = barSize.x * std::clamp(fraction, 0.f, 1.f);
        if (fillW > rnd * 2.f)
            dl->AddRectFilled(pos, ImVec2(pos.x + fillW, pos.y + barH), fg, rnd);
    }
    else if (running)
    {
        float t = fmodf(static_cast<float>(ImGui::GetTime()) * 1.4f, 1.f);
        float pw = barSize.x * 0.30f;
        float px = pos.x + (barSize.x + pw) * t - pw;
        float x0 = std::max(px, pos.x);
        float x1 = std::min(px + pw, pos.x + barSize.x);
        if (x1 > x0)
            dl->AddRectFilled(ImVec2(x0, pos.y), ImVec2(x1, pos.y + barH), fg, rnd);
    }

    ImGui::Dummy(barSize);
}

// ---- Render one step row (indented) ----------------------------
void TaskRunner::renderStep(const TaskStep& s, size_t stepIdx,
    bool /*isLast*/,
    const ImVec4& colRunning,
    const ImVec4& colPending,
    const ImVec4& colDone,
    const ImVec4& colFailed)
{
    if (!s.progress) return;

    bool started = s.startTime.time_since_epoch().count() != 0;
    bool done = s.progress->done.load();
    bool failed = s.progress->failed.load();
    bool running = started && !done && !failed;
    bool pending = !started;
    float frac = s.progress->fraction.load();

    ImGui::PushID(static_cast<int>(stepIdx + 1000));
    ImGui::Indent(style.subIndent);

    // Step label + badge on right
    const std::string badge = badgeText(running, done, failed, pending);
    ImVec4 bCol = badgeColor(running, done, failed, pending,
        colRunning, colDone, colFailed, colPending);

    float badgeW = ImGui::CalcTextSize(badge.c_str()).x + 6.f;
    ImGui::TextUnformatted(s.label.c_str());
    ImGui::SameLine();
    float rightX = ImGui::GetWindowWidth()
        - badgeW
        - style.subIndent
        - ImGui::GetStyle().WindowPadding.x;
    ImGui::SetCursorPosX(rightX);
    ImGui::TextColored(bCol, "%s", badge.c_str());

    // Sub-bar
    if (s.showProgressBar && started)
        renderBar(frac, running, done, failed,
            style.subBarHeight, colRunning, colDone, colFailed);

    // Time
    if (started && (s.showTimeElapsed || s.showTimeEstimate))
    {
        auto now = std::chrono::steady_clock::now();
        auto endPt = (done || failed) ? s.finishTime : now;
        float elapsed = std::chrono::duration<float>(endPt - s.startTime).count();

        std::string timeStr;
        if (s.showTimeElapsed)
            timeStr = "Elapsed: " + formatDuration(elapsed);

        if (s.showTimeEstimate && running && frac > 0.001f)
        {
            float eta = elapsed / frac * (1.f - frac);
            if (!timeStr.empty()) timeStr += "   ";
            timeStr += "ETA: " + formatDuration(eta);
        }

        if (!timeStr.empty())
        {
            ImGui::PushStyleColor(ImGuiCol_Text,
                ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
            ImGui::TextUnformatted(timeStr.c_str());
            ImGui::PopStyleColor();
        }
    }

    // Message
    std::string msg = s.progress->getMessage();
    if (!msg.empty())
    {
        ImGui::PushStyleColor(ImGuiCol_Text,
            ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
        ImGui::TextUnformatted(msg.c_str());
        ImGui::PopStyleColor();
    }

    ImGui::Unindent(style.subIndent);
    ImGui::PopID();
}

// ---- Render one group row --------------------------------------
void TaskRunner::renderGroup(const TaskGroup& g, size_t idx,
    const ImVec4& colRunning,
    const ImVec4& colPending,
    const ImVec4& colDone,
    const ImVec4& colFailed)
{
    bool done = g.isDone();
    bool failed = g.isFailed();
    bool running = g.isRunning();
    bool pending = !g.started.load();
    float frac = g.aggregateFraction();

    auto now = std::chrono::steady_clock::now();
    auto endPt = (done || failed) ? g.finishTime : now;
    float elapsed = g.startTime.time_since_epoch().count() == 0
        ? 0.f
        : std::chrono::duration<float>(endPt - g.startTime).count();

    ImGui::PushID(static_cast<int>(idx));

    // ---- Group header ----
    const std::string badge = badgeText(running, done, failed, pending);
    ImVec4 bCol = badgeColor(running, done, failed, pending,
        colRunning, colDone, colFailed, colPending);

    // Kind tag
    const char* kindTag = (g.kind == GroupKind::Sequential) ? "[SEQ]" : "[PAR]";
    bool multiStep = g.steps.size() > 1;

    // Label row
    ImGui::TextUnformatted(g.label.c_str());
    if (multiStep)
    {
        ImGui::SameLine();
        ImGui::PushStyleColor(ImGuiCol_Text,
            ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
        ImGui::TextUnformatted(kindTag);
        ImGui::PopStyleColor();
    }

    float badgeW = ImGui::CalcTextSize(badge.c_str()).x + 10.f;
    float rightX = ImGui::GetWindowWidth()
        - badgeW
        - ImGui::GetStyle().WindowPadding.x;
    ImGui::SameLine();
    ImGui::SetCursorPosX(rightX);
    ImGui::TextColored(bCol, "%s", badge.c_str());

    // Step counter for multi-step groups
    if (multiStep)
    {
        int finished = g.finishedCount();
        ImGui::PushStyleColor(ImGuiCol_Text,
            ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
        ImGui::Text("  %d / %d steps", finished, (int)g.steps.size());
        ImGui::PopStyleColor();
    }

    // ---- Aggregate progress bar ----
    if (g.showProgressBar)
        renderBar(frac, running, done, failed,
            style.barHeight, colRunning, colDone, colFailed);

    // ---- Time row ----
    if (running || done || failed)
    {
        if (g.showTimeElapsed || g.showTimeEstimate)
        {
            std::string timeStr;
            if (g.showTimeElapsed)
                timeStr = "Elapsed: " + formatDuration(elapsed);

            if (g.showTimeEstimate && running && frac > 0.001f)
            {
                float eta = elapsed / frac * (1.f - frac);
                if (!timeStr.empty()) timeStr += "   ";
                timeStr += "ETA: " + formatDuration(eta);
            }
            else if (g.showTimeEstimate && (done || failed))
            {
                if (!timeStr.empty()) timeStr += "   ";
                timeStr += (failed ? "Failed after " : "Took ")
                    + formatDuration(elapsed);
            }

            if (!timeStr.empty())
            {
                ImGui::PushStyleColor(ImGuiCol_Text,
                    ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
                ImGui::TextUnformatted(timeStr.c_str());
                ImGui::PopStyleColor();
            }
        }
    }

    // ---- Sub-steps ----
    if (g.showSubSteps && multiStep)
    {
        for (size_t si = 0; si < g.steps.size(); ++si)
            renderStep(g.steps[si], si, si + 1 == g.steps.size(),
                colRunning, colPending, colDone, colFailed);
    }

    ImGui::PopID();
}

// ---- Main render entry -----------------------------------------
void TaskRunner::renderUI(const char* windowTitle, bool* p_open, float windowWidth)
{
    pumpCallbacks();
    pruneFinished(10.0f);

    std::vector<std::shared_ptr<TaskGroup>> snapshot;
    {
        std::lock_guard<std::mutex> lk(mtx_);
        if (entries_.empty()) return;
        for (auto& e : entries_)
            snapshot.push_back(e.group);
    }

    ImGui::SetNextWindowSizeConstraints(
        ImVec2(windowWidth, 0.f),
        ImVec2(windowWidth, FLT_MAX));

    ImGuiWindowFlags flags =
        ImGuiWindowFlags_AlwaysAutoResize |
        ImGuiWindowFlags_NoCollapse;

    if (!ImGui::Begin(windowTitle, p_open, flags))
    {
        ImGui::End(); return;
    }

    // Resolve colours
    ImVec4 colRunning = ImGui::GetStyleColorVec4(ImGuiCol_PlotHistogram);
    ImVec4 colPending = ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled);
    ImVec4 colDone = ImVec4(0.25f, 0.78f, 0.35f, 1.f);
    ImVec4 colFailed = ImVec4(0.85f, 0.20f, 0.20f, 1.f);
    if (style.colorRunning) colRunning = *style.colorRunning;
    if (style.colorPending) colPending = *style.colorPending;
    if (style.colorDone)    colDone = *style.colorDone;
    if (style.colorFailed)  colFailed = *style.colorFailed;

    for (size_t i = 0; i < snapshot.size(); ++i)
    {
        renderGroup(*snapshot[i], i,
            colRunning, colPending, colDone, colFailed);

        if (style.showDividers && i + 1 < snapshot.size())
        {
            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();
        }
        else
        {
            ImGui::Spacing();
        }
    }

    ImGui::End();
}