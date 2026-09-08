// SPDX-License-Identifier: MIT
//
// `HOUSE-00561`. §44's eye-height spring: *"`eyeHeight` smoothed by a critically-damped spring
// (ω = 18 rad/s) so step-ups and stair climbing do not jolt the view"*, stiffened to ω = 24 on
// stairs (§48.2) *"so the view rises steadily rather than bobbing per step"*.
//
// "Critically damped" is the whole specification, and it is a testable one: the spring must reach
// its target in the shortest time possible WITHOUT ever going past it. Overshoot is the failure
// mode a spring tuned by feel has, and one line of arithmetic separates the two.
#include <cmath>

#include <gtest/gtest.h>

#include "cnahouse/player/EyeSpring.hpp"

namespace
{
    using cnahouse::player::EyeSpring;
    using cnahouse::player::kEyeSpringOmega;
    using cnahouse::player::kEyeSpringStairsOmega;

    constexpr float kDt = 1.0F / 120.0F;

    /// Steps @p spring towards @p target for @p seconds, reporting how far it ever went PAST it.
    float Overshoot(EyeSpring& spring, float target, float seconds, bool stiff = false)
    {
        const float from = spring.Height();
        float worst = 0.0F;
        const int steps = static_cast<int>(seconds / kDt);
        for (int i = 0; i < steps; ++i)
        {
            const float height = spring.Update(target, kDt, stiff);
            const float past = target > from ? height - target : target - height;
            worst = std::max(worst, past);
        }
        return worst;
    }

    /// How long, in seconds, until the spring is within @p tolerance of @p target.
    float SettleTime(EyeSpring& spring, float target, bool stiff = false)
    {
        for (int i = 0; i < 2400; ++i)
        {
            spring.Update(target, kDt, stiff);
            if (std::fabs(spring.Height() - target) < 0.001F)
            {
                return static_cast<float>(i + 1) * kDt;
            }
        }
        return 1000.0F;
    }

} // namespace

TEST(EyeSpringTests, TheTwoStiffnessesAreTheOnesTheDesignStates)
{
    EXPECT_FLOAT_EQ(kEyeSpringOmega, 18.0F);
    EXPECT_FLOAT_EQ(kEyeSpringStairsOmega, 24.0F);
    EXPECT_GT(kEyeSpringStairsOmega, kEyeSpringOmega) << "the stair spring must be the STIFFER one";
}

TEST(EyeSpringTests, ItNeverOvershootsWhichIsWhatCriticallyDampedMeans)
{
    // The one property that separates a critically damped spring from a tuned one. A 0.22 m
    // step-up -- §43.1's largest -- must bring the view up to the new height and stop there, not
    // sail past it and come back, which reads as a bounce.
    EyeSpring spring;
    spring.Snap(0.0F);
    EXPECT_LT(Overshoot(spring, 0.22F, 2.0F), 1e-4F);
    EXPECT_NEAR(spring.Height(), 0.22F, 1e-3F);

    // Downwards too: a 0.45 m step-down is the same spring in the other direction.
    spring.Snap(0.45F);
    EXPECT_LT(Overshoot(spring, 0.0F, 2.0F), 1e-4F);

    // ...and on the stiffer stair setting, where an under-damped spring would ring hardest.
    spring.Snap(0.0F);
    EXPECT_LT(Overshoot(spring, 0.18F, 2.0F, true), 1e-4F);
}

TEST(EyeSpringTests, TheStairSpringIsFasterThanTheWalkingOne)
{
    // §48.2's reason for stiffening it: a climb is a sequence of small lifts arriving at 120 Hz,
    // and a spring soft enough to hide a single kerb turns that sequence into a wallow.
    EyeSpring soft;
    soft.Snap(0.0F);
    EyeSpring stiff;
    stiff.Snap(0.0F);
    const float softTime = SettleTime(soft, 0.18F, false);
    const float stiffTime = SettleTime(stiff, 0.18F, true);
    EXPECT_LT(stiffTime, softTime);
    // ω = 24 against ω = 18 is a third stiffer, and the settling times are in that ratio to
    // within a step: a critically damped spring's time scale is exactly 1/ω.
    EXPECT_NEAR(softTime / stiffTime, kEyeSpringStairsOmega / kEyeSpringOmega, 0.1F);
}

TEST(EyeSpringTests, ItArrivesInTheTimeTheStiffnessImplies)
{
    // A critically damped spring's remaining error is `(1 + ωt)e^(-ωt)` of the step it started
    // with. A millimetre out of §43.1's largest step-up, 0.22 m, is 0.45 % -- which that
    // expression reaches at `ωt = 7.55`, so 0.42 s at ω = 18.
    //
    // The measured figure is a few per cent longer because the integrator is semi-implicit, which
    // damps very slightly more than the continuous solution it approximates. That is the price of
    // never diverging on a long frame, it is stated here rather than hidden in a loose tolerance,
    // and what the number has to mean is unchanged: a step-up is over in well under half a second.
    EyeSpring spring;
    spring.Snap(0.0F);
    const float measured = SettleTime(spring, 0.22F);
    EXPECT_NEAR(measured, 7.55F / kEyeSpringOmega, 0.05F);
    EXPECT_LT(measured, 0.5F) << "a step-up that takes half a second is a lift, not a step";
}

TEST(EyeSpringTests, ASnapDoesNotTravel)
{
    // A spawn, a teleport and a save load put the eye somewhere new, and the view must NOT sweep
    // across the house to get there.
    EyeSpring spring;
    spring.Snap(0.0F);
    spring.Update(2.0F, kDt);
    EXPECT_GT(spring.Velocity(), 0.0F);

    spring.Snap(9.0F);
    EXPECT_FLOAT_EQ(spring.Height(), 9.0F);
    EXPECT_FLOAT_EQ(spring.Velocity(), 0.0F) << "the snap kept the motion it had";
}

TEST(EyeSpringTests, ALongFrameSettlesTheEyeRatherThanLaunchingIt)
{
    // The reason the integrator is solved rather than stepped. The explicit form diverges once
    // `ω·dt` passes 2 -- at ω = 24 that is an 83 ms frame, which is a loading hitch, not a
    // hypothetical -- and a diverging eye height is a view that leaves the building.
    for (const float dt : {0.05F, 0.1F, 0.25F, 1.0F})
    {
        EyeSpring spring;
        spring.Snap(0.0F);
        for (int i = 0; i < 40; ++i)
        {
            spring.Update(1.0F, dt, true);
            ASSERT_TRUE(std::isfinite(spring.Height())) << "dt = " << dt;
            ASSERT_LE(spring.Height(), 1.0F + 1e-4F) << "dt = " << dt << " overshot";
            ASSERT_GE(spring.Height(), -1e-4F) << "dt = " << dt;
        }
        EXPECT_NEAR(spring.Height(), 1.0F, 1e-2F) << "dt = " << dt << " never arrived";
    }
}

TEST(EyeSpringTests, AMovingTargetIsFollowedWithoutRunningAway)
{
    // What a staircase actually looks like to this: a target that rises a riser at a time, 120
    // times a second, for a couple of seconds. The eye must stay behind it and catch up, never
    // pass it, and never fall further behind than the spring's own lag.
    EyeSpring spring;
    spring.Snap(0.0F);
    float target = 0.0F;
    float worstLag = 0.0F;
    for (int i = 0; i < 360; ++i)
    {
        target += 0.1794F * kDt / 0.28F * 1.35F * 0.72F; // §12.4's riser over its going, at stair pace
        const float height = spring.Update(target, kDt, true);
        EXPECT_LE(height, target + 1e-4F) << "the eye passed the stair";
        worstLag = std::max(worstLag, target - height);
    }
    // A critically damped spring following a ramp settles into a constant lag of `2·v/ω`.
    const float speed = 0.1794F / 0.28F * 1.35F * 0.72F;
    EXPECT_NEAR(worstLag, 2.0F * speed / kEyeSpringStairsOmega, 0.01F);
}
