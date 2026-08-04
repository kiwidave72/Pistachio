#pragma once

// -----------------------------------------------------------------------
// TaskProgressReporterImGui.h
//
// The ImGui-drawing half of what used to be TaskRunner::renderUI(). Reads
// TaskRunner's state via snapshotGroups() (read-only, thread-safe) — has
// no special access to TaskRunner internals, same as any other consumer.
//
// Lives outside pistachio_core deliberately, since it genuinely needs
// <imgui.h>.
// -----------------------------------------------------------------------

#include "ports/ITaskProgressReporter.h"
#include "core/TaskRunner.h"

#include <imgui.h>
#include <string>

namespace adapters::ui {

    class TaskProgressReporterImGui final : public ports::ITaskProgressReporter
    {
    public:
        explicit TaskProgressReporterImGui(TaskRunner& runner) : m_runner(runner) {}

        void display() override;

        // Was TaskRunner::Style — moved here unchanged.
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

        // Were renderUI()'s parameters — now configurable members, since
        // display() takes no arguments (interface contract).
        std::string windowTitle = "Background Tasks";
        bool* p_open = nullptr;
        float windowWidth = 460.f;

    private:
        void renderGroup(const TaskGroup& g, size_t idx,
            const ImVec4& colRunning, const ImVec4& colPending,
            const ImVec4& colDone, const ImVec4& colFailed);

        void renderStep(const TaskStep& s, size_t stepIdx, bool isLast,
            const ImVec4& colRunning, const ImVec4& colPending,
            const ImVec4& colDone, const ImVec4& colFailed);

        void renderBar(float fraction, bool running, bool done, bool failed,
            float barH,
            const ImVec4& colRunning, const ImVec4& colDone, const ImVec4& colFailed);

        static ImVec4 badgeColor(bool running, bool done, bool failed, bool pending,
            const ImVec4& cr, const ImVec4& cd,
            const ImVec4& cf, const ImVec4& cp);

        TaskRunner& m_runner;
    };

} // namespace adapters