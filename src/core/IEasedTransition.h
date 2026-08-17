#pragma once

// -----------------------------------------------------------------------
// IEasedTransition.h
//
// Pure interface — the actual easing curve/implementation is a
// swappable policy, matching IModelRenderStrategy's own pattern. Used
// by AnimatedModel for both its transform and color transitions,
// injected at construction rather than owned/hardcoded.
// -----------------------------------------------------------------------

namespace core {

    class IEasedTransition
    {
    public:
        virtual ~IEasedTransition() = default;

        virtual void tick(float deltaSeconds) = 0;
        virtual void start(float durationSeconds) = 0;
        virtual float progress() const = 0;   // 0.0 -> 1.0, already eased
        virtual bool isAnimating() const = 0;
    };

} // namespace core