// SPDX-License-Identifier: MIT
//
// `HOUSE-01540`. What a 250 ms hitch does to the two things that measure time, and why they must
// answer differently.
//
// `FrameContext` carries two deltas and they are equal on every frame but this one. §49.3's
// accumulator integrates the CLAMPED delta, because 250 ms integrated whole is a body teleported
// through a wall; §35's clock advances by the REAL one, because the afternoon does not stop
// because a texture upload took a quarter of a second. A frame that hitches therefore simulates
// 6 simulated seconds of physics while the wall clock gains 15, and that gap is the design rather
// than a defect in either half.
//
// `HOUSE-00139` and `HOUSE-00549` proved the accumulator's own behaviour and `HOUSE-01531` the
// clock's. What is proved here is the JOIN: that the two are fed from different fields, that the
// fields differ when it matters, and that feeding the clock the wrong one is a session that
// quietly loses time.
#include <cmath>
#include <cstdint>

#include <gtest/gtest.h>

#include "cnahouse/app/FrameTimer.hpp"
#include "cnahouse/environment/SimClock.hpp"

namespace
{
    using cnahouse::app::FrameContext;
    using cnahouse::app::FrameTimer;
    using cnahouse::environment::kDefaultTimeScale;
    using cnahouse::environment::SimClock;

    constexpr float kHitchSeconds = 0.250F;
    constexpr float kOrdinaryFrame = 1.0F / 60.0F;

} // namespace

TEST(ClockHitchTests, TheHitchFrameCarriesTwoDifferentDeltas)
{
    FrameTimer timer;
    const FrameContext frame = timer.Advance(kHitchSeconds);

    EXPECT_FLOAT_EQ(frame.realDeltaSeconds, kHitchSeconds) << "the real delta was clamped away";
    EXPECT_FLOAT_EQ(frame.deltaSeconds, FrameTimer::kMaxDeltaSeconds);
    EXPECT_LT(frame.deltaSeconds, frame.realDeltaSeconds)
        << "the two deltas are the same on a hitch frame, so nothing below distinguishes them";
    // §49.3's cap: four steps, 33 ms of simulation, whatever the frame asked for.
    EXPECT_EQ(frame.fixedSteps, FrameTimer::kMaxFixedSteps);
    EXPECT_FLOAT_EQ(frame.fixedStepSeconds, FrameTimer::kFixedStepSeconds);
    // ...and the rest is DISCARDED and counted. The clamped 0.100 s asks for 12 steps at 120 Hz;
    // four run and the residue is eight, which the counter says out loud rather than banking.
    EXPECT_EQ(timer.DroppedFixedSteps(), 8U)
        << "the cap dropped a different number of steps than the clamped delta accounts for";
}

TEST(ClockHitchTests, TheClockGainsTheTimeThatReallyPassed)
{
    FrameTimer timer;
    const FrameContext frame = timer.Advance(kHitchSeconds);

    SimClock clock;
    clock.Advance(static_cast<double>(frame.realDeltaSeconds));
    EXPECT_DOUBLE_EQ(clock.epochSeconds, 15.0)
        << "250 ms at 60x is 15 simulated seconds, and the clock did not gain them";

    // The number the clock would have if it were fed the physics delta, so the difference is a
    // measurement rather than an assertion about which field the code happens to name.
    SimClock clamped;
    clamped.Advance(static_cast<double>(frame.deltaSeconds));
    // Near and not exactly equal: the clamp is a `float`, so 0.100 F x 60 lands 90 ns off 6 s.
    EXPECT_NEAR(clamped.epochSeconds, 6.0, 1e-6);
    EXPECT_GT(clock.epochSeconds - clamped.epochSeconds, 8.0)
        << "the two deltas produce the same clock, so this test cannot tell them apart";
}

TEST(ClockHitchTests, AnOrdinaryFrameLeavesNoGapAtAll)
{
    // The other half of the claim: the divergence exists only where the clamp bites. On a 60 Hz
    // frame both deltas are the same number and both halves of the frame agree, which is what
    // makes the hitch case a special case rather than a permanent drift.
    FrameTimer timer;
    const FrameContext frame = timer.Advance(kOrdinaryFrame);
    EXPECT_FLOAT_EQ(frame.realDeltaSeconds, frame.deltaSeconds);
    EXPECT_EQ(frame.fixedSteps, 2) << "60 Hz is two 120 Hz steps";
    EXPECT_EQ(timer.DroppedFixedSteps(), 0U);

    SimClock real;
    SimClock physics;
    real.Advance(static_cast<double>(frame.realDeltaSeconds));
    physics.Advance(static_cast<double>(frame.deltaSeconds));
    EXPECT_DOUBLE_EQ(real.epochSeconds, physics.epochSeconds);
}

TEST(ClockHitchTests, ASessionOfHitchesKeepsTheClockOnRealTimeAndTheSimulationBehind)
{
    // Sixty consecutive hitch frames -- fifteen seconds of wall clock, which is a pack load or a
    // window-manager stall, not a hypothetical. The clock must be exactly where fifteen real
    // seconds put it; the simulation must be measurably behind; and the dropped-step counter must
    // account for the difference rather than leaving it to be noticed as "it feels sluggish".
    constexpr int kFrames = 60;
    FrameTimer timer;
    SimClock clock;
    double realElapsed = 0.0;
    for (int frame = 0; frame < kFrames; ++frame)
    {
        const FrameContext context = timer.Advance(kHitchSeconds);
        clock.Advance(static_cast<double>(context.realDeltaSeconds));
        realElapsed += static_cast<double>(context.realDeltaSeconds);
    }
    const FrameContext last = timer.Advance(kHitchSeconds);

    EXPECT_NEAR(realElapsed, static_cast<double>(kFrames) * static_cast<double>(kHitchSeconds), 1e-4);
    EXPECT_NEAR(clock.epochSeconds, realElapsed * kDefaultTimeScale, 1e-3)
        << "fifteen real seconds of hitches did not become fifteen simulated minutes";
    EXPECT_NEAR(clock.epochSeconds, 900.0, 1e-3) << "which is 15 simulated minutes";

    // `totalSeconds` is accumulated from the CLAMPED delta, so it is what the simulation thinks
    // has happened. After sixty hitches it is 6 s against 15 s of wall clock.
    EXPECT_NEAR(last.totalSeconds,
                static_cast<double>(kFrames + 1) * static_cast<double>(FrameTimer::kMaxDeltaSeconds),
                1e-3)
        << "the sixty-first frame is the one that produced `last`, and it counts";
    EXPECT_LT(last.totalSeconds, realElapsed * 0.75)
        << "the simulation kept up with a session of 250 ms frames, which it cannot have done";

    // 8 dropped steps a frame, every frame, and nothing banked: the spiral of death would show
    // here as a growing number of steps RUN, not dropped.
    EXPECT_EQ(timer.DroppedFixedSteps(), 8U * static_cast<std::uint64_t>(kFrames + 1));
    EXPECT_EQ(last.fixedSteps, FrameTimer::kMaxFixedSteps)
        << "the accumulator banked catch-up and is running more steps than the cap allows";
}

TEST(ClockHitchTests, TheClockRefusesTheDeltaThatWouldDestroyIt)
{
    // A platform timer that glitches hands back a negative or a NaN, and a calendar that took one
    // would be beyond recovery in a single frame -- there is no "undo" for `epochSeconds = NaN`.
    // `FrameTimer` floors the real delta at zero, so the clock sees a 0 rather than a −5; this
    // checks both, because the two are separate protections and either could be removed alone.
    FrameTimer timer;
    const FrameContext backwards = timer.Advance(-5.0F);
    EXPECT_FLOAT_EQ(backwards.realDeltaSeconds, 0.0F) << "the frame timer passed a negative delta on";
    EXPECT_EQ(backwards.fixedSteps, 0);

    SimClock clock;
    clock.Advance(0.5);
    const double before = clock.epochSeconds;
    clock.Advance(static_cast<double>(backwards.realDeltaSeconds));
    clock.Advance(-5.0);
    clock.Advance(std::nan(""));
    EXPECT_DOUBLE_EQ(clock.epochSeconds, before);
    EXPECT_TRUE(std::isfinite(clock.epochSeconds));
}
