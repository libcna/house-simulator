// SPDX-License-Identifier: MIT
//
// `HOUSE-00664`. §25.2's `kMinPortalNdcArea`: *"about 2 x 2 pixels at 1280 x 720. Below that the
// target cell contributes nothing and the chain stops."*
//
// One number, and the thing that keeps a corridor of open doors from costing the whole house. What
// is worth testing about it is the arithmetic behind it -- how far away a doorway has to be before
// it stops mattering -- and the one case where the answer must be "do not cull whatever the number
// says": a portal the camera is standing in.
#include <cmath>
#include <cstdio>
#include <vector>

#include <gtest/gtest.h>

#include "cnahouse/player/FirstPersonCamera.hpp"
#include "cnahouse/visibility/PortalArea.hpp"

namespace
{
    using cnahouse::player::FirstPersonCamera;
    using cnahouse::player::kPlayerEyeHeight;
    using cnahouse::player::PlayerState;
    using cnahouse::visibility::kFullScreenNdcArea;
    using cnahouse::visibility::kMinPortalNdcArea;
    using cnahouse::visibility::NdcArea;
    using cnahouse::visibility::PortalContributes;
    using Microsoft::Xna::Framework::Matrix;
    using Microsoft::Xna::Framework::Vector3;

    /// §12's doorway, 0.9 x 2.04 m, @p z metres north of the origin.
    std::vector<Vector3> Doorway(float z)
    {
        return {Vector3(-0.45F, 0.0F, z),
                Vector3(0.45F, 0.0F, z),
                Vector3(0.45F, 2.04F, z),
                Vector3(-0.45F, 2.04F, z)};
    }

    FirstPersonCamera Camera()
    {
        PlayerState state;
        state.position = Vector3(0.0F, 0.90F, 0.0F);
        FirstPersonCamera camera;
        camera.SetAspect(16.0F / 9.0F);
        camera.Update(state, kPlayerEyeHeight, 0.0F);
        return camera;
    }

    Matrix ViewProjection(const FirstPersonCamera& camera)
    {
        return camera.View() * camera.Projection();
    }

} // namespace

TEST(PortalAreaTests, TheCutoffIsWhatSectionTwentyFiveSaysItIsInPixels)
{
    EXPECT_FLOAT_EQ(kMinPortalNdcArea, 1.2e-5F);
    EXPECT_FLOAT_EQ(kFullScreenNdcArea, 4.0F);

    // §25.2 calls it *"about 2 x 2 pixels at 1280 x 720"*. One pixel there is 4/(1280·720) of the
    // NDC square, so the constant is 2.76 pixels -- 1.66 x 1.66, not 2 x 2. Asserted here so that
    // nobody re-derives the sentence into 1.74e-5 and wonders why the traversals got shorter.
    const double onePixel = 4.0 / (1280.0 * 720.0);
    EXPECT_NEAR(static_cast<double>(kMinPortalNdcArea) / onePixel, 2.76, 0.05);
}

TEST(PortalAreaTests, ADoorwayGetsSmallerWithDistanceAndTheCutoffHasAReach)
{
    const FirstPersonCamera camera = Camera();
    const Matrix viewProjection = ViewProjection(camera);

    float previous = kFullScreenNdcArea;
    for (const float z : {-1.5F, -3.0F, -6.0F, -12.0F, -24.0F})
    {
        const float area = NdcArea(Doorway(z), viewProjection);
        EXPECT_LT(area, previous) << "a doorway at " << z << " is not smaller than the one nearer";
        EXPECT_GT(area, 0.0F);
        previous = area;
    }

    // Where does §25.2's cutoff bite on a full-sized doorway? Measured: nowhere inside §10.3's
    // world. That is the RIGHT answer and worth stating -- the cutoff is not there to cull doors,
    // it is there to stop a CHAIN of them, and a chain ends because each reduction admits less
    // than the last until what is left is a sliver.
    float reach = 0.0F;
    for (float z = -1.0F; z > -2000.0F; z -= 1.0F)
    {
        if (!PortalContributes(Doorway(z), viewProjection))
        {
            reach = -z;
            break;
        }
    }
    std::printf("  a 0.9 x 2.04 m doorway falls under §25.2's cutoff at %.0f m; at §10.3's 420 m far "
                "plane it is still %.1f pixels at 1280x720\n",
                static_cast<double>(reach),
                static_cast<double>(NdcArea(Doorway(-420.0F), viewProjection)) / (4.0 / (1280.0 * 720.0)));
    // 419 m, measured -- and §10.3's far plane is 420. The two numbers were chosen in different
    // sections for different reasons and land within a metre of each other: a doorway stops being
    // worth traversing exactly where the world stops being drawn.
    EXPECT_GT(reach, 400.0F) << "§25.2's cutoff culls a doorway inside §10.3's world";
    EXPECT_LT(reach, 600.0F) << "the cutoff never bites at all, so it is not a cutoff";
}

TEST(PortalAreaTests, TheCutoffBitesOnTheSliversAChainOfPortalsLeaves)
{
    // What §25.2's cutoff is FOR. A doorway seen square on is never small enough to cull inside
    // this world; a doorway seen almost edge-on, or the sliver a chain of reductions has clipped
    // one down to, is -- and those are exactly the cases where the room beyond contributes a few
    // pixels for a whole cell's worth of traversal.
    const FirstPersonCamera camera = Camera();
    const Matrix viewProjection = ViewProjection(camera);

    // A 20 mm gap at 8 m -- what a chain of reductions leaves of a doorway after four rooms, and
    // about the width of a door left on the latch.
    const auto gap = [](float size, float z)
    {
        return std::vector<Vector3>{Vector3(-size * 0.5F, 1.4F, z),
                                    Vector3(size * 0.5F, 1.4F, z),
                                    Vector3(size * 0.5F, 1.4F + size, z),
                                    Vector3(-size * 0.5F, 1.4F + size, z)};
    };
    const double onePixel = 4.0 / (1280.0 * 720.0);
    std::printf("  a 20 mm gap is %.1f px at 8 m and %.0f px at 2 m; §25.2's cutoff is %.1f px\n",
                static_cast<double>(NdcArea(gap(0.02F, -8.0F), viewProjection)) / onePixel,
                static_cast<double>(NdcArea(gap(0.02F, -2.0F), viewProjection)) / onePixel,
                static_cast<double>(kMinPortalNdcArea) / onePixel);

    EXPECT_FALSE(PortalContributes(gap(0.02F, -8.0F), viewProjection))
        << "area " << NdcArea(gap(0.02F, -8.0F), viewProjection) << " against " << kMinPortalNdcArea;
    // ...and the same gap in the same room is worth walking through.
    EXPECT_TRUE(PortalContributes(gap(0.02F, -2.0F), viewProjection));

    // The finding this test exists to record: the cutoff is PERMISSIVE. A whole doorway survives
    // to the far plane and a 20 mm gap survives to four metres, so nothing is culled by area that
    // a player could plausibly see through -- which is the right side to err on (§25.4), and it
    // means the chain is stopped by `maxDepthFor` (`HOUSE-00667`) far more often than by this.
}

TEST(PortalAreaTests, APortalFillingTheViewIsMostOfTheScreen)
{
    // A wall-sized opening a metre away: the NDC area is a large fraction of the 4 the square has.
    const FirstPersonCamera camera = Camera();
    const std::vector<Vector3> wide = {Vector3(-4.0F, -2.0F, -1.0F),
                                       Vector3(4.0F, -2.0F, -1.0F),
                                       Vector3(4.0F, 4.0F, -1.0F),
                                       Vector3(-4.0F, 4.0F, -1.0F)};
    const float area = NdcArea(wide, ViewProjection(camera));
    EXPECT_GT(area, 3.0F);
    EXPECT_TRUE(PortalContributes(wide, ViewProjection(camera)));
}

TEST(PortalAreaTests, APortalTheCameraIsStandingInIsNeverCulled)
{
    // The failure this exists to prevent: a vertex behind the eye divides by a negative `w` and
    // lands somewhere it is not, and the polygon's area comes out tiny. A room that vanishes when
    // the player stands in its doorway is the worst possible culling bug -- so a portal with any
    // vertex at or behind the eye is reported as the WHOLE SCREEN.
    const FirstPersonCamera camera = Camera();
    const Matrix viewProjection = ViewProjection(camera);

    // A doorway in the plane of the eye: two corners in front, two behind.
    const std::vector<Vector3> around = {Vector3(-0.45F, 0.0F, 0.5F),
                                         Vector3(0.45F, 0.0F, 0.5F),
                                         Vector3(0.45F, 2.04F, -0.5F),
                                         Vector3(-0.45F, 2.04F, -0.5F)};
    EXPECT_FLOAT_EQ(NdcArea(around, viewProjection), kFullScreenNdcArea);
    EXPECT_TRUE(PortalContributes(around, viewProjection));

    // ...and one entirely behind, which is the same rule and the same answer. Culling it is the
    // back-face test's job (`HOUSE-00666`), not this one's.
    const std::vector<Vector3> behind = Doorway(4.0F);
    EXPECT_FLOAT_EQ(NdcArea(behind, viewProjection), kFullScreenNdcArea);
}

TEST(PortalAreaTests, TheAreaDoesNotDependOnWhichWayRoundThePolygonIs)
{
    // A portal's winding depends on which side of the wall the camera is standing (`HOUSE-00663`),
    // and an area is an area either way.
    const FirstPersonCamera camera = Camera();
    const Matrix viewProjection = ViewProjection(camera);
    const std::vector<Vector3> forwards = Doorway(-3.0F);
    const std::vector<Vector3> backwards(forwards.rbegin(), forwards.rend());

    EXPECT_FLOAT_EQ(NdcArea(forwards, viewProjection), NdcArea(backwards, viewProjection));
    EXPECT_GT(NdcArea(forwards, viewProjection), 0.0F);
}

TEST(PortalAreaTests, APortalClippedToNothingContributesNothing)
{
    const FirstPersonCamera camera = Camera();
    const Matrix viewProjection = ViewProjection(camera);

    EXPECT_FLOAT_EQ(NdcArea({}, viewProjection), 0.0F);
    const std::vector<Vector3> line = {Vector3(0.0F, 0.0F, -3.0F), Vector3(0.9F, 0.0F, -3.0F)};
    EXPECT_FLOAT_EQ(NdcArea(line, viewProjection), 0.0F);
    EXPECT_FALSE(PortalContributes(line, viewProjection));

    // Three vertices in a row have no area either, and that is the shape a doorway clipped to a
    // sliver becomes.
    const std::vector<Vector3> flat = {
        Vector3(-0.45F, 1.0F, -3.0F), Vector3(0.0F, 1.0F, -3.0F), Vector3(0.45F, 1.0F, -3.0F)};
    EXPECT_NEAR(NdcArea(flat, viewProjection), 0.0F, 1e-9F);
    EXPECT_FALSE(PortalContributes(flat, viewProjection));
}

TEST(PortalAreaTests, ThePortalOffToOneSideStillHasItsOwnArea)
{
    // NDC is not clipped by this function: a portal outside the frustum has a perfectly good area
    // somewhere off the edge of the screen, and it is the FRUSTUM's job to have rejected it. What
    // must not happen is an off-screen portal reporting zero and being culled twice for the same
    // reason -- which would hide a frustum bug behind an area one.
    const FirstPersonCamera camera = Camera();
    const Matrix viewProjection = ViewProjection(camera);
    const std::vector<Vector3> aside = {Vector3(19.55F, 0.0F, -3.0F),
                                        Vector3(20.45F, 0.0F, -3.0F),
                                        Vector3(20.45F, 2.04F, -3.0F),
                                        Vector3(19.55F, 2.04F, -3.0F)};
    EXPECT_GT(NdcArea(aside, viewProjection), 0.0F);
}
