// SPDX-License-Identifier: MIT
//
// `HOUSE-00542`. The loose grid, asked what a swept box might hit. Two halves: a cell built here
// by hand, where every answer is countable, and the REAL house, where the number that matters is
// how many shapes a sweep is handed — §49.2 promises about six rather than nine hundred, and a
// broad phase is worth having only if that is true of the house it is built for.
#include <algorithm>
#include <iostream>
#include <limits>
#include <memory>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "System/IO/FileAccess.hpp"
#include "System/IO/FileMode.hpp"
#include "System/IO/FileStream.hpp"

#include "cnahouse/physics/BroadPhase.hpp"
#include "cnahouse/physics/CollisionLoader.hpp"

namespace
{
    using cnahouse::physics::BroadPhase;
    using cnahouse::physics::CollisionCell;
    using cnahouse::physics::CollisionLoader;
    using cnahouse::physics::CollisionWorld;
    using Microsoft::Xna::Framework::BoundingBox;
    using Microsoft::Xna::Framework::Vector3;

    BoundingBox Box(float x0, float z0, float x1, float z1)
    {
        return BoundingBox(Vector3(x0, -10.0f, z0), Vector3(x1, 10.0f, z1));
    }

    /// A 3 x 2 grid at the origin holding four shapes:
    ///
    ///     j=1 |  2  |  2,3 | 3 |
    ///     j=0 | 0,1 |  1   |   |
    ///          i=0    i=1   i=2
    ///
    /// Shape 1 spans two buckets and shape 2 and 3 share one, so "returned once" and "only what
    /// overlaps" are both countable.
    CollisionCell Fixture()
    {
        CollisionCell cell;
        cell.id = "L0_FIXTURE";
        cell.bounds = BoundingBox(Vector3(0.0f, 0.0f, 0.0f), Vector3(3.0f, 2.0f, 2.0f));
        cell.shapes = {10u, 11u, 12u, 13u};
        cell.nx = 3u;
        cell.nz = 2u;
        cell.originX = 0.0f;
        cell.originZ = 0.0f;
        cell.buckets = {
            {0u, 1u},
            {1u},
            {},
            {2u},
            {2u, 3u},
            {3u},
        };
        return cell;
    }

} // namespace

TEST(BroadPhaseTests, AQueryOverTheWholeCellReturnsEveryShapeExactlyOnce)
{
    const CollisionCell cell = Fixture();
    BroadPhase broad;
    const auto& hits = broad.Query(cell, Box(-1.0f, -1.0f, 4.0f, 3.0f));
    std::vector<std::uint32_t> sorted(hits.begin(), hits.end());
    std::sort(sorted.begin(), sorted.end());
    const std::vector<std::uint32_t> expected = {10u, 11u, 12u, 13u};
    EXPECT_EQ(sorted, expected) << "a shape listed in several buckets came back several times, or "
                                   "one went missing";
    EXPECT_EQ(broad.BucketsVisited(), 6u);
    // Seven entries were walked to produce four results -- shape 1 is in two buckets, shape 2 in
    // two and shape 3 in two -- so the de-duplication is doing something rather than nothing.
    EXPECT_EQ(broad.EntriesWalked(), 7u);
}

TEST(BroadPhaseTests, TheResultsAreGlobalShapeIndices)
{
    // The caller is about to index `CollisionWorld::obbs` with these. A local index is meaningless
    // outside the cell it came from, and one that happened to be in range would be silently wrong.
    const CollisionCell cell = Fixture();
    BroadPhase broad;
    const auto& hits = broad.Query(cell, Box(0.1f, 0.1f, 0.2f, 0.2f));
    ASSERT_EQ(hits.size(), 2u);
    EXPECT_EQ(hits[0], 10u);
    EXPECT_EQ(hits[1], 11u);
}

TEST(BroadPhaseTests, AQueryInOneBucketReturnsOnlyThatBucket)
{
    const CollisionCell cell = Fixture();
    BroadPhase broad;
    const auto& hits = broad.Query(cell, Box(2.1f, 1.1f, 2.9f, 1.9f));
    ASSERT_EQ(hits.size(), 1u);
    EXPECT_EQ(hits[0], 13u);
    EXPECT_EQ(broad.BucketsVisited(), 1u) << "the grid looked in more buckets than the box covers";
}

TEST(BroadPhaseTests, AQueryStraddlingTwoBucketsGetsBoth)
{
    const CollisionCell cell = Fixture();
    BroadPhase broad;
    // 0.9 to 1.1 in x crosses the boundary at 1.0, so buckets (0,0) and (1,0).
    const auto& hits = broad.Query(cell, Box(0.9f, 0.1f, 1.1f, 0.2f));
    std::vector<std::uint32_t> sorted(hits.begin(), hits.end());
    std::sort(sorted.begin(), sorted.end());
    const std::vector<std::uint32_t> expected = {10u, 11u};
    EXPECT_EQ(sorted, expected);
    EXPECT_EQ(broad.BucketsVisited(), 2u);
}

TEST(BroadPhaseTests, AQueryOutsideTheGridReturnsNothing)
{
    // Not the nearest bucket. A body querying outside a cell has no candidates in it, and
    // answering with the nearest ones is how it gets stopped by a wall in another room.
    const CollisionCell cell = Fixture();
    BroadPhase broad;
    EXPECT_TRUE(broad.Query(cell, Box(-5.0f, -5.0f, -4.0f, -4.0f)).empty());
    EXPECT_TRUE(broad.Query(cell, Box(9.0f, 0.5f, 10.0f, 0.6f)).empty());
    EXPECT_TRUE(broad.Query(cell, Box(0.5f, 9.0f, 0.6f, 10.0f)).empty());
    EXPECT_EQ(broad.BucketsVisited(), 0u);
}

TEST(BroadPhaseTests, AQueryPartlyOutsideIsClampedAndNotWrapped)
{
    const CollisionCell cell = Fixture();
    BroadPhase broad;
    const auto& hits = broad.Query(cell, Box(-9.0f, -9.0f, 0.5f, 0.5f));
    ASSERT_EQ(hits.size(), 2u);
    EXPECT_EQ(broad.BucketsVisited(), 1u) << "the part outside the grid was folded onto a row";
}

TEST(BroadPhaseTests, AnInvertedOrNonFiniteQueryReturnsNothing)
{
    const CollisionCell cell = Fixture();
    BroadPhase broad;
    EXPECT_TRUE(broad.Query(cell, Box(2.0f, 1.0f, 1.0f, 0.0f)).empty());
    const float nan = std::numeric_limits<float>::quiet_NaN();
    EXPECT_TRUE(broad.Query(cell, Box(nan, 0.0f, 1.0f, 1.0f)).empty())
        << "a NaN compares false against everything and must not be read as the whole cell";
}

TEST(BroadPhaseTests, AnEmptyCellAnswersNothingRatherThanCrashing)
{
    CollisionCell empty;
    empty.id = "EMPTY";
    BroadPhase broad;
    EXPECT_TRUE(broad.Query(empty, Box(-1.0f, -1.0f, 1.0f, 1.0f)).empty());
}

TEST(BroadPhaseTests, TheSameObjectAnswersSuccessiveQueriesIndependently)
{
    // The de-duplication is a stamp rather than a clear, so a stale stamp would make the second
    // query drop shapes the first one saw. This is the test for that.
    const CollisionCell cell = Fixture();
    BroadPhase broad;
    const std::size_t first = broad.Query(cell, Box(-1.0f, -1.0f, 4.0f, 3.0f)).size();
    ASSERT_EQ(first, 4u);
    for (int i = 0; i < 5; ++i)
    {
        EXPECT_EQ(broad.Query(cell, Box(-1.0f, -1.0f, 4.0f, 3.0f)).size(), first)
            << "query " << i << " lost shapes the first one found";
    }
    EXPECT_EQ(broad.Query(cell, Box(2.1f, 1.1f, 2.9f, 1.9f)).size(), 1u);
    EXPECT_EQ(broad.Query(cell, Box(-1.0f, -1.0f, 4.0f, 3.0f)).size(), 4u);
}

TEST(BroadPhaseTests, TheGridEarnsItsKeepOnTheRealHouse)
{
    // §49.2's promise, measured: "a capsule sweep tests ~6 shapes, not 900". The query is a
    // player-sized box swept a step -- §70.5's 0.62 m capsule plus a 0.5 m stride -- placed at the
    // middle of every cell that has geometry. What it must beat is the cell's whole shape list,
    // and the number worth printing is how much by.
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
    std::size_t cellsTested = 0;
    std::size_t candidates = 0;
    std::size_t everything = 0;
    std::size_t worst = 0;
    std::string worstCell;
    for (const auto& cell : world->cells)
    {
        if (cell.shapes.empty() || cell.nx == 0u || cell.nz == 0u)
        {
            continue;
        }
        const float midX = cell.originX + static_cast<float>(cell.nx) * 0.5f;
        const float midZ = cell.originZ + static_cast<float>(cell.nz) * 0.5f;
        const auto& hits = broad.Query(cell, Box(midX - 0.56f, midZ - 0.56f, midX + 0.56f, midZ + 0.56f));
        ++cellsTested;
        candidates += hits.size();
        everything += cell.shapes.size();
        if (hits.size() > worst)
        {
            worst = hits.size();
            worstCell = cell.id;
        }
        // Whatever it returns must be a real shape, in this cell's list.
        for (const std::uint32_t index : hits)
        {
            ASSERT_LT(index, world->ShapeCount()) << cell.id;
            ASSERT_NE(std::find(cell.shapes.begin(), cell.shapes.end(), index), cell.shapes.end())
                << cell.id << " returned a shape it does not hold";
        }
    }
    ASSERT_GT(cellsTested, 50u);
    const double mean = static_cast<double>(candidates) / static_cast<double>(cellsTested);
    const double whole = static_cast<double>(everything) / static_cast<double>(cellsTested);
    // On the record, because §49.2's "~6 shapes, not 900" is a claim about this house and the only
    // way to know is to measure it: printed by every run, not only a failing one.
    std::cout << "[  MEASURED ] a step-sized query over " << cellsTested << " cells hands the "
              << "narrow phase " << mean << " shapes on average against " << whole << " with no grid; worst "
              << worst << " in " << worstCell << std::endl;
    EXPECT_LT(mean, 12.0) << "a step-sized query hands the narrow phase " << mean
                          << " shapes on average, against " << whole
                          << " for the cell; the grid is not earning its keep";
    EXPECT_LT(static_cast<double>(worst), whole * 2.0)
        << "the worst cell, " << worstCell << ", returned " << worst;
    EXPECT_LT(mean, whole) << "the grid returned as much as no grid at all";
}
