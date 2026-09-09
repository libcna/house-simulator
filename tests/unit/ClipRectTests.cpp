// SPDX-License-Identifier: MIT
//
// `HOUSE-00662`. §25.2's `ClipRectToFrustum`: the doorway, cut down to what can be seen through it.
//
// The thing this has to get right is the DIRECTION of its errors. §25.4's rule is that culling may
// never remove something visible, so a clip that keeps too much is a frame that draws a room
// nobody can see -- and a clip that keeps too little is a room that vanishes. Every case here is
// therefore either an exact answer or a bound in the safe direction.
#include <array>
#include <cmath>
#include <numbers>
#include <vector>

#include <gtest/gtest.h>

#include "cnahouse/player/FirstPersonCamera.hpp"
#include "cnahouse/visibility/ClipFrustum.hpp"
#include "cnahouse/visibility/ClipRect.hpp"

namespace
{
    using cnahouse::player::FirstPersonCamera;
    using cnahouse::player::kPlayerEyeHeight;
    using cnahouse::player::PlayerState;
    using cnahouse::visibility::ClipFrustum;
    using cnahouse::visibility::ClippedPolygon;
    using cnahouse::visibility::ClipRectToFrustum;
    using cnahouse::visibility::kMaxClippedVertices;
    using cnahouse::visibility::PolygonArea;
    using Microsoft::Xna::Framework::ContainmentType;
    using Microsoft::Xna::Framework::Plane;
    using Microsoft::Xna::Framework::Vector3;

    /// §12's doorway: 0.9 m wide and 2.04 m tall, in the z = @p z plane, centred on @p x.
    std::vector<Vector3> Doorway(float x = 0.0F, float z = -3.0F, float width = 0.9F, float height = 2.04F)
    {
        return {Vector3(x - width * 0.5F, 0.0F, z),
                Vector3(x + width * 0.5F, 0.0F, z),
                Vector3(x + width * 0.5F, height, z),
                Vector3(x - width * 0.5F, height, z)};
    }

    /// A half-space whose outside is `x > at`.
    Plane RightOf(float at)
    {
        return Plane(Vector3(1.0F, 0.0F, 0.0F), -at);
    }

    /// ...and whose outside is `x < at`.
    Plane LeftOf(float at)
    {
        return Plane(Vector3(-1.0F, 0.0F, 0.0F), at);
    }

    Plane Above(float at)
    {
        return Plane(Vector3(0.0F, 1.0F, 0.0F), -at);
    }

} // namespace

TEST(ClipRectTests, ADoorwayEntirelyInsideComesBackUntouched)
{
    const std::vector<Vector3> rect = Doorway();
    const Plane planes[] = {RightOf(5.0F), LeftOf(-5.0F), Above(5.0F)};
    const ClippedPolygon clipped = ClipRectToFrustum(rect, planes);

    ASSERT_EQ(clipped.count, 4U);
    EXPECT_FALSE(clipped.overflowed);
    for (std::size_t i = 0; i < 4; ++i)
    {
        EXPECT_FLOAT_EQ(clipped.points[i].X, rect[i].X) << i;
        EXPECT_FLOAT_EQ(clipped.points[i].Y, rect[i].Y) << i;
        EXPECT_FLOAT_EQ(clipped.points[i].Z, rect[i].Z) << i;
    }
    EXPECT_NEAR(PolygonArea(clipped.Points()), 0.9F * 2.04F, 1e-5F);
}

TEST(ClipRectTests, ADoorwayEntirelyOutsideIsGone)
{
    // The portal is not visible, and §25.2's traversal stops there rather than walking into a room
    // it cannot see.
    const ClippedPolygon clipped = ClipRectToFrustum(Doorway(), std::array{RightOf(-4.0F)});
    EXPECT_TRUE(clipped.Empty());
    EXPECT_FLOAT_EQ(PolygonArea(clipped.Points()), 0.0F);
}

TEST(ClipRectTests, OnePlaneThroughItLeavesTheHalfThatIsInside)
{
    // A doorway half out of frame: still four corners, two of them moved onto the plane, and
    // exactly half the area.
    const ClippedPolygon clipped = ClipRectToFrustum(Doorway(), std::array{RightOf(0.0F)});
    ASSERT_EQ(clipped.count, 4U);
    EXPECT_NEAR(PolygonArea(clipped.Points()), 0.45F * 2.04F, 1e-5F);
    for (std::size_t i = 0; i < clipped.count; ++i)
    {
        EXPECT_LE(clipped.points[i].X, 1e-5F) << "a vertex is on the wrong side of the plane";
        EXPECT_GE(clipped.points[i].X, -0.45F - 1e-5F);
    }
}

TEST(ClipRectTests, ACornerCutAddsAVertexAndTakesArea)
{
    // Sutherland-Hodgman adds at most one vertex per plane, and a plane through a corner is where
    // it adds one: four corners in, five out.
    // Through ONE corner: the top right is outside `x + y > 2.3` and the other three are not.
    const Plane diagonal(Vector3(1.0F, 1.0F, 0.0F), -2.3F);
    const ClippedPolygon clipped = ClipRectToFrustum(Doorway(), std::array{diagonal});

    EXPECT_EQ(clipped.count, 5U);
    EXPECT_LT(PolygonArea(clipped.Points()), 0.9F * 2.04F);
    EXPECT_GT(PolygonArea(clipped.Points()), 0.5F * 0.9F * 2.04F);
    for (std::size_t i = 0; i < clipped.count; ++i)
    {
        EXPECT_LE(diagonal.DotCoordinate(clipped.points[i]), 1e-4F);
    }
}

TEST(ClipRectTests, ADoorwayLyingExactlyInAPlaneSurvivesIt)
{
    // Not hypothetical: a body standing IN a doorway is looking through a portal that lies in one
    // of its own frustum's planes, and §10.3's near plane is 0.10 m in front of an eye that can be
    // 0.30 m from a wall. A vertex lost to rounding here is half a room culled.
    const std::vector<Vector3> rect = Doorway(0.0F, -3.0F);
    const Plane inThePlane(Vector3(0.0F, 0.0F, -1.0F), -3.0F);
    for (const Vector3& corner : rect)
    {
        ASSERT_NEAR(inThePlane.DotCoordinate(corner), 0.0F, 1e-6F) << "the fixture is not in the plane";
    }

    const ClippedPolygon clipped = ClipRectToFrustum(rect, std::array{inThePlane});
    EXPECT_EQ(clipped.count, 4U);
    EXPECT_NEAR(PolygonArea(clipped.Points()), 0.9F * 2.04F, 1e-5F);
}

TEST(ClipRectTests, WhatComesOutIsInsideTheFrustumItWasClippedAgainst)
{
    // The property §25.2 actually needs: every vertex of the result is inside every plane, so the
    // reduced frustum built from its edges cannot be wider than the one it came from.
    PlayerState state;
    state.position = Vector3(0.0F, 0.90F, 0.0F);
    state.yaw = 0.35F;
    FirstPersonCamera camera;
    camera.SetAspect(16.0F / 9.0F);
    camera.Update(state, kPlayerEyeHeight, -0.15F);

    const ClipFrustum frustum(camera.Frustum());
    std::vector<Plane> planes;
    for (std::size_t i = 0; i < frustum.PlaneCount(); ++i)
    {
        planes.push_back(frustum[i]);
    }

    // Doorways at four places, two of them off to the side where the frustum really does cut them.
    for (const float x : {0.0F, 1.5F, 2.6F, -2.6F})
    {
        const std::vector<Vector3> rect = Doorway(x, -4.0F);
        const ClippedPolygon clipped = ClipRectToFrustum(rect, planes);
        for (std::size_t i = 0; i < clipped.count; ++i)
        {
            // Inside every plane, to the tolerance the clip itself works to: a vertex the clip
            // PLACED on a plane is on it to within rounding, and `Contains` is an exact test --
            // which is right for a point that came from somewhere else and wrong for this one.
            for (std::size_t p = 0; p < frustum.PlaneCount(); ++p)
            {
                EXPECT_LE(frustum[p].DotCoordinate(clipped.points[i]), 1e-4F)
                    << "vertex " << i << " of the doorway at x " << x << " is outside plane " << p;
            }
        }
        // ...and never more area than it started with.
        EXPECT_LE(PolygonArea(clipped.Points()), 0.9F * 2.04F + 1e-5F);
    }

    // The one straight ahead is not cut at all; the one far to the side is gone entirely.
    EXPECT_EQ(ClipRectToFrustum(Doorway(0.0F, -4.0F), planes).count, 4U);
    EXPECT_TRUE(ClipRectToFrustum(Doorway(-8.0F, -4.0F), planes).Empty());
}

TEST(ClipRectTests, EightPlanesStayInsideTheTwelveVertexBound)
{
    // §25.2's arithmetic: one vertex per plane at most, so a four-sided doorway against a reduced
    // frustum's eight sides cannot exceed twelve. An octagon of planes is the worst case there is.
    std::vector<Plane> planes;
    for (int i = 0; i < 8; ++i)
    {
        const float angle = static_cast<float>(i) * std::numbers::pi_v<float> / 4.0F;
        planes.push_back(Plane(Vector3(std::cos(angle), std::sin(angle), 0.0F), -0.8F));
    }

    // A square CENTRED on the octagon, so all eight planes cut it: a doorway sitting on the
    // floor is cut by the upper half of them and comes back with six vertices, which tests six.
    const std::array<Vector3, 4> square{Vector3(-2.0F, -2.0F, -3.0F),
                                        Vector3(2.0F, -2.0F, -3.0F),
                                        Vector3(2.0F, 2.0F, -3.0F),
                                        Vector3(-2.0F, 2.0F, -3.0F)};
    const ClippedPolygon clipped = ClipRectToFrustum(square, planes);
    EXPECT_FALSE(clipped.overflowed);
    EXPECT_LE(clipped.count, kMaxClippedVertices);
    EXPECT_GE(clipped.count, 8U) << "the octagon cut nothing, so the bound was not tested";
    for (const Plane& plane : planes)
    {
        for (std::size_t i = 0; i < clipped.count; ++i)
        {
            EXPECT_LE(plane.DotCoordinate(clipped.points[i]), 1e-4F);
        }
    }
}

TEST(ClipRectTests, TheOrderRoundThePolygonIsKept)
{
    // `ReduceFrustum` builds one side plane per EDGE, so consecutive vertices have to still be
    // consecutive. Checked by walking the result and asserting it turns the same way at every
    // corner, which a reordered polygon does not.
    const ClippedPolygon clipped =
        ClipRectToFrustum(Doorway(), std::array{RightOf(0.2F), LeftOf(-0.2F), Above(1.5F)});
    ASSERT_GE(clipped.count, 4U);

    float sign = 0.0F;
    for (std::size_t i = 0; i < clipped.count; ++i)
    {
        const Vector3& a = clipped.points[i];
        const Vector3& b = clipped.points[(i + 1) % clipped.count];
        const Vector3& c = clipped.points[(i + 2) % clipped.count];
        // The doorway is in a z plane, so the turn is the z component of the cross product.
        const float turn = (b.X - a.X) * (c.Y - b.Y) - (b.Y - a.Y) * (c.X - b.X);
        if (std::fabs(turn) < 1e-6F)
        {
            continue;
        }
        if (sign == 0.0F)
        {
            sign = turn;
        }
        EXPECT_GT(turn * sign, 0.0F) << "the polygon turns back on itself at vertex " << i;
    }
    EXPECT_NE(sign, 0.0F) << "every corner was straight, so nothing was checked";
}

TEST(ClipRectTests, AnAreaOfNothingIsNothing)
{
    EXPECT_FLOAT_EQ(PolygonArea({}), 0.0F);
    const std::array<Vector3, 2> line{Vector3(0.0F, 0.0F, 0.0F), Vector3(1.0F, 0.0F, 0.0F)};
    EXPECT_FLOAT_EQ(PolygonArea(line), 0.0F);
    // A rectangle clipped to a line has three or four vertices and no area, which is what
    // `kMinPortalNdcArea` (`HOUSE-00664`) is going to cut off.
    const ClippedPolygon flat = ClipRectToFrustum(Doorway(), std::array{RightOf(-0.45F)});
    EXPECT_NEAR(PolygonArea(flat.Points()), 0.0F, 1e-6F);
}
