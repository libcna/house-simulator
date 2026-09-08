// SPDX-License-Identifier: MIT
//
// `HOUSE-00630`. The frustum §25's visibility system asks everything against, built from the
// camera that draws the frame.
//
// `HOUSE-00104` already probed `BoundingFrustum` itself against analytic answers -- 24 of them,
// including the tangent sphere and the face-touching box that an epsilon error flips. What is
// being asserted here is the other half: that the frustum this camera hands out is the one its
// OWN view and projection describe, that it moves when the camera does, and that it is taken
// after §44's pull-back rather than before it.
#include <cmath>
#include <numbers>

#include <gtest/gtest.h>

#include "Microsoft/Xna/Framework/BoundingBox.hpp"
#include "Microsoft/Xna/Framework/ContainmentType.hpp"

#include "cnahouse/player/FirstPersonCamera.hpp"

namespace
{
    using cnahouse::player::ClearanceForProbe;
    using cnahouse::player::FirstPersonCamera;
    using cnahouse::player::kDefaultFovDegrees;
    using cnahouse::player::kFarPlane;
    using cnahouse::player::kNearPlane;
    using cnahouse::player::kNearPlaneClose;
    using cnahouse::player::kPlayerEyeHeight;
    using cnahouse::player::PlayerState;
    using Microsoft::Xna::Framework::BoundingBox;
    using Microsoft::Xna::Framework::ContainmentType;
    using Microsoft::Xna::Framework::Vector3;

    constexpr float kPi = std::numbers::pi_v<float>;

    /// A camera at the origin's eye height, looking north (-Z) along §14's yaw 0.
    FirstPersonCamera LookingNorth(float pitch = 0.0F)
    {
        PlayerState state;
        state.position = Vector3(0.0F, 0.90F, 0.0F);
        state.yaw = 0.0F;
        FirstPersonCamera camera;
        camera.SetAspect(16.0F / 9.0F);
        camera.Update(state, kPlayerEyeHeight, pitch);
        return camera;
    }

    bool Sees(const FirstPersonCamera& camera, const Vector3& point)
    {
        return camera.Frustum().Contains(point) != ContainmentType::Disjoint;
    }

} // namespace

TEST(CameraFrustumTests, WhatIsInFrontIsInsideAndWhatIsBehindIsNot)
{
    const FirstPersonCamera camera = LookingNorth();
    const Vector3 eye = camera.Pose().eye;

    EXPECT_TRUE(Sees(camera, Vector3(eye.X, eye.Y, eye.Z - 5.0F))) << "a point straight ahead";
    EXPECT_FALSE(Sees(camera, Vector3(eye.X, eye.Y, eye.Z + 5.0F))) << "a point behind the head";
    EXPECT_FALSE(Sees(camera, eye)) << "the eye is not inside its own near plane";
}

TEST(CameraFrustumTests, TheClipPlanesAreWhereTheProjectionPutThem)
{
    const FirstPersonCamera camera = LookingNorth();
    const Vector3 eye = camera.Pose().eye;
    constexpr float kEpsilon = 1e-3F;

    // §10.3's 0.10 m and 420 m, measured along the view rather than read off the matrix.
    EXPECT_FALSE(Sees(camera, Vector3(eye.X, eye.Y, eye.Z - kNearPlane + kEpsilon)));
    EXPECT_TRUE(Sees(camera, Vector3(eye.X, eye.Y, eye.Z - kNearPlane - kEpsilon)));
    // 50 mm either side of the far plane rather than one, because a plane extracted from a
    // matrix that spans 420 m is only good to about a part in 10^6 -- which is 0.4 mm out there,
    // and single precision has nothing finer to offer. It is 4 200 times the near plane away;
    // that is the price of §10.3's range and not an error in the frustum.
    constexpr float kFarEpsilon = 0.05F;
    EXPECT_TRUE(Sees(camera, Vector3(eye.X, eye.Y, eye.Z - kFarPlane + kFarEpsilon)));
    EXPECT_FALSE(Sees(camera, Vector3(eye.X, eye.Y, eye.Z - kFarPlane - kFarEpsilon)));
}

TEST(CameraFrustumTests, TheSidePlanesAreTheFieldOfViewSeenAsGeometry)
{
    const FirstPersonCamera camera = LookingNorth();
    const Vector3 eye = camera.Pose().eye;

    // At 10 m ahead the top of the frame is `10·tan(fovY/2)` up, and the side is that times the
    // aspect -- §44's two numbers, asked of the planes instead of of the matrix.
    const float half = kDefaultFovDegrees * 0.5F * kPi / 180.0F;
    const float top = 10.0F * std::tan(half);
    const float side = top * 16.0F / 9.0F;

    EXPECT_TRUE(Sees(camera, Vector3(eye.X, eye.Y + top * 0.99F, eye.Z - 10.0F)));
    EXPECT_FALSE(Sees(camera, Vector3(eye.X, eye.Y + top * 1.01F, eye.Z - 10.0F)));
    EXPECT_TRUE(Sees(camera, Vector3(eye.X + side * 0.99F, eye.Y, eye.Z - 10.0F)));
    EXPECT_FALSE(Sees(camera, Vector3(eye.X + side * 1.01F, eye.Y, eye.Z - 10.0F)));

    // ...and a narrower lens really does cut the same point out. The frustum is ASKED FOR first,
    // so the cache is full when the setting changes: a camera nobody has asked yet would answer
    // correctly however badly the invalidation was written.
    FirstPersonCamera narrow = LookingNorth();
    ASSERT_TRUE(Sees(narrow, Vector3(eye.X, eye.Y + top * 0.99F, eye.Z - 10.0F)));
    narrow.SetFieldOfView(55.0F);
    EXPECT_FALSE(Sees(narrow, Vector3(eye.X, eye.Y + top * 0.99F, eye.Z - 10.0F)))
        << "the frustum did not notice the field of view changing";

    // The same for the window shape, which moves the SIDE planes and nothing else: at 1:1 the
    // point that was just inside the edge at 16:9 is well outside it.
    FirstPersonCamera square = LookingNorth();
    ASSERT_TRUE(Sees(square, Vector3(eye.X + side * 0.99F, eye.Y, eye.Z - 10.0F)));
    square.SetAspect(1.0F);
    EXPECT_FALSE(Sees(square, Vector3(eye.X + side * 0.99F, eye.Y, eye.Z - 10.0F)))
        << "the frustum did not notice the window changing shape";
    EXPECT_TRUE(Sees(square, Vector3(eye.X, eye.Y + top * 0.99F, eye.Z - 10.0F)))
        << "a square window narrows the view sideways, not vertically";
}

TEST(CameraFrustumTests, TheFrustumFollowsTheCameraRatherThanTheOneItWasBuiltFrom)
{
    // The cache's whole risk: six planes kept from a frame the camera has since left.
    PlayerState state;
    state.position = Vector3(0.0F, 0.90F, 0.0F);
    state.yaw = 0.0F;

    FirstPersonCamera camera;
    camera.SetAspect(16.0F / 9.0F);
    camera.Update(state, kPlayerEyeHeight, 0.0F);
    const Vector3 ahead(0.0F, kPlayerEyeHeight, -5.0F);
    ASSERT_TRUE(Sees(camera, ahead));

    // Turned round: the same point is now behind the head.
    state.yaw = kPi;
    camera.Update(state, kPlayerEyeHeight, 0.0F);
    EXPECT_FALSE(Sees(camera, ahead)) << "the frustum was kept from before the turn";

    // Walked away, still facing south: the point is behind and stays out.
    state.position = Vector3(0.0F, 0.90F, 20.0F);
    camera.Update(state, kPlayerEyeHeight, 0.0F);
    EXPECT_FALSE(Sees(camera, ahead));

    // ...and turning back round finds it again at 25 m.
    state.yaw = 0.0F;
    camera.Update(state, kPlayerEyeHeight, 0.0F);
    EXPECT_TRUE(Sees(camera, ahead));

    // Asked twice with nothing moving, the same six planes come back.
    const Vector3 corner = camera.Frustum().GetCorners()[0];
    const Vector3 again = camera.Frustum().GetCorners()[0];
    EXPECT_FLOAT_EQ(corner.X, again.X);
    EXPECT_FLOAT_EQ(corner.Y, again.Y);
    EXPECT_FLOAT_EQ(corner.Z, again.Z);
}

TEST(CameraFrustumTests, TheFrustumIsTakenAfterTheNearSurfacePullBack)
{
    // §44's pull-back moves the eye back and the near plane in (`HOUSE-00628`), and a frustum
    // built before it would cull the sliver of wall the pull-back has just made visible.
    FirstPersonCamera camera = LookingNorth();
    const Vector3 eye = camera.Pose().eye;
    const Vector3 justInFront(eye.X, eye.Y, eye.Z - 0.07F);
    ASSERT_FALSE(Sees(camera, justInFront)) << "0.07 m is inside §10.3's 0.10 m near plane";

    camera.ApplyClearance(ClearanceForProbe(true, 0.0F));
    EXPECT_FLOAT_EQ(camera.NearPlane(), kNearPlaneClose);
    EXPECT_TRUE(Sees(camera, justInFront)) << "the frustum kept the near plane the pull-back moved";
}

TEST(CameraFrustumTests, ABoxStraddlingThePlanesIsIntersectingAndOneBehindIsDisjoint)
{
    // What §25.4 actually asks: cells and chunks are boxes, not points.
    const FirstPersonCamera camera = LookingNorth();
    const Vector3 eye = camera.Pose().eye;

    const BoundingBox ahead(Vector3(eye.X - 1.0F, eye.Y - 1.0F, eye.Z - 6.0F),
                            Vector3(eye.X + 1.0F, eye.Y + 1.0F, eye.Z - 4.0F));
    EXPECT_EQ(camera.Frustum().Contains(ahead), ContainmentType::Contains);
    EXPECT_TRUE(camera.Frustum().Intersects(ahead));

    const BoundingBox straddling(Vector3(eye.X - 40.0F, eye.Y - 40.0F, eye.Z - 6.0F),
                                 Vector3(eye.X + 40.0F, eye.Y + 40.0F, eye.Z - 4.0F));
    EXPECT_EQ(camera.Frustum().Contains(straddling), ContainmentType::Intersects);

    const BoundingBox behind(Vector3(eye.X - 1.0F, eye.Y - 1.0F, eye.Z + 4.0F),
                             Vector3(eye.X + 1.0F, eye.Y + 1.0F, eye.Z + 6.0F));
    EXPECT_EQ(camera.Frustum().Contains(behind), ContainmentType::Disjoint);
    EXPECT_FALSE(camera.Frustum().Intersects(behind));

    const BoundingBox beyondTheFar(Vector3(eye.X - 1.0F, eye.Y - 1.0F, eye.Z - kFarPlane - 20.0F),
                                   Vector3(eye.X + 1.0F, eye.Y + 1.0F, eye.Z - kFarPlane - 10.0F));
    EXPECT_EQ(camera.Frustum().Contains(beyondTheFar), ContainmentType::Disjoint);
}

TEST(CameraFrustumTests, LookingUpTakesTheFrustumWithIt)
{
    // The pitch is not in `PlayerState`, so this is the one part of the pose that reaches the
    // frustum by a different road from the rest of it.
    const FirstPersonCamera level = LookingNorth(0.0F);
    const FirstPersonCamera up = LookingNorth(1.0F);
    const Vector3 eye = level.Pose().eye;

    const Vector3 high(eye.X, eye.Y + 8.0F, eye.Z - 5.0F);
    EXPECT_FALSE(Sees(level, high));
    EXPECT_TRUE(Sees(up, high));

    const Vector3 straightOn(eye.X, eye.Y, eye.Z - 5.0F);
    EXPECT_TRUE(Sees(level, straightOn));
    EXPECT_FALSE(Sees(up, straightOn));
}
