// SPDX-License-Identifier: MIT
//
// `HOUSE-00541`. The one check neither side can make alone: a `collision.bin` written by
// `tools/world/build_collision.py` is read by `src/physics/CollisionLoader.cpp`, and what comes
// back out is what went in.
//
// `CollisionLoaderTests` builds its bytes by hand, which makes it an excellent test of the READER
// and no test at all of the two agreeing. `HOUSE-00225` is the standing lesson: a reader and a
// writer each tested against their own hand-written fixtures both passed while producing and
// expecting different bytes. A writer that emitted `halfExtents` before `centre`, or a bucket's
// entries as global rather than local indices, would pass everything else here and stop the player
// in the wrong place.
//
// The fixture is generated at BUILD time into the build tree (`tests/CMakeLists.txt`) rather than
// committed, for the same reason no compiled content is committed.
#include <memory>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "System/IO/FileAccess.hpp"
#include "System/IO/FileMode.hpp"
#include "System/IO/FileStream.hpp"

#include "cnahouse/physics/CollisionLoader.hpp"

namespace
{
    using cnahouse::physics::CollisionKind;
    using cnahouse::physics::CollisionLoader;
    using cnahouse::physics::CollisionWorld;

    /// The facts `build_collision.py`'s `fixture_world` authors. Written out here rather than read
    /// from the tool's own output, because a test that asked the writer what it wrote and then
    /// checked the reader agreed would pass with both of them wrong in the same way.
    constexpr const char* kWorldHash = "0123456789abcdef0123456789abcdef";
    constexpr float kYaw = 0.7853982f;

    CollisionWorld Load()
    {
        System::IO::FileStream stream(std::string(CNAHOUSE_TEST_COLLISION_FIXTURE),
                                      System::IO::FileMode::Open,
                                      System::IO::FileAccess::Read);
        auto world = CollisionLoader::Read(stream, CNAHOUSE_TEST_COLLISION_FIXTURE);
        EXPECT_TRUE(world) << (world ? std::string() : world.Error().Message());
        return world ? std::move(*world) : CollisionWorld{};
    }

} // namespace

TEST(CollisionRoundTripTests, TheWriterAndTheReaderAgreeOnTheHeader)
{
    const CollisionWorld world = Load();
    EXPECT_EQ(world.worldHash, kWorldHash);
    EXPECT_FLOAT_EQ(world.gridCell, 1.0f);
    // Four: the two the shapes name, plus the two §3.5's ground names through the same table.
    ASSERT_EQ(world.surfaces.size(), 4u);
    EXPECT_EQ(world.surfaces[0], "tile");
    EXPECT_EQ(world.surfaces[1], "wood");
    EXPECT_EQ(world.surfaces[2], "grass");
    EXPECT_EQ(world.surfaces[3], "gravel");
    ASSERT_EQ(world.obbs.size(), 2u);
    ASSERT_EQ(world.meshes.size(), 1u);
    ASSERT_EQ(world.cells.size(), 1u);
    EXPECT_EQ(world.ShapeCount(), 3u);
}

TEST(CollisionRoundTripTests, AnObbIsCentreThenHalfExtentsThenYaw)
{
    // Three different half-extents and a yaw that is not zero, on purpose: a writer that emitted
    // the half-extents first, or dropped the yaw, lands on a wrong number here rather than on a
    // plausible one.
    const CollisionWorld world = Load();
    const auto& floor = world.obbs[0];
    EXPECT_FLOAT_EQ(floor.centre.X, 1.0f);
    EXPECT_FLOAT_EQ(floor.centre.Y, 0.5f);
    EXPECT_FLOAT_EQ(floor.centre.Z, 2.0f);
    EXPECT_FLOAT_EQ(floor.halfExtents.X, 0.25f);
    EXPECT_FLOAT_EQ(floor.halfExtents.Y, 0.5f);
    EXPECT_FLOAT_EQ(floor.halfExtents.Z, 1.5f);
    EXPECT_FLOAT_EQ(floor.yaw, kYaw) << "the yaw is RADIANS about +Y, not degrees";
    EXPECT_EQ(floor.kind, CollisionKind::Floor);
    EXPECT_EQ(world.SurfaceName(floor.surface), "tile");

    const auto& wall = world.obbs[1];
    EXPECT_EQ(wall.kind, CollisionKind::Wall) << "both OBBs came back with the same kind";
    EXPECT_EQ(world.SurfaceName(wall.surface), "wood");
    EXPECT_FLOAT_EQ(wall.yaw, 0.0f);
}

TEST(CollisionRoundTripTests, AMeshCarriesItsOwnBoundsKindAndTriangles)
{
    const CollisionWorld world = Load();
    const auto& mesh = world.meshes[0];
    EXPECT_EQ(mesh.kind, CollisionKind::Stair)
        << "a mesh's kind is its own, not the kind of the OBBs before it";
    EXPECT_EQ(world.SurfaceName(mesh.surface), "wood");
    ASSERT_EQ(mesh.vertices.size(), 4u);
    ASSERT_EQ(mesh.TriangleCount(), 2u);
    EXPECT_FLOAT_EQ(mesh.vertices[3].Z, 2.5f);
    const std::vector<std::uint16_t> expected = {0, 1, 2, 0, 2, 3};
    EXPECT_EQ(mesh.indices, expected);
    // The bounds are WRITTEN, not recomputed, and they are the corners of those four vertices.
    EXPECT_FLOAT_EQ(mesh.bounds.Min.X, 0.0f);
    EXPECT_FLOAT_EQ(mesh.bounds.Max.X, 1.0f);
    EXPECT_FLOAT_EQ(mesh.bounds.Max.Y, 0.75f);
    EXPECT_FLOAT_EQ(mesh.bounds.Max.Z, 2.5f);
}

TEST(CollisionRoundTripTests, ACellsGridSurvivesWithItsOriginAndItsBuckets)
{
    const CollisionWorld world = Load();
    const auto* cell = world.Cell("L0_FIXTURE");
    ASSERT_NE(cell, nullptr);
    EXPECT_EQ(world.Cell("NOT_A_CELL"), nullptr);
    // 2 x 3 and not 3 x 2: a writer or reader that swapped them would index the grid transposed
    // and still fill exactly six buckets.
    EXPECT_EQ(cell->nx, 2u);
    EXPECT_EQ(cell->nz, 3u);
    EXPECT_FLOAT_EQ(cell->originX, -1.1f);
    EXPECT_FLOAT_EQ(cell->originZ, -1.5f);
    ASSERT_EQ(cell->buckets.size(), 6u);
    const std::vector<std::uint32_t> shapes = {0u, 1u, 2u};
    EXPECT_EQ(cell->shapes, shapes);

    // Bucket entries are LOCAL indices into the cell's own list. Here the two happen to coincide,
    // and the count and placement are what this asserts.
    EXPECT_EQ(cell->buckets[0].size(), 2u);
    EXPECT_EQ(cell->buckets[3].size(), 1u);
    EXPECT_TRUE(cell->buckets[1].empty());
    EXPECT_TRUE(cell->buckets[5].empty());
    EXPECT_EQ(cell->buckets[3][0], 2u);
}

TEST(CollisionRoundTripTests, ABucketIsFoundByWhereAPointIs)
{
    const CollisionWorld world = Load();
    const auto* cell = world.Cell("L0_FIXTURE");
    ASSERT_NE(cell, nullptr);
    // The origin corner is bucket 0, which holds two shapes.
    const auto* corner = cell->BucketAt(-1.0f, -1.4f);
    ASSERT_NE(corner, nullptr);
    EXPECT_EQ(corner->size(), 2u);
    // One metre further along z is bucket 2 (row 1, column 0), which is empty.
    const auto* along = cell->BucketAt(-1.0f, -0.4f);
    ASSERT_NE(along, nullptr);
    EXPECT_TRUE(along->empty());
    // ...and one metre further still, plus one in x, is bucket 5.
    EXPECT_NE(cell->BucketAt(0.5f, 0.6f), nullptr);
    // Outside the grid is refused rather than clamped: a point left of the origin folded onto
    // column 0 would test the wrong shapes and look like a wall that follows you.
    EXPECT_EQ(cell->BucketAt(-9.0f, -1.4f), nullptr);
    EXPECT_EQ(cell->BucketAt(-1.0f, -9.0f), nullptr);
    EXPECT_EQ(cell->BucketAt(9.0f, -1.4f), nullptr);
    EXPECT_EQ(cell->BucketAt(-1.0f, 9.0f), nullptr);
}

TEST(CollisionRoundTripTests, TheWholeHouseReadsBackWhenItHasBeenBuilt)
{
    // The fixture is three shapes. `build_collision.py` over the real layout writes more than a
    // thousand, and the one thing three cannot show is a reader that copes with a small file and
    // not a real one -- a cell with 160 000 buckets, a shape index that needs all 32 bits, a
    // surface table with more than one entry in use.
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
    EXPECT_GT(world->obbs.size(), 500u);
    EXPECT_GT(world->cells.size(), 50u);
    EXPECT_FALSE(world->surfaces.empty());
    EXPECT_FLOAT_EQ(world->gridCell, 1.0f);

    std::size_t withGeometry = 0;
    for (const auto& cell : world->cells)
    {
        withGeometry += cell.shapes.empty() ? 0u : 1u;
        // Every bucket entry of every cell of the real house is a local index into that cell's
        // own list. The loader enforces it; this is the real data satisfying it.
        for (const auto& bucket : cell.buckets)
        {
            for (const std::uint16_t local : bucket)
            {
                ASSERT_LT(local, cell.shapes.size()) << cell.id;
            }
        }
    }
    EXPECT_EQ(withGeometry, world->cells.size());
    EXPECT_GT(world->TriangleCount(), 0u);

    // §11.5's ground came along with it: 81 x 65 samples a metre apart over §10.3's playable area.
    ASSERT_TRUE(world->terrain.present);
    EXPECT_EQ(world->terrain.samplesX, 81u);
    EXPECT_EQ(world->terrain.samplesZ, 65u);
    EXPECT_FLOAT_EQ(world->terrain.step, 1.0f);
    EXPECT_FLOAT_EQ(world->terrain.originX, -40.0f);
    EXPECT_FLOAT_EQ(world->terrain.originZ, -52.0f);
    EXPECT_EQ(world->terrain.heights.size(), 81u * 65u);
    EXPECT_EQ(world->terrain.materialIndex.size(), 81u * 65u);
    EXPECT_EQ(world->terrain.materials.size(), 8u);
}

TEST(CollisionRoundTripTests, TheGroundIsWhatTheWriterWrote)
{
    // §3.5 (`HOUSE-00553`). Three samples by two, so `samplesX` and `samplesZ` cannot be swapped
    // unnoticed, a step that is NOT the world grid's 1.0, heights that are not a plane, and two
    // materials of which the second is not the first entry of the surface table.
    const CollisionWorld world = Load();
    const auto& terrain = world.terrain;
    ASSERT_TRUE(terrain.present);
    EXPECT_EQ(terrain.samplesX, 3u);
    EXPECT_EQ(terrain.samplesZ, 2u);
    EXPECT_FLOAT_EQ(terrain.originX, -1.0f);
    EXPECT_FLOAT_EQ(terrain.originZ, -2.0f);
    EXPECT_FLOAT_EQ(terrain.step, 2.0f);
    EXPECT_FLOAT_EQ(terrain.MaxX(), 3.0f);
    EXPECT_FLOAT_EQ(terrain.MaxZ(), 0.0f);

    // Row-major with z OUTER: the second row is the far one, and a reader that transposed would
    // read 0.5 here.
    ASSERT_EQ(terrain.heights.size(), 6u);
    EXPECT_FLOAT_EQ(terrain.Height(0u, 0u), 0.0f);
    EXPECT_FLOAT_EQ(terrain.Height(2u, 0u), 0.5f);
    EXPECT_FLOAT_EQ(terrain.Height(0u, 1u), 1.0f);
    EXPECT_FLOAT_EQ(terrain.Height(2u, 1u), 2.0f);

    // Out of range clamps to the edge rather than reading past the end.
    EXPECT_FLOAT_EQ(terrain.Height(9u, 9u), 2.0f);

    // Materials resolve through the SHARED surface table, so a footstep on the lawn and a
    // footstep on a tiled floor ask one question of one table.
    ASSERT_EQ(terrain.materials.size(), 2u);
    EXPECT_EQ(world.SurfaceName(terrain.Material(0u, 0u)), "grass");
    EXPECT_EQ(world.SurfaceName(terrain.Material(2u, 0u)), "gravel");
    EXPECT_EQ(world.SurfaceName(terrain.Material(1u, 1u)), "grass");
}
