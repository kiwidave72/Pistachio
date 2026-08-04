#pragma once

// -----------------------------------------------------------------------
// ITaskProgressReporter.h
//
// Lets a headless context and a UI context report TaskRunner progress
// differently, without TaskRunner itself knowing or caring which one is
// active. Chosen once, at construction, by TaskRunnerFactory — nothing
// downstream (TaskRunner itself, or anything submitting work to it) ever
// branches on "am I headless?".
//
// display() is expected to call the bound TaskRunner's pumpCallbacks()
// internally as its first step — callers never need to remember to call
// it separately (matches TaskRunner::renderUI()'s existing behavior,
// which already does this).
// -----------------------------------------------------------------------

namespace ports {

    class ITaskProgressReporter
    {
    public:
        virtual ~ITaskProgressReporter() = default;

        virtual void display() = 0;
    };

} // namespace ports