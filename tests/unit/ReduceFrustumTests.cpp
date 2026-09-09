// SPDX-License-Identifier: MIT
//
// `HOUSE-00663`. §25.2's `ReduceFrustum`: the cone a doorway admits.
//
// This is the step that makes portal traversal worth doing at all. Without it the next room is
// tested against the whole camera frustum and the recursion touches everything; with it the room
// beyond a door is tested against the door.
//
// Every assertion here is either "the portal is still inside" or "something the portal cannot show
// is now outside", because those are the two halves of §25.4's rule: never cull something visible,
// and cull as much as possible of what is not.
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <vector>

#include <gtest/gtest.h>

#include "Microsoft/Xna/Framework/BoundingBox.hpp"
#include "Microsoft/Xna/Framework/BoundingFrustum.hpp"

#include "cnahouse/player/FirstPersonCamera.hpp"
#include "cnahouse/util/Rng.hpp"
#include "cnahouse/visibility/ClipRect.hpp"
#include "cnahouse/visibility/ReduceFrustum.hpp"

namespace
{
    using cnahouse::player::FirstPersonCamera;
    using cnahouse::player::kPlayerEyeHeight;
    using cnahouse::player::PlayerState;
    using cnahouse::visibility::ClipFrustum;
    using cnahouse::visibility::ReducedFrustum;
    using cnahouse::visibility::ReduceFrustum;
    using Microsoft::Xna::Framework::BoundingBox;
    using Microsoft::Xna::Framework::ContainmentType;
    using Microsoft::Xna::Framework::Plane;
    using Microsoft::Xna::Framework::Vector3;

    /// §12's doorway, 0.9 x 2.04 m, in the z = @p z plane.
    std::vector<Vector3> Doorway(float z = -3.0F, float x = 0.0F)
    {
        return {Vector3(x - 0.45F, 0.0F, z),
                Vector3(x + 0.45F, 0.0F, z),
                Vector3(x + 0.45F, 2.04F, z),
                Vector3(x - 0.45F, 2.04F, z)};
    }

    BoundingBox BoxAt(const Vector3& centre, float half)
    {
        return BoundingBox(Vector3(centre.X - half, centre.Y - half, centre.Z - half),
                           Vector3(centre.X + half, centre.Y + half, centre.Z + half));
    }

    /// A camera at the origin's eye height looking north (-Z), and its own two end planes.
    FirstPersonCamera Camera()
    {
        PlayerState state;
        state.position = Vector3(0.0F, 0.90F, 0.0F);
        FirstPersonCamera camera;
        camera.SetAspect(16.0F / 9.0F);
        camera.Update(state, kPlayerEyeHeight, 0.0F);
        return camera;
    }

} // namespace

TEST(ReduceFrustumTests, TheDoorwayItWasBuiltFromIsStillInside)
{
    // The first half of §25.4's rule. Every vertex of the portal, and every point on the line from
    // the eye through it, has to survive the frustum that portal produced.
    const FirstPersonCamera camera = Camera();
    const Vector3 eye = camera.Pose().eye;
    const std::vector<Vector3> door = Doorway();
    const ReducedFrustum reduced =
        ReduceFrustum(eye, door, camera.Frustum().getNearProperty(), camera.Frustum().getFarProperty());

    EXPECT_TRUE(reduced.complete);
    EXPECT_EQ(reduced.dropped, 0U);
    // Two end planes plus one side per edge.
    EXPECT_EQ(reduced.frustum.PlaneCount(), 6U);

    for (const Vector3& corner : door)
    {
        for (std::size_t i = 0; i < reduced.frustum.PlaneCount(); ++i)
        {
            EXPECT_LE(reduced.frustum[i].DotCoordinate(corner), 1e-4F)
                << "a corner of the doorway is outside";
        }
    }
    // ...and the middle of the doorway, and a box just behind it in the next room.
    EXPECT_EQ(reduced.frustum.Contains(BoxAt(Vector3(0.0F, 1.0F, -3.0F), 0.05F)), ContainmentType::Contains);
    EXPECT_NE(reduced.frustum.Contains(BoxAt(Vector3(0.0F, 1.4F, -4.0F), 0.3F)), ContainmentType::Disjoint);
}

TEST(ReduceFrustumTests, WhatTheDoorwayCannotShowIsCulled)
{
    // The other half. A box beside the door, or above its head, is inside the CAMERA's frustum and
    // outside the door's cone -- which is the whole point of the reduction.
    const FirstPersonCamera camera = Camera();
    const Vector3 eye = camera.Pose().eye;
    const ReducedFrustum reduced =
        ReduceFrustum(eye, Doorway(), camera.Frustum().getNearProperty(), camera.Frustum().getFarProperty());

    const BoundingBox beside = BoxAt(Vector3(3.0F, 1.4F, -6.0F), 0.4F);
    const BoundingBox above = BoxAt(Vector3(0.0F, 5.0F, -6.0F), 0.4F);
    // Well below the cone: the ray from the eye through the doorway's threshold reaches
    // y = -1.68 at this distance, so a box AT -2.0 straddles the boundary and is correctly
    // reported as intersecting -- which is what the first version of this fixture measured.
    const BoundingBox below = BoxAt(Vector3(0.0F, -3.5F, -6.0F), 0.4F);

    ASSERT_NE(camera.Frustum().Contains(beside), ContainmentType::Disjoint)
        << "the fixture is not testing the reduction";
    ASSERT_NE(camera.Frustum().Contains(above), ContainmentType::Disjoint);

    EXPECT_EQ(reduced.frustum.Contains(beside), ContainmentType::Disjoint);
    EXPECT_EQ(reduced.frustum.Contains(above), ContainmentType::Disjoint);
    EXPECT_EQ(reduced.frustum.Contains(below), ContainmentType::Disjoint);
}

TEST(ReduceFrustumTests, TheCameraKeepsItsOwnEndsAndTheEyeIsOnEverySidePlane)
{
    // §25.2: *"keeping the original near and far planes"*. A reduced frustum that lost its far
    // plane is unbounded, and the traversal behind it tests the whole world.
    const FirstPersonCamera camera = Camera();
    const Vector3 eye = camera.Pose().eye;
    const ReducedFrustum reduced =
        ReduceFrustum(eye, Doorway(), camera.Frustum().getNearProperty(), camera.Frustum().getFarProperty());

    // A box down the same line but past §10.3's 420 m, and one behind the eye.
    EXPECT_EQ(reduced.frustum.Contains(BoxAt(Vector3(0.0F, 1.4F, -500.0F), 1.0F)), ContainmentType::Disjoint);
    EXPECT_EQ(reduced.frustum.Contains(BoxAt(Vector3(0.0F, 1.4F, 5.0F), 0.5F)), ContainmentType::Disjoint);

    // The apex: every side plane contains the camera, so the eye is on all of them.
    for (std::size_t i = 2; i < reduced.frustum.PlaneCount(); ++i)
    {
        EXPECT_NEAR(reduced.frustum[i].DotCoordinate(eye), 0.0F, 1e-4F) << "side plane " << i;
    }
}

TEST(ReduceFrustumTests, TheWindingOfThePolygonDoesNotMatter)
{
    // A clipped polygon's order is whatever the clip produced from whatever order the portal's
    // corners were stored in, and the same portal seen from its other side reverses it. Deciding
    // "outward" from the centroid is what makes that a non-question.
    const FirstPersonCamera camera = Camera();
    const Vector3 eye = camera.Pose().eye;
    const std::vector<Vector3> forwards = Doorway();
    const std::vector<Vector3> backwards(forwards.rbegin(), forwards.rend());

    const ReducedFrustum a =
        ReduceFrustum(eye, forwards, camera.Frustum().getNearProperty(), camera.Frustum().getFarProperty());
    const ReducedFrustum b =
        ReduceFrustum(eye, backwards, camera.Frustum().getNearProperty(), camera.Frustum().getFarProperty());

    ASSERT_EQ(a.frustum.PlaneCount(), b.frustum.PlaneCount());
    for (int x = -4; x <= 4; ++x)
    {
        for (int y = -2; y <= 4; ++y)
        {
            const BoundingBox box =
                BoxAt(Vector3(static_cast<float>(x) * 0.6F, static_cast<float>(y) * 0.8F, -6.0F), 0.25F);
            EXPECT_EQ(a.frustum.Contains(box), b.frustum.Contains(box)) << x << "," << y;
        }
    }
}

TEST(ReduceFrustumTests, ADoorSeenFromTheOtherSideAdmitsTheSameCone)
{
    // The same doorway, a camera on each side of it: what each can see through it is its own cone,
    // and neither is inside out. The failure this catches is a normal whose sign came from the
    // portal's stored order rather than from where the camera is.
    const std::vector<Vector3> door = Doorway(-3.0F);
    const Vector3 south(0.0F, kPlayerEyeHeight, 0.0F);
    const Vector3 north(0.0F, kPlayerEyeHeight, -6.0F);
    const Plane wide(Vector3(0.0F, 0.0F, 0.0F), 0.0F);

    // No end planes here -- a zero plane never rejects anything -- so only the sides decide.
    const ReducedFrustum fromSouth = ReduceFrustum(south, door, wide, wide);
    const ReducedFrustum fromNorth = ReduceFrustum(north, door, wide, wide);

    // Each sees the room BEYOND the door and not the one it is standing in.
    EXPECT_NE(fromSouth.frustum.Contains(BoxAt(Vector3(0.0F, 1.0F, -4.0F), 0.2F)), ContainmentType::Disjoint);
    EXPECT_NE(fromNorth.frustum.Contains(BoxAt(Vector3(0.0F, 1.0F, -2.0F), 0.2F)), ContainmentType::Disjoint);
    // ...and neither can see sideways through a 0.9 m opening.
    EXPECT_EQ(fromSouth.frustum.Contains(BoxAt(Vector3(4.0F, 1.0F, -4.0F), 0.2F)), ContainmentType::Disjoint);
    EXPECT_EQ(fromNorth.frustum.Contains(BoxAt(Vector3(-4.0F, 1.0F, -2.0F), 0.2F)),
              ContainmentType::Disjoint);
}

TEST(ReduceFrustumTests, ANarrowerDoorwayGivesANarrowerCone)
{
    // The reduction has to be monotone in the thing it reduces, or the traversal's depth would not
    // converge: each door along a corridor admits less than the one before it.
    const FirstPersonCamera camera = Camera();
    const Vector3 eye = camera.Pose().eye;
    const Plane near = camera.Frustum().getNearProperty();
    const Plane far = camera.Frustum().getFarProperty();

    const std::vector<Vector3> wide = Doorway();
    const std::vector<Vector3> narrow = {Vector3(-0.1F, 0.9F, -3.0F),
                                         Vector3(0.1F, 0.9F, -3.0F),
                                         Vector3(0.1F, 1.3F, -3.0F),
                                         Vector3(-0.1F, 1.3F, -3.0F)};

    const ReducedFrustum big = ReduceFrustum(eye, wide, near, far);
    const ReducedFrustum small = ReduceFrustum(eye, narrow, near, far);

    int narrowed = 0;
    for (int x = -6; x <= 6; ++x)
    {
        for (int y = -3; y <= 6; ++y)
        {
            const BoundingBox box =
                BoxAt(Vector3(static_cast<float>(x) * 0.3F, static_cast<float>(y) * 0.4F, -6.0F), 0.1F);
            const bool inBig = big.frustum.Intersects(box);
            const bool inSmall = small.frustum.Intersects(box);
            EXPECT_TRUE(inBig || !inSmall) << "the narrow door admitted something the wide one did not";
            narrowed += inBig && !inSmall ? 1 : 0;
        }
    }
    EXPECT_GT(narrowed, 20) << "the two cones are the same, so nothing was tested";
}

TEST(ReduceFrustumTests, APortalClippedToNothingProducesNoFrustum)
{
    const FirstPersonCamera camera = Camera();
    const Plane near = camera.Frustum().getNearProperty();
    const Plane far = camera.Frustum().getFarProperty();
    const Vector3 eye = camera.Pose().eye;

    for (const std::size_t count : {std::size_t{0}, std::size_t{1}, std::size_t{2}})
    {
        const std::vector<Vector3> degenerate(count, Vector3(0.0F, 1.0F, -3.0F));
        const ReducedFrustum reduced = ReduceFrustum(eye, degenerate, near, far);
        EXPECT_EQ(reduced.frustum.PlaneCount(), 0U) << count;
        EXPECT_FALSE(reduced.complete);
    }
}

TEST(ReduceFrustumTests, AnEdgeThroughTheEyeIsDroppedRatherThanGuessed)
{
    // A portal whose plane contains the camera -- a body standing exactly in a doorway -- has
    // edges the eye is collinear with, and those have no plane. Dropping one widens the cone,
    // which over-draws; inventing one could cull the room.
    const Plane wide(Vector3(0.0F, 0.0F, 0.0F), 0.0F);
    const std::vector<Vector3> door = Doorway(-3.0F);
    // On the line of the doorway's bottom edge, so that edge and the eye are collinear.
    const Vector3 eye(-2.0F, 0.0F, -3.0F);

    const ReducedFrustum reduced = ReduceFrustum(eye, door, wide, wide);
    EXPECT_FALSE(reduced.complete);
    EXPECT_EQ(reduced.dropped, 1U);
    EXPECT_EQ(reduced.frustum.PlaneCount(), 5U) << "two end planes and three sides";
    // What is left still admits the doorway.
    for (const Vector3& corner : door)
    {
        for (std::size_t i = 2; i < reduced.frustum.PlaneCount(); ++i)
        {
            EXPECT_LE(reduced.frustum[i].DotCoordinate(corner), 1e-4F);
        }
    }
}

TEST(ReduceFrustumTests, MoreEdgesThanThereIsRoomForLosesSIDESAndKeepsTheEnds)
{
    // §25.2 bounds the polygon at eight vertices, so this cannot happen -- and if it ever does,
    // the far plane is the one thing that must survive: a frustum without it is unbounded, and
    // the traversal behind it tests the whole world.
    const FirstPersonCamera camera = Camera();
    const Vector3 eye = camera.Pose().eye;
    std::vector<Vector3> many;
    for (int i = 0; i < 11; ++i)
    {
        const float angle = static_cast<float>(i) * 2.0F * 3.14159265F / 11.0F;
        many.push_back(Vector3(std::cos(angle) * 0.5F, 1.4F + std::sin(angle) * 0.5F, -3.0F));
    }

    const ReducedFrustum reduced =
        ReduceFrustum(eye, many, camera.Frustum().getNearProperty(), camera.Frustum().getFarProperty());
    EXPECT_FALSE(reduced.complete);
    EXPECT_EQ(reduced.dropped, 3U);
    EXPECT_EQ(reduced.frustum.PlaneCount(), ClipFrustum::kMaxPlanes);
    EXPECT_EQ(reduced.frustum.Contains(BoxAt(Vector3(0.0F, 1.4F, -500.0F), 1.0F)), ContainmentType::Disjoint)
        << "the far plane was pushed out by a side";
}

TEST(ReduceFrustumTests, ContainmentProperty)
{
    // `HOUSE-00663`'s acceptance, and the only statement of it that is worth anything: **the
    // reduced frustum contains every point the portal can see, and no point outside the parent**
    // -- 100 000 random samples, half of them aimed THROUGH the doorway and half of them aimed
    // anywhere at all.
    //
    // The two halves are the two directions §25.4 cares about. A point visible through the portal
    // that the reduction culls is a hole in the world; a point outside the parent that the
    // reduction admits is work the parent had already decided against.
    const FirstPersonCamera camera = Camera();
    const Vector3 eye = camera.Pose().eye;
    const ClipFrustum parent(camera.Frustum());

    // The doorway CLIPPED by the parent, which is what §25.2's traversal reduces: a polygon that
    // stuck out of the parent frustum would make the second half of the property false by
    // construction rather than by a bug.
    std::vector<Plane> planes;
    for (std::size_t i = 0; i < parent.PlaneCount(); ++i)
    {
        planes.push_back(parent[i]);
    }
    const cnahouse::visibility::ClippedPolygon portal =
        cnahouse::visibility::ClipRectToFrustum(Doorway(-3.0F, 0.6F), planes);
    ASSERT_GE(portal.count, 3U);

    const ReducedFrustum reduced = ReduceFrustum(
        eye, portal.Points(), camera.Frustum().getNearProperty(), camera.Frustum().getFarProperty());
    ASSERT_TRUE(reduced.complete);

    cnahouse::util::Rng rng(0x00663ULL);
    constexpr int kSamples = 50000;
    int seen = 0;
    int culled = 0;
    int admitted = 0;
    int leaked = 0;
    float worstLeak = 0.0F;

    // Half: points the portal really can see -- somewhere inside the polygon, some distance along
    // the ray from the eye through it. Sampled 2 % INSIDE the polygon's edge, because a point
    // exactly on the boundary is on a plane of the reduced frustum and rounding decides it.
    const Vector3 middle = [&portal]
    {
        Vector3 sum(0.0F, 0.0F, 0.0F);
        for (const Vector3& point : portal.Points())
        {
            sum = Vector3(sum.X + point.X, sum.Y + point.Y, sum.Z + point.Z);
        }
        const auto n = static_cast<float>(portal.count);
        return Vector3(sum.X / n, sum.Y / n, sum.Z / n);
    }();

    for (int sample = 0; sample < kSamples; ++sample)
    {
        // A point in the polygon: a random triangle of the fan, random barycentric coordinates.
        const auto corner =
            static_cast<std::size_t>(rng.NextInt(1, static_cast<std::int32_t>(portal.count) - 1));
        const Vector3& a = portal.points[0];
        const Vector3& b = portal.points[corner];
        const Vector3& c = portal.points[(corner + 1) % portal.count];
        float u = rng.NextFloat();
        float v = rng.NextFloat();
        if (u + v > 1.0F)
        {
            u = 1.0F - u;
            v = 1.0F - v;
        }
        Vector3 point(a.X + (b.X - a.X) * u + (c.X - a.X) * v,
                      a.Y + (b.Y - a.Y) * u + (c.Y - a.Y) * v,
                      a.Z + (b.Z - a.Z) * u + (c.Z - a.Z) * v);
        // 2 % towards the middle, off the boundary.
        point = Vector3(point.X + (middle.X - point.X) * 0.02F,
                        point.Y + (middle.Y - point.Y) * 0.02F,
                        point.Z + (middle.Z - point.Z) * 0.02F);

        const float t = rng.NextFloat(1.02F, 40.0F);
        const Vector3 beyond(
            eye.X + (point.X - eye.X) * t, eye.Y + (point.Y - eye.Y) * t, eye.Z + (point.Z - eye.Z) * t);
        if (parent.Contains(beyond) == ContainmentType::Disjoint)
        {
            // Past §10.3's far plane or outside the camera: not a point the parent admits either,
            // so the portal cannot be asked to keep it.
            continue;
        }
        ++seen;
        if (reduced.frustum.Contains(beyond) == ContainmentType::Disjoint)
        {
            ++culled;
        }
    }

    // ...and half anywhere in a 60 m cube around the camera: whatever the reduction admits, the
    // parent has to admit too.
    for (int sample = 0; sample < kSamples; ++sample)
    {
        const Vector3 point(
            rng.NextFloat(-30.0F, 30.0F), rng.NextFloat(-30.0F, 30.0F), rng.NextFloat(-30.0F, 30.0F));
        if (reduced.frustum.Contains(point) == ContainmentType::Disjoint)
        {
            continue;
        }
        ++admitted;
        if (parent.Contains(point) != ContainmentType::Disjoint)
        {
            continue;
        }
        // Outside by how much? A point ON a shared plane is decided by rounding, and both
        // frustums carry the same near and far planes.
        float worst = 0.0F;
        for (std::size_t i = 0; i < parent.PlaneCount(); ++i)
        {
            worst = std::max(worst, parent[i].DotCoordinate(point));
        }
        if (worst > 1e-4F)
        {
            ++leaked;
            worstLeak = std::max(worstLeak, worst);
        }
    }

    std::printf("  reduce: %d of %d rays through the portal admitted, %d culled; %d points admitted by the "
                "reduction, %d of them outside the parent (worst %.6f m)\n",
                seen - culled,
                kSamples,
                culled,
                admitted,
                leaked,
                static_cast<double>(worstLeak));

    EXPECT_GT(seen, 20000) << "the fixture aimed almost nothing through the portal";
    // 459 of 50 000, measured: the cone through a 0.9 x 2.04 m doorway is a small part of a 60 m
    // cube, and it is meant to be. The bound guards against a fixture that sampled nothing useful.
    EXPECT_GT(admitted, 300)
        << "the reduced frustum admitted almost nothing, so the second half proves little";
    EXPECT_EQ(culled, 0) << "the reduction culled a point the portal can see";
    EXPECT_EQ(leaked, 0) << "the reduction admitted a point the parent had already rejected, worst "
                         << worstLeak << " m outside";
}
