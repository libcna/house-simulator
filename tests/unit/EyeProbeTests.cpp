// SPDX-License-Identifier: MIT
//
// `HOUSE-00628`. §44's camera collision, which is one special case and not a spring arm:
// *"the eye is inside the player capsule, so walls are already handled. The only special case is a
// near-plane clip against a surface the capsule is touching -- solved by pulling the near plane to
// 0.05 m and pushing the eye 0.06 m back along the view direction when a 0.10 m forward probe
// hits."*
//
// The thing worth testing is not that the numbers are the numbers. It is that the response is
// CONTINUOUS -- it arrives as the player does and leaves as they step off -- and that it cannot
// push the eye out of the body it belongs to.
#include <cmath>
#include <numbers>
#include <vector>

#include <gtest/gtest.h>

#include "Microsoft/Xna/Framework/Vector4.hpp"

#include "cnahouse/physics/BroadPhase.hpp"
#include "cnahouse/player/EyeProbe.hpp"

namespace
{
    using cnahouse::physics::BroadPhase;
    using cnahouse::physics::CollisionCell;
    using cnahouse::physics::CollisionKind;
    using cnahouse::physics::CollisionObb;
    using cnahouse::physics::CollisionWorld;
    using cnahouse::player::CameraPose;
    using cnahouse::player::ClearanceForProbe;
    using cnahouse::player::EyeClearance;
    using cnahouse::player::FirstPersonCamera;
    using cnahouse::player::kEyeProbeDistance;
    using cnahouse::player::kEyePullBack;
    using cnahouse::player::kNearPlane;
    using cnahouse::player::kNearPlaneClose;
    using cnahouse::player::kPlayerEyeHeight;
    using cnahouse::player::kPlayerRadius;
    using cnahouse::player::PlayerState;
    using cnahouse::player::ProbeAhead;
    using Microsoft::Xna::Framework::BoundingBox;
    using Microsoft::Xna::Framework::Matrix;
    using Microsoft::Xna::Framework::Vector3;
    using Microsoft::Xna::Framework::Vector4;

    /// A wall standing in the z = @p z plane, 4 m wide and 4 m tall, 0.10 m thick.
    CollisionObb Wall(float z)
    {
        CollisionObb obb;
        obb.centre = Vector3(0.0f, 1.5f, z);
        obb.halfExtents = Vector3(2.0f, 1.5f, 0.05f);
        obb.kind = CollisionKind::Wall;
        return obb;
    }

    CollisionWorld OneCell(std::vector<CollisionObb> obbs)
    {
        CollisionWorld world;
        world.gridCell = 1.0f;
        world.surfaces = {"plaster"};
        world.obbs = std::move(obbs);

        CollisionCell cell;
        cell.id = "L0_TEST";
        cell.bounds = BoundingBox(Vector3(-8.0f, -8.0f, -8.0f), Vector3(8.0f, 8.0f, 8.0f));
        cell.nx = 16u;
        cell.nz = 16u;
        cell.originX = -8.0f;
        cell.originZ = -8.0f;
        cell.buckets.assign(static_cast<std::size_t>(cell.nx) * cell.nz, {});
        for (std::uint32_t i = 0; i < world.ShapeCount(); ++i)
        {
            cell.shapes.push_back(i);
            for (auto& bucket : cell.buckets)
            {
                bucket.push_back(static_cast<std::uint16_t>(i));
            }
        }
        world.cells = {cell};
        return world;
    }

    /// The camera of a body standing at @p z looking north (-Z), which is at the wall.
    CameraPose LookingNorthFrom(float z, float pitch = 0.0f)
    {
        PlayerState state;
        state.position = Vector3(0.0f, 0.90f, z);
        state.yaw = 0.0f;
        FirstPersonCamera camera;
        camera.Update(state, kPlayerEyeHeight, pitch);
        return camera.Pose();
    }

} // namespace

TEST(EyeProbeTests, TheNumbersAreTheOnesTheDesignStates)
{
    EXPECT_FLOAT_EQ(kEyeProbeDistance, 0.10F);
    EXPECT_FLOAT_EQ(kEyePullBack, 0.06F);
    EXPECT_FLOAT_EQ(kNearPlaneClose, 0.05F);

    // At the surface, §44's response exactly.
    const EyeClearance touching = ClearanceForProbe(true, 0.0F);
    EXPECT_FLOAT_EQ(touching.pullBack, kEyePullBack);
    EXPECT_FLOAT_EQ(touching.nearPlane, kNearPlaneClose);

    // A probe that met nothing costs nothing, and §10.3's near plane is what stands.
    const EyeClearance clear = ClearanceForProbe(false, 0.0F);
    EXPECT_FLOAT_EQ(clear.pullBack, 0.0F);
    EXPECT_FLOAT_EQ(clear.nearPlane, kNearPlane);
}

TEST(EyeProbeTests, TheResponseArrivesAsThePlayerDoesRatherThanAllAtOnce)
{
    // Applied as a switch this is 60 mm of eye travel and 50 mm of near plane in ONE frame, and
    // the frame a player's nose reaches a door is the frame they are looking at it hardest.
    const EyeClearance atReach = ClearanceForProbe(true, kEyeProbeDistance);
    EXPECT_FLOAT_EQ(atReach.pullBack, 0.0F) << "the probe's own reach is where the response starts";
    EXPECT_FLOAT_EQ(atReach.nearPlane, kNearPlane);

    const EyeClearance half = ClearanceForProbe(true, kEyeProbeDistance * 0.5F);
    EXPECT_FLOAT_EQ(half.pullBack, kEyePullBack * 0.5F);
    EXPECT_FLOAT_EQ(half.nearPlane, (kNearPlane + kNearPlaneClose) * 0.5F);

    // Monotone from the reach to the surface, with no step anywhere in it.
    float previousPullBack = -1.0F;
    float previousNear = 1.0F;
    for (int i = 10; i >= 0; --i)
    {
        const EyeClearance one = ClearanceForProbe(true, kEyeProbeDistance * static_cast<float>(i) / 10.0F);
        EXPECT_GE(one.pullBack, previousPullBack);
        EXPECT_LE(one.nearPlane, previousNear);
        EXPECT_LE(one.pullBack, kEyePullBack);
        EXPECT_GE(one.nearPlane, kNearPlaneClose);
        previousPullBack = one.pullBack;
        previousNear = one.nearPlane;
    }

    // A probe reporting a distance beyond its own reach is not a hit, and a negative one -- an
    // origin already inside the geometry -- is the full response and not more than it.
    EXPECT_FLOAT_EQ(ClearanceForProbe(true, 0.5F).pullBack, 0.0F);
    EXPECT_FLOAT_EQ(ClearanceForProbe(true, -0.2F).pullBack, kEyePullBack);
}

TEST(EyeProbeTests, TheEyeStepsBackAlongTheViewAndTheNearPlaneFollowsIt)
{
    PlayerState state;
    state.position = Vector3(1.0f, 0.90f, 2.0f);
    state.yaw = 0.0F;

    FirstPersonCamera camera;
    camera.Update(state, kPlayerEyeHeight, 0.0F);
    const Vector3 unpulled = camera.Pose().eye;
    EXPECT_FLOAT_EQ(camera.NearPlane(), kNearPlane);

    camera.ApplyClearance(ClearanceForProbe(true, 0.0F));
    // Looking north is -Z, so a step back along the view is +Z.
    EXPECT_FLOAT_EQ(camera.Pose().eye.Z, unpulled.Z + kEyePullBack);
    EXPECT_FLOAT_EQ(camera.Pose().eye.X, unpulled.X);
    EXPECT_FLOAT_EQ(camera.Pose().eye.Y, unpulled.Y);
    EXPECT_FLOAT_EQ(camera.NearPlane(), kNearPlaneClose);

    // Idempotent: the pull-back is measured from the eye `Update` left, so a second application
    // of the same clearance is the same eye and not 0.12 m of it.
    camera.ApplyClearance(ClearanceForProbe(true, 0.0F));
    EXPECT_FLOAT_EQ(camera.Pose().eye.Z, unpulled.Z + kEyePullBack);

    // Again on a view that is not down an axis, so that all three components have to be right and
    // not just the one a north-facing fixture happens to exercise.
    state.yaw = 2.1F;
    camera.Update(state, kPlayerEyeHeight, -0.6F);
    const Vector3 base = camera.Pose().eye;
    const Vector3 forward = camera.Pose().forward;
    ASSERT_GT(std::abs(forward.X), 0.1F);
    ASSERT_GT(std::abs(forward.Y), 0.1F);
    ASSERT_GT(std::abs(forward.Z), 0.1F);

    camera.ApplyClearance(ClearanceForProbe(true, 0.0F));
    EXPECT_NEAR(camera.Pose().eye.X, base.X - forward.X * kEyePullBack, 1e-6F);
    EXPECT_NEAR(camera.Pose().eye.Y, base.Y - forward.Y * kEyePullBack, 1e-6F);
    EXPECT_NEAR(camera.Pose().eye.Z, base.Z - forward.Z * kEyePullBack, 1e-6F);

    camera.ApplyClearance(ClearanceForProbe(true, 0.0F));
    EXPECT_NEAR(camera.Pose().eye.X, base.X - forward.X * kEyePullBack, 1e-6F) << "the pull-back accumulated";
    EXPECT_NEAR(camera.Pose().eye.Y, base.Y - forward.Y * kEyePullBack, 1e-6F);
    EXPECT_NEAR(camera.Pose().eye.Z, base.Z - forward.Z * kEyePullBack, 1e-6F);

    state.yaw = 0.0F;
    camera.Update(state, kPlayerEyeHeight, 0.0F);
    camera.ApplyClearance(ClearanceForProbe(true, 0.0F));

    // ...and the projection really uses the moved plane: a point 0.05 m ahead is ON the near face.
    const Matrix projection = camera.Projection();
    const Vector3 point(0.0F, 0.0F, -kNearPlaneClose);
    const float z = point.Z * projection.M33 + projection.M43;
    const float w = point.Z * projection.M34 + projection.M44;
    EXPECT_NEAR(z / w, 0.0F, 1e-4F);

    // The next frame starts clear again, so a player who stepped away gets §10.3's plane back
    // without anything having to notice they did.
    camera.Update(state, kPlayerEyeHeight, 0.0F);
    EXPECT_FLOAT_EQ(camera.NearPlane(), kNearPlane);
    EXPECT_FLOAT_EQ(camera.Pose().eye.Z, unpulled.Z);
}

TEST(EyeProbeTests, TheStepBackCannotPushTheEyeOutOfTheBody)
{
    // §44's whole argument is that *"the eye is inside the player capsule, so walls are already
    // handled"*. A pull-back that took the eye outside would hand the case back, and it is a
    // 0.06 m step against §43.1's 0.30 m radius -- worth asserting, because the two numbers are
    // written down in different sections and only their RATIO makes the argument work.
    PlayerState state;
    state.position = Vector3(0.0f, 0.90f, 0.0f);

    for (const float pitch : {-1.484F, -0.7F, 0.0F, 0.7F, 1.484F})
    {
        for (const float yaw : {0.0F, 1.1F, 3.0F, 5.5F})
        {
            state.yaw = yaw;
            FirstPersonCamera camera;
            camera.Update(state, kPlayerEyeHeight, pitch);
            camera.ApplyClearance(ClearanceForProbe(true, 0.0F));

            const Vector3 eye = camera.Pose().eye;
            const float fromAxis = std::sqrt((eye.X - state.position.X) * (eye.X - state.position.X) +
                                             (eye.Z - state.position.Z) * (eye.Z - state.position.Z));
            EXPECT_LT(fromAxis, kPlayerRadius) << "the eye left the capsule sideways";
            const float feet = state.Feet().Y;
            EXPECT_GT(eye.Y, feet) << "the eye went through the floor";
            EXPECT_LT(eye.Y, feet + 2.0F * (kPlayerEyeHeight - feet)) << "the eye went through the ceiling";
        }
    }
}

TEST(EyeProbeTests, AWallInFrontOfTheEyeIsWhatTheProbeFinds)
{
    // The probe against real geometry. The wall's face is at z = -3.05, so a body at z = -2.99 has
    // its eye 0.06 m from it -- inside §44's 0.10 m reach and not touching the wall, which is the
    // case the capsule cannot help with.
    CollisionWorld world = OneCell({Wall(-3.10f)});
    BroadPhase broad;

    const EyeClearance close = ProbeAhead(world, world.cells[0], broad, LookingNorthFrom(-2.99f));
    EXPECT_GT(close.pullBack, 0.0F);
    EXPECT_LT(close.nearPlane, kNearPlane);
    EXPECT_NEAR(close.pullBack, kEyePullBack * (1.0F - 0.06F / kEyeProbeDistance), 1e-3F);

    // A step further back and the probe reaches nothing.
    const EyeClearance away = ProbeAhead(world, world.cells[0], broad, LookingNorthFrom(-2.5f));
    EXPECT_FLOAT_EQ(away.pullBack, 0.0F);
    EXPECT_FLOAT_EQ(away.nearPlane, kNearPlane);

    // ...and so does the same body looking the other way, which is the reason the probe follows
    // the VIEW and not the body: a player with their back to a wall is not clipping through it.
    PlayerState south;
    south.position = Vector3(0.0f, 0.90f, -2.99f);
    south.yaw = std::numbers::pi_v<float>;
    FirstPersonCamera camera;
    camera.Update(south, kPlayerEyeHeight, 0.0F);
    EXPECT_FLOAT_EQ(ProbeAhead(world, world.cells[0], broad, camera.Pose()).pullBack, 0.0F);
}

TEST(EyeProbeTests, LookingDownAtTheFloorIsTheCommonestSurfaceAtArmsLength)
{
    // A floor slab under the body, and a player looking straight down at it. The eye is 1.68 m up
    // and the probe is 0.10 m long, so this must NOT fire -- the interesting half of the test is
    // that a crouched player with their face near the boards does.
    CollisionObb floor;
    floor.centre = Vector3(0.0f, -0.05f, 0.0f);
    floor.halfExtents = Vector3(4.0f, 0.05f, 4.0f);
    floor.kind = CollisionKind::Floor;
    CollisionWorld world = OneCell({floor});
    BroadPhase broad;

    PlayerState state;
    state.position = Vector3(0.0f, 0.90f, 0.0f);
    FirstPersonCamera camera;
    camera.Update(state, kPlayerEyeHeight, -1.484F);
    EXPECT_FLOAT_EQ(ProbeAhead(world, world.cells[0], broad, camera.Pose()).pullBack, 0.0F);

    camera.Update(state, 0.04F, -1.484F);
    const EyeClearance nose = ProbeAhead(world, world.cells[0], broad, camera.Pose());
    EXPECT_GT(nose.pullBack, 0.0F) << "the floor 40 mm under the eye did not reach the probe";
    EXPECT_LT(nose.nearPlane, kNearPlane);
}
