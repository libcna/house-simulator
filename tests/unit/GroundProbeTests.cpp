// SPDX-License-Identifier: MIT
//
// `HOUSE-00548`. §49.3 step 3: *"GroundProbe: a short downward sweep; sets onGround, groundNormal,
// surfaceKind, cellId"*. It is the step that decides whether the body is walking or falling, which
// footstep to play, and which cell it is in — so the interesting cases are the ones where the
// answer is not simply yes or no: a body a hair over the floor, one on a slope too steep to stand
// on, one over a hole, and one on the lawn rather than on a shape.
#include <cmath>
#include <memory>
#include <string>

#include <gtest/gtest.h>

#include "System/IO/FileAccess.hpp"
#include "System/IO/FileMode.hpp"
#include "System/IO/FileStream.hpp"

#include "cnahouse/physics/BroadPhase.hpp"
#include "cnahouse/physics/CollisionLoader.hpp"
#include "cnahouse/physics/Ground.hpp"

namespace
{
    using cnahouse::physics::BroadPhase;
    using cnahouse::physics::Capsule;
    using cnahouse::physics::CollisionCell;
    using cnahouse::physics::CollisionKind;
    using cnahouse::physics::CollisionLoader;
    using cnahouse::physics::CollisionMesh;
    using cnahouse::physics::CollisionObb;
    using cnahouse::physics::CollisionTerrain;
    using cnahouse::physics::CollisionWorld;
    using cnahouse::physics::GroundProbe;
    using cnahouse::physics::GroundProbeResult;
    using cnahouse::physics::kGroundProbeReach;
    using cnahouse::physics::kStepDownHeight;
    using cnahouse::physics::OverlapCell;
    using cnahouse::physics::Sphere;
    using Microsoft::Xna::Framework::BoundingBox;
    using Microsoft::Xna::Framework::Vector3;

    constexpr float kBodyRadius = 0.30F;
    constexpr float kBodyHalfHeight = 0.60F;
    constexpr float kStand = kBodyHalfHeight + kBodyRadius;

    CollisionObb
    Box(const Vector3& centre, const Vector3& half, CollisionKind kind, std::uint16_t surface = 0u)
    {
        CollisionObb obb;
        obb.centre = centre;
        obb.halfExtents = half;
        obb.kind = kind;
        obb.surface = surface;
        return obb;
    }

    /// A slab whose top is at @p top, spanning x in [@p x0, @p x1].
    CollisionObb Slab(float top, float x0, float x1, CollisionKind kind, std::uint16_t surface = 0u)
    {
        return Box(Vector3((x0 + x1) * 0.5F, top - 0.25F, 0.0F),
                   Vector3((x1 - x0) * 0.5F, 0.25F, 6.0F),
                   kind,
                   surface);
    }

    CollisionTerrain Lawn(float height, std::uint16_t material)
    {
        CollisionTerrain terrain;
        terrain.present = true;
        terrain.samplesX = 8u;
        terrain.samplesZ = 8u;
        terrain.originX = -4.0F;
        terrain.originZ = -4.0F;
        terrain.step = 1.0F;
        terrain.heights.assign(64u, height);
        terrain.materials = {material};
        terrain.materialIndex.assign(64u, 0u);
        return terrain;
    }

    CollisionWorld OneCell(std::vector<CollisionObb> obbs,
                           std::vector<CollisionMesh> meshes = {},
                           CollisionTerrain terrain = {})
    {
        CollisionWorld world;
        world.gridCell = 1.0F;
        world.surfaces = {"tile", "wood", "grass"};
        world.obbs = std::move(obbs);
        world.meshes = std::move(meshes);
        world.terrain = std::move(terrain);

        CollisionCell cell;
        cell.id = "L0_PROBE";
        cell.bounds = BoundingBox(Vector3(-8.0F, -8.0F, -8.0F), Vector3(8.0F, 8.0F, 8.0F));
        cell.nx = 16u;
        cell.nz = 16u;
        cell.originX = -8.0F;
        cell.originZ = -8.0F;
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

    Capsule Body(float x, float feet)
    {
        return Capsule{Vector3(x, feet + kStand, 0.0F), kBodyHalfHeight, kBodyRadius};
    }

} // namespace

TEST(GroundProbeTests, TheReachSitsBetweenTheTwoThingsItHasTo)
{
    // Above the largest gap the fixed step can leave under a body that IS standing -- the
    // depenetration's 0.02 m push -- and well under §43.1's 0.45 m step-down, which is the
    // mechanism for a gap bigger than resting and must not be pre-empted.
    EXPECT_FLOAT_EQ(kGroundProbeReach, 0.05F);
    EXPECT_GT(kGroundProbeReach, cnahouse::physics::kDepenetrationStep);
    EXPECT_LT(kGroundProbeReach, kStepDownHeight);
}

TEST(GroundProbeTests, ABodyOnAFloorIsOnTheGroundAndKnowsWhatItIsMadeOf)
{
    const CollisionWorld world = OneCell({Slab(0.0F, -4.0F, 4.0F, CollisionKind::Floor, 1u)});
    BroadPhase broad;
    const GroundProbeResult ground = GroundProbe(world, world.cells[0], broad, Body(0.0F, 0.002F));
    EXPECT_TRUE(ground.onGround);
    EXPECT_FALSE(ground.steep);
    EXPECT_FALSE(ground.terrain);
    EXPECT_NEAR(ground.distance, 0.002F, 1e-3F);
    EXPECT_NEAR(ground.height, 0.0F, 2e-3F);
    EXPECT_NEAR(ground.normal.Y, 1.0F, 1e-4F);
    EXPECT_EQ(world.SurfaceName(ground.surface), "wood");
    EXPECT_EQ(ground.kind, CollisionKind::Floor);
    EXPECT_EQ(ground.cellId, "L0_PROBE");
}

TEST(GroundProbeTests, OverAHoleIsNotOnTheGround)
{
    // The floor stops at x = 1 and the body is at x = 2.5, which is further than its own radius
    // past the edge. Nothing within reach, so `onGround` is false and `steep` is false too --
    // over nothing is a different answer from over something unstandable.
    const CollisionWorld world = OneCell({Slab(0.0F, -4.0F, 1.0F, CollisionKind::Floor)});
    BroadPhase broad;
    const GroundProbeResult ground = GroundProbe(world, world.cells[0], broad, Body(2.5F, 0.002F));
    EXPECT_FALSE(ground.onGround);
    EXPECT_FALSE(ground.steep);
    EXPECT_EQ(ground.cellId, "L0_PROBE") << "the cell is known whether or not the ground is";
}

TEST(GroundProbeTests, WallContactCannotHideTheRealSupportingFloor)
{
    const auto wall = Box(Vector3(0.55F, 1.0F, 0.0F), Vector3(0.25F, 1.0F, 3.0F), CollisionKind::Wall);
    const auto world = OneCell({wall, Slab(0.0F, -6.0F, 6.0F, CollisionKind::Floor, 1u)});
    BroadPhase broad;
    const auto ground = GroundProbe(world, world.cells[0], broad, Body(0.0F, 0.002F));
    ASSERT_TRUE(ground.onGround);
    EXPECT_FALSE(ground.steep);
    EXPECT_EQ(ground.kind, CollisionKind::Floor);
    EXPECT_EQ(ground.surface, 1u);
    EXPECT_NEAR(ground.height, 0.0F, 1.0e-4F);
    const auto hole = OneCell({wall});
    EXPECT_FALSE(GroundProbe(hole, hole.cells[0], broad, Body(0.0F, 0.002F)).onGround);
    const auto tooFar = OneCell({wall, Slab(-0.10F, -6.0F, 6.0F, CollisionKind::Floor)});
    EXPECT_FALSE(GroundProbe(tooFar, tooFar.cells[0], broad, Body(0.0F, 0.002F)).onGround);
}

TEST(GroundProbeTests, SlowGarageStepEdgeKeepsItsWalkableSupport)
{
    const std::string path = std::string(CNAHOUSE_TEST_CONTENT_ROOT) + "/world/collision.bin";
    System::IO::FileStream stream(path, System::IO::FileMode::Open, System::IO::FileAccess::Read);
    const auto world = CollisionLoader::Read(stream, path);
    ASSERT_TRUE(world);
    const auto* cell = world->Cell("L0_GARAGE");
    ASSERT_NE(cell, nullptr);
    BroadPhase broad;
    const Capsule body{Vector3(9.087337494F, 1.345744252F, -16.36265945F), kBodyHalfHeight, kBodyRadius};
    const auto ground = GroundProbe(*world, *cell, broad, body);
    ASSERT_TRUE(ground.onGround);
    EXPECT_FALSE(ground.steep);
    EXPECT_EQ(ground.kind, CollisionKind::Stair);
    EXPECT_GT(ground.normal.Y, 0.89F);
    EXPECT_LT(ground.distance, cnahouse::physics::kGroundProbeReach);
}

TEST(GroundProbeTests, FlatStepTopDoesNotOverrideTheEstablishedWallContact)
{
    // The ramp correction must not reinterpret a doorway contact merely because
    // a flat step is under its centre. That changed the terrace slider response
    // in the full controller tour. Only a genuinely sloping stair face qualifies.
    const auto wall = Box(Vector3(0.52F, 1.0F, 0.0F), Vector3(0.25F, 1.0F, 3.0F), CollisionKind::Wall);
    const auto world = OneCell({wall, Slab(0.0F, -6.0F, 6.0F, CollisionKind::Stair, 1u)});
    BroadPhase broad;
    const auto ground = GroundProbe(world, world.cells[0], broad, Body(0.0F, 0.002F));
    EXPECT_FALSE(ground.onGround);
    EXPECT_TRUE(ground.steep);
    EXPECT_EQ(ground.kind, CollisionKind::Wall);
}

TEST(GroundProbeTests, JustOutOfReachIsNotOnTheGroundEither)
{
    // 0.05 m is the whole of the question. At 0.04 m over the floor the body is standing; at
    // 0.06 m it is falling, and §43.1's step-down is what catches it if it should not be.
    const CollisionWorld world = OneCell({Slab(0.0F, -4.0F, 4.0F, CollisionKind::Floor)});
    BroadPhase broad;
    EXPECT_TRUE(GroundProbe(world, world.cells[0], broad, Body(0.0F, 0.04F)).onGround);
    EXPECT_FALSE(GroundProbe(world, world.cells[0], broad, Body(0.0F, 0.06F)).onGround);
}

TEST(GroundProbeTests, ASlopeTooSteepToStandOnIsReportedAsItselfAndNotAsThinAir)
{
    // §43.1's 46°. A 60° bank is not a floor -- `onGround` is false -- but the body is not over
    // nothing either, and the two lead to different behaviour: it slides down the bank rather
    // than falling through it. A probe that reported "no ground" would lose that distinction.
    const float radians = 60.0F * 3.14159265F / 180.0F;
    const float rise = 6.0F * std::tan(radians);
    CollisionMesh bank;
    bank.vertices = {Vector3(-6.0F, rise, -4.0F),
                     Vector3(-6.0F, rise, 4.0F),
                     Vector3(6.0F, -rise, 4.0F),
                     Vector3(6.0F, -rise, -4.0F)};
    bank.indices = {0u, 1u, 2u, 0u, 2u, 3u};
    bank.surface = 0u;
    bank.kind = CollisionKind::Exterior;
    const CollisionWorld world = OneCell({}, {bank});
    BroadPhase broad;

    // Resting on the bank: the cap centre is `radius` from the plane along its normal.
    const float capY = (0.30F - std::sin(radians)) / std::cos(radians);
    const Capsule body{Vector3(1.0F, capY + kBodyHalfHeight + 0.01F, 0.0F), kBodyHalfHeight, kBodyRadius};
    const GroundProbeResult ground = GroundProbe(world, world.cells[0], broad, body);
    EXPECT_FALSE(ground.onGround);
    EXPECT_TRUE(ground.steep);
    EXPECT_NEAR(ground.normal.Y, std::cos(radians), 1e-3F);
}

TEST(GroundProbeTests, AStairIsGroundAndSaysThatItIsAStair)
{
    // §60 turns `SurfaceKind::Stairs` into a footstep set and a speed reduction, and this is
    // where it comes from.
    const CollisionWorld world = OneCell({Slab(0.0F, -4.0F, 4.0F, CollisionKind::Stair, 1u)});
    BroadPhase broad;
    const GroundProbeResult ground = GroundProbe(world, world.cells[0], broad, Body(0.0F, 0.002F));
    ASSERT_TRUE(ground.onGround);
    EXPECT_EQ(ground.kind, CollisionKind::Stair);
}

TEST(GroundProbeTests, TheLawnIsGroundToo)
{
    // §49.2: exterior collision is the height field PLUS the OBBs, so a body in the garden is
    // standing on §11.5's ground and its footsteps are grass.
    const CollisionWorld world = OneCell({}, {}, Lawn(0.0F, 2u));
    BroadPhase broad;
    const GroundProbeResult ground = GroundProbe(world, world.cells[0], broad, Body(0.0F, 0.002F));
    ASSERT_TRUE(ground.onGround);
    EXPECT_TRUE(ground.terrain);
    EXPECT_EQ(world.SurfaceName(ground.surface), "grass");
    EXPECT_NEAR(ground.height, 0.0F, 2e-3F);
    EXPECT_NEAR(ground.normal.Y, 1.0F, 1e-4F);
}

TEST(GroundProbeTests, ATerraceIsTheTerraceAndNotTheLawnThreeCentimetresUnderIt)
{
    // Both are within the probe's reach and only one is what the body is standing on. Taking
    // whichever was found first would put a body on a paved terrace and play it a grass footstep.
    //
    // There is also a DECOY slab, listed first and elsewhere in the cell: the surface reported has
    // to come from the shape that was hit, not from shape 0.
    const CollisionWorld world = OneCell(
        {
            Slab(0.45F, 20.0F, 24.0F, CollisionKind::Floor, 1u), // wood, nowhere near the body
            Slab(0.45F, -4.0F, 4.0F, CollisionKind::Floor, 0u),  // tile, the terrace it is on
        },
        {},
        Lawn(0.42F, 2u));
    BroadPhase broad;
    const GroundProbeResult ground = GroundProbe(world, world.cells[0], broad, Body(0.0F, 0.452F));
    ASSERT_TRUE(ground.onGround);
    EXPECT_FALSE(ground.terrain) << "it reported the lawn 30 mm under the terrace";
    EXPECT_EQ(world.SurfaceName(ground.surface), "tile") << "it reported the decoy slab's surface";
    EXPECT_NEAR(ground.height, 0.45F, 3e-3F);
    EXPECT_NEAR(ground.distance, 0.002F, 2e-3F);

    // ...and with the terrace taken away, the same body IS on the lawn -- so the case above is
    // the two answers being told apart, not the lawn being unreachable.
    const CollisionWorld open = OneCell({}, {}, Lawn(0.42F, 2u));
    const GroundProbeResult lawn = GroundProbe(open, open.cells[0], broad, Body(0.0F, 0.452F));
    ASSERT_TRUE(lawn.onGround);
    EXPECT_TRUE(lawn.terrain);
    EXPECT_EQ(open.SurfaceName(lawn.surface), "grass");
}

TEST(GroundProbeTests, ABodyRestingOnASlopeIsFoundByItsOwnSHAPEAndNotByALookup)
{
    // The reason this is a sweep. A capsule on a slope touches it UPHILL of its centre, so its
    // lowest point is not over the ground under its middle: on this 20° ramp the difference is
    // 21 mm, which is more than a third of the probe's whole reach. `HOUSE-00553` found the same
    // thing the hard way.
    const float radians = 20.0F * 3.14159265F / 180.0F;
    const float slope = std::tan(radians);
    CollisionMesh ramp;
    ramp.vertices = {Vector3(-6.0F, -6.0F * slope, -4.0F),
                     Vector3(-6.0F, -6.0F * slope, 4.0F),
                     Vector3(6.0F, 6.0F * slope, 4.0F),
                     Vector3(6.0F, 6.0F * slope, -4.0F)};
    ramp.indices = {0u, 1u, 2u, 0u, 2u, 3u};
    ramp.surface = 0u;
    ramp.kind = CollisionKind::Floor;
    const CollisionWorld world = OneCell({}, {ramp});
    BroadPhase broad;

    // Resting: the cap centre is 0.30 from the plane along its normal, which is 0.30 / cos(20°)
    // above the ground vertically -- 21 mm more than the 0.30 the flat-ground shortcut gives.
    const float over = 0.30F / std::cos(radians);
    EXPECT_NEAR(over - 0.30F, 0.0193F, 1e-3F);
    const Capsule body{Vector3(0.0F, over + kBodyHalfHeight + 0.002F, 0.0F), kBodyHalfHeight, kBodyRadius};
    const GroundProbeResult ground = GroundProbe(world, world.cells[0], broad, body);
    ASSERT_TRUE(ground.onGround) << "the sweep did not find a ramp the body is resting on";
    EXPECT_NEAR(ground.distance, 0.002F, 2e-3F);
    EXPECT_NEAR(ground.normal.Y, std::cos(radians), 1e-3F);

    // And the flat-ground shortcut really would have missed it: a body placed 0.30 m over the
    // ramp, as if it were level, is INSIDE the ramp.
    const Capsule wrong{Vector3(0.0F, 0.30F + kBodyHalfHeight, 0.0F), kBodyHalfHeight, kBodyRadius};
    EXPECT_TRUE(OverlapCell(world, world.cells[0], broad, wrong).overlapped);
}

TEST(GroundProbeTests, TheWholeRealHouseIsStoodOn)
{
    // Every cell of the house, at the height a body settles to: the probe finds ground, names a
    // surface that is in the table, gives a unit normal that faces up, and reports the cell it
    // was asked about.
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

    BroadPhase broad;
    std::size_t stood = 0;
    std::size_t onStairs = 0;
    for (const auto& cell : world->cells)
    {
        if (cell.shapes.empty() || cell.nx == 0u || cell.nz == 0u)
        {
            continue;
        }
        const float midX = cell.originX + static_cast<float>(cell.nx) * 0.5F;
        const float midZ = cell.originZ + static_cast<float>(cell.nz) * 0.5F;
        const float height = cell.bounds.Max.Y - cell.bounds.Min.Y;
        if (height < 2.0F * kStand)
        {
            continue;
        }
        // Find the floor by dropping a 50 mm probe, then stand the body 2 mm over it -- inside
        // the probe's 50 mm reach, and where the rest of §49.3 would leave it.
        const float pebble = 0.05F;
        const Capsule falling =
            Sphere(Vector3(midX, (cell.bounds.Min.Y + cell.bounds.Max.Y) * 0.5F, midZ), pebble);
        if (OverlapCell(*world, cell, broad, falling).overlapped)
        {
            continue;
        }
        const auto landed = SweepCell(*world, cell, broad, falling, Vector3(0.0F, -height, 0.0F));
        if (!landed.hit)
        {
            continue;
        }
        const float floorY = falling.centre.Y - height * landed.time - pebble;
        Capsule body{Vector3(midX, floorY + kStand + 0.002F, midZ), kBodyHalfHeight, kBodyRadius};
        if (OverlapCell(*world, cell, broad, body).overlapped)
        {
            continue; // a stair tread under its middle; `HOUSE-00547` counts these
        }

        const GroundProbeResult ground = GroundProbe(*world, cell, broad, body);
        ASSERT_TRUE(ground.onGround || ground.steep)
            << cell.id << ": a body settled onto this cell's floor was over nothing";
        ++stood;
        onStairs += ground.kind == CollisionKind::Stair ? 1u : 0u;
        EXPECT_EQ(ground.cellId, cell.id);
        EXPECT_LT(ground.surface, world->surfaces.size()) << cell.id;
        EXPECT_LE(ground.distance, 0.05F + 1e-4F) << cell.id;
        EXPECT_NEAR(ground.height, body.Bottom() - ground.distance, 1e-4F) << cell.id;
        const float length = std::sqrt(ground.normal.X * ground.normal.X + ground.normal.Y * ground.normal.Y +
                                       ground.normal.Z * ground.normal.Z);
        EXPECT_NEAR(length, 1.0F, 1e-3F) << cell.id;
        // A body standing hard against a wall has its downward sweep graze the wall's SIDE, whose
        // normal is horizontal -- which is `steep` rather than ground, and is why the height of
        // the normal is only claimed for the cells that reported ground.
        if (ground.onGround)
        {
            EXPECT_GE(ground.normal.Y, cnahouse::physics::kSlopeLimitCosine) << cell.id;
        }
        EXPECT_GE(ground.normal.Y, -1e-4F) << cell.id << ": the ground faced down";
    }
    ASSERT_GT(stood, 40u) << "only " << stood << " cells were stood in";
}
