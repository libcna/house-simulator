// SPDX-License-Identifier: MIT
//
// `HOUSE-00612`, §49.5's first guarantee: **the player cannot pass any closed door.** All 62 of
// them -- 54 interior, 4 double, 4 exterior -- from both sides, at three lateral offsets across
// the doorway, and with the leaf hung against either jamb.
//
// A validation rule cannot answer this. `validate_world.py` checks that a doorway is big enough
// to walk through; nothing checks that shutting it makes it small enough NOT to. Those are
// different questions and this house answers the second one with two separate facts: the leaf
// covers the middle of the opening, and whatever is left over beside it is narrower than the
// 0.62 m capsule. Either could stop being true when a doorway is widened by 100 mm.
//
// The leaf is a closed door and a closed door does not move, so for the walk it is geometry --
// added to the collision cell and driven at with the REAL `PlayerStep`, slide, step assist,
// gravity and depenetration included. It is ALSO checked through §49.4's dynamic list, which is
// where the runtime will find it (`HOUSE-00554`), because that is the path a swinging leaf takes
// and the two must agree about a shut one.
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <iterator>
#include <memory>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include "System/IO/FileAccess.hpp"
#include "System/IO/FileMode.hpp"
#include "System/IO/FileStream.hpp"

#include "cnahouse/physics/BroadPhase.hpp"
#include "cnahouse/physics/CollisionLoader.hpp"
#include "cnahouse/physics/DynamicObstacles.hpp"
#include "cnahouse/player/PlayerController.hpp"
#include "cnahouse/world/WorldData.hpp"
#include "cnahouse/world/WorldLoader.hpp"

namespace
{
    using cnahouse::physics::BroadPhase;
    using cnahouse::physics::Capsule;
    using cnahouse::physics::CollisionCell;
    using cnahouse::physics::CollisionKind;
    using cnahouse::physics::CollisionLoader;
    using cnahouse::physics::CollisionObb;
    using cnahouse::physics::CollisionWorld;
    using cnahouse::physics::DynamicKind;
    using cnahouse::physics::DynamicObstacle;
    using cnahouse::physics::DynamicObstacles;
    using cnahouse::physics::DynamicSweepHit;
    using cnahouse::player::InputState;
    using cnahouse::player::kPlayerRadius;
    using cnahouse::player::PlayerState;
    using cnahouse::player::PlayerStep;
    using cnahouse::util::IdRegistry;
    using Microsoft::Xna::Framework::Vector3;

    namespace world = cnahouse::world;

    /// Where a body cannot be stood 0.55 m from the doorway to push with, and why. The shut walk
    /// still runs from these sides -- it can only add -- but the OPEN control cannot pass there,
    /// so it would otherwise look like a leaf stopping a body that nothing was stopping.
    ///
    /// A new exemption has to be added here, in a commit, with a reason. That is the difference between
    /// a guarantee and a test that has been quietly taught to expect its own failures.
    const std::vector<std::string>& ApproachExemptions()
    {
        static const std::vector<std::string> kExempt{
            // Sorted, because the diff below is a `set_difference` and because a failure then
            // reads as one.
            //
            // `HOUSE-00489` restored the basement and guest-room approaches, removing their
            // old exemptions. Only designed non-passable openings remain here.
            // An appliance. §70.5's capsule does not fit through a refrigerator door, and
            // `PortalClearanceTests` exempts the same portal for the same reason.
            "P_FRIDGE_INTERIOR from CELL_FRIDGE_INTERIOR",
            "P_FRIDGE_INTERIOR from L0_KITCHEN",
            // A Juliet balcony is a doorway with a railing and NO floor beyond it. There is
            // nowhere to stand outside one, which is what makes it a Juliet balcony.
            "P_L2_LANDING__L2_BALCONY_JULIET from L2_BALCONY_JULIET",
            // The rail across a Juliet's opening is 0.06 m past the plane and in the BALCONY's
            // list. A body on the landing meets it before the leaf, which is the balcony working.
            "P_L2_LANDING__L2_BALCONY_JULIET from L2_LANDING",
        };
        return kExempt;
    }

    /// The doors with no side to push from at all, and therefore no open control anywhere.
    ///
    /// A door in this list is NOT proved shut by this test, so each one has to say why it cannot
    /// be and what would put it back. `HOUSE-00489` removed the two stair defects; the
    /// refrigerator and Juliet balcony remain intentionally impassable.
    const std::vector<std::string>& NoApproachAtAll()
    {
        static const std::vector<std::string> kNone{
            // An appliance: §70.5's capsule does not fit through a refrigerator door.
            "P_FRIDGE_INTERIOR",
            // A Juliet balcony is a doorway with a rail across it and no floor beyond: there is
            // nowhere to stand on one side and nothing but the rail on the other. By design, and
            // not a defect.
            "P_L2_LANDING__L2_BALCONY_JULIET",
        };
        return kNone;
    }

    /// An id's text, which is what a failure message has to print: `Id` is a hash.
    std::string_view Name(cnahouse::util::Id id)
    {
        return cnahouse::util::IdRegistry::NameOf(id);
    }

    constexpr float kDt = 1.0F / 120.0F; // §49.3's fixed step
    /// 1.25 s of shoving at §43.2's fast walk, from 0.75 m out: about 0.5 s to arrive with speed
    /// and 0.75 s of leaning on it, which is what finds a leaf that only stops a body that taps
    /// it. A body that is going to squeeze past one does it in that time -- §49.3's depenetration
    /// moves at most 0.08 m a step, and 0.75 s is ninety of them.
    ///
    /// MEASURED: 0.75 m is not a round number, it is the closest a body can be STARTED to a
    /// doorway and still walk through it. At 0.55 m, 27 of the 124 approaches could not get
    /// through an OPEN door -- the body starts against the frame and spends the walk being
    /// pushed out of it -- against 6 at 0.75 m.
    constexpr int kSteps = 150;
    constexpr float kStandOff = 0.75F;

    /// A doorway wider than its leaf by more than this is walked at from both jambs as well as
    /// the middle. Below it there is no slot to aim a 0.60 m body at: the slack is the 20 mm of
    /// clearance a leaf needs to swing, and the middle walk says everything the jamb walks would.
    constexpr float kSlotWorthTrying = 0.05F;

    struct Approach
    {
        Vector3 start;
        float yaw = 0.0F;
        /// +1 when the body walks towards increasing x/z, -1 the other way.
        float towards = 1.0F;
    };

    /// The leaf of a shut door, as a box in the doorway. @p flush is 0 for the leaf hung against
    /// the low jamb and 1 for the high one: `HOUSE-01182` has not chosen a convention yet, so the
    /// guarantee is asserted for BOTH, which is stronger than whichever it picks.
    CollisionObb ClosedLeaf(const world::Portal& portal, const world::Leaf& leaf, int flush, int leaves)
    {
        const float width = leaf.width * static_cast<float>(leaves);
        const float u0 = flush == 0 ? portal.minU : portal.maxU - width;
        const float centreU = u0 + width * 0.5F;
        const float centreV = portal.minV + leaf.height * 0.5F;

        CollisionObb obb;
        obb.kind = CollisionKind::Wall;
        obb.surface = 0u;
        if (portal.axis == world::PlaneAxis::X)
        {
            obb.centre = Vector3(portal.planeValue, centreV, centreU);
            obb.halfExtents = Vector3(leaf.thickness * 0.5F, leaf.height * 0.5F, width * 0.5F);
        }
        else
        {
            obb.centre = Vector3(centreU, centreV, portal.planeValue);
            obb.halfExtents = Vector3(width * 0.5F, leaf.height * 0.5F, leaf.thickness * 0.5F);
        }
        return obb;
    }

    /// A copy of the static world with ONE spare OBB on the end -- the leaf slot.
    ///
    /// Copying the whole 745 KB world once per door was most of this test's running time. The
    /// slot is filled and emptied instead: appending an OBB shifts every mesh's global index, so
    /// that renumbering is done once, here, rather than 372 times.
    CollisionWorld WithLeafSlot(const CollisionWorld& source, std::uint32_t& slot)
    {
        CollisionWorld world = source;
        slot = static_cast<std::uint32_t>(world.obbs.size());
        world.obbs.push_back(CollisionObb{});
        for (CollisionCell& cell : world.cells)
        {
            for (std::uint32_t& shape : cell.shapes)
            {
                if (shape >= slot)
                {
                    ++shape;
                }
            }
        }
        return world;
    }

    /// Hangs @p leaf in @p cellId. Every bucket rather than the ones its box covers: the broad
    /// phase has its own tests, and a guarantee that depended on this test's bucketing being
    /// right would be a guarantee about the test.
    void
    HangLeaf(CollisionWorld& world, std::uint32_t slot, const std::string& cellId, const CollisionObb& leaf)
    {
        world.obbs[slot] = leaf;
        for (CollisionCell& cell : world.cells)
        {
            if (cell.id != cellId)
            {
                continue;
            }
            const auto local = static_cast<std::uint16_t>(cell.shapes.size());
            cell.shapes.push_back(slot);
            for (std::vector<std::uint16_t>& bucket : cell.buckets)
            {
                bucket.push_back(local);
            }
        }
    }

    void TakeLeafDown(CollisionWorld& world, const std::string& cellId)
    {
        for (CollisionCell& cell : world.cells)
        {
            if (cell.id != cellId)
            {
                continue;
            }
            cell.shapes.pop_back();
            for (std::vector<std::uint16_t>& bucket : cell.buckets)
            {
                bucket.pop_back();
            }
        }
    }

    /// The side of @p portal that @p cell is on, as -1 or +1 along the plane's axis.
    float SideOf(const world::WorldData& data, const world::Cell& cell, const world::Portal& portal)
    {
        float centre = 0.0F;
        for (const world::Footprint& box : cell.boxes)
        {
            centre += portal.axis == world::PlaneAxis::X ? (box.minX + box.maxX) * 0.5F
                                                         : (box.minZ + box.maxZ) * 0.5F;
        }
        centre /= static_cast<float>(std::max<std::size_t>(cell.boxes.size(), 1u));
        (void)data;
        return centre < portal.planeValue ? 1.0F : -1.0F;
    }

    /// Where along the plane's axis the body is, which is the number the guarantee is about.
    float Across(const Vector3& position, world::PlaneAxis axis)
    {
        return axis == world::PlaneAxis::X ? position.X : position.Z;
    }

    Approach StartAt(const world::Portal& portal, float lateral, float side, float feetY)
    {
        // §14: yaw 0 looks north (-Z) and positive turns east, so forward is (sin, 0, -cos).
        Approach approach;
        approach.towards = side;
        const float centre = (portal.minU + portal.maxU) * 0.5F + lateral;
        const float rise = cnahouse::player::kPlayerHalfHeight + kPlayerRadius;
        if (portal.axis == world::PlaneAxis::X)
        {
            approach.start = Vector3(portal.planeValue - side * kStandOff, feetY + rise, centre);
            approach.yaw = side > 0.0F ? 1.5707963F : -1.5707963F;
        }
        else
        {
            approach.start = Vector3(centre, feetY + rise, portal.planeValue - side * kStandOff);
            approach.yaw = side > 0.0F ? 3.1415927F : 0.0F;
        }
        return approach;
    }

    /// Walks at the door for 1.25 s and returns how far past the plane the body's CENTRE got.
    /// Negative is "never reached it".
    float PushAtTheDoor(const CollisionWorld& world,
                        const CollisionCell& cell,
                        const Approach& approach,
                        const world::Portal& portal)
    {
        BroadPhase broad;
        PlayerState state;
        state.position = approach.start;
        state.yaw = approach.yaw;
        state.fastWalk = true;
        state.cellId = cell.id;

        InputState input;
        input.move.Y = 1.0F;

        float furthest = -1e9F;
        for (int i = 0; i < kSteps; ++i)
        {
            PlayerStep(world, cell, broad, state, input, kDt);
            const float past = approach.towards * (Across(state.position, portal.axis) - portal.planeValue);
            furthest = std::max(furthest, past);
        }
        return furthest;
    }

} // namespace

TEST(ClosedDoorTests, NoClosedDoorInTheHouseCanBeWalkedThrough)
{
    IdRegistry::ResetForTesting();
    const std::string directory = "content/world";
    const std::string collisionPath = "content/world/collision.bin";
    if (!std::filesystem::exists(directory + "/layout.portals.json") ||
        !std::filesystem::exists(collisionPath))
    {
        GTEST_SKIP() << "no deployed world; run tools/ci/build_content.py --only world";
    }

    world::WorldData::Contents contents;
    ASSERT_TRUE(world::WorldLoader::LoadLevels(directory, contents));
    ASSERT_TRUE(world::WorldLoader::LoadCells(directory, contents));
    ASSERT_TRUE(world::WorldLoader::LoadPortals(directory, contents));
    ASSERT_TRUE(world::WorldLoader::LoadOpenings(directory, contents));
    auto built = world::WorldData::Create(std::move(contents));
    ASSERT_TRUE(built) << built.Error().ToString();
    const world::WorldData& data = built.Value();

    const std::unique_ptr<System::IO::FileStream> stream(
        new System::IO::FileStream(collisionPath, System::IO::FileMode::Open, System::IO::FileAccess::Read));
    const auto loaded = CollisionLoader::Read(*stream, collisionPath);
    ASSERT_TRUE(loaded) << loaded.Error().Message();
    const CollisionWorld& statics = loaded.Value();

    std::uint32_t slot = 0;
    CollisionWorld shut = WithLeafSlot(statics, slot);

    int doors = 0;
    int walks = 0;
    int openControls = 0;
    float worstPast = -1e9F;
    std::string worstDoor;
    float widestSlot = -1e9F;
    std::string widestSlotDoor;
    std::set<std::string> proven;      // a door with at least one side that is walkable open
    std::vector<std::string> passed;   // a body that got through a SHUT door
    std::vector<std::string> unproven; // a doorway the open control could not get through either

    for (const world::Portal& portal : data.Portals())
    {
        if (portal.kind != world::PortalKind::Door && portal.kind != world::PortalKind::DoubleDoor &&
            portal.kind != world::PortalKind::ExteriorDoor)
        {
            continue;
        }
        ++doors;
        ASSERT_NE(portal.axis, world::PlaneAxis::Y)
            << Name(portal.id) << ": a door on a horizontal plane is a hatch";

        const world::Opening* opening = data.FindOpening(portal.aperture);
        ASSERT_NE(opening, nullptr) << Name(portal.id) << " has no leaf to shut";
        const int leaves = portal.kind == world::PortalKind::DoubleDoor ? 2 : 1;
        const float uncovered = portal.Width() - opening->leaf.width * static_cast<float>(leaves);
        if (uncovered > widestSlot)
        {
            widestSlot = uncovered;
            widestSlotDoor = Name(portal.id);
        }
        // What is left of the doorway beside the leaf. The guarantee needs this to be narrower
        // than the capsule -- a leaf that covers the middle of a doorway two capsules wide
        // stops nobody.
        EXPECT_LT(uncovered, 2.0F * kPlayerRadius)
            << Name(portal.id) << ": " << uncovered << " m of doorway is not covered by the leaf";

        for (const cnahouse::util::Id cellId : {portal.cellA, portal.cellB})
        {
            const world::Cell* cell = data.FindCell(cellId);
            ASSERT_NE(cell, nullptr) << Name(portal.id);
            const CollisionCell* collision = statics.Cell(Name(cellId));
            if (collision == nullptr || collision->shapes.empty())
            {
                continue; // a cell with no geometry cannot hold a body up to push with
            }
            const float side = SideOf(data, *cell, portal);
            const float feetY = portal.minV;

            // The open control FIRST. A test that only ever proves a body cannot get through has
            // no way of knowing whether it was the leaf or a mistake in the fixture that stopped
            // it, and a doorway walled up by accident would pass silently for ever.
            const Approach middle = StartAt(portal, 0.0F, side, feetY);
            const float open = PushAtTheDoor(statics, *collision, middle, portal);
            ++openControls;
            if (open <= kPlayerRadius)
            {
                unproven.push_back(std::string(Name(portal.id)) + " from " + std::string(Name(cellId)));
            }
            else
            {
                proven.insert(std::string(Name(portal.id)));
            }

            // The middle of the doorway, and as far to either jamb as a 0.60 m body can get. Each
            // offset is walked against the leaf hung on the FAR jamb from it -- the placement that
            // leaves the whole uncovered slot in front of the body, which is the worst case
            // whichever convention `HOUSE-01182` picks for a real hinge.
            const float reach = std::max(0.0F, portal.Width() * 0.5F - kPlayerRadius);
            std::vector<std::pair<float, int>> attempts{{0.0F, 0}};
            if (uncovered > kSlotWorthTrying)
            {
                // A doorway wider than its leaf. THIS is where a body could get past a shut door,
                // so both jambs are walked, each against the leaf hung on the far one. Where the
                // slack is a couple of centimetres -- 57 of the 62 -- there is no slot to aim at
                // and the middle says everything the jambs would.
                attempts.push_back({reach, 0});
                attempts.push_back({-reach, 1});
            }
            for (const auto& [lateral, flush] : attempts)
            {
                const CollisionObb leaf = ClosedLeaf(portal, opening->leaf, flush, leaves);
                HangLeaf(shut, slot, std::string(Name(cellId)), leaf);
                const CollisionCell* shutCell = shut.Cell(Name(cellId));
                ASSERT_NE(shutCell, nullptr);

                const Approach approach = StartAt(portal, lateral, side, feetY);
                const float past = PushAtTheDoor(shut, *shutCell, approach, portal);
                TakeLeafDown(shut, std::string(Name(cellId)));
                ++walks;
                if (past > worstPast)
                {
                    worstPast = past;
                    worstDoor = Name(portal.id);
                }
                if (past > 0.0F)
                {
                    passed.push_back(std::string(Name(portal.id)) + " from " + std::string(Name(cellId)) +
                                     " at " + std::to_string(lateral));
                }
            }

            // §49.4's list is where the runtime finds a door, so a shut one has to stop a sweep
            // there too -- and name itself, because §50 reports "blocked" against a PARTICULAR
            // door and a hit that only said "something" could not.
            DynamicObstacles obstacles;
            obstacles.BeginFrame();
            DynamicObstacle leafObstacle;
            leafObstacle.shape = ClosedLeaf(portal, opening->leaf, 0, leaves);
            leafObstacle.source = opening->id;
            leafObstacle.kind = DynamicKind::Door;
            obstacles.Add(Name(cellId), leafObstacle);

            const Approach middleAgain = StartAt(portal, 0.0F, side, feetY);
            const Capsule body{middleAgain.start, cnahouse::player::kPlayerHalfHeight, kPlayerRadius};
            Vector3 motion;
            const float travel = 1.5F * side;
            if (portal.axis == world::PlaneAxis::X)
            {
                motion = Vector3(travel, 0.0F, 0.0F);
            }
            else
            {
                motion = Vector3(0.0F, 0.0F, travel);
            }
            const DynamicSweepHit hit = SweepDynamic(obstacles, Name(cellId), body, motion);
            ASSERT_TRUE(hit.hit) << Name(portal.id) << ": the shut leaf is not in the way of a sweep";
            EXPECT_EQ(hit.source, opening->id) << "the hit did not name the door";
            EXPECT_EQ(hit.kind, DynamicKind::Door);
            EXPECT_LT(hit.time * 1.5F, kStandOff) << Name(portal.id) << ": stopped past the doorway";
        }
    }

    EXPECT_EQ(doors, 62) << "§12.3's door count changed; this guarantee counts them on purpose";
    std::printf("  %d door(s), %d walk(s), %d open control(s); worst approach %.3f m past the plane"
                " (%s); widest uncovered slot %.3f m (%s)\n",
                doors,
                walks,
                openControls,
                static_cast<double>(worstPast),
                worstDoor.c_str(),
                static_cast<double>(widestSlot),
                widestSlotDoor.c_str());

    EXPECT_TRUE(passed.empty()) << "a body walked through " << passed.size() << " shut door(s), first: "
                                << (passed.empty() ? std::string() : passed.front());
    EXPECT_TRUE(passed.empty()) << "a body walked through " << passed.size() << " shut door(s), first: "
                                << (passed.empty() ? std::string() : passed.front());

    // The open control, as a diff against the six places a body cannot stand. Both directions:
    // an exemption that has stopped being needed has to be noticed too, or the list becomes a
    // list of things that used to be true.
    std::sort(unproven.begin(), unproven.end());
    std::vector<std::string> unexpected;
    std::vector<std::string> gone;
    std::set_difference(unproven.begin(),
                        unproven.end(),
                        ApproachExemptions().begin(),
                        ApproachExemptions().end(),
                        std::back_inserter(unexpected));
    std::set_difference(ApproachExemptions().begin(),
                        ApproachExemptions().end(),
                        unproven.begin(),
                        unproven.end(),
                        std::back_inserter(gone));
    EXPECT_TRUE(unexpected.empty()) << unexpected.size()
                                    << " doorway(s) stopped the body with the door OPEN, so the shut walk "
                                    << "proves nothing there; first: "
                                    << (unexpected.empty() ? std::string() : unexpected.front());
    EXPECT_TRUE(gone.empty()) << gone.size() << " recorded exemption(s) no longer apply; first: "
                              << (gone.empty() ? std::string() : gone.front());

    // ...and every door except the refrigerator's has at least one side a body can be stood on
    // and driven from, which is what makes its shut walk mean anything.
    std::vector<std::string> unprovable;
    for (const world::Portal& portal : data.Portals())
    {
        if (portal.kind != world::PortalKind::Door && portal.kind != world::PortalKind::DoubleDoor &&
            portal.kind != world::PortalKind::ExteriorDoor)
        {
            continue;
        }
        const std::string name(Name(portal.id));
        const auto& none = NoApproachAtAll();
        if (std::find(none.begin(), none.end(), name) == none.end() && proven.find(name) == proven.end())
        {
            unprovable.push_back(name);
        }
    }
    EXPECT_TRUE(unprovable.empty()) << unprovable.size()
                                    << " door(s) could not be walked at from EITHER side; first: "
                                    << (unprovable.empty() ? std::string() : unprovable.front());
}
