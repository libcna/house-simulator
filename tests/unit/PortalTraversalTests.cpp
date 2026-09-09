// SPDX-License-Identifier: MIT
//
// `HOUSE-00668`. §25.2's portal walk, over §12's actual house.
//
// Everything before this task was a piece of arithmetic with a test of its own; this is the first
// one that answers the question the whole system exists for -- *which rooms can be seen from
// here* -- and the only fixture worth asking it of is the house itself. A synthetic two-room
// world would pass every test here and say nothing about a building with 96 cells, 179 portals,
// three storeys and a stair well open through all of them.
#include <algorithm>
#include <cstdio>
#include <filesystem>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "cnahouse/player/FirstPersonCamera.hpp"
#include "cnahouse/visibility/PortalTraversal.hpp"
#include "cnahouse/world/WorldData.hpp"
#include "cnahouse/world/WorldLoader.hpp"

namespace
{
    using cnahouse::player::FirstPersonCamera;
    using cnahouse::player::kPlayerEyeHeight;
    using cnahouse::player::PlayerState;
    using cnahouse::util::IdRegistry;
    using cnahouse::visibility::CameraSide;
    using cnahouse::visibility::ClipFrustum;
    using cnahouse::visibility::kMaxFrustaPerCell;
    using cnahouse::visibility::PortalRuntime;
    using cnahouse::visibility::PortalTraversal;
    using Microsoft::Xna::Framework::Vector3;

    namespace world = cnahouse::world;

    bool WorldIsDeployed()
    {
        return std::filesystem::exists("content/world/layout.cells.json");
    }

    world::WorldData Load()
    {
        world::WorldData::Contents contents;
        EXPECT_TRUE(world::WorldLoader::LoadLevels("content/world", contents).HasValue());
        EXPECT_TRUE(world::WorldLoader::LoadCells("content/world", contents).HasValue());
        EXPECT_TRUE(world::WorldLoader::LoadPortals("content/world", contents).HasValue());
        auto built = world::WorldData::Create(std::move(contents));
        EXPECT_TRUE(built) << built.Error().ToString();
        return std::move(built.Value());
    }

    /// One runtime per portal, in `Portals()` order. §65.6's starting state: anything with a leaf
    /// is shut, a cased opening or a stair well is open because it has no leaf to shut.
    std::vector<PortalRuntime> Runtimes(const world::WorldData& data)
    {
        std::vector<PortalRuntime> runtimes;
        runtimes.reserve(data.Portals().size());
        for (const world::Portal& portal : data.Portals())
        {
            runtimes.emplace_back(portal);
        }
        return runtimes;
    }

    void OpenEverything(std::vector<PortalRuntime>& runtimes)
    {
        for (PortalRuntime& runtime : runtimes)
        {
            runtime.SetAperture(1.0F);
        }
    }

    /// A camera standing in the middle of @p cell at §43.1's eye height, facing @p yawDegrees.
    struct Standing
    {
        FirstPersonCamera camera;
        Vector3 eye;
        cnahouse::util::Id cell;
    };

    Standing StandIn(const world::WorldData& data, std::string_view cellName, float yawDegrees)
    {
        Standing standing;
        standing.cell = cnahouse::util::Intern(std::string(cellName));
        const world::Cell* cell = data.FindCell(standing.cell);
        EXPECT_NE(cell, nullptr) << cellName;
        if (cell == nullptr)
        {
            return standing;
        }
        const world::Level* level = data.FindLevel(cell->level);
        EXPECT_NE(level, nullptr);
        const world::Footprint& box = cell->boxes.front();

        PlayerState state;
        state.position = Vector3((box.minX + box.maxX) * 0.5F,
                                 (level == nullptr ? 0.0F : level->ffl) + state.Rise(),
                                 (box.minZ + box.maxZ) * 0.5F);
        state.yaw = yawDegrees * 3.14159265F / 180.0F;
        standing.camera.SetAspect(16.0F / 9.0F);
        standing.camera.Update(state, kPlayerEyeHeight, 0.0F);
        standing.eye = standing.camera.Pose().eye;
        return standing;
    }

    PortalTraversal::Input InputFor(const world::WorldData& data,
                                    const std::vector<PortalRuntime>& runtimes,
                                    const Standing& standing,
                                    CameraSide side = CameraSide::Interior)
    {
        PortalTraversal::Input input;
        input.world = &data;
        input.portals = runtimes;
        input.cameraCell = standing.cell;
        input.eye = standing.eye;
        input.viewProjection = standing.camera.View() * standing.camera.Projection();
        input.cameraFrustum = ClipFrustum(standing.camera.Frustum());
        input.nearPlane = standing.camera.Frustum().getNearProperty();
        input.farPlane = standing.camera.Frustum().getFarProperty();
        input.side = side;
        return input;
    }

    std::vector<std::string> Names(const PortalTraversal& walk)
    {
        std::vector<std::string> names;
        for (const auto& cell : walk.Visible())
        {
            names.emplace_back(IdRegistry::NameOf(cell.cell));
        }
        std::sort(names.begin(), names.end());
        return names;
    }

} // namespace

TEST(PortalTraversalTests, TheCameraCellIsAlwaysVisibleEvenWithEverythingShut)
{
    IdRegistry::ResetForTesting();
    if (!WorldIsDeployed())
    {
        GTEST_SKIP() << "no deployed world";
    }
    const world::WorldData data = Load();
    const std::vector<PortalRuntime> shut = Runtimes(data);

    const Standing standing = StandIn(data, "L0_HALL", 0.0F);
    PortalTraversal walk;
    walk.Run(InputFor(data, shut, standing));

    ASSERT_FALSE(walk.Visible().empty());
    EXPECT_TRUE(walk.IsVisible(standing.cell));
    EXPECT_EQ(walk.Visible()[0].cell, standing.cell) << "the camera's own cell is the walk's root";
    EXPECT_EQ(walk.Visible()[0].depth, 0);
    EXPECT_EQ(walk.Visible()[0].frustumCount, 1U) << "the camera's own frustum";
}

TEST(PortalTraversalTests, OpeningEveryDoorLetsTheWalkSeeFurther)
{
    IdRegistry::ResetForTesting();
    if (!WorldIsDeployed())
    {
        GTEST_SKIP() << "no deployed world";
    }
    const world::WorldData data = Load();
    std::vector<PortalRuntime> shut = Runtimes(data);
    std::vector<PortalRuntime> open = Runtimes(data);
    OpenEverything(open);

    const Standing standing = StandIn(data, "L0_HALL", 0.0F);
    PortalTraversal closedWalk;
    closedWalk.Run(InputFor(data, shut, standing));
    PortalTraversal openWalk;
    openWalk.Run(InputFor(data, open, standing));

    std::printf("  from L0_HALL: %zu cell(s) with the doors shut, %zu with them open "
                "(%d portals tested, %d crossed, %d closed, %d facing away, %d too deep, "
                "%d clipped away, %d too small, %d already covered, max depth %d)\n",
                closedWalk.Visible().size(),
                openWalk.Visible().size(),
                openWalk.Stats().portalsTested,
                openWalk.Stats().portalsCrossed,
                openWalk.Stats().skippedClosed,
                openWalk.Stats().skippedFacing,
                openWalk.Stats().skippedDepth,
                openWalk.Stats().skippedClipped,
                openWalk.Stats().skippedArea,
                openWalk.Stats().skippedContained,
                openWalk.Stats().maxDepth);

    std::printf("  visible from L0_HALL looking north:");
    for (const std::string& name : Names(openWalk))
    {
        std::printf(" %s", name.c_str());
    }
    std::printf("\n");

    EXPECT_GT(openWalk.Visible().size(), closedWalk.Visible().size())
        << "opening every door in the house changed nothing, so the aperture is not being read";
    EXPECT_GT(closedWalk.Stats().skippedClosed, 0) << "§25.3's closed doors stopped nothing";

    // §25.2's back-face test, said as a fact about the house rather than as a counter: L0_FOYER is
    // SOUTH of the hall and the camera is looking north, so the doorway between them is being seen
    // from behind and the foyer is not visible through it. Without that test the walk turns round
    // and re-enters the room it came from, and everything beyond it comes back too.
    EXPECT_FALSE(openWalk.IsVisible(cnahouse::util::Intern("L0_FOYER")))
        << "the walk went out through a doorway it was looking at from behind";
    EXPECT_GT(openWalk.Stats().skippedFacing, 0) << "not one portal faced away, in a house of 179";
    // ...and the rooms in front of it ARE there, or the test above would pass on an empty walk.
    EXPECT_TRUE(openWalk.IsVisible(cnahouse::util::Intern("L0_KITCHEN")));
    // Every cell the shut walk found is also found by the open one: opening a door never hides a
    // room.
    for (const auto& cell : closedWalk.Visible())
    {
        EXPECT_TRUE(openWalk.IsVisible(cell.cell)) << IdRegistry::NameOf(cell.cell) << " went missing";
    }
}

TEST(PortalTraversalTests, TheDepthCapStopsTheChain)
{
    IdRegistry::ResetForTesting();
    if (!WorldIsDeployed())
    {
        GTEST_SKIP() << "no deployed world";
    }
    const world::WorldData data = Load();
    std::vector<PortalRuntime> open = Runtimes(data);
    OpenEverything(open);

    const Standing standing = StandIn(data, "L0_HALL", 0.0F);
    PortalTraversal walk;
    walk.Run(InputFor(data, open, standing));

    // §25.2's deepest interior cap is 6; nothing may be reached past it.
    EXPECT_LE(walk.Stats().maxDepth, 6);
    for (const auto& cell : walk.Visible())
    {
        EXPECT_LE(cell.depth, 6) << IdRegistry::NameOf(cell.cell);
    }
    EXPECT_GT(walk.Stats().skippedDepth, 0) << "the cap never bit, so it is untested here";
}

TEST(PortalTraversalTests, NoCellKeepsMoreThanFourFrusta)
{
    IdRegistry::ResetForTesting();
    if (!WorldIsDeployed())
    {
        GTEST_SKIP() << "no deployed world";
    }
    const world::WorldData data = Load();
    std::vector<PortalRuntime> open = Runtimes(data);
    OpenEverything(open);

    int deepest = 0;
    for (const char* room : {"L0_HALL", "L0_FOYER", "L1_LANDING", "B1_HALL", "L0_KITCHEN"})
    {
        const Standing standing = StandIn(data, room, 45.0F);
        PortalTraversal walk;
        walk.Run(InputFor(data, open, standing));
        for (const auto& cell : walk.Visible())
        {
            EXPECT_LE(cell.frustumCount, kMaxFrustaPerCell) << IdRegistry::NameOf(cell.cell);
            deepest = std::max(deepest, static_cast<int>(cell.frustumCount));
        }
    }
    EXPECT_GT(deepest, 1) << "no room was ever seen through two openings, so the cap is untested";
}

TEST(PortalTraversalTests, TheVisibleSetStaysInsideTheBudget)
{
    IdRegistry::ResetForTesting();
    if (!WorldIsDeployed())
    {
        GTEST_SKIP() << "no deployed world";
    }
    const world::WorldData data = Load();
    std::vector<PortalRuntime> open = Runtimes(data);
    OpenEverything(open);

    // §71.2: 9 visible cells typical, 22 worst case, 30 a hard fail. With EVERY door in the house
    // open -- which no player will ever arrange -- the walk still has to stay inside the hard stop.
    std::size_t worst = 0;
    std::string worstRoom;
    for (const char* room : {"L0_HALL",
                             "L0_FOYER",
                             "L0_KITCHEN",
                             "L0_LIVING",
                             "L1_LANDING",
                             "L1_HALL",
                             "L2_LANDING",
                             "B1_HALL",
                             "L3_ROOM",
                             "L0_GARAGE"})
    {
        for (const float yaw : {0.0F, 90.0F, 180.0F, 270.0F})
        {
            const Standing standing = StandIn(data, room, yaw);
            PortalTraversal walk;
            walk.Run(InputFor(data, open, standing));
            if (walk.Visible().size() > worst)
            {
                worst = walk.Visible().size();
                worstRoom = std::string(room) + " at " + std::to_string(static_cast<int>(yaw)) + " deg";
            }
        }
    }
    std::printf(
        "  worst visible set over 40 poses with every door open: %zu cells (%s)\n", worst, worstRoom.c_str());
    EXPECT_LE(worst, 30U) << "§71.2's hard stop, from " << worstRoom;
}

TEST(PortalTraversalTests, TheContainmentSkipFiresAndTheWalkIsDeterministic)
{
    IdRegistry::ResetForTesting();
    if (!WorldIsDeployed())
    {
        GTEST_SKIP() << "no deployed world";
    }
    const world::WorldData data = Load();
    std::vector<PortalRuntime> open = Runtimes(data);
    OpenEverything(open);

    const Standing standing = StandIn(data, "L0_FOYER", 180.0F);
    PortalTraversal first;
    first.Run(InputFor(data, open, standing));
    PortalTraversal second;
    second.Run(InputFor(data, open, standing));

    EXPECT_EQ(Names(first), Names(second)) << "the same input gave two different answers";
    EXPECT_EQ(first.Stats().portalsCrossed, second.Stats().portalsCrossed);

    // Where does §25.2's containment skip actually fire? Counted over every room in the house,
    // both ways round, rather than asserted from one pose -- the answer is a fact about §16's
    // graph and it is worth having in the record.
    int fired = 0;
    int poses = 0;
    int tooSmall = 0;
    for (const world::Cell& cell : data.Cells())
    {
        for (const float yaw : {0.0F, 120.0F, 240.0F})
        {
            const Standing here = StandIn(data, IdRegistry::NameOf(cell.id), yaw);
            PortalTraversal walk;
            walk.Run(InputFor(data, open, here));
            fired += walk.Stats().skippedContained;
            tooSmall += walk.Stats().skippedArea;
            ++poses;
        }
    }
    std::printf("  §25.2's containment skip fired %d time(s) and its area cutoff %d time(s) over %d poses of "
                "the whole house\n",
                fired,
                tooSmall,
                poses);
    EXPECT_GT(fired, 0) << "the containment skip never fires, so it is untested by this house";
    // 4 of 288, measured. §25.2's cutoff is permissive by design (`HOUSE-00664`), but it does fire,
    // so the traversal's use of it is exercised rather than merely present.
    EXPECT_GT(tooSmall, 0) << "§25.2's area cutoff never fired anywhere in the house";

    // The invariant the containment skip exists to keep: no cone a cell keeps is covered by one
    // it kept EARLIER. A cell can be queued twice before either arrival is popped, which is what
    // the pop-side skip is for, and this is that stated as a property of the result rather than as
    // a counter.
    //
    // The other direction is deliberately NOT asserted, because §25.2 does not provide it: a later
    // cone can be WIDER than one already stored -- measured, `EXT_BACKYARD` seen from `L0_PANTRY`
    // through two openings -- and nothing goes back to drop the narrow one it subsumes. The cost
    // is one redundant frustum test against that cell's contents, never a wrong answer, and
    // removing it would mean rewriting the stored list mid-walk for a saving §71.2 has not asked
    // for. `HOUSE-00695` is where that would be measured if it ever mattered.
    for (const world::Cell& cell : data.Cells())
    {
        const Standing here = StandIn(data, IdRegistry::NameOf(cell.id), 60.0F);
        PortalTraversal walk;
        walk.Run(InputFor(data, open, here));
        for (const auto& seen : walk.Visible())
        {
            for (std::size_t j = 1; j < seen.frustumCount; ++j)
            {
                for (std::size_t i = 0; i < j; ++i)
                {
                    EXPECT_FALSE(seen.rects[i].Contains(seen.rects[j]))
                        << IdRegistry::NameOf(seen.cell) << " kept cone " << j << ", which the earlier cone "
                        << i << " already covers (standing in " << IdRegistry::NameOf(cell.id) << ")";
                }
            }
        }
    }

    // ...and running the same object twice is the same as running it once: the buffers are reused
    // between frames, so a stale entry would show up here and nowhere else.
    first.Run(InputFor(data, open, standing));
    EXPECT_EQ(Names(first), Names(second)) << "the second run kept something from the first";
}

TEST(PortalTraversalTests, AWalkFromNowhereIsEmptyRatherThanUndefined)
{
    IdRegistry::ResetForTesting();
    if (!WorldIsDeployed())
    {
        GTEST_SKIP() << "no deployed world";
    }
    const world::WorldData data = Load();
    const std::vector<PortalRuntime> runtimes = Runtimes(data);
    const Standing standing = StandIn(data, "L0_HALL", 0.0F);

    PortalTraversal walk;
    PortalTraversal::Input input = InputFor(data, runtimes, standing);
    input.cameraCell = cnahouse::util::Id{};
    walk.Run(input);
    EXPECT_TRUE(walk.Visible().empty()) << "a camera in no cell saw something";

    input = InputFor(data, runtimes, standing);
    input.world = nullptr;
    walk.Run(input);
    EXPECT_TRUE(walk.Visible().empty());
}
