// SPDX-License-Identifier: MIT
//
// `HOUSE-00476`. The debug camera, asked the questions a person flying about could only answer by
// noticing something felt wrong. It reads `player::InputState`, which is intent rather than keys,
// so every one of these runs without a window, a device or a person.
#include <cmath>

#include <gtest/gtest.h>

#include "cnahouse/debug/FreeFlyCamera.hpp"

namespace
{
    using cnahouse::debug::FreeFlyCamera;
    using cnahouse::player::InputState;
    using Microsoft::Xna::Framework::Vector2;
    using Microsoft::Xna::Framework::Vector3;

    InputState Still()
    {
        return InputState{};
    }

    float Length(const Vector3& v)
    {
        return std::sqrt(v.X * v.X + v.Y * v.Y + v.Z * v.Z);
    }

} // namespace

TEST(FreeFlyCameraTests, ItStartsLookingNorth)
{
    // §14: yaw 0 faces north, which is −Z. A camera whose zero faced +Z would put every angle in
    // this file 180 degrees out and still look self-consistent.
    const FreeFlyCamera camera;
    const Vector3 forward = camera.Forward();
    EXPECT_NEAR(forward.X, 0.0f, 1e-6f);
    EXPECT_NEAR(forward.Y, 0.0f, 1e-6f);
    EXPECT_NEAR(forward.Z, -1.0f, 1e-6f);
    EXPECT_NEAR(Length(forward), 1.0f, 1e-6f);
}

TEST(FreeFlyCameraTests, PositiveYawTurnsEast)
{
    // §14 again: "positive yaw turns east (clockwise seen from above)". East is +X.
    FreeFlyCamera camera;
    InputState input = Still();
    input.look = Vector2(1.5707963f, 0.0f); // a quarter turn
    camera.Update(input, true, 1.0f / 60.0f);
    const Vector3 forward = camera.Forward();
    EXPECT_NEAR(forward.X, 1.0f, 1e-5f);
    EXPECT_NEAR(forward.Z, 0.0f, 1e-5f);
}

TEST(FreeFlyCameraTests, PitchIsClampedShortOfStraightUp)
{
    FreeFlyCamera camera;
    InputState input = Still();
    input.look = Vector2(0.0f, 10.0f);
    camera.Update(input, true, 1.0f / 60.0f);
    EXPECT_FLOAT_EQ(camera.Pitch(), FreeFlyCamera::kPitchLimit);
    input.look = Vector2(0.0f, -100.0f);
    camera.Update(input, true, 1.0f / 60.0f);
    EXPECT_FLOAT_EQ(camera.Pitch(), -FreeFlyCamera::kPitchLimit);
    // ...and the forward vector is still a unit vector at the limit, which is the thing the clamp
    // is protecting: a camera looking exactly at the pole has no horizon and starts to roll.
    EXPECT_NEAR(Length(camera.Forward()), 1.0f, 1e-6f);
}

TEST(FreeFlyCameraTests, YawWrapsRatherThanGrowing)
{
    FreeFlyCamera camera;
    InputState input = Still();
    input.look = Vector2(1.0f, 0.0f);
    for (int turn = 0; turn < 100; ++turn)
    {
        camera.Update(input, true, 1.0f / 60.0f);
    }
    EXPECT_LE(std::fabs(camera.Yaw()), 3.1415927f + 1e-5f)
        << "yaw grew without bound: after an hour of turning it would lose its precision";
}

TEST(FreeFlyCameraTests, LookIsIgnoredWhenTheWindowCannotProvideIt)
{
    // `HOUSE-00100` measured `Game::IsActive` true on all 9 999 frames while the mouse snapshot
    // never advanced. A camera that integrated the delta anyway would spin on an unfocused window.
    FreeFlyCamera camera;
    InputState input = Still();
    input.look = Vector2(1.0f, 0.5f);
    camera.Update(input, false, 1.0f / 60.0f);
    EXPECT_FLOAT_EQ(camera.Yaw(), 0.0f);
    EXPECT_FLOAT_EQ(camera.Pitch(), 0.0f);
}

TEST(FreeFlyCameraTests, ForwardMovementFollowsTheView)
{
    // Along the VIEW, not along the ground: this camera is for getting to the ridge of a roof, and
    // one that only moved horizontally would need a separate control to climb.
    FreeFlyCamera camera;
    InputState input = Still();
    input.look = Vector2(0.0f, 0.7853982f); // 45 degrees up
    camera.Update(input, true, 1.0f / 60.0f);
    input.look = Vector2(0.0f, 0.0f);
    input.move = Vector2(0.0f, 1.0f);
    camera.Update(input, true, 1.0f);
    // 1.7 is where the camera starts, which is eye height and is what makes it a camera rather
    // than a point at the origin.
    EXPECT_NEAR(camera.Position().Y, 1.7f + FreeFlyCamera::kSpeed * 0.70710678f, 1e-4f);
    EXPECT_NEAR(camera.Position().Z, -FreeFlyCamera::kSpeed * 0.70710678f, 1e-4f);
}

TEST(FreeFlyCameraTests, StrafeStaysLevelHoweverFarTheViewIsPitched)
{
    // The one thing a camera used to inspect a wall must do is hold its height while it slides
    // along one. A right vector that tilted with the view would climb.
    FreeFlyCamera camera;
    InputState input = Still();
    input.look = Vector2(0.0f, 1.2f);
    camera.Update(input, true, 1.0f / 60.0f);
    input.look = Vector2(0.0f, 0.0f);
    input.move = Vector2(1.0f, 0.0f);
    camera.Update(input, true, 1.0f);
    EXPECT_NEAR(camera.Position().Y, 1.7f, 1e-6f) << "strafing changed the camera's height";
    EXPECT_NEAR(camera.Position().X, FreeFlyCamera::kSpeed, 1e-4f);
}

TEST(FreeFlyCameraTests, UpAndDownAreWorldUpAndDown)
{
    FreeFlyCamera camera;
    InputState input = Still();
    input.look = Vector2(2.0f, 1.0f);
    camera.Update(input, true, 1.0f / 60.0f);
    const Vector3 before = camera.Position();
    input.look = Vector2(0.0f, 0.0f);
    input.jump = true;
    camera.Update(input, true, 1.0f);
    EXPECT_NEAR(camera.Position().Y - before.Y, FreeFlyCamera::kSpeed, 1e-4f);
    EXPECT_NEAR(camera.Position().X, before.X, 1e-6f);
    EXPECT_NEAR(camera.Position().Z, before.Z, 1e-6f);

    input.jump = false;
    input.crouch = true;
    camera.Update(input, true, 1.0f);
    EXPECT_NEAR(camera.Position().Y, before.Y, 1e-4f);
}

TEST(FreeFlyCameraTests, RunMultipliesTheSpeedAndNothingElse)
{
    FreeFlyCamera walking;
    FreeFlyCamera running;
    InputState input = Still();
    input.move = Vector2(0.0f, 1.0f);
    walking.Update(input, true, 1.0f);
    input.run = true;
    running.Update(input, true, 1.0f);
    EXPECT_NEAR(running.Position().Z, walking.Position().Z * FreeFlyCamera::kRunMultiplier, 1e-4f);
    EXPECT_FLOAT_EQ(running.Yaw(), walking.Yaw());
}

TEST(FreeFlyCameraTests, MovementIsPerSecondAndNotPerFrame)
{
    // Two half-seconds must travel exactly as far as one second, or the camera's speed would
    // depend on the frame rate -- and this camera is used while looking at frame timings.
    FreeFlyCamera once;
    FreeFlyCamera twice;
    InputState input = Still();
    input.move = Vector2(0.0f, 1.0f);
    once.Update(input, true, 1.0f);
    twice.Update(input, true, 0.5f);
    twice.Update(input, true, 0.5f);
    EXPECT_NEAR(once.Position().Z, twice.Position().Z, 1e-5f);
}

TEST(FreeFlyCameraTests, AdoptingAFixedCameraDoesNotJumpTheView)
{
    // Taking over while looking at something is the whole point of being able to switch to this
    // camera, and a view that snapped to north on the way in would lose whatever you were looking
    // at.
    cnahouse::rendering::Camera fixed;
    fixed.eye = Vector3(17.0f, 14.0f, 17.0f);
    fixed.target = Vector3(-1.0f, 5.0f, -19.0f);

    FreeFlyCamera camera;
    camera.Adopt(fixed);
    EXPECT_FLOAT_EQ(camera.Position().X, 17.0f);
    EXPECT_FLOAT_EQ(camera.Position().Y, 14.0f);

    cnahouse::rendering::Camera applied;
    camera.ApplyTo(applied);
    const Vector3 wanted(
        fixed.target.X - fixed.eye.X, fixed.target.Y - fixed.eye.Y, fixed.target.Z - fixed.eye.Z);
    const float scale = Length(wanted);
    const Vector3 got(
        applied.target.X - applied.eye.X, applied.target.Y - applied.eye.Y, applied.target.Z - applied.eye.Z);
    EXPECT_NEAR(got.X, wanted.X / scale, 1e-5f);
    EXPECT_NEAR(got.Y, wanted.Y / scale, 1e-5f);
    EXPECT_NEAR(got.Z, wanted.Z / scale, 1e-5f);
    // ...and the lens is left alone: `Adopt` is about where you are, not what you see through.
    EXPECT_FLOAT_EQ(applied.fieldOfViewDegrees, cnahouse::rendering::Camera{}.fieldOfViewDegrees);
}

TEST(FreeFlyCameraTests, ACameraLookingAtItsOwnEyeKeepsThePoseItHad)
{
    cnahouse::rendering::Camera degenerate;
    degenerate.eye = Vector3(1.0f, 2.0f, 3.0f);
    degenerate.target = degenerate.eye;

    FreeFlyCamera camera;
    InputState input = Still();
    input.look = Vector2(0.9f, 0.3f);
    camera.Update(input, true, 1.0f / 60.0f);
    const float yaw = camera.Yaw();
    const float pitch = camera.Pitch();
    camera.Adopt(degenerate);
    EXPECT_FLOAT_EQ(camera.Yaw(), yaw) << "a target with no direction in it invented one";
    EXPECT_FLOAT_EQ(camera.Pitch(), pitch);
    EXPECT_FLOAT_EQ(camera.Position().X, 1.0f);
}
