// SPDX-License-Identifier: MIT
//
// `HOUSE-00632`. The assembly: §44's camera and the three things that move the eye inside it.
//
// The order is the content. The eye height is sprung (§44), the bob is added after the spring
// (§44 again -- through it, it would be a 1.8 Hz wobble fed into a filter built to remove one),
// the landing dip is a third term on the same height (§43.1), and the near-surface pull-back is
// last because it needs the view direction the rest of it produces. Each of those is tested where
// it lives; what is tested here is that putting them together did not change any of them.
#include <cmath>

#include <gtest/gtest.h>

#include "cnahouse/player/FirstPersonView.hpp"

namespace
{
    using cnahouse::physics::Landing;
    using cnahouse::player::FirstPersonView;
    using cnahouse::player::HeadBobLevel;
    using cnahouse::player::kBobStrideNormal;
    using cnahouse::player::kPlayerCrouchEyeHeight;
    using cnahouse::player::kPlayerEyeHeight;
    using cnahouse::player::kPlayerHalfHeight;
    using cnahouse::player::kPlayerRadius;
    using cnahouse::player::PlayerState;
    using cnahouse::player::PlayerStepReport;
    using Microsoft::Xna::Framework::Vector3;

    constexpr float kDt = 1.0F / 120.0F;
    constexpr float kRise = kPlayerHalfHeight + kPlayerRadius;

    PlayerState Standing(float x = 0.0F, float feetY = 0.0F, float z = 0.0F)
    {
        PlayerState state;
        state.position = Vector3(x, feetY + kRise, z);
        return state;
    }

} // namespace

TEST(FirstPersonViewTests, TheSpringChasesTheEyesWORLDHeightAndNotItsHeightAboveTheFeet)
{
    // Above the feet the eye height is a constant -- §43.1's 1.68 m -- and a spring chasing a
    // constant does nothing at all. What §44 asks for is that a step up does not jolt the view,
    // and a step up moves the FEET: the eye is left behind in world space and catches up.
    PlayerState state = Standing();
    FirstPersonView view;
    view.Snap(state);
    EXPECT_FLOAT_EQ(view.SprungHeight(), kPlayerEyeHeight);
    EXPECT_FLOAT_EQ(view.TargetHeight(), kPlayerEyeHeight);

    // §43.1's step-up limit, all at once, the way a kerb arrives.
    state.position = Vector3(0.0F, kRise + 0.22F, 0.0F);
    PlayerStepReport report;
    report.steppedUp = true;
    view.Update(state, report, 0.0F, kDt);

    EXPECT_FLOAT_EQ(view.TargetHeight(), 0.22F + kPlayerEyeHeight) << "the target did not follow the feet";
    EXPECT_LT(view.SprungHeight(), view.TargetHeight() - 0.20F) << "the eye jumped with the feet";
    // ...and the camera is handed the difference, because a camera takes a height above the feet.
    EXPECT_NEAR(view.Camera().Pose().eye.Y, view.SprungHeight(), 1e-5F);

    // A quarter of a second later -- §44's 4/omega -- it has covered nine tenths of the step, and
    // half a second later it is within five millimetres. (4/omega is where a critically damped
    // spring has done 1 - 5·e^-4 = 91 % of a step, not where it has finished: the last centimetre
    // of a 22 cm kerb takes as long again, and is the part nobody sees.)
    for (int frame = 0; frame < 30; ++frame)
    {
        view.Update(state, report, 0.0F, kDt);
    }
    EXPECT_GT(view.SprungHeight() - kPlayerEyeHeight, 0.9F * 0.22F);
    for (int frame = 0; frame < 30; ++frame)
    {
        view.Update(state, report, 0.0F, kDt);
    }
    EXPECT_NEAR(view.SprungHeight(), view.TargetHeight(), 0.005F);
    EXPECT_NEAR(view.Camera().Pose().eye.Y, 0.22F + kPlayerEyeHeight, 0.005F);
}

TEST(FirstPersonViewTests, TheBobAndTheDipAreAddedAfterTheSpringAndNotIntoIt)
{
    // Through the spring, §44's bob would be a 1.8 Hz wobble fed into a filter that is there to
    // remove one, and §43.1's landing dip a 62 ms impulse in the same place. Both are added to
    // what the spring produced.
    PlayerState state = Standing();
    FirstPersonView view;
    view.Snap(state);

    // Walk half a stride due north, so the bob is somewhere in the middle of its cycle.
    PlayerStepReport report;
    for (int frame = 0; frame < 60; ++frame)
    {
        state.position = Vector3(state.position.X, state.position.Y, state.position.Z - 1.35F * kDt);
        view.Update(state, report, 0.0F, kDt);
    }
    ASSERT_GT(view.BobOffset().vertical, 0.0F) << "the fixture produced no bob to look for";

    // The spring is exactly on its target -- the feet have not moved vertically -- and the eye is
    // that plus the bob. If the bob went in BEFORE, the spring would be somewhere else.
    EXPECT_NEAR(view.SprungHeight(), view.TargetHeight(), 1e-4F) << "the bob reached the spring";
    EXPECT_NEAR(view.Camera().Pose().eye.Y, view.SprungHeight() + view.BobOffset().vertical, 1e-5F);

    // ...and the same for the dip, which arrives on a landing.
    PlayerStepReport landed;
    landed.landing = Landing::Hard;
    landed.landingDrop = 3.0F;
    view.Update(state, landed, 0.0F, kDt);
    for (int frame = 0; frame < 10; ++frame)
    {
        view.Update(state, PlayerStepReport{}, 0.0F, kDt);
    }
    EXPECT_LT(view.DipOffset(), 0.0F);
    EXPECT_NEAR(view.SprungHeight(), view.TargetHeight(), 1e-4F) << "the dip reached the spring";
    EXPECT_NEAR(view.Camera().Pose().eye.Y,
                view.SprungHeight() + view.BobOffset().vertical + view.DipOffset(),
                1e-5F);
}

TEST(FirstPersonViewTests, TheStairsFlagReachesTheSpring)
{
    // §48.2: *"the eye-height spring is stiffened (ω = 24) on stairs so the view rises steadily
    // rather than bobbing per step"*. The same climb twice, and the stiff one arrives first.
    const auto climb = [](bool stairs)
    {
        PlayerState state = Standing();
        FirstPersonView view;
        view.Snap(state);
        PlayerStepReport report;
        report.stairs = stairs ? PlayerStepReport::Stairs::Up : PlayerStepReport::Stairs::None;

        state.position = Vector3(0.0F, kRise + 0.18F, 0.0F);
        for (int frame = 0; frame < 12; ++frame)
        {
            view.Update(state, report, 0.0F, kDt);
        }
        return view.TargetHeight() - view.SprungHeight();
    };

    EXPECT_LT(climb(true), climb(false)) << "§48.2's stiffening did not reach the spring";
}

TEST(FirstPersonViewTests, ASnapIsATeleportAndNotAWalk)
{
    // §57's teleport: the distance between one frame and the next is not a walk. Without the
    // snap, the whole distance across the house arrives as one frame's travel -- a hundred
    // footsteps in one tick, and an eye springing in from wherever the body used to be.
    PlayerState state = Standing(0.0F, 0.0F, 0.0F);
    FirstPersonView view;
    view.Snap(state);

    PlayerStepReport report;
    for (int frame = 0; frame < 200; ++frame)
    {
        state.position = Vector3(state.position.X, state.position.Y, state.position.Z - 1.35F * kDt);
        view.Update(state, report, 0.0F, kDt);
    }
    ASSERT_GT(view.Bob().StepPhase(), 0.0F);

    // Across the house and up two storeys.
    state.position = Vector3(20.0F, kRise + 6.55F, -30.0F);
    view.Snap(state);
    EXPECT_FLOAT_EQ(view.Bob().StepPhase(), 0.0F) << "the teleport was counted as strides";
    EXPECT_FLOAT_EQ(view.SprungHeight(), view.TargetHeight()) << "the eye sprang in from the old room";
    EXPECT_FLOAT_EQ(view.DipOffset(), 0.0F);
    EXPECT_FLOAT_EQ(view.BobOffset().vertical, 0.0F);
    EXPECT_FLOAT_EQ(view.Camera().Pose().eye.Y, kPlayerEyeHeight + 6.55F);

    // ...and the frame after a snap is one frame's travel, not the teleport's distance.
    state.position = Vector3(20.0F, kRise + 6.55F, -30.0F - 1.35F * kDt);
    view.Update(state, report, 0.0F, kDt);
    EXPECT_LT(view.Bob().StepPhase(), 0.05F);
}

TEST(FirstPersonViewTests, AViewThatWasNeverSnappedSnapsItself)
{
    // The other half of the same failure: a view whose first frame is an `Update` has no previous
    // position, and the travel would be the whole distance from the origin to wherever the body
    // happens to be -- which in this house is up to 40 m, or fifty footsteps in one tick.
    PlayerState state = Standing(12.0F, 3.65F, -18.0F);
    FirstPersonView view;
    view.Update(state, PlayerStepReport{}, 0.0F, kDt);

    EXPECT_FLOAT_EQ(view.Bob().StepPhase(), 0.0F);
    EXPECT_FLOAT_EQ(view.SprungHeight(), 3.65F + kPlayerEyeHeight);
    EXPECT_FLOAT_EQ(view.Camera().Pose().eye.X, 12.0F);
}

TEST(FirstPersonViewTests, ACrouchMovesTheTargetAndTheSpringTakesTheEyeDownSmoothly)
{
    // §43.1's crouch is the biggest single change the eye height ever makes: 1.68 m to 1.15 m,
    // and the body's centre drops by what its half-height loses so the FEET stay where they are.
    PlayerState state = Standing();
    FirstPersonView view;
    view.Snap(state);

    state.crouched = true;
    state.position = Vector3(0.0F, state.Rise(), 0.0F);
    PlayerStepReport report;
    report.crouchChanged = true;
    view.Update(state, report, 0.0F, kDt);

    EXPECT_FLOAT_EQ(view.TargetHeight(), kPlayerCrouchEyeHeight) << "the feet moved when the body crouched";
    EXPECT_GT(view.SprungHeight(), kPlayerCrouchEyeHeight + 0.40F) << "the eye dropped in one frame";

    for (int frame = 0; frame < 60; ++frame)
    {
        view.Update(state, PlayerStepReport{}, 0.0F, kDt);
    }
    EXPECT_NEAR(view.SprungHeight(), kPlayerCrouchEyeHeight, 0.01F);
}
