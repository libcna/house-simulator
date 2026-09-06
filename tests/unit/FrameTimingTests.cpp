// SPDX-License-Identifier: MIT
//
// `HOUSE-00139`'s acceptance, literally: a 250 ms hitch clamps to 4 physics substeps and does not
// spiral.
#include <gtest/gtest.h>

#include "cnahouse/app/FrameTimer.hpp"

namespace
{
    using cnahouse::app::FrameContext;
    using cnahouse::app::FrameTimer;

    TEST(FrameTimingTests, ASteadySixtyHertzRunsOneFixedStepPerFrame)
    {
        FrameTimer timer;
        // The accumulator drifts by a fraction of a step each frame at exactly 1/60, so an occasional
        // 0- or 2-step frame is correct behaviour; what must hold is that the AVERAGE is one.
        int steps = 0;
        constexpr int kFrames = 600;
        for (int i = 0; i < kFrames; ++i)
        {
            steps += timer.Advance(1.0f / 60.0f).fixedSteps;
        }
        EXPECT_NEAR(static_cast<double>(steps) / kFrames, 1.0, 0.02);
        EXPECT_EQ(timer.DroppedFixedSteps(), 0u) << "a steady frame rate must drop nothing";
    }

    TEST(FrameTimingTests, AHitchClampsToFourSubstepsAndDoesNotSpiral)
    {
        FrameTimer timer;
        for (int i = 0; i < 10; ++i)
        {
            (void)timer.Advance(1.0f / 60.0f);
        }

        // The hitch. 250 ms of real time would be 15 fixed steps if the accumulator were honoured.
        const FrameContext hitch = timer.Advance(0.250f);
        EXPECT_EQ(hitch.fixedSteps, FrameTimer::kMaxFixedSteps);
        EXPECT_FLOAT_EQ(hitch.realDeltaSeconds, 0.250f) << "the real delta is still reported";
        EXPECT_FLOAT_EQ(hitch.deltaSeconds, FrameTimer::kMaxDeltaSeconds)
            << "but what systems integrate is clamped, so nobody teleports through a wall";

        // And then it recovers immediately. This is the half that matters: a timer that carried the
        // residue would ask for more steps on the NEXT frame, which would take longer, which would ask
        // for more still -- the spiral of death.
        for (int i = 0; i < 30; ++i)
        {
            const FrameContext after = timer.Advance(1.0f / 60.0f);
            ASSERT_LE(after.fixedSteps, 2) << "frame " << i << " after the hitch is still catching up";
        }
    }

    TEST(FrameTimingTests, RepeatedHitchesStillDoNotSpiral)
    {
        FrameTimer timer;
        for (int i = 0; i < 100; ++i)
        {
            const FrameContext frame = timer.Advance(0.5f);
            ASSERT_LE(frame.fixedSteps, FrameTimer::kMaxFixedSteps);
            ASSERT_FLOAT_EQ(frame.deltaSeconds, FrameTimer::kMaxDeltaSeconds);
        }
        EXPECT_GT(timer.DroppedFixedSteps(), 0u)
            << "sustained overload must be REPORTED, not hidden -- it is what a player feels as "
               "sluggishness and what is otherwise invisible";
    }

    TEST(FrameTimingTests, TotalTimeAccumulatesFromTheClampedDelta)
    {
        FrameTimer timer;
        (void)timer.Advance(1.0f); // one absurd frame
        const FrameContext frame = timer.Advance(0.0f);
        // The clamped delta is what advances the clock, so a hitch does not jump the time of day by a
        // second and move the sun visibly.
        EXPECT_NEAR(frame.totalSeconds, static_cast<double>(FrameTimer::kMaxDeltaSeconds), 1e-6);
    }

    TEST(FrameTimingTests, FrameIndexCountsFromZeroAndNeverRepeats)
    {
        FrameTimer timer;
        for (std::uint64_t i = 0; i < 100; ++i)
        {
            EXPECT_EQ(timer.Advance(0.016f).frameIndex, i);
        }
    }

    TEST(FrameTimingTests, ANegativeDeltaIsTreatedAsZero)
    {
        // A clock that steps backwards is not impossible -- suspend/resume, an NTP correction -- and a
        // negative delta integrated by the physics system would move everything the wrong way.
        FrameTimer timer;
        const FrameContext frame = timer.Advance(-1.0f);
        EXPECT_FLOAT_EQ(frame.deltaSeconds, 0.0f);
        EXPECT_EQ(frame.fixedSteps, 0);
    }

    TEST(FrameTimingTests, ResetReturnsEverythingToItsInitialState)
    {
        FrameTimer timer;
        for (int i = 0; i < 50; ++i)
        {
            (void)timer.Advance(0.5f);
        }
        timer.Reset();
        const FrameContext frame = timer.Advance(1.0f / 60.0f);
        EXPECT_EQ(frame.frameIndex, 0u);
        EXPECT_EQ(timer.DroppedFixedSteps(), 0u);
        EXPECT_NEAR(frame.totalSeconds, 1.0 / 60.0, 1e-6);
    }

} // namespace
