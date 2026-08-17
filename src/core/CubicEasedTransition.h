#pragma once

// -----------------------------------------------------------------------
// CubicEasedTransition.h
//
// Real, concrete implementation — ease-in-out cubic. Matches the
// easing behavior originally designed for SceneLayout's camera
// transitions; m_t defaults to 1.0 (idle/complete), same convention
// SceneLayout itself uses.
// -----------------------------------------------------------------------

#include "core/IEasedTransition.h"

#include <algorithm>
#include <cmath>

namespace core {

    class CubicEasedTransition final : public IEasedTransition
    {
    public:
        void tick(float deltaSeconds) override
        {
            if (m_duration <= 0.0f) return;
            m_t = (std::min)(1.0f, m_t + deltaSeconds / m_duration);
        }

        void start(float durationSeconds) override
        {
            m_duration = durationSeconds;
            m_t = 0.0f;
        }

        float progress() const override
        {
            return m_t < 0.5f
                ? 4.0f * m_t * m_t * m_t
                : 1.0f - std::pow(-2.0f * m_t + 2.0f, 3.0f) / 2.0f;
        }

        bool isAnimating() const override { return m_t < 1.0f; }

    private:
        float m_t = 1.0f;   // 1.0 = idle/complete
        float m_duration = 0.0f;
    };

} // namespace core