// SPDX-License-Identifier: MIT
//
// `HOUSE-00661`. §25.2's reduced frustum: up to ten planes, tested the way XNA tests six.
//
// The whole design of this type is "be `BoundingFrustum`, but with room for the sides a portal's
// clipped polygon produces", so the test is mostly that claim: build both from the same camera and
// ask them the same questions until one of them disagrees. `HOUSE-00104` already verified
// `BoundingFrustum` itself against analytic answers -- including the tangent sphere and the
// face-touching box that an epsilon error flips -- so agreeing with it is worth something.
#include <cmath>
#include <numbers>
#include <vector>

#include <gtest/gtest.h>

#include "Microsoft/Xna/Framework/BoundingBox.hpp"
#include "Microsoft/Xna/Framework/BoundingFrustum.hpp"
#include "Microsoft/Xna/Framework/BoundingSphere.hpp"

#include "cnahouse/player/FirstPersonCamera.hpp"
#include "cnahouse/visibility/ClipFrustum.hpp"

namespace
{
    using cnahouse::player::FirstPersonCamera;
    using cnahouse::player::kPlayerEyeHeight;
    using cnahouse::player::PlayerState;
    using cnahouse::visibility::ClipFrustum;
    using Microsoft::Xna::Framework::BoundingBox;
    using Microsoft::Xna::Framework::BoundingFrustum;
    using Microsoft::Xna::Framework::BoundingSphere;
    using Microsoft::Xna::Framework::ContainmentType;
    using Microsoft::Xna::Framework::Plane;
    using Microsoft::Xna::Framework::Vector3;

    /// §44's camera, looking north from the middle of a room.
    FirstPersonCamera Camera(float yaw = 0.0F, float pitch = 0.0F)
    {
        PlayerState state;
        state.position = Vector3(0.0F, 0.90F, 0.0F);
        state.yaw = yaw;
        FirstPersonCamera camera;
        camera.SetAspect(16.0F / 9.0F);
        camera.Update(state, kPlayerEyeHeight, pitch);
        return camera;
    }

    BoundingBox BoxAt(const Vector3& centre, float half)
    {
        return BoundingBox(Vector3(centre.X - half, centre.Y - half, centre.Z - half),
                           Vector3(centre.X + half, centre.Y + half, centre.Z + half));
    }

} // namespace

TEST(ClipFrustumTests, TheSixPlaneCaseIsBoundingFrustumForEveryBoxAndSphere)
{
    // 1 331 boxes and the same number of spheres, over a 40 m cube around the camera, at three
    // sizes each: inside, straddling and far outside are all in there, and so is every edge and
    // corner case the frustum's six planes can produce.
    const FirstPersonCamera camera = Camera(0.7F, -0.2F);
    const BoundingFrustum& xna = camera.Frustum();
    const ClipFrustum mine(xna);
    ASSERT_EQ(mine.PlaneCount(), 6U);

    int inside = 0;
    int straddling = 0;
    int outside = 0;
    int compared = 0;
    for (int x = -5; x <= 5; ++x)
    {
        for (int y = -5; y <= 5; ++y)
        {
            for (int z = -5; z <= 5; ++z)
            {
                const Vector3 centre(static_cast<float>(x) * 4.0F,
                                     static_cast<float>(y) * 4.0F + kPlayerEyeHeight,
                                     static_cast<float>(z) * 4.0F);
                for (const float half : {0.05F, 1.0F, 6.0F})
                {
                    const BoundingBox box = BoxAt(centre, half);
                    const ContainmentType expected = xna.Contains(box);
                    ASSERT_EQ(mine.Contains(box), expected) << "box at (" << centre.X << ", " << centre.Y
                                                            << ", " << centre.Z << ") half " << half;
                    EXPECT_EQ(mine.Intersects(box), xna.Intersects(box));

                    const BoundingSphere sphere(centre, half);
                    ASSERT_EQ(mine.Contains(sphere), xna.Contains(sphere))
                        << "sphere at (" << centre.X << ", " << centre.Y << ", " << centre.Z << ") r "
                        << half;
                    EXPECT_EQ(mine.Intersects(sphere), xna.Intersects(sphere));

                    ++compared;
                    inside += expected == ContainmentType::Contains ? 1 : 0;
                    straddling += expected == ContainmentType::Intersects ? 1 : 0;
                    outside += expected == ContainmentType::Disjoint ? 1 : 0;
                }
            }
        }
    }

    // The fixture has to contain all three answers or it is not testing the thing it claims to.
    EXPECT_EQ(compared, 3993);
    EXPECT_GT(inside, 20) << "no box was fully inside the frustum";
    EXPECT_GT(straddling, 20) << "no box straddled a plane, which is where the answers differ";
    EXPECT_GT(outside, 1000);
}

TEST(ClipFrustumTests, ThePointTestAgreesWithTheBoxOne)
{
    const FirstPersonCamera camera = Camera();
    const ClipFrustum mine(camera.Frustum());
    const BoundingFrustum& xna = camera.Frustum();

    for (const Vector3 point : {Vector3(0.0F, kPlayerEyeHeight, -5.0F),
                                Vector3(0.0F, kPlayerEyeHeight, 5.0F),
                                Vector3(0.0F, kPlayerEyeHeight, -0.05F),
                                Vector3(20.0F, kPlayerEyeHeight, -5.0F),
                                Vector3(0.0F, kPlayerEyeHeight + 40.0F, -5.0F)})
    {
        EXPECT_EQ(mine.Contains(point), xna.Contains(point))
            << "(" << point.X << ", " << point.Y << ", " << point.Z << ")";
    }
}

TEST(ClipFrustumTests, AFrustumWithNoPlanesContainsEverything)
{
    // The identity the traversal starts from, and what a caller falls back to when a reduction
    // fails: no planes means no outside.
    const ClipFrustum empty;
    EXPECT_EQ(empty.PlaneCount(), 0U);
    EXPECT_EQ(empty.Contains(BoxAt(Vector3(1000.0F, -400.0F, 12.0F), 3.0F)), ContainmentType::Contains);
    EXPECT_EQ(empty.Contains(BoundingSphere(Vector3(-90.0F, 8.0F, 4.0F), 1.0F)), ContainmentType::Contains);
    EXPECT_EQ(empty.Contains(Vector3(0.0F, 0.0F, 0.0F)), ContainmentType::Contains);
    EXPECT_TRUE(empty.Intersects(BoxAt(Vector3(0.0F, 0.0F, 0.0F), 1.0F)));
}

TEST(ClipFrustumTests, EveryPlaneAddedNarrowsIt)
{
    // §25.2's reduction: each side plane contains the camera and one edge of the clipped polygon.
    // Modelled here by four planes that cut a 2 m column out of the world, which is what a
    // doorway does to the view through it.
    ClipFrustum frustum;
    const BoundingBox inside = BoxAt(Vector3(0.0F, 0.0F, -5.0F), 0.5F);
    const BoundingBox aside = BoxAt(Vector3(4.0F, 0.0F, -5.0F), 0.5F);
    EXPECT_EQ(frustum.Contains(aside), ContainmentType::Contains);

    // Normals OUTWARD, XNA's convention: `x > 1` is outside.
    EXPECT_TRUE(frustum.Add(Plane(Vector3(1.0F, 0.0F, 0.0F), -1.0F)));
    EXPECT_EQ(frustum.PlaneCount(), 1U);
    EXPECT_EQ(frustum.Contains(aside), ContainmentType::Disjoint) << "the plane did not cut anything";
    EXPECT_EQ(frustum.Contains(inside), ContainmentType::Contains);

    EXPECT_TRUE(frustum.Add(Plane(Vector3(-1.0F, 0.0F, 0.0F), -1.0F)));
    EXPECT_TRUE(frustum.Add(Plane(Vector3(0.0F, 1.0F, 0.0F), -1.0F)));
    EXPECT_TRUE(frustum.Add(Plane(Vector3(0.0F, -1.0F, 0.0F), -1.0F)));
    EXPECT_EQ(frustum.PlaneCount(), 4U);
    EXPECT_EQ(frustum.Contains(inside), ContainmentType::Contains);
    // A box that pokes out of the column straddles it rather than being culled -- §25.4 must never
    // cull something visible, so `Intersects` is the answer that has to be conservative.
    EXPECT_EQ(frustum.Contains(BoxAt(Vector3(0.9F, 0.0F, -5.0F), 0.5F)), ContainmentType::Intersects);
    EXPECT_TRUE(frustum.Intersects(BoxAt(Vector3(0.9F, 0.0F, -5.0F), 0.5F)));
}

TEST(ClipFrustumTests, TenPlanesFitAndTheEleventhIsRefusedRatherThanDropped)
{
    // §25.2's bound: eight sides from a clipped polygon of at most eight vertices, plus the near
    // and far planes the camera started with. An eleventh means the traversal has produced
    // something this design did not expect, and it has to be able to SEE that -- a silently
    // dropped plane makes the frustum too wide, which over-draws invisibly.
    ClipFrustum frustum;
    for (std::size_t i = 0; i < ClipFrustum::kMaxPlanes; ++i)
    {
        const float angle = static_cast<float>(i) * 0.6F;
        EXPECT_TRUE(frustum.Add(Plane(Vector3(std::cos(angle), std::sin(angle), 0.0F), -2.0F))) << i;
    }
    EXPECT_EQ(frustum.PlaneCount(), ClipFrustum::kMaxPlanes);

    const ContainmentType before = frustum.Contains(BoxAt(Vector3(0.0F, 0.0F, 0.0F), 0.1F));
    EXPECT_FALSE(frustum.Add(Plane(Vector3(0.0F, 0.0F, 1.0F), 0.0F)));
    EXPECT_EQ(frustum.PlaneCount(), ClipFrustum::kMaxPlanes) << "the refused plane was stored anyway";
    EXPECT_EQ(frustum.Contains(BoxAt(Vector3(0.0F, 0.0F, 0.0F), 0.1F)), before)
        << "the refused plane changed the answer";
}

TEST(ClipFrustumTests, TheOrderOfThePlanesDoesNotChangeTheAnswer)
{
    // Every plane is tested, so the order cannot matter -- but the early return on `Front` means
    // an implementation that stopped at the first `Intersecting` would depend on it, which is the
    // bug this is here to notice.
    const FirstPersonCamera camera = Camera(1.2F, 0.3F);
    const BoundingFrustum& xna = camera.Frustum();
    ClipFrustum forwards;
    ClipFrustum backwards;
    const Plane planes[] = {xna.getNearProperty(),
                            xna.getFarProperty(),
                            xna.getLeftProperty(),
                            xna.getRightProperty(),
                            xna.getTopProperty(),
                            xna.getBottomProperty()};
    for (std::size_t i = 0; i < 6; ++i)
    {
        EXPECT_TRUE(forwards.Add(planes[i]));
        EXPECT_TRUE(backwards.Add(planes[5 - i]));
    }

    for (int x = -3; x <= 3; ++x)
    {
        for (int z = -3; z <= 3; ++z)
        {
            const BoundingBox box = BoxAt(
                Vector3(static_cast<float>(x) * 3.0F, kPlayerEyeHeight, static_cast<float>(z) * 3.0F), 1.0F);
            EXPECT_EQ(forwards.Contains(box), backwards.Contains(box)) << x << "," << z;
            EXPECT_EQ(forwards.Contains(box), xna.Contains(box));
        }
    }
}
