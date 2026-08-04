#pragma once

// -----------------------------------------------------------------------
// TaskProgressReporterConsole.h
//
// Headless-safe progress reporter — printf instead of ImGui. Genuinely
// ImGui-free, safe to compile into pistachio_core.
//
// Throttled: only prints when a group's rounded percentage or message
// actually changes since the last display() call, so calling this every
// tick of a headless loop doesn't spam identical lines.
// -----------------------------------------------------------------------

#include "ports/ITaskProgressReporter.h"
#include "core/TaskRunner.h"

#include <unordered_map>
#include <string>

namespace adapters::console {

    class TaskProgressReporterConsole final : public ports::ITaskProgressReporter
    {
    public:
        explicit TaskProgressReporterConsole(TaskRunner& runner) : m_runner(runner) {}

        void display() override;

    private:
        struct LastPrinted
        {
            int percent = -1;          // -1 = never printed
            std::string message;
        };

        TaskRunner& m_runner;
        std::unordered_map<const TaskGroup*, LastPrinted> m_lastPrinted;
    };

} // namespace adapters::console