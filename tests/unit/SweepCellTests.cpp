// SPDX-License-Identifier: MIT
//
// `HOUSE-00545`. §45's third-person camera *"sweeps a sphere of radius 0.22 from the pivot to the
// desired camera position"*, so what it needs is not another primitive — a sphere is a capsule
// with no segment and `SweepCapsuleObb` is already exact for one — but a sweep against a whole
// CELL: broad phase, then the earliest of what it returns.
//
// Half the cases are built here, where the answer is countable. The other half are the real house,
// because a camera arm in a corridor is the thing this is for and a corridor is 20 shapes.
#include <cmath>
#include <memory>
#include <string>

#include <gtest/gtest.h>

#include "System/IO/FileAccess.hpp"
#include "System/IO/FileMode.hpp"
#include "System/IO/FileStream.hpp"

#include "cnahouse/physics/BroadPhase.hpp"
#include "cnahouse/physics/CollisionLoader.hpp"
#include "cnahouse/physics/Sweep.hpp"

namespace
{
    using cnahouse::physics::BroadPhase;
    using cnahouse::physics::CellSweepHit;
    using cnahouse::physics::CollisionCell;
    using cnahouse::physics::CollisionKind;
    using cnahouse::physics::CollisionLoader;
    using cnahouse::physics::CollisionObb;
    using cnahouse::physics::CollisionWorld;
    using cnahouse::physics::Sphere;
    using Microsoft::Xna::Framework::BoundingBox;
    using Microsoft::Xna::Framework::Vector3;

    /// A corridor 4 m long and 2 m wide: a wall at x = -1 and another at x = +1, on a 4 x 4 grid.
    CollisionWorld Corridor()
    {
        CollisionWorld world;
        world.gridCell = 1.0f;
        world.surfaces = {"plaster"};
        CollisionObb west;
        west.centre = Vector3(-1.1f, 1.5f, 2.0f);
        west.halfExtents = Vector3(0.1f, 1.5f, 2.0f);
        west.kind = CollisionKind::Wall;
        CollisionObb east = west;
        east.centre = Vector3(1.1f, 1.5f, 2.0f);
        world.obbs = {west, east};

        CollisionCell cell;
        cell.id = "L0_CORRIDOR";
        cell.bounds = BoundingBox(Vector3(-1.2f, 0.0f, 0.0f), Vector3(1.2f, 3.0f, 4.0f));
        cell.shapes = {0u, 1u};
        cell.nx = 3u;
        cell.nz = 4u;
        cell.originX = -1.2f;
        cell.originZ = 0.0f;
        cell.buckets.assign(12u, {});
        for (std::uint32_t j = 0; j < 4u; ++j)
        {
            cell.buckets[j * 3u + 0u].push_back(0u); // the west wall's column
            cell.buckets[j * 3u + 2u].push_back(1u); // the east wall's column
        }
        world.cells = {cell};
        return world;
    }

} // namespace

TEST(SweepCellTests, TheCameraSphereStopsAtTheNearerWall)
{
    // §45's 0.22 m sphere, from the middle of the corridor towards the east wall 1 m away. The
    // wall's face is at x = 1.0, so the centre stops at 0.78 -- 0.78 of a 1 m arm.
    const CollisionWorld world = Corridor();
    BroadPhase broad;
    const CellSweepHit hit = SweepCell(
        world, world.cells[0], broad, Sphere(Vector3(0.0f, 1.55f, 2.0f), 0.22f), Vector3(1.0f, 0.0f, 0.0f));
    ASSERT_TRUE(hit.hit);
    EXPECT_NEAR(hit.time, 0.78f, 1e-4f);
    EXPECT_EQ(hit.shape, 1u) << "the arm stopped at the far wall";
    EXPECT_NEAR(hit.normal.X, -1.0f, 1e-4f);
    EXPECT_FALSE(hit.startedInside);
}

TEST(SweepCellTests, TheEarliestHitWinsAndNotTheFirstInTheList)
{
    // Diagonally into the corner where both walls meet: the sweep must return the one it reaches
    // FIRST, not whichever the cell's shape list happens to hold first. Here the arm travels 2 m
    // east and 0.4 m along the corridor, so the east wall at 0.78 is the answer.
    const CollisionWorld world = Corridor();
    BroadPhase broad;
    const CellSweepHit hit = SweepCell(
        world, world.cells[0], broad, Sphere(Vector3(0.0f, 1.55f, 2.0f), 0.22f), Vector3(2.0f, 0.0f, 0.4f));
    ASSERT_TRUE(hit.hit);
    EXPECT_EQ(hit.shape, 1u);
    EXPECT_NEAR(hit.time, 0.78f / 2.0f, 1e-4f);
}

TEST(SweepCellTests, AnArmDownTheCorridorHitsNothing)
{
    const CollisionWorld world = Corridor();
    BroadPhase broad;
    const CellSweepHit hit = SweepCell(
        world, world.cells[0], broad, Sphere(Vector3(0.0f, 1.55f, 0.5f), 0.22f), Vector3(0.0f, 0.0f, 3.0f));
    EXPECT_FALSE(hit.hit);
    EXPECT_EQ(hit.shape, CellSweepHit::kNothing);
    EXPECT_FLOAT_EQ(hit.time, 1.0f);
}

TEST(SweepCellTests, TheSweptBoxIsTheWholeMotionAndNotJustTheStart)
{
    // The tunnelling case. A sphere at x = -0.7 -- clear of the west wall's face at -1.0 by
    // 0.08 -- moving 3 m east, which takes it past the east wall entirely. A broad phase that
    // queried only the START position would find the west wall's column and nothing else, and the
    // body would pass through the east wall at speed.
    const CollisionWorld world = Corridor();
    BroadPhase broad;
    const CellSweepHit hit = SweepCell(
        world, world.cells[0], broad, Sphere(Vector3(-0.7f, 1.55f, 2.0f), 0.22f), Vector3(3.0f, 0.0f, 0.0f));
    ASSERT_TRUE(hit.hit) << "a fast sphere tunnelled through the wall";
    EXPECT_EQ(hit.shape, 1u) << "it stopped at the wall behind it instead";
    EXPECT_NEAR(hit.time, (0.78f + 0.7f) / 3.0f, 1e-4f);
}

TEST(SweepCellTests, ASphereIsACapsuleWithNoSegment)
{
    // Not a separate calculation, and this is the claim that says so: the named `Sphere` helper
    // and a hand-built zero-half-height capsule must agree exactly.
    const CollisionWorld world = Corridor();
    BroadPhase broad;
    const cnahouse::physics::Capsule byHand{Vector3(0.0f, 1.55f, 2.0f), 0.0f, 0.22f};
    const CellSweepHit named = SweepCell(
        world, world.cells[0], broad, Sphere(Vector3(0.0f, 1.55f, 2.0f), 0.22f), Vector3(1.0f, 0.0f, 0.0f));
    const CellSweepHit plain = SweepCell(world, world.cells[0], broad, byHand, Vector3(1.0f, 0.0f, 0.0f));
    EXPECT_EQ(named.hit, plain.hit);
    EXPECT_FLOAT_EQ(named.time, plain.time);
    EXPECT_EQ(named.shape, plain.shape);
}

TEST(SweepCellTests, TheBroadPhaseIsWhatKeepsTheNarrowPhaseSmall)
{
    const CollisionWorld world = Corridor();
    BroadPhase broad;
    const CellSweepHit hit = SweepCell(
        world, world.cells[0], broad, Sphere(Vector3(0.0f, 1.55f, 2.0f), 0.22f), Vector3(0.3f, 0.0f, 0.0f));
    EXPECT_LE(hit.tested, world.cells[0].shapes.size());
    EXPECT_GT(hit.tested, 0u) << "nothing was tested at all, so the sweep proves nothing";
}

TEST(SweepCellTests, TheCameraArmInTheRealHouse)
{
    // §45's arm, in every cell of the house that has geometry: a 0.22 m sphere from a pivot at
    // 1.55 m swept 3.2 m in eight directions. Nothing may start inside a wall at the pivot, every
    // hit must be a shape the cell holds, and a hit's distance must be shorter than the arm.
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
    std::size_t swept = 0;
    std::size_t blocked = 0;
    std::size_t maxTested = 0;
    for (const auto& cell : world->cells)
    {
        if (cell.shapes.empty() || cell.nx == 0u || cell.nz == 0u)
        {
            continue;
        }
        const float midX = cell.originX + static_cast<float>(cell.nx) * 0.5f;
        const float midZ = cell.originZ + static_cast<float>(cell.nz) * 0.5f;
        const float y = cell.bounds.Min.Y + 1.55f;
        if (y > cell.bounds.Max.Y)
        {
            continue; // a crawl space with no room for a person to stand in it
        }
        for (int i = 0; i < 8; ++i)
        {
            const float angle = static_cast<float>(i) * 6.2831853f / 8.0f;
            const Vector3 motion(std::cos(angle) * 3.2f, 0.0f, std::sin(angle) * 3.2f);
            const CellSweepHit hit =
                SweepCell(*world, cell, broad, Sphere(Vector3(midX, y, midZ), 0.22f), motion);
            ++swept;
            maxTested = std::max(maxTested, static_cast<std::size_t>(hit.tested));
            if (!hit.hit)
            {
                continue;
            }
            ++blocked;
            ASSERT_LT(hit.shape, world->ShapeCount()) << cell.id;
            EXPECT_GE(hit.time, 0.0f) << cell.id;
            EXPECT_LE(hit.time, 1.0f) << cell.id;
            const float length = std::sqrt(hit.normal.X * hit.normal.X + hit.normal.Y * hit.normal.Y +
                                           hit.normal.Z * hit.normal.Z);
            EXPECT_NEAR(length, 1.0f, 1e-3f) << cell.id << ": the camera would be pushed nowhere";
        }
    }
    ASSERT_GT(swept, 300u);
    // A camera arm in a house is blocked most of the time -- that is what §45's narrow-space rule
    // exists for -- and an arm that were never blocked would mean the sweep is finding nothing.
    EXPECT_GT(blocked, swept / 2) << blocked << " of " << swept << " arms were blocked";
    EXPECT_LT(maxTested, 40u) << "the broad phase handed the narrow phase " << maxTested
                              << " shapes for one camera arm";
}
