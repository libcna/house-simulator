// SPDX-License-Identifier: MIT
//
// `HOUSE-00356`. §16.4's point→cell lookup.
//
// The verification the task asks for is "against brute force on 10^5 random points", and that is
// the test that matters: the index exists only to be faster than the loop it replaces, so the one
// thing it must never do is give a different answer. Everything else here is about the two steps
// brute force does not have -- the incremental test with its hysteresis, and the neighbour walk.
#include <gtest/gtest.h>

#include <cstdint>
#include <random>
#include <string>

#include "Microsoft/Xna/Framework/Vector3.hpp"
#include "cnahouse/util/Ids.hpp"
#include "cnahouse/world/SpatialIndex.hpp"

namespace
{
    using cnahouse::util::Id;
    using cnahouse::util::IdRegistry;
    using cnahouse::util::Intern;
    using Vector3 = Microsoft::Xna::Framework::Vector3;

    namespace world = cnahouse::world;

    world::Footprint Box(float minX, float maxX, float minZ, float maxZ)
    {
        return world::Footprint{minX, maxX, minZ, maxZ};
    }

    world::Cell MakeCell(std::string_view id, std::string_view level, std::vector<world::Footprint> boxes)
    {
        world::Cell cell;
        cell.id = Intern(id);
        cell.level = Intern(level);
        cell.kind = world::CellKind::Room;
        cell.boxes = std::move(boxes);
        return cell;
    }

    /// Two storeys of rooms that share footprints across levels, plus an L-shaped hall, plus a
    /// gap no cell covers. The stacked pair is the case a grid keyed only on `(x, z)` has to get
    /// right, and the gap is the case that must answer "no cell" rather than the nearest one.
    world::WorldData::Contents Fixture()
    {
        world::WorldData::Contents contents;

        world::Level l0;
        l0.id = Intern("L0");
        l0.ffl = 0.60F;
        l0.ceiling = 3.30F;
        l0.structureDepth = 0.35F;
        world::Level l1;
        l1.id = Intern("L1");
        l1.ffl = 3.65F;
        l1.ceiling = 6.20F;
        l1.structureDepth = 0.35F;
        contents.levels = {l0, l1};

        contents.cells = {
            MakeCell("L0_FOYER", "L0", {Box(-2.0F, 2.0F, 0.0F, 4.0F)}),
            MakeCell("L0_HALL", "L0", {Box(-2.0F, 2.0F, 4.0F, 10.0F), Box(2.0F, 6.0F, 8.0F, 10.0F)}),
            MakeCell("L0_WC1", "L0", {Box(2.0F, 4.0F, 4.0F, 6.0F)}),
            // Directly over the foyer and the hall, so a bucket holds cells from both levels.
            MakeCell("L1_BED", "L1", {Box(-2.0F, 2.0F, 0.0F, 4.0F)}),
            MakeCell("L1_LANDING", "L1", {Box(-2.0F, 2.0F, 4.0F, 10.0F)}),
        };

        world::Portal foyerHall;
        foyerHall.id = Intern("P_FOYER__HALL");
        foyerHall.cellA = Intern("L0_FOYER");
        foyerHall.cellB = Intern("L0_HALL");
        foyerHall.axis = world::PlaneAxis::Z;
        foyerHall.planeValue = 4.0F;
        foyerHall.minU = -0.5F;
        foyerHall.maxU = 0.5F;
        foyerHall.minV = 0.60F;
        foyerHall.maxV = 2.65F;
        foyerHall.kind = world::PortalKind::CasedOpening;

        world::Portal hallWc = foyerHall;
        hallWc.id = Intern("P_HALL__WC1");
        hallWc.cellA = Intern("L0_HALL");
        hallWc.cellB = Intern("L0_WC1");
        hallWc.axis = world::PlaneAxis::X;
        hallWc.planeValue = 2.0F;
        hallWc.minU = 4.6F;
        hallWc.maxU = 5.5F;
        hallWc.kind = world::PortalKind::Door;

        contents.portals = {foyerHall, hallWc};
        return contents;
    }

    /// The loop the index replaces: every cell, in order, first hit wins.
    Id BruteForce(const world::WorldData& world, const Vector3& point)
    {
        for (const world::Cell& cell : world.Cells())
        {
            if (world.CellContains(cell, point))
            {
                return cell.id;
            }
        }
        return {};
    }

    class SpatialIndexTest : public ::testing::Test
    {
    protected:
        void SetUp() override
        {
            IdRegistry::ResetForTesting();
        }

        void TearDown() override
        {
            IdRegistry::ResetForTesting();
        }
    };

    TEST_F(SpatialIndexTest, TheIndexAgreesWithBruteForceOnAHundredThousandPoints)
    {
        auto loaded = world::WorldData::Create(Fixture());
        ASSERT_TRUE(loaded) << loaded.Error().ToString();
        const world::WorldData& data = loaded.Value();
        const world::SpatialIndex index = world::SpatialIndex::Build(data);

        // A fixed seed, because a lookup that disagrees once in ten thousand points must fail the
        // same way on the machine that has to fix it.
        std::mt19937 rng(20260907U);
        std::uniform_real_distribution<float> x(-8.0F, 8.0F);
        std::uniform_real_distribution<float> y(-1.0F, 8.0F);
        std::uniform_real_distribution<float> z(-4.0F, 14.0F);

        std::size_t inside = 0;
        std::size_t outside = 0;
        for (int sample = 0; sample < 100000; ++sample)
        {
            const Vector3 point(x(rng), y(rng), z(rng));
            const Id expected = BruteForce(data, point);
            // No `current`: this is the grid path, which is the one brute force can be compared
            // with. Steps 1 and 2 are about history, and history is what the next tests are for.
            const Id actual = index.Find(data, point);
            ASSERT_EQ(actual, expected)
                << "at (" << point.X << ", " << point.Y << ", " << point.Z << "): index said "
                << IdRegistry::NameOf(actual) << ", brute force said " << IdRegistry::NameOf(expected);
            (expected.IsValid() ? inside : outside) += 1;
        }

        // The sample must actually straddle the house, or "they agree" is a claim about ten
        // thousand points in the same empty corner.
        EXPECT_GT(inside, 5000U) << "the sample must land inside cells often enough to mean anything";
        EXPECT_GT(outside, 5000U) << "...and outside them, which is the answer the grid can get wrong";
    }

    TEST_F(SpatialIndexTest, AnOpenLowerBalconyDoesNotCaptureTheStoreyAbove)
    {
        // HOUSE-03640: open-air extents intentionally reach above their own storey.
        // First-hit grid selection assigned the upper balcony to the lower balcony,
        // so its collision and portal set lost the upper floor/interior altogether.
        auto contents = Fixture();
        contents.cells[0].kind = world::CellKind::Exterior;
        contents.cells[0].yOverride = world::Extent{0.60F, 9.00F};
        auto loaded = world::WorldData::Create(std::move(contents));
        ASSERT_TRUE(loaded);
        const auto& data = loaded.Value();
        const auto index = world::SpatialIndex::Build(data);
        EXPECT_EQ(index.Find(data, Vector3(0.0F, 4.55F, 2.0F)), Intern("L1_BED"));
        EXPECT_EQ(index.Find(data, Vector3(0.0F, 1.50F, 2.0F)), Intern("L0_FOYER"));
    }

    TEST_F(SpatialIndexTest, ABucketHoldsAtMostSixCells)
    {
        // §16.4's number. It holds because cells on one level do not overlap, so a 2 m square sees
        // one cell per level plus the exterior -- which is also why the grid needs no third key.
        auto loaded = world::WorldData::Create(Fixture());
        ASSERT_TRUE(loaded);
        const world::SpatialIndex index = world::SpatialIndex::Build(loaded.Value());

        EXPECT_GT(index.BucketCount(), 0U);
        EXPECT_LE(index.LargestBucket(), 6U);
        // And it really does hold cells from two levels at once, or the bound above is a bound on
        // a grid that never had to hold anything.
        EXPECT_EQ(index.Bucket(0.0F, 2.0F).size(), 2U) << "the foyer and the bedroom above it";
    }

    TEST_F(SpatialIndexTest, TheGridSeparatesTwoCellsOfTheSameFootprintByHeight)
    {
        auto loaded = world::WorldData::Create(Fixture());
        ASSERT_TRUE(loaded);
        const world::WorldData& data = loaded.Value();
        const world::SpatialIndex index = world::SpatialIndex::Build(data);

        EXPECT_EQ(index.Find(data, Vector3(0.0F, 1.5F, 2.0F)), Intern("L0_FOYER"));
        EXPECT_EQ(index.Find(data, Vector3(0.0F, 4.5F, 2.0F)), Intern("L1_BED"));
        EXPECT_FALSE(index.Find(data, Vector3(0.0F, 3.45F, 2.0F)).IsValid())
            << "the slab between them belongs to neither";
    }

    TEST_F(SpatialIndexTest, APointInNoCellIsNoCellAndNotTheNearestOne)
    {
        auto loaded = world::WorldData::Create(Fixture());
        ASSERT_TRUE(loaded);
        const world::WorldData& data = loaded.Value();
        const world::SpatialIndex index = world::SpatialIndex::Build(data);

        // Inside the hall's bounding box and in neither of its two boxes: the notch of the L.
        EXPECT_FALSE(index.Find(data, Vector3(4.0F, 1.5F, 6.5F)).IsValid());
        // Off the end of the house entirely, where the grid has no bucket at all.
        EXPECT_FALSE(index.Find(data, Vector3(40.0F, 1.5F, 40.0F)).IsValid());
        EXPECT_TRUE(index.Bucket(40.0F, 40.0F).empty());
    }

    TEST_F(SpatialIndexTest, TheIncrementalTestIsStickyWithinTheHysteresis)
    {
        // Without it a player standing exactly on a boundary flips between two cells every frame
        // and every system keyed off the current cell churns with them.
        auto loaded = world::WorldData::Create(Fixture());
        ASSERT_TRUE(loaded);
        const world::WorldData& data = loaded.Value();
        const world::SpatialIndex index = world::SpatialIndex::Build(data);

        // 3 cm past the foyer/hall boundary, which is inside the hall.
        const Vector3 justOver(0.0F, 1.5F, 4.03F);
        EXPECT_EQ(index.Find(data, justOver), Intern("L0_HALL")) << "with no history, the hall";
        EXPECT_EQ(index.Find(data, justOver, Intern("L0_FOYER")), Intern("L0_FOYER"))
            << "coming from the foyer, still the foyer";
        // 8 cm past it is beyond the 5 cm margin, so the answer changes.
        EXPECT_EQ(index.Find(data, Vector3(0.0F, 1.5F, 4.08F), Intern("L0_FOYER")), Intern("L0_HALL"));
    }

    TEST_F(SpatialIndexTest, ArrivingIsNotSticky)
    {
        // The margin is on step 1 only. If the grid step had one too, two cells would claim the
        // same point and there would be no rule for choosing between them.
        auto loaded = world::WorldData::Create(Fixture());
        ASSERT_TRUE(loaded);
        const world::WorldData& data = loaded.Value();
        const world::SpatialIndex index = world::SpatialIndex::Build(data);

        // 3 cm short of the foyer's boundary: inside the foyer, within the hall's margin.
        const Vector3 justShort(0.0F, 1.5F, 3.97F);
        EXPECT_EQ(index.Find(data, justShort), Intern("L0_FOYER"));
        EXPECT_EQ(index.Find(data, justShort, Intern("L0_HALL")), Intern("L0_HALL"))
            << "leaving the hall is sticky";
        EXPECT_EQ(index.Find(data, justShort, Intern("L1_BED")), Intern("L0_FOYER"))
            << "but a cell that never contained the point does not get to keep it";
    }

    TEST_F(SpatialIndexTest, TheNeighbourWalkAnswersBeforeTheGridDoes)
    {
        // Step 2 exists because a player who left a room is almost always in the room next door.
        // The observable claim is that it gives the same answer as the grid; the reason it is
        // worth having is that it does so without touching the grid at all.
        auto loaded = world::WorldData::Create(Fixture());
        ASSERT_TRUE(loaded);
        const world::WorldData& data = loaded.Value();
        const world::SpatialIndex index = world::SpatialIndex::Build(data);

        const Vector3 inTheWc(3.0F, 1.5F, 5.0F);
        world::SpatialIndex::Step step = world::SpatialIndex::Step::None;

        EXPECT_EQ(index.Find(data, inTheWc, Intern("L0_HALL"), &step), Intern("L0_WC1"))
            << "the WC is through one of the hall's portals";
        EXPECT_EQ(step, world::SpatialIndex::Step::Neighbour)
            << "and the walk is what answered, not the grid -- which is the whole reason it is "
               "there, and the only thing about it the returned id cannot show";

        EXPECT_EQ(index.Find(data, inTheWc, Intern("L1_BED"), &step), Intern("L0_WC1"))
            << "the grid gets there too, from a cell with no portal to it";
        EXPECT_EQ(step, world::SpatialIndex::Step::Grid);

        EXPECT_EQ(index.Find(data, inTheWc, Id{}, &step), Intern("L0_WC1"));
        EXPECT_EQ(step, world::SpatialIndex::Step::Grid);
    }

    TEST_F(SpatialIndexTest, EveryStepReportsItself)
    {
        auto loaded = world::WorldData::Create(Fixture());
        ASSERT_TRUE(loaded);
        const world::WorldData& data = loaded.Value();
        const world::SpatialIndex index = world::SpatialIndex::Build(data);

        world::SpatialIndex::Step step = world::SpatialIndex::Step::None;
        const Vector3 inTheFoyer(0.0F, 1.5F, 2.0F);

        EXPECT_EQ(index.Find(data, inTheFoyer, Intern("L0_FOYER"), &step), Intern("L0_FOYER"));
        EXPECT_EQ(step, world::SpatialIndex::Step::Incremental);

        EXPECT_EQ(index.Find(data, inTheFoyer, Intern("L0_HALL"), &step), Intern("L0_FOYER"));
        EXPECT_EQ(step, world::SpatialIndex::Step::Neighbour);

        EXPECT_EQ(index.Find(data, inTheFoyer, Intern("L1_BED"), &step), Intern("L0_FOYER"));
        EXPECT_EQ(step, world::SpatialIndex::Step::Grid);

        EXPECT_FALSE(index.Find(data, Vector3(40.0F, 1.5F, 40.0F), Id{}, &step).IsValid());
        EXPECT_EQ(step, world::SpatialIndex::Step::None);
    }

    TEST_F(SpatialIndexTest, AnUnknownCurrentCellFallsStraightToTheGrid)
    {
        // What a spawn, a teleport and a save load all do: there is no last cell to start from.
        auto loaded = world::WorldData::Create(Fixture());
        ASSERT_TRUE(loaded);
        const world::WorldData& data = loaded.Value();
        const world::SpatialIndex index = world::SpatialIndex::Build(data);

        EXPECT_EQ(index.Find(data, Vector3(0.0F, 1.5F, 2.0F), Id{}), Intern("L0_FOYER"));
        EXPECT_EQ(index.Find(data, Vector3(0.0F, 1.5F, 2.0F), Intern("L9_NOWHERE")), Intern("L0_FOYER"));
    }

    TEST_F(SpatialIndexTest, TheBucketCoordinateFloorsRatherThanTruncating)
    {
        // A truncating cast rounds toward zero, so the bucket containing the origin would be twice
        // as wide as every other and the two either side of it would overlap.
        EXPECT_EQ(world::SpatialIndex::Coordinate(0.0F), 0);
        EXPECT_EQ(world::SpatialIndex::Coordinate(1.9F), 0);
        EXPECT_EQ(world::SpatialIndex::Coordinate(2.0F), 1);
        EXPECT_EQ(world::SpatialIndex::Coordinate(-0.1F), -1);
        EXPECT_EQ(world::SpatialIndex::Coordinate(-2.0F), -1);
        EXPECT_EQ(world::SpatialIndex::Coordinate(-2.1F), -2);
    }

    TEST_F(SpatialIndexTest, ACellIsBucketedOnceEvenWhenTwoOfItsBoxesShareASquare)
    {
        // The hall's two boxes meet along `x = 2`, so the squares there see it twice. A duplicate
        // is not wrong, but it makes the bucket-size bound meaningless and the contents depend on
        // the order the boxes were authored.
        auto loaded = world::WorldData::Create(Fixture());
        ASSERT_TRUE(loaded);
        const world::SpatialIndex index = world::SpatialIndex::Build(loaded.Value());

        const auto bucket = index.Bucket(2.5F, 9.0F);
        std::vector<std::uint32_t> seen(bucket.begin(), bucket.end());
        std::sort(seen.begin(), seen.end());
        EXPECT_EQ(std::unique(seen.begin(), seen.end()), seen.end())
            << "a cell appears at most once in a bucket";
    }

    TEST_F(SpatialIndexTest, TwoBuildsOfOneWorldAreIdentical)
    {
        auto loaded = world::WorldData::Create(Fixture());
        ASSERT_TRUE(loaded);
        const world::SpatialIndex first = world::SpatialIndex::Build(loaded.Value());
        const world::SpatialIndex second = world::SpatialIndex::Build(loaded.Value());

        EXPECT_EQ(first.BucketCount(), second.BucketCount());
        EXPECT_EQ(first.LargestBucket(), second.LargestBucket());
        for (float x = -8.0F; x <= 8.0F; x += 2.0F)
        {
            for (float z = -4.0F; z <= 14.0F; z += 2.0F)
            {
                const auto a = first.Bucket(x, z);
                const auto b = second.Bucket(x, z);
                ASSERT_EQ(a.size(), b.size()) << x << ", " << z;
                EXPECT_TRUE(std::equal(a.begin(), a.end(), b.begin())) << x << ", " << z;
            }
        }
    }
} // namespace
