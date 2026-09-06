// SPDX-License-Identifier: MIT
#include "cnahouse/app/FrameTimer.hpp"

#include <algorithm>

namespace cnahouse::app
{

    FrameContext FrameTimer::Advance(float realDeltaSeconds) noexcept
    {
        FrameContext context;
        context.realDeltaSeconds = std::max(0.0f, realDeltaSeconds);
        // Clamped, so a hitch makes the simulation run slow for a frame rather than run wrong.
        context.deltaSeconds = std::min(context.realDeltaSeconds, kMaxDeltaSeconds);

        totalSeconds_ += static_cast<double>(context.deltaSeconds);
        context.totalSeconds = totalSeconds_;
        context.frameIndex = frameIndex_++;

        accumulator_ += context.deltaSeconds;
        int steps = 0;
        while (accumulator_ >= kFixedStepSeconds && steps < kMaxFixedSteps)
        {
            accumulator_ -= kFixedStepSeconds;
            ++steps;
        }
        if (accumulator_ >= kFixedStepSeconds)
        {
            // The cap was hit. The residue is DISCARDED rather than carried: carrying it is precisely
            // what compounds into the spiral of death, because next frame would then ask for even more
            // steps than this one could not deliver.
            const auto dropped = static_cast<std::uint64_t>(accumulator_ / kFixedStepSeconds);
            droppedSteps_ += dropped;
            accumulator_ = 0.0f;
        }
        context.fixedSteps = steps;
        context.fixedStepSeconds = kFixedStepSeconds;
        return context;
    }

    void FrameTimer::Reset() noexcept
    {
        totalSeconds_ = 0.0;
        frameIndex_ = 0;
        accumulator_ = 0.0f;
        droppedSteps_ = 0;
    }

} // namespace cnahouse::app
