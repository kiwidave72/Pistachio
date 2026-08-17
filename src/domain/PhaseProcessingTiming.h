#pragma once

// -----------------------------------------------------------------------
// PhaseProcessingTiming.h
//
// Small, reusable timing capture — attached directly to each phase's
// result, not a separate log. Matches the DiagnosticMessage philosophy:
// data about the thing, not a parallel stream.
// -----------------------------------------------------------------------

#include <chrono>

namespace domain::v1 {

    struct PhaseProcessingTiming
    {
        double milliseconds = 0.0;
    };

    class PhaseProcessingTimer
    {
    public:
        PhaseProcessingTimer() : m_start(std::chrono::high_resolution_clock::now()) {}

        PhaseProcessingTiming elapsed() const
        {
            auto now = std::chrono::high_resolution_clock::now();
            return { std::chrono::duration<double, std::milli>(now - m_start).count() };
        }

    private:
        std::chrono::high_resolution_clock::time_point m_start;
    };

} // namespace domain::v1