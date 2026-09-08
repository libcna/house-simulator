// SPDX-License-Identifier: MIT
//
// `HOUSE-00621`. §44's first-person camera: the eye the controller carries, the view matrix it
// looks through and the projection §10.3's clip planes define.
//
// The interesting assertions are the ones about AGREEMENT. §44 gives a vertical field of view and
// then says "≈ 100° horizontal at 16:9"; §14 says yaw 0 looks north and positive turns east; §43.1
// puts the eye 1.68 m over the feet standing and 1.15 m crouched. Each of those is a number that
// exists somewhere else too, and a camera is where they meet.
#include <cmath>
#include <limits>
#include <numbers>

#include <gtest/gtest.h>

#include "Microsoft/Xna/Framework/Vector4.hpp"

#include "cnahouse/player/FirstPersonCamera.hpp"

namespace
{
    using cnahouse::player::CameraPose;
    using cnahouse::player::FirstPersonCamera;
    using cnahouse::player::kDefaultFovDegrees;
    using cnahouse::player::kFarPlane;
    using cnahouse::player::kMaxFovDegrees;
    using cnahouse::player::kMinFovDegrees;
    using cnahouse::player::kNearPlane;
    using cnahouse::player::kPlayerCrouchEyeHeight;
    using cnahouse::player::kPlayerEyeHeight;
    using cnahouse::player::PlayerState;
    using Microsoft::Xna::Framework::Matrix;
    using Microsoft::Xna::Framework::Vector3;
    using Microsoft::Xna::Framework::Vector4;

    constexpr float kPi = std::numbers::pi_v<float>;

    /// A point through a matrix, with the w a projection needs.
    Vector4 Through(const Matrix& matrix, const Vector3& point)
    {
        return Vector4(point.X * matrix.M11 + point.Y * matrix.M21 + point.Z * matrix.M31 + matrix.M41,
                       point.X * matrix.M12 + point.Y * matrix.M22 + point.Z * matrix.M32 + matrix.M42,
                       point.X * matrix.M13 + point.Y * matrix.M23 + point.Z * matrix.M33 + matrix.M43,
                       point.X * matrix.M14 + point.Y * matrix.M24 + point.Z * matrix.M34 + matrix.M44);
    }

    PlayerState Standing(float x, float y, float z, float yaw = 0.0F)
    {
        PlayerState state;
        state.position = Vector3(x, y, z);
        state.yaw = yaw;
        return state;
    }

} // namespace

TEST(FirstPersonCameraTests, TheNumbersAreTheOnesTheDesignStates)
{
    // §44's field of view and §10.3's clip planes, asserted where they are used rather than
    // trusted to stay right in two places.
    EXPECT_FLOAT_EQ(kDefaultFovDegrees, 70.0F);
    EXPECT_FLOAT_EQ(kMinFovDegrees, 55.0F);
    EXPECT_FLOAT_EQ(kMaxFovDegrees, 95.0F);
    EXPECT_FLOAT_EQ(kNearPlane, 0.10F);
    EXPECT_FLOAT_EQ(kFarPlane, 420.0F);

    FirstPersonCamera camera;
    EXPECT_FLOAT_EQ(camera.FieldOfViewDegrees(), kDefaultFovDegrees);
    camera.SetFieldOfView(200.0F);
    EXPECT_FLOAT_EQ(camera.FieldOfViewDegrees(), kMaxFovDegrees) << "the settings band is not enforced";
    camera.SetFieldOfView(1.0F);
    EXPECT_FLOAT_EQ(camera.FieldOfViewDegrees(), kMinFovDegrees);
    camera.SetFieldOfView(90.0F);
    EXPECT_FLOAT_EQ(camera.FieldOfViewDegrees(), 90.0F);
}

TEST(FirstPersonCameraTests, SeventyDegreesVerticalIsAHundredHorizontalAtSixteenByNine)
{
    // §44 states both numbers and only one of them is a decision: `tan(h/2) = aspect · tan(v/2)`.
    // If somebody tunes the vertical figure this says what the other sentence now reads.
    //
    // It is 102.4°, and §44 said "≈ 100°" until this test worked it out -- 2.4° of daylight
    // between a stated number and its own arithmetic, which is the sort of thing that gets
    // transcribed into a settings tooltip and then into a bug report about the field of view.
    FirstPersonCamera camera;
    camera.SetAspect(16.0F / 9.0F);
    const float vertical = kDefaultFovDegrees * kPi / 180.0F;
    const float horizontal = 2.0F * std::atan((16.0F / 9.0F) * std::tan(vertical * 0.5F));
    EXPECT_NEAR(horizontal * 180.0F / kPi, 102.4F, 0.1F);
}

TEST(FirstPersonCameraTests, TheEyeIsOverTheFEETAndNotTheCapsulesMiddle)
{
    // §43.1's 1.68 m standing and 1.15 m crouched are heights above the SOLES, and a crouch drops
    // the body's centre by exactly what its half-height loses. Measuring from the centre would
    // make the crouched eye 0.275 m too high -- the difference between looking at a rafter and
    // looking through it.
    FirstPersonCamera camera;
    PlayerState state = Standing(1.0F, 0.90F + 0.002F, -2.0F);
    camera.Update(state, kPlayerEyeHeight, 0.0F);
    EXPECT_NEAR(camera.Pose().eye.Y, 0.002F + kPlayerEyeHeight, 1e-5F);
    EXPECT_FLOAT_EQ(camera.Pose().eye.X, 1.0F);
    EXPECT_FLOAT_EQ(camera.Pose().eye.Z, -2.0F);

    // Crouched, the same body: the feet have not moved, so the eye is exactly §43.1's crouched
    // height above them.
    state.crouched = true;
    state.position = Vector3(1.0F, 0.625F + 0.002F, -2.0F);
    camera.Update(state, kPlayerCrouchEyeHeight, 0.0F);
    EXPECT_NEAR(camera.Pose().eye.Y, 0.002F + kPlayerCrouchEyeHeight, 1e-5F);
}

TEST(FirstPersonCameraTests, YawZeroLooksNorthAndPositiveTurnsEast)
{
    // §14, the convention the whole project is built on.
    FirstPersonCamera camera;
    camera.Update(Standing(0.0F, 0.90F, 0.0F, 0.0F), kPlayerEyeHeight, 0.0F);
    EXPECT_NEAR(camera.Pose().forward.X, 0.0F, 1e-6F);
    EXPECT_NEAR(camera.Pose().forward.Z, -1.0F, 1e-6F) << "yaw 0 does not look north";
    EXPECT_NEAR(camera.Pose().right.X, 1.0F, 1e-6F);

    camera.Update(Standing(0.0F, 0.90F, 0.0F, kPi * 0.5F), kPlayerEyeHeight, 0.0F);
    EXPECT_NEAR(camera.Pose().forward.X, 1.0F, 1e-6F) << "a quarter turn does not face east";
    EXPECT_NEAR(camera.Pose().forward.Z, 0.0F, 1e-6F);
    EXPECT_NEAR(camera.Pose().right.Z, 1.0F, 1e-6F);
}

TEST(FirstPersonCameraTests, RollIsZeroAtEveryPitchIncludingStraightUp)
{
    // §44: *"Roll is always zero"*. Built rather than asserted: the right vector comes from the
    // yaw alone, so it is horizontal whatever the pitch is -- which is also what makes a pitch of
    // exactly ±90° an ordinary case instead of the one where `CreateLookAt` divides by zero.
    FirstPersonCamera camera;
    for (const float pitch : {-kPi * 0.5F, -1.0F, -0.3F, 0.0F, 0.3F, 1.0F, kPi * 0.5F})
    {
        camera.Update(Standing(0.0F, 0.90F, 0.0F, 0.7F), kPlayerEyeHeight, pitch);
        const CameraPose& pose = camera.Pose();
        EXPECT_NEAR(pose.right.Y, 0.0F, 1e-6F) << "the horizon tilted at pitch " << pitch;
        EXPECT_NEAR(pose.forward.Y, std::sin(pitch), 1e-6F);

        // An orthonormal basis, still, at the poles.
        EXPECT_NEAR(pose.forward.X * pose.right.X + pose.forward.Y * pose.right.Y +
                        pose.forward.Z * pose.right.Z,
                    0.0F,
                    1e-6F);
        const float upLength =
            std::sqrt(pose.up.X * pose.up.X + pose.up.Y * pose.up.Y + pose.up.Z * pose.up.Z);
        EXPECT_NEAR(upLength, 1.0F, 1e-5F);
    }
}

TEST(FirstPersonCameraTests, TheViewMatrixPutsTheEyeAtTheOriginLookingDownNegativeZ)
{
    // XNA's view space is right-handed and looks down -Z, which is what every projection in the
    // framework assumes. A camera that got the handedness wrong renders the world inside out and
    // nothing else in the project would say so.
    FirstPersonCamera camera;
    camera.Update(Standing(3.0F, 0.90F, -5.0F, 0.9F), kPlayerEyeHeight, 0.2F);
    const Matrix view = camera.View();
    const CameraPose& pose = camera.Pose();

    const Vector4 eye = Through(view, pose.eye);
    EXPECT_NEAR(eye.X, 0.0F, 1e-4F);
    EXPECT_NEAR(eye.Y, 0.0F, 1e-4F);
    EXPECT_NEAR(eye.Z, 0.0F, 1e-4F);

    const Vector3 ahead(pose.eye.X + pose.forward.X * 4.0F,
                        pose.eye.Y + pose.forward.Y * 4.0F,
                        pose.eye.Z + pose.forward.Z * 4.0F);
    const Vector4 seen = Through(view, ahead);
    EXPECT_NEAR(seen.X, 0.0F, 1e-4F);
    EXPECT_NEAR(seen.Y, 0.0F, 1e-4F);
    EXPECT_NEAR(seen.Z, -4.0F, 1e-4F) << "four metres ahead is not four metres down -Z";

    // ...and something to the right is at +X in view space.
    const Vector3 beside(pose.eye.X + pose.right.X, pose.eye.Y + pose.right.Y, pose.eye.Z + pose.right.Z);
    EXPECT_NEAR(Through(view, beside).X, 1.0F, 1e-4F);
}

TEST(FirstPersonCameraTests, TheProjectionPutsTheClipPlanesWhereTheDesignSaid)
{
    FirstPersonCamera camera;
    camera.SetAspect(16.0F / 9.0F);
    camera.Update(Standing(0.0F, 0.90F, 0.0F), kPlayerEyeHeight, 0.0F);
    const Matrix projection = camera.Projection();

    // A point ON the near plane lands on the near clip face (z/w == 0 in XNA's [0,1] range), and
    // one on the far plane lands on the far face.
    const Vector4 near = Through(projection, Vector3(0.0F, 0.0F, -kNearPlane));
    EXPECT_NEAR(near.Z / near.W, 0.0F, 1e-4F);
    const Vector4 far = Through(projection, Vector3(0.0F, 0.0F, -kFarPlane));
    EXPECT_NEAR(far.Z / far.W, 1.0F, 1e-4F);

    // ...and the field of view is the angle at the top of the screen: at 10 m ahead, a point
    // `10·tan(35°)` up is exactly at the top edge.
    const float half = kDefaultFovDegrees * 0.5F * kPi / 180.0F;
    const Vector4 top = Through(projection, Vector3(0.0F, 10.0F * std::tan(half), -10.0F));
    EXPECT_NEAR(top.Y / top.W, 1.0F, 1e-4F);
    // The same angle sideways is WIDER by the aspect, which is the other half of §44's sentence.
    const Vector4 side = Through(projection, Vector3(10.0F * std::tan(half), 0.0F, -10.0F));
    EXPECT_NEAR(side.X / side.W, 9.0F / 16.0F, 1e-4F);
}

TEST(FirstPersonCameraTests, ANonsenseAspectIsIgnoredRatherThanMultipliedOut)
{
    // A window minimised to nothing reports a height of 0. A projection built from that is a
    // matrix of infinities, and every vertex the renderer transforms by it afterwards is one too.
    FirstPersonCamera camera;
    const float sane = camera.Aspect();
    camera.SetAspect(0.0F);
    EXPECT_FLOAT_EQ(camera.Aspect(), sane);
    camera.SetAspect(-2.0F);
    EXPECT_FLOAT_EQ(camera.Aspect(), sane);
    camera.SetAspect(std::numeric_limits<float>::quiet_NaN());
    EXPECT_FLOAT_EQ(camera.Aspect(), sane);
    camera.SetAspect(4.0F / 3.0F);
    EXPECT_FLOAT_EQ(camera.Aspect(), 4.0F / 3.0F);
}
