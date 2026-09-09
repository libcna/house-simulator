// SPDX-License-Identifier: MIT
#pragma once

#include <algorithm>

namespace cnahouse::player
{

    /// @brief §49.3's fixed step: *"dt = 1/120 s, accumulated from `GameTime`, max 4 steps per
    ///        frame"*.
    inline constexpr float kFixedStepSeconds = 1.0F / 120.0F;
    inline constexpr int kMaxFixedStepsPerFrame = 4;

    /// @brief How many fixed steps @p frameSeconds buys, carrying the remainder in @p accumulator.
    ///
    /// **The cap is not a performance guard, it is a correctness one.** A frame that took a second
    /// -- a loading hitch, a breakpoint, a laptop lid -- would otherwise be simulated in full, and
    /// 120 steps of walking arrive between one drawn frame and the next: the body crosses rooms
    /// nobody saw it walk through, and every one-shot those rooms would have fired is skipped.
    /// Four steps is a thirtieth of a second of catch-up; past that the game runs slow, which is
    /// visible and recoverable, rather than teleporting, which is neither.
    ///
    /// The remainder is what makes the step size mean anything: dropping it would make the
    /// simulation run at whatever fraction of 120 Hz the frame rate happened to divide into.
    [[nodiscard]] inline int FixedSteps(float& accumulator, float frameSeconds) noexcept
    {
        if (frameSeconds > 0.0F)
        {
            accumulator += frameSeconds;
        }
        // Clamped BEFORE the loop rather than by stopping it early, so the time a capped frame
        // could not simulate is discarded instead of arriving as a second burst next frame.
        accumulator = std::min(accumulator, static_cast<float>(kMaxFixedStepsPerFrame) * kFixedStepSeconds);
        int steps = 0;
        while (accumulator >= kFixedStepSeconds && steps < kMaxFixedStepsPerFrame)
        {
            accumulator -= kFixedStepSeconds;
            ++steps;
        }
        return steps;
    }

} // namespace cnahouse::player
