// SPDX-License-Identifier: MIT
//
// `HOUSE-00564`. §10's fifth and last containment layer: *"a final invisible boundary at the
// playable-volume box, 6 m beyond every believable barrier, as a safety net. Crossing it is
// impossible in normal play; if it is ever touched, a debug counter increments so tests can detect
// a gap in the real barriers."*
//
// The counter is the point. The fence, the hedges, the road's termination and the planted terrain
// are what actually keep the player in; this box is where a gap in THEM becomes visible, and
// `HOUSE-00618` asserts the count is still zero after twenty minutes of walking.
#include <memory>
#include <string>

#include <gtest/gtest.h>

#include "System/IO/FileAccess.hpp"
#include "System/IO/FileMode.hpp"
#include "System/IO/FileStream.hpp"

#include "cnahouse/physics/CollisionLoader.hpp"
#include "cnahouse/player/BoundaryGuard.hpp"

namespace
{
    using cnahouse::physics::CollisionLoader;
    using cnahouse::player::BoundaryGuard;
    using cnahouse::player::PlayableVolume;
    using Vector3 = Microsoft::Xna::Framework::Vector3;

} // namespace

TEST(BoundaryGuardTests, TheBoxIsSectionTenPointThreesPlayableVolume)
{
    const PlayableVolume volume;
    EXPECT_FLOAT_EQ(volume.minX, -40.0F);
    EXPECT_FLOAT_EQ(volume.maxX, 40.0F);
    EXPECT_FLOAT_EQ(volume.minY, -3.5F);
    EXPECT_FLOAT_EQ(volume.maxY, 20.0F);
    EXPECT_FLOAT_EQ(volume.minZ, -52.0F);
    EXPECT_FLOAT_EQ(volume.maxZ, 12.0F);

    // §10.3's own numbers say it is 6 m clear of the fenced property on every side, which is what
    // "6 m beyond every believable barrier" means for the layer inside it.
    EXPECT_LE(volume.minX, -22.5F - 6.0F);
    EXPECT_GE(volume.maxX, 22.5F + 6.0F);
    EXPECT_LE(volume.minZ, -48.0F - 4.0F);
}

TEST(BoundaryGuardTests, NormalPlayNeverTouchesIt)
{
    // Inside, at the corners, and on the faces: none of it counts. A boundary that fired on the
    // boundary itself would make `HOUSE-00618`'s "still zero" assertion impossible to satisfy.
    BoundaryGuard guard;
    const PlayableVolume v = guard.Volume();
    for (Vector3 point : {Vector3(0.0F, 1.0F, -20.0F),
                          Vector3(v.minX, v.minY, v.minZ),
                          Vector3(v.maxX, v.maxY, v.maxZ),
                          Vector3(v.minX, 0.0F, 0.0F),
                          Vector3(0.0F, v.maxY, 0.0F)})
    {
        Vector3 moved = point;
        EXPECT_FALSE(guard.Contain(moved)) << point.X << ", " << point.Y << ", " << point.Z;
        EXPECT_FLOAT_EQ(moved.X, point.X);
        EXPECT_FLOAT_EQ(moved.Y, point.Y);
        EXPECT_FLOAT_EQ(moved.Z, point.Z);
    }
    EXPECT_EQ(guard.Escapes(), 0u);
    EXPECT_FALSE(guard.Outside());
}

TEST(BoundaryGuardTests, CrossingItIsCountedAndUndone)
{
    // A net that only took attendance would not be one: a player who has found a gap keeps going,
    // and the further they get the less recoverable the state is. So it clamps as well as counts.
    BoundaryGuard guard;
    Vector3 escaped(45.0F, 1.0F, -20.0F);
    EXPECT_TRUE(guard.Contain(escaped));
    EXPECT_EQ(guard.Escapes(), 1u);
    EXPECT_TRUE(guard.Outside());
    EXPECT_FLOAT_EQ(escaped.X, 40.0F) << "it was counted but not put back";
    EXPECT_FLOAT_EQ(escaped.Y, 1.0F) << "an axis that was inside was moved";
    EXPECT_FLOAT_EQ(escaped.Z, -20.0F);

    // Every axis, in both directions, and one corner where all three are out at once.
    BoundaryGuard each;
    for (const Vector3 point : {Vector3(-41.0F, 0.0F, 0.0F),
                                Vector3(0.0F, -4.0F, 0.0F),
                                Vector3(0.0F, 21.0F, 0.0F),
                                Vector3(0.0F, 0.0F, -53.0F),
                                Vector3(0.0F, 0.0F, 13.0F),
                                Vector3(-99.0F, -99.0F, -99.0F)})
    {
        Vector3 moved = point;
        EXPECT_TRUE(each.Contain(moved));
        EXPECT_TRUE(each.Volume().Contains(moved)) << "it was clamped to somewhere still outside";
        // Bring it back inside so the next one counts as a fresh crossing.
        Vector3 home(0.0F, 0.0F, -20.0F);
        each.Contain(home);
    }
    EXPECT_EQ(each.Escapes(), 6u);
}

TEST(BoundaryGuardTests, HoldingAgainstItIsOneEscapeAndNotAHundredAndTwenty)
{
    // The counter is an EDGE, for the same reason `CellEntered` is. A body pressed against the
    // boundary for a second is one gap in the barriers, not 120 of them, and `HOUSE-00618`'s
    // assertion is only meaningful if one escape counts as one.
    BoundaryGuard guard;
    for (int i = 0; i < 120; ++i)
    {
        Vector3 pushing(41.0F, 1.0F, -20.0F);
        guard.Contain(pushing);
    }
    EXPECT_EQ(guard.Escapes(), 1u);

    // Coming back inside and leaving again is a second one.
    Vector3 home(0.0F, 1.0F, -20.0F);
    guard.Contain(home);
    EXPECT_FALSE(guard.Outside());
    Vector3 again(41.0F, 1.0F, -20.0F);
    guard.Contain(again);
    EXPECT_EQ(guard.Escapes(), 2u);
}

TEST(BoundaryGuardTests, TheBoxAgreesWithTheGroundTheHouseActuallyHas)
{
    // §10.3's playable volume and §11.5's height field are two statements of the same extent, and
    // they are in two different places -- one in the architecture, one in `layout.exterior.json`.
    // This is where a drift between them shows up.
    const std::string path = "content/world/collision.bin";
    System::IO::FileStream* probe = nullptr;
    try
    {
        probe = new System::IO::FileStream(path, System::IO::FileMode::Open, System::IO::FileAccess::Read);
    }
    catch (const std::exception&)
    {
        GTEST_SKIP() << "no " << path << "; run tools/ci/build_content.py --only world";
    }
    const std::unique_ptr<System::IO::FileStream> stream(probe);
    const auto world = CollisionLoader::Read(*stream, path);
    ASSERT_TRUE(world) << world.Error().Message();
    ASSERT_TRUE(world->terrain.present);

    const PlayableVolume volume;
    EXPECT_FLOAT_EQ(world->terrain.originX, volume.minX);
    EXPECT_FLOAT_EQ(world->terrain.MaxX(), volume.maxX);
    EXPECT_FLOAT_EQ(world->terrain.originZ, volume.minZ);
    EXPECT_FLOAT_EQ(world->terrain.MaxZ(), volume.maxZ);

    // ...and it contains everywhere the player can actually GO -- §10.3's fenced property and its
    // accessible road corridor -- with room to spare, which is what makes it a safety net rather
    // than a wall met in ordinary play.
    for (const float x : {-22.5F, 22.5F, -35.0F, 35.0F})
    {
        EXPECT_GT(x, volume.minX);
        EXPECT_LT(x, volume.maxX);
    }
    for (const float z : {-48.0F, 0.0F, 11.5F})
    {
        EXPECT_GT(z, volume.minZ);
        EXPECT_LT(z, volume.maxZ);
    }

    // Geometry OUTSIDE it is expected and is not a defect: §10 says "the road geometry continues
    // visually far beyond", and §10.3 gives the neighbourhood shell as ±220 m. Measured here:
    // `EXT_WORLD` spans ±200 m and `EXT_ROAD` reaches z = +13.40 against the boundary's +12. What
    // must hold is the other direction -- that nothing the player can WALK on needs the boundary
    // to stop them, which is what `HOUSE-00618`'s count of zero will say.
    //
    // Three until `HOUSE-00774`, when the cell boundaries between two open yards stopped being
    // walls: `EXT_NORTHSTRIP` was outside only because of the one it shared with `EXT_WORLD`, at
    // z = -52.075 against -52.0, and its own floor stops exactly on the boundary.
    std::size_t beyond = 0;
    for (const auto& cell : world->cells)
    {
        if (cell.shapes.empty())
        {
            continue;
        }
        if (cell.bounds.Min.X < volume.minX || cell.bounds.Max.X > volume.maxX ||
            cell.bounds.Min.Z < volume.minZ || cell.bounds.Max.Z > volume.maxZ)
        {
            ++beyond;
        }
    }
    EXPECT_EQ(beyond, 2u) << "the set of cells with geometry outside the playable volume changed";
}
