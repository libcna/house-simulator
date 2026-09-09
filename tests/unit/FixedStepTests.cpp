// SPDX-License-Identifier: MIT
//
// `HOUSE-00633`. §49.3's accumulator: *"fixed step dt = 1/120 s, accumulated from GameTime, max 4
// steps per frame"*.
//
// Three lines of arithmetic with three ways to be wrong -- dropping the remainder, capping by
// stopping early, and letting a hitch through -- and every one of them is invisible until the
// frame rate changes on somebody else's machine.
#include <gtest/gtest.h>

#include "cnahouse/player/FixedStep.hpp"

namespace
{
    using cnahouse::player::FixedSteps;
    using cnahouse::player::kFixedStepSeconds;
    using cnahouse::player::kMaxFixedStepsPerFrame;
} // namespace

TEST(FixedStepTests, TheStepIsTheOneTheDesignStates)
{
    EXPECT_FLOAT_EQ(kFixedStepSeconds, 1.0F / 120.0F);
    EXPECT_EQ(kMaxFixedStepsPerFrame, 4);
}

TEST(FixedStepTests, SixtyFramesASecondIsTwoStepsEach)
{
    float accumulator = 0.0F;
    for (int frame = 0; frame < 100; ++frame)
    {
        EXPECT_EQ(FixedSteps(accumulator, 1.0F / 60.0F), 2);
    }
}

TEST(FixedStepTests, TheRemainderIsCarriedRatherThanDropped)
{
    // 100 fps is 1.2 steps a frame: alternating 1 and 2, and 120 of them in a second. Dropping
    // the remainder would run the simulation at 100 Hz while calling it 120, and every number
    // §43 states in metres per second would be 17 % out.
    float accumulator = 0.0F;
    int steps = 0;
    for (int frame = 0; frame < 100; ++frame)
    {
        steps += FixedSteps(accumulator, 1.0F / 100.0F);
    }
    EXPECT_GE(steps, 119);
    EXPECT_LE(steps, 120);

    // ...and at a rate that divides into nothing at all.
    accumulator = 0.0F;
    steps = 0;
    for (int frame = 0; frame < 144; ++frame)
    {
        steps += FixedSteps(accumulator, 1.0F / 144.0F);
    }
    EXPECT_GE(steps, 119);
    EXPECT_LE(steps, 120);
}

TEST(FixedStepTests, AHitchIsCappedAndNotBanked)
{
    // A one-second frame: four steps, and the other 116 are DISCARDED. Banking them would arrive
    // as a burst on the frames after -- the body crossing rooms nobody saw it walk through, with
    // every one-shot in them skipped.
    float accumulator = 0.0F;
    EXPECT_EQ(FixedSteps(accumulator, 1.0F), kMaxFixedStepsPerFrame);
    EXPECT_LT(accumulator, kFixedStepSeconds) << "a second of catch-up was banked for the next frame";
    EXPECT_EQ(FixedSteps(accumulator, 1.0F / 60.0F), 2) << "the frame after a hitch is an ordinary frame";
}

TEST(FixedStepTests, AFrameTooShortToStepBanksItsTime)
{
    // 1000 fps: eight frames of nothing and then one step. The alternative -- a step every frame
    // whatever its length -- is a simulation whose speed is the frame rate.
    float accumulator = 0.0F;
    int steps = 0;
    for (int frame = 0; frame < 1000; ++frame)
    {
        steps += FixedSteps(accumulator, 1.0F / 1000.0F);
    }
    EXPECT_GE(steps, 119);
    EXPECT_LE(steps, 120);
}

TEST(FixedStepTests, APausedFrameStepsNothing)
{
    float accumulator = 0.0F;
    EXPECT_EQ(FixedSteps(accumulator, 0.0F), 0);
    EXPECT_EQ(FixedSteps(accumulator, -1.0F), 0) << "a negative delta ran the world backwards";
    EXPECT_FLOAT_EQ(accumulator, 0.0F);
}
