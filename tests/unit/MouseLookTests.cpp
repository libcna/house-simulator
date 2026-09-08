// SPDX-License-Identifier: MIT
//
// `HOUSE-00622`. §44's mouse look, the half of it that is not `IInputSource`'s: the pixels, the
// 0.0022 rad/px, the invert-Y and the recentring all belong to `HOUSE-00140`'s source and are
// tested there. What is here is what the angles do with the radians it hands over.
#include <cmath>
#include <numbers>

#include <gtest/gtest.h>

#include "cnahouse/player/FirstPersonCamera.hpp"
#include "cnahouse/player/MouseLook.hpp"

namespace
{
    using cnahouse::player::ApplyLook;
    using cnahouse::player::ClampedPitch;
    using cnahouse::player::InputState;
    using cnahouse::player::kMaxPitchDegrees;
    using cnahouse::player::LookAngles;

    constexpr float kPi = std::numbers::pi_v<float>;

    InputState Look(float x, float y)
    {
        InputState input;
        input.look.X = x;
        input.look.Y = y;
        return input;
    }

} // namespace

TEST(MouseLookTests, RightTurnsEastAndForwardLooksUp)
{
    // §14's convention and §44's: the mouse to the right turns towards east, which is positive
    // yaw, and the mouse away from the player looks up, which is positive pitch. Screen Y grows
    // DOWNWARD, so the second one is a subtraction and the sign is the whole test.
    LookAngles angles;
    ApplyLook(angles, Look(0.25F, 0.0F), true);
    EXPECT_FLOAT_EQ(angles.yaw, 0.25F);
    EXPECT_FLOAT_EQ(angles.pitch, 0.0F);

    ApplyLook(angles, Look(0.0F, -0.10F), true);
    EXPECT_GT(angles.pitch, 0.0F) << "the mouse forward looked DOWN";
    EXPECT_FLOAT_EQ(angles.pitch, 0.10F);

    ApplyLook(angles, Look(0.0F, 0.30F), true);
    EXPECT_FLOAT_EQ(angles.pitch, -0.20F);
}

TEST(MouseLookTests, TheSensitivityIsNotAppliedTwice)
{
    // `InputState::look` is in RADIANS -- the source has already spent the pixels, the setting and
    // the invert -- so this must pass them through unchanged. A multiplier here would be the same
    // setting twice and the second one would be the one nobody could find.
    LookAngles angles;
    ApplyLook(angles, Look(0.0022F, 0.0F), true);
    EXPECT_FLOAT_EQ(angles.yaw, 0.0022F) << "one pixel of motion moved the view by something else";
}

TEST(MouseLookTests, LookThatIsNotAvailableMovesNothing)
{
    // `HOUSE-00100`: an unfocused window and a still hand are indistinguishable from here, and
    // neither should move the view. The angles must not drift while a menu is open.
    LookAngles angles{0.7F, -0.2F};
    ApplyLook(angles, Look(1.0F, 1.0F), false);
    EXPECT_FLOAT_EQ(angles.yaw, 0.7F);
    EXPECT_FLOAT_EQ(angles.pitch, -0.2F);
}

TEST(MouseLookTests, TheYawWrapsAndStaysWhereItCanBeCompared)
{
    // A `float` that reaches 10 000 rad has lost a thousandth of a radian of precision, and a
    // player who spins for an hour would then face a slightly different way from one who did not
    // -- in the same house, from the same save file.
    LookAngles angles;
    for (int turn = 0; turn < 200; ++turn)
    {
        ApplyLook(angles, Look(kPi * 0.5F, 0.0F), true);
        EXPECT_LE(angles.yaw, kPi + 1e-5F);
        EXPECT_GT(angles.yaw, -kPi - 1e-5F);
    }
    // Fifty full turns later, facing exactly where it started, to the last bit a float has.
    EXPECT_NEAR(std::fabs(angles.yaw), 0.0F, 1e-4F);

    // ...and the wrap keeps the two ways round the compass the same number.
    LookAngles west;
    ApplyLook(west, Look(-kPi, 0.0F), true);
    LookAngles east;
    ApplyLook(east, Look(kPi, 0.0F), true);
    EXPECT_NEAR(std::fabs(west.yaw), kPi, 1e-5F);
    EXPECT_NEAR(std::fabs(east.yaw), kPi, 1e-5F);
    EXPECT_FLOAT_EQ(west.yaw, east.yaw) << "north-by-west and north-by-east are different numbers";
}

TEST(MouseLookTests, ThePitchStopsFiveDegreesShortOfThePoleAndNeverWraps)
{
    // `HOUSE-00623`, §44: *"Pitch clamped to ±85°"*. Short of the pole and not at it -- at exactly
    // ±90° the view has no horizon to level against and the smallest yaw becomes a spin. And
    // CLAMPED, never wrapped: wrapping would put a player who looked all the way up back at the
    // floor, which is a different game.
    const float limit = kMaxPitchDegrees * kPi / 180.0F;
    LookAngles angles;
    for (int push = 0; push < 20; ++push)
    {
        ApplyLook(angles, Look(0.0F, -kPi * 0.25F), true);
        EXPECT_LE(angles.pitch, limit + 1e-6F);
    }
    EXPECT_NEAR(angles.pitch, limit, 1e-6F) << "it did not reach the limit at all";
    EXPECT_LT(angles.pitch, kPi * 0.5F) << "it reached the pole";

    for (int push = 0; push < 40; ++push)
    {
        ApplyLook(angles, Look(0.0F, kPi * 0.25F), true);
        EXPECT_GE(angles.pitch, -limit - 1e-6F);
    }
    EXPECT_NEAR(angles.pitch, -limit, 1e-6F);

    // The view comes back the INSTANT the mouse comes back: a clamp applied where the pitch is
    // read instead of where it changes lets the stored angle run past the pole while the mouse is
    // pushed, and the player then takes the same distance back before anything moves.
    ApplyLook(angles, Look(0.0F, -0.02F), true);
    EXPECT_NEAR(angles.pitch, -limit + 0.02F, 1e-6F);
}

TEST(MouseLookTests, TheClampIsTheOneNumberTheDesignStates)
{
    EXPECT_FLOAT_EQ(kMaxPitchDegrees, 85.0F);
    const float limit = kMaxPitchDegrees * kPi / 180.0F;
    EXPECT_FLOAT_EQ(ClampedPitch(0.0F), 0.0F);
    EXPECT_FLOAT_EQ(ClampedPitch(limit * 0.5F), limit * 0.5F);
    EXPECT_FLOAT_EQ(ClampedPitch(kPi), limit);
    EXPECT_FLOAT_EQ(ClampedPitch(-kPi), -limit);
}
