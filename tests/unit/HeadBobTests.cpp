// SPDX-License-Identifier: MIT
//
// `HOUSE-00627`. §44's head motion, on §62.4's cadence.
//
// Two things are being asserted here and they pull in opposite directions. The first is that the
// motion is TIED to the footsteps -- the same accumulator, the eye at rest on every plant -- so
// that when phase 12's footstep director arrives the sound lands with the step and not near it.
// The second is that it is SMALL: §44's rule is *"any bob large enough to notice is too large"*,
// and the numbers here are the ones that keep a tester walking the house for twenty minutes.
#include <algorithm>
#include <cmath>
#include <numbers>
#include <vector>

#include <gtest/gtest.h>

#include "cnahouse/player/FirstPersonCamera.hpp"
#include "cnahouse/player/HeadBob.hpp"

namespace
{
    using cnahouse::player::HeadBob;
    using cnahouse::player::HeadBobLevel;
    using cnahouse::player::HeadBobLevelFromName;
    using cnahouse::player::HeadBobLevelName;
    using cnahouse::player::HeadBobOffset;
    using cnahouse::player::kBobAmplitude;
    using cnahouse::player::kBobReferenceSpeed;
    using cnahouse::player::kBobStrideFast;
    using cnahouse::player::kBobStrideNormal;
    using cnahouse::player::kBobSwayDegrees;
    using cnahouse::player::kNormalBobScale;
    using Microsoft::Xna::Framework::Vector3;

    constexpr float kPi = std::numbers::pi_v<float>;

    /// One frame's travel due north at @p speed.
    Vector3 North(float speed, float dt)
    {
        return Vector3(0.0F, 0.0F, -speed * dt);
    }

    struct Frame
    {
        float distance = 0.0F;
        int steps = 0;
        HeadBobOffset offset;
    };

    /// Walks @p metres at @p speed and keeps every frame, so a test can ask where in the walk
    /// something happened rather than only what the total was.
    std::vector<Frame> Walk(HeadBob& bob, float metres, float speed, float dt, bool fastWalk = false)
    {
        std::vector<Frame> frames;
        float walked = 0.0F;
        while (walked < metres)
        {
            const HeadBobOffset offset = bob.Update(North(speed, dt), dt, true, fastWalk);
            walked += speed * dt;
            frames.push_back(Frame{walked, bob.TakeSteps(), offset});
        }
        return frames;
    }

    int TotalSteps(const std::vector<Frame>& frames)
    {
        int total = 0;
        for (const Frame& frame : frames)
        {
            total += frame.steps;
        }
        return total;
    }

} // namespace

TEST(HeadBobTests, TheNumbersAreTheOnesTheDesignStates)
{
    EXPECT_FLOAT_EQ(kBobStrideNormal, 0.75F) << "§62.4's walking stride";
    EXPECT_FLOAT_EQ(kBobStrideFast, 0.95F) << "§62.4's fast-walk stride";
    EXPECT_FLOAT_EQ(kBobAmplitude, 0.012F) << "§44's amplitude at walking pace";
    EXPECT_FLOAT_EQ(kBobReferenceSpeed, 1.35F) << "§43.2's walk, which the amplitude is stated at";
    EXPECT_FLOAT_EQ(kBobSwayDegrees, 0.35F) << "§44's lateral sway";
    EXPECT_FLOAT_EQ(kNormalBobScale, 2.0F);

    // §68's table: Off / Subtle / Normal, defaulting to Subtle.
    EXPECT_EQ(HeadBob{}.Level(), HeadBobLevel::Subtle);
    EXPECT_EQ(HeadBobLevelName(HeadBobLevel::Off), "off");
    EXPECT_EQ(HeadBobLevelName(HeadBobLevel::Subtle), "subtle");
    EXPECT_EQ(HeadBobLevelName(HeadBobLevel::Normal), "normal");
    EXPECT_EQ(HeadBobLevelFromName("normal", HeadBobLevel::Off), HeadBobLevel::Normal);
    EXPECT_EQ(HeadBobLevelFromName("gentle", HeadBobLevel::Subtle), HeadBobLevel::Subtle)
        << "an unknown level must fall back rather than fail";
}

TEST(HeadBobTests, AStepEveryStrideAndTheEyeIsAtRestOnEveryOne)
{
    // §62.4: one footstep per 0.75 m walked. 3.1 m is four of them, and the fifth is not due yet.
    HeadBob bob;
    const std::vector<Frame> frames = Walk(bob, 3.1F, kBobReferenceSpeed, 1.0F / 120.0F);
    EXPECT_EQ(TotalSteps(frames), 4);

    // The tie itself: on the frame a step is reported the eye is back at the height §43.1 gives
    // it. That is what makes the footstep sound land with the foot rather than near it.
    int checked = 0;
    for (const Frame& frame : frames)
    {
        if (frame.steps > 0)
        {
            EXPECT_NEAR(frame.offset.vertical, 0.0F, 1e-4F) << "the plant is not at the bottom of the cycle";
            // A twentieth of the peak: the frame the step is reported on is already a
            // hundredth of a stride past the plant, and the sway leaves zero linearly while the
            // rise leaves it quadratically.
            EXPECT_LT(std::abs(frame.offset.yaw), 0.05F * kBobSwayDegrees * kPi / 180.0F)
                << "the sway does not pass through zero at the plant";
            ++checked;
        }
    }
    EXPECT_EQ(checked, 4);

    // ...and the top of the rise is halfway between two plants, which is where the leg carrying
    // the body is straightest.
    float peakAt = 0.0F;
    float peak = 0.0F;
    for (const Frame& frame : frames)
    {
        if (frame.distance < kBobStrideNormal && frame.offset.vertical > peak)
        {
            peak = frame.offset.vertical;
            peakAt = frame.distance;
        }
    }
    EXPECT_NEAR(peakAt / kBobStrideNormal, 0.5F, 0.02F);
}

TEST(HeadBobTests, TheAmplitudeIsTheDesignsAtWalkingPaceAndScalesWithTheSpeed)
{
    // §44: *"amplitude 0.012 m · (speed/1.35)"*, and `Subtle` is the level §44 is describing --
    // §68 makes it the default and §44 calls the default *"on but low"*.
    struct Case
    {
        HeadBobLevel level;
        float speed;
        float expected;
    };

    const Case cases[] = {
        {HeadBobLevel::Subtle, kBobReferenceSpeed, kBobAmplitude},
        {HeadBobLevel::Subtle, 2.05F, kBobAmplitude * 2.05F / kBobReferenceSpeed},
        {HeadBobLevel::Subtle, 0.60F, kBobAmplitude * 0.60F / kBobReferenceSpeed},
        {HeadBobLevel::Normal, kBobReferenceSpeed, kBobAmplitude * kNormalBobScale},
        {HeadBobLevel::Off, kBobReferenceSpeed, 0.0F},
    };

    for (const Case& one : cases)
    {
        HeadBob bob;
        bob.SetLevel(one.level);
        float peak = 0.0F;
        float trough = 0.0F;
        for (const Frame& frame : Walk(bob, 3.0F, one.speed, 1.0F / 240.0F))
        {
            peak = std::max(peak, frame.offset.vertical);
            trough = std::min(trough, frame.offset.vertical);
        }
        EXPECT_NEAR(peak, one.expected, 1e-4F)
            << "level " << HeadBobLevelName(one.level) << " at " << one.speed;
        // Never BELOW the standing height: §43.1's eye is the bottom of a walking cycle, not its
        // middle, so the head rises over the carrying leg and comes back down to where it was.
        EXPECT_FLOAT_EQ(trough, 0.0F);
    }

    // The acceptance criterion, as its own assertion: at `Subtle` the amplitude is <= 0.012 m at
    // every speed §43.2 allows, including the fast walk.
    for (const float speed : {0.6F, 1.35F, 2.05F})
    {
        HeadBob bob;
        for (const Frame& frame : Walk(bob, 2.0F, speed, 1.0F / 240.0F))
        {
            EXPECT_LE(frame.offset.vertical, kBobAmplitude * speed / kBobReferenceSpeed + 1e-5F);
        }
    }
}

TEST(HeadBobTests, TheSwayGoesOneWayOnOneFootAndTheOtherWayOnTheNext)
{
    // §44's 0.35°, and the half-rate that makes it a sway rather than a second bob: the weight
    // goes left, then right, so the cycle is two steps long and not one.
    HeadBob bob;
    const std::vector<Frame> frames = Walk(bob, 3.1F, kBobReferenceSpeed, 1.0F / 240.0F);

    float firstStepPeak = 0.0F;
    float secondStepPeak = 0.0F;
    for (const Frame& frame : frames)
    {
        if (frame.distance < kBobStrideNormal)
        {
            firstStepPeak = std::max(firstStepPeak, frame.offset.yaw);
        }
        else if (frame.distance < 2.0F * kBobStrideNormal)
        {
            secondStepPeak = std::min(secondStepPeak, frame.offset.yaw);
        }
    }
    const float expected = kBobSwayDegrees * kPi / 180.0F;
    EXPECT_NEAR(firstStepPeak, expected, 1e-4F);
    EXPECT_NEAR(secondStepPeak, -expected, 1e-4F) << "both steps swayed the same way";

    // And the third step is back where the first was: the cycle closes after two.
    float thirdStepPeak = 0.0F;
    for (const Frame& frame : frames)
    {
        if (frame.distance >= 2.0F * kBobStrideNormal && frame.distance < 3.0F * kBobStrideNormal)
        {
            thirdStepPeak = std::max(thirdStepPeak, frame.offset.yaw);
        }
    }
    EXPECT_NEAR(thirdStepPeak, expected, 1e-4F);
}

TEST(HeadBobTests, TheCadenceIsDistanceAndNotTime)
{
    // The same 4 m walked at 30 fps and at 240 fps: the same steps, the same phase, the same eye.
    // A bob on a timer would put the feet in different places on different machines, and
    // `HOUSE-00632`'s tuning would then only be true on the machine it was tuned on.
    HeadBob slow;
    HeadBob fast;
    const std::vector<Frame> slowFrames = Walk(slow, 4.0F, kBobReferenceSpeed, 1.0F / 30.0F);
    const std::vector<Frame> fastFrames = Walk(fast, 4.0F, kBobReferenceSpeed, 1.0F / 240.0F);

    EXPECT_EQ(TotalSteps(slowFrames), TotalSteps(fastFrames));
    EXPECT_NEAR(slow.StepPhase(), fast.StepPhase(), 1e-3F);
    EXPECT_NEAR(slowFrames.back().offset.vertical, fastFrames.back().offset.vertical, 1e-4F);
    EXPECT_NEAR(slowFrames.back().offset.yaw, fastFrames.back().offset.yaw, 1e-4F);
}

TEST(HeadBobTests, TheFastWalkTakesLongerStrides)
{
    // §62.4's other stride. 2.4 m of corridor is THREE walking steps (0.75, 1.50, 2.25) and TWO
    // fast ones (0.95, 1.90) -- the same ground at the same speed, and a different number of feet
    // on it, which is the only way to tell the two strides apart.
    HeadBob fast;
    const std::vector<Frame> fastFrames = Walk(fast, 2.4F, 2.05F, 1.0F / 240.0F, true);
    EXPECT_EQ(TotalSteps(fastFrames), 2);

    HeadBob normal;
    EXPECT_EQ(TotalSteps(Walk(normal, 2.4F, 2.05F, 1.0F / 240.0F, false)), 3);

    // ...and the fast walker's first foot lands at 0.95 m and not at 0.75.
    float firstStepAt = 0.0F;
    for (const Frame& frame : fastFrames)
    {
        if (frame.steps > 0 && firstStepAt == 0.0F)
        {
            firstStepAt = frame.distance;
        }
    }
    EXPECT_NEAR(firstStepAt, kBobStrideFast, 0.02F);
}

TEST(HeadBobTests, TheStepsKeepComingWithTheBobTurnedOff)
{
    // The reason this class holds the accumulator: §62.4's footsteps come out of it, and a CAMERA
    // setting must not change when the house makes a sound. `Off` stills the view and nothing else.
    HeadBob off;
    off.SetLevel(HeadBobLevel::Off);
    HeadBob on;

    const std::vector<Frame> offFrames = Walk(off, 5.0F, kBobReferenceSpeed, 1.0F / 120.0F);
    const std::vector<Frame> onFrames = Walk(on, 5.0F, kBobReferenceSpeed, 1.0F / 120.0F);

    EXPECT_EQ(TotalSteps(offFrames), TotalSteps(onFrames));
    EXPECT_EQ(TotalSteps(offFrames), 6);
    EXPECT_FLOAT_EQ(off.StepPhase(), on.StepPhase());
    for (const Frame& frame : offFrames)
    {
        EXPECT_FLOAT_EQ(frame.offset.vertical, 0.0F);
        EXPECT_FLOAT_EQ(frame.offset.yaw, 0.0F);
    }
}

TEST(HeadBobTests, StandingStillIsStill)
{
    HeadBob bob;
    Walk(bob, 0.5F, kBobReferenceSpeed, 1.0F / 120.0F);

    // Mid-step, so there IS something to fall back from.
    EXPECT_GT(bob.Offsets().vertical, 0.0F);
    const HeadBobOffset stopped = bob.Update(Vector3(0.0F, 0.0F, 0.0F), 1.0F / 120.0F, true, false);
    EXPECT_FLOAT_EQ(stopped.vertical, 0.0F);
    EXPECT_FLOAT_EQ(stopped.yaw, 0.0F);
    EXPECT_EQ(bob.TakeSteps(), 0);

    // A zero-length frame is not a stride either, and it must not divide by it.
    const HeadBobOffset paused = bob.Update(North(kBobReferenceSpeed, 0.008F), 0.0F, true, false);
    EXPECT_TRUE(std::isfinite(paused.vertical));
    EXPECT_TRUE(std::isfinite(paused.yaw));
    EXPECT_EQ(bob.TakeSteps(), 0);
}

TEST(HeadBobTests, AirborneFinishesTheStepAndThenHoldsStill)
{
    // A body in the air is taking no steps -- but it was taking one when it left the ground, and
    // cutting that off mid-swing is a jump in the view. It finishes the step and then holds, which
    // leaves the eye exactly at rest for the landing (`HOUSE-00629`'s dip starts from there).
    HeadBob bob;
    Walk(bob, 0.40F, kBobReferenceSpeed, 1.0F / 120.0F);
    ASSERT_GT(bob.Offsets().vertical, 0.0F) << "the fixture did not leave the walk mid-step";

    const float dt = 1.0F / 120.0F;
    for (int frame = 0; frame < 120; ++frame)
    {
        bob.Update(North(kBobReferenceSpeed, dt), dt, false, false);
    }
    EXPECT_FLOAT_EQ(bob.Offsets().vertical, 0.0F) << "the eye is still moving in mid-air";
    EXPECT_FLOAT_EQ(bob.Offsets().yaw, 0.0F);
    EXPECT_EQ(bob.TakeSteps(), 1) << "the step that was under way did not finish";
    EXPECT_NEAR(bob.StepPhase(), 1.0F, 1e-3F) << "the phase did not stop on a plant";

    // ...and the next stride on the ground picks up from the plant it stopped on: one more step
    // for one more stride, and not two because the frozen one was still owed.
    EXPECT_EQ(TotalSteps(Walk(bob, kBobStrideNormal, kBobReferenceSpeed, dt)), 1);
}

TEST(HeadBobTests, ResetPutsTheBodyBackOnAPlant)
{
    // §57's teleport: the distance between one frame and the next is not a walk, and the step it
    // would otherwise count is a footstep sound in an empty room.
    HeadBob bob;
    Walk(bob, 0.40F, kBobReferenceSpeed, 1.0F / 120.0F);
    ASSERT_GT(bob.Offsets().vertical, 0.0F);

    bob.Reset();
    EXPECT_FLOAT_EQ(bob.StepPhase(), 0.0F);
    EXPECT_EQ(bob.TakeSteps(), 0);
    EXPECT_FLOAT_EQ(bob.Offsets().vertical, 0.0F);
}

TEST(HeadBobTests, AStairIsClimbedRatherThanWalked)
{
    // Only the HORIZONTAL part of a frame's travel is a stride. §62.4 counts a stair in risers and
    // §48.2 stiffens the eye spring there *"so the view rises steadily rather than bobbing per
    // step"*, so a body going straight up a lift shaft takes no steps at all.
    HeadBob bob;
    const float dt = 1.0F / 120.0F;
    for (int frame = 0; frame < 240; ++frame)
    {
        bob.Update(Vector3(0.0F, 1.0F * dt, 0.0F), dt, true, false);
    }
    EXPECT_EQ(bob.TakeSteps(), 0);
    EXPECT_FLOAT_EQ(bob.Offsets().vertical, 0.0F);
}

TEST(HeadBobTests, TheBobLiftsTheEyeAndTurnsTheViewWithoutRollingIt)
{
    // What the camera does with it (`HOUSE-00621`). The eye rises by the offset and the view turns
    // by the sway -- and §44's *"roll is always zero"* still holds, because the sway is about the
    // vertical axis and the basis is still built from a yaw.
    using cnahouse::player::FirstPersonCamera;
    using cnahouse::player::kPlayerEyeHeight;
    using cnahouse::player::PlayerState;

    PlayerState state;
    state.position = Vector3(2.0F, 0.90F, -3.0F);
    state.yaw = 0.5F;

    FirstPersonCamera plain;
    plain.Update(state, kPlayerEyeHeight, 0.1F);
    FirstPersonCamera bobbed;
    HeadBobOffset offset;
    offset.vertical = 0.012F;
    offset.yaw = 0.35F * kPi / 180.0F;
    bobbed.Update(state, kPlayerEyeHeight, 0.1F, offset);

    EXPECT_NEAR(bobbed.Pose().eye.Y - plain.Pose().eye.Y, 0.012F, 1e-5F);
    EXPECT_FLOAT_EQ(bobbed.Pose().eye.X, plain.Pose().eye.X);
    EXPECT_FLOAT_EQ(bobbed.Pose().eye.Z, plain.Pose().eye.Z);
    EXPECT_FLOAT_EQ(bobbed.Pose().yaw, state.yaw + offset.yaw);

    // The view really turned -- the basis was rebuilt around the swayed yaw and not left on the
    // body's -- and it turned by the sway and nothing else.
    const float turned = std::atan2(bobbed.Pose().forward.X, -bobbed.Pose().forward.Z) -
                         std::atan2(plain.Pose().forward.X, -plain.Pose().forward.Z);
    EXPECT_NEAR(turned, offset.yaw, 1e-5F);

    // §44: the horizon stays level whatever the bob is doing.
    EXPECT_FLOAT_EQ(bobbed.Pose().right.Y, 0.0F);
    EXPECT_NEAR(bobbed.Pose().up.X * bobbed.Pose().right.X + bobbed.Pose().up.Y * bobbed.Pose().right.Y +
                    bobbed.Pose().up.Z * bobbed.Pose().right.Z,
                0.0F,
                1e-6F);
}
