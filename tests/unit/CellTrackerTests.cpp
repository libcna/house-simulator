// SPDX-License-Identifier: MIT
//
// `HOUSE-00559`. §43.4: *"The controller publishes `CellEntered` whenever `currentCell` changes.
// That single event drives the visibility root, the audio listener's cell, the residency request
// set, the ambience bed cross-fade and the debug overlay. Cell membership uses 5 cm of hysteresis
// so standing exactly in a doorway does not thrash."*
//
// §16.4's lookup already exists (`HOUSE-00356`) and already applies the 5 cm. What is added here
// is the STATE it needs -- the cell the player was in last -- and the event that fires when it
// changes. One place remembers it, because two systems each keeping their own `currentCell` would
// each get their own idea of when the player crossed a doorway.
#include <gtest/gtest.h>

#include <string>
#include <vector>

#include "Microsoft/Xna/Framework/Vector3.hpp"
#include "cnahouse/app/EventQueue.hpp"
#include "cnahouse/player/CellTracker.hpp"
#include "cnahouse/util/Ids.hpp"

namespace
{
    using cnahouse::app::EventQueue;
    using cnahouse::player::CellTracker;
    using cnahouse::util::Id;
    using cnahouse::util::Intern;
    using cnahouse::world::CellEntered;
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

    /// A foyer and a hall meeting at z = 4, with a doorway between them -- which is the case §43.4
    /// names: standing exactly in it must not thrash.
    world::WorldData::Contents Fixture()
    {
        world::WorldData::Contents contents;

        world::Level l0;
        l0.id = Intern("L0");
        l0.ffl = 0.60F;
        l0.ceiling = 3.30F;
        l0.structureDepth = 0.35F;
        contents.levels = {l0};

        contents.cells = {
            MakeCell("L0_FOYER", "L0", {Box(-2.0F, 2.0F, 0.0F, 4.0F)}),
            MakeCell("L0_HALL", "L0", {Box(-2.0F, 2.0F, 4.0F, 10.0F)}),
            MakeCell("L0_STORE", "L0", {Box(6.0F, 8.0F, 0.0F, 2.0F)}),
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
        contents.portals = {foyerHall};
        return contents;
    }

    /// A point in a cell, at standing height on L0.
    Vector3 At(float x, float z)
    {
        return Vector3(x, 1.60F, z);
    }

} // namespace

TEST(CellTrackerTests, TheFirstUpdateEntersACellWithoutLeavingOne)
{
    auto loaded = world::WorldData::Create(Fixture());
    ASSERT_TRUE(loaded) << loaded.Error().Message();
    const world::WorldData& data = loaded.Value();
    const auto index = world::SpatialIndex::Build(data);

    EventQueue queue;
    std::vector<CellEntered> seen;
    queue.Subscribe<CellEntered>([&](const CellEntered& event) { seen.push_back(event); });

    CellTracker tracker;
    EXPECT_FALSE(tracker.Current().IsValid()) << "it claimed a cell before it was asked";

    EXPECT_TRUE(tracker.Update(data, index, At(0.0F, 2.0F), &queue));
    EXPECT_EQ(tracker.Current(), Intern("L0_FOYER"));
    // A spawn starts from the GRID: there is no previous cell for the incremental test to try.
    EXPECT_EQ(tracker.LastStep(), world::SpatialIndex::Step::Grid);

    queue.Drain();
    ASSERT_EQ(seen.size(), 1u);
    EXPECT_EQ(seen[0].cell, Intern("L0_FOYER"));
    EXPECT_FALSE(seen[0].previous.IsValid()) << "a spawn entered a cell it had left";
}

TEST(CellTrackerTests, StandingStillPublishesNothing)
{
    // The event drives five systems (§43.4). One published every tick would cross-fade the
    // ambience 120 times a second.
    auto loaded = world::WorldData::Create(Fixture());
    ASSERT_TRUE(loaded) << loaded.Error().Message();
    const world::WorldData& data = loaded.Value();
    const auto index = world::SpatialIndex::Build(data);

    EventQueue queue;
    std::size_t count = 0;
    queue.Subscribe<CellEntered>([&](const CellEntered&) { ++count; });

    CellTracker tracker;
    tracker.Update(data, index, At(0.0F, 2.0F), &queue);
    for (int i = 0; i < 120; ++i)
    {
        EXPECT_FALSE(tracker.Update(data, index, At(0.0F, 2.0F), &queue)) << "tick " << i;
        // ...and the incremental step is the one answering, which is what makes it O(1).
        EXPECT_EQ(tracker.LastStep(), world::SpatialIndex::Step::Incremental);
    }
    queue.Drain();
    EXPECT_EQ(count, 1u) << "it published on a tick where nothing changed";
}

TEST(CellTrackerTests, CrossingADoorwayPublishesOnceAndNamesBothCells)
{
    auto loaded = world::WorldData::Create(Fixture());
    ASSERT_TRUE(loaded) << loaded.Error().Message();
    const world::WorldData& data = loaded.Value();
    const auto index = world::SpatialIndex::Build(data);

    EventQueue queue;
    std::vector<CellEntered> seen;
    queue.Subscribe<CellEntered>([&](const CellEntered& event) { seen.push_back(event); });

    CellTracker tracker;
    tracker.Update(data, index, At(0.0F, 2.0F), &queue);
    // Walk north through the doorway at z = 4, a centimetre at a time.
    for (float z = 2.0F; z < 6.0F; z += 0.01F)
    {
        tracker.Update(data, index, At(0.0F, z), &queue);
    }
    queue.Drain();

    ASSERT_EQ(seen.size(), 2u) << "the doorway was crossed more than once";
    EXPECT_EQ(seen[1].cell, Intern("L0_HALL"));
    EXPECT_EQ(seen[1].previous, Intern("L0_FOYER")) << "it did not say where the player came from";
    EXPECT_EQ(tracker.Current(), Intern("L0_HALL"));
}

TEST(CellTrackerTests, TheFiveCentimetreHysteresisIsWhatStopsADoorwayThrashing)
{
    // §43.4's own case. The player stands astride the boundary and jitters by a couple of
    // millimetres, which is what a physics step does to a body pressed against a doorframe. The
    // hysteresis makes STAYING sticky, so the cell is decided once.
    auto loaded = world::WorldData::Create(Fixture());
    ASSERT_TRUE(loaded) << loaded.Error().Message();
    const world::WorldData& data = loaded.Value();
    const auto index = world::SpatialIndex::Build(data);

    EventQueue queue;
    std::size_t count = 0;
    queue.Subscribe<CellEntered>([&](const CellEntered&) { ++count; });

    CellTracker tracker;
    tracker.Update(data, index, At(0.0F, 3.99F), &queue); // in the foyer, a centimetre short
    ASSERT_EQ(tracker.Current(), Intern("L0_FOYER"));

    for (int i = 0; i < 200; ++i)
    {
        const float z = 4.0F + (i % 2 == 0 ? 0.002F : -0.002F);
        tracker.Update(data, index, At(0.0F, z), &queue);
        EXPECT_EQ(tracker.Current(), Intern("L0_FOYER")) << "tick " << i;
    }
    queue.Drain();
    EXPECT_EQ(count, 1u) << "the doorway thrashed";

    // ...and 5 cm past it, the hysteresis is spent and the player really has arrived.
    EXPECT_TRUE(tracker.Update(data, index, At(0.0F, 4.06F), &queue));
    EXPECT_EQ(tracker.Current(), Intern("L0_HALL"));
}

TEST(CellTrackerTests, ForgettingSendsTheNextLookupToTheGrid)
{
    // A teleport, a spawn and a save load are all "where they were tells you nothing". Without
    // this, the incremental test answers such a query with the room they were in before the load
    // -- expanded by 5 cm, and wrong by a house.
    auto loaded = world::WorldData::Create(Fixture());
    ASSERT_TRUE(loaded) << loaded.Error().Message();
    const world::WorldData& data = loaded.Value();
    const auto index = world::SpatialIndex::Build(data);

    CellTracker tracker;
    tracker.Update(data, index, At(0.0F, 2.0F));
    ASSERT_EQ(tracker.Current(), Intern("L0_FOYER"));

    tracker.Forget();
    EXPECT_FALSE(tracker.Current().IsValid());
    EXPECT_TRUE(tracker.Update(data, index, At(7.0F, 1.0F)));
    EXPECT_EQ(tracker.Current(), Intern("L0_STORE"));
    EXPECT_EQ(tracker.LastStep(), world::SpatialIndex::Step::Grid);
}

TEST(CellTrackerTests, APointInNoCellKeepsTheLastGoodOne)
{
    // §16.4's step 4: no cell is a world-data bug, and the clause says to clamp to the last good
    // cell rather than to move the player nowhere. Five systems cross-fading to an invalid id is
    // a worse failure than a stale one.
    auto loaded = world::WorldData::Create(Fixture());
    ASSERT_TRUE(loaded) << loaded.Error().Message();
    const world::WorldData& data = loaded.Value();
    const auto index = world::SpatialIndex::Build(data);

    EventQueue queue;
    std::size_t count = 0;
    queue.Subscribe<CellEntered>([&](const CellEntered&) { ++count; });

    CellTracker tracker;
    tracker.Update(data, index, At(0.0F, 2.0F), &queue);
    ASSERT_EQ(tracker.Current(), Intern("L0_FOYER"));

    EXPECT_FALSE(tracker.Update(data, index, At(40.0F, 40.0F), &queue));
    EXPECT_EQ(tracker.Current(), Intern("L0_FOYER")) << "it moved the player to nowhere";
    queue.Drain();
    EXPECT_EQ(count, 1u);
}
