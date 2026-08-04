#include "adapters/ui/TaskProgressReporterImGui.h"

#include <algorithm>
#include <cmath>

namespace adapters::ui {

    // ---- Render one progress bar ------------------------------------
    void TaskProgressReporterImGui::renderBar(float fraction, bool running, bool done, bool failed,
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

    ImVec4 TaskProgressReporterImGui::badgeColor(bool running, bool done, bool failed, bool pending,
        const ImVec4& cr, const ImVec4& cd,
        const ImVec4& cf, const ImVec4& cp)
    {
        if (failed)  return cf;
        if (done)    return cd;
        if (running) return cr;
        return cp;
    }

    // ---- Render one step row (indented) ----------------------------
    void TaskProgressReporterImGui::renderStep(const TaskStep& s, size_t stepIdx,
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
        const std::string badge = TaskRunner::badgeText(running, done, failed, pending);
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
                timeStr = "Elapsed: " + TaskRunner::formatDuration(elapsed);

            if (s.showTimeEstimate && running && frac > 0.001f)
            {
                float eta = elapsed / frac * (1.f - frac);
                if (!timeStr.empty()) timeStr += "   ";
                timeStr += "ETA: " + TaskRunner::formatDuration(eta);
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
    void TaskProgressReporterImGui::renderGroup(const TaskGroup& g, size_t idx,
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
        const std::string badge = TaskRunner::badgeText(running, done, failed, pending);
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
                    timeStr = "Elapsed: " + TaskRunner::formatDuration(elapsed);

                if (g.showTimeEstimate && running && frac > 0.001f)
                {
                    float eta = elapsed / frac * (1.f - frac);
                    if (!timeStr.empty()) timeStr += "   ";
                    timeStr += "ETA: " + TaskRunner::formatDuration(eta);
                }
                else if (g.showTimeEstimate && (done || failed))
                {
                    if (!timeStr.empty()) timeStr += "   ";
                    timeStr += (failed ? "Failed after " : "Took ")
                        + TaskRunner::formatDuration(elapsed);
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

    // ---- Main entry point (was TaskRunner::renderUI) ----------------
    void TaskProgressReporterImGui::display()
    {
        m_runner.pumpCallbacks();
        m_runner.pruneFinished(5.0f);

        std::vector<std::shared_ptr<TaskGroup>> snapshot = m_runner.snapshotGroups();
        if (snapshot.empty()) return;

        // Anchor to the bottom-right corner of the main viewport, with a
        // fixed pixel margin from the edges.
        const ImGuiViewport* viewport = ImGui::GetMainViewport();
        const float padding =24.0f;
        ImVec2 anchorPos(
            viewport->WorkPos.x + viewport->WorkSize.x - padding,
            viewport->WorkPos.y + viewport->WorkSize.y - padding);
        ImVec2 anchorPivot(1.0f, 1.0f); // pivot = bottom-right corner of the window itself

        ImGui::SetNextWindowPos(anchorPos, ImGuiCond_Always, anchorPivot);
        ImGui::SetNextWindowSizeConstraints(
            ImVec2(windowWidth, 0.f),
            ImVec2(windowWidth, FLT_MAX));

        ImGuiWindowFlags flags =
            ImGuiWindowFlags_AlwaysAutoResize |
            ImGuiWindowFlags_NoCollapse |
            ImGuiWindowFlags_NoMove;   // anchored — don't let it be dragged off position

        if (!ImGui::Begin(windowTitle.c_str(), p_open, flags))
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

} // namespace adapters