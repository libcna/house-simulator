// SPDX-License-Identifier: MIT
//
// `HOUSE-00622`. §44's mouse look, the half of it that is not `IInputSource`'s: the pixels, the
// 0.0022 rad/px, the invert-Y and the recentring all belong to `HOUSE-00140`'s source and are
// tested there. What is here is what the angles do with the radians it hands over.
#include <cmath>
#include <numbers>

#include <gtest/gtest.h>

#include "cnahouse/player/MouseLook.hpp"

namespace
{
    using cnahouse::player::ApplyLook;
    using cnahouse::player::InputState;
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

TEST(MouseLookTests, ThePitchIsNotWrapped)
{
    // Wrapping the pitch would put a player who looked all the way up back at the floor, which is
    // what §44's ±85° clamp (`HOUSE-00623`) exists to prevent -- and a wrap here would hide the
    // absence of that clamp rather than leaving it visible.
    LookAngles angles;
    ApplyLook(angles, Look(0.0F, -kPi), true);
    EXPECT_NEAR(angles.pitch, kPi, 1e-5F);
    // ...and past the pole, where a wrap would put the view back at the horizon facing the other
    // way instead of leaving it absurdly far up, which is what the clamp is there to notice.
    ApplyLook(angles, Look(0.0F, -kPi), true);
    EXPECT_NEAR(angles.pitch, 2.0F * kPi, 1e-4F);
}
