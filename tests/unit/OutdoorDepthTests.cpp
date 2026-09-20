// SPDX-License-Identifier: MIT
//
// `HOUSE-00680`. §25.2's asymmetry, and its acceptance criterion: *"standing in the garden, exactly
// one room is visible through each window, not the whole house."*
//
// The depth table was already right (`HOUSE-00667`) and the walk already read it
// (`HOUSE-00668`) -- what was wrong is WHOSE number it is. A cap read per portal lets a window
// admit the chain at depth 1 and the room's own door, whose cap is 2, carry it straight on, which
// is the exact thing §25.2 says must not happen. The cap belongs to the chain, so the allowance a
// chain carries is the minimum of every cap it has crossed.
#include <algorithm>
#include <cstdio>
#include <deque>
#include <filesystem>
#include <set>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "cnahouse/player/FirstPersonCamera.hpp"
#include "cnahouse/visibility/PortalDepth.hpp"
#include "cnahouse/visibility/VisibilitySystem.hpp"
#include "cnahouse/world/WorldData.hpp"
#include "cnahouse/world/WorldLoader.hpp"

namespace
{
    using cnahouse::app::FrameContext;
    using cnahouse::player::FirstPersonCamera;
    using cnahouse::player::kPlayerEyeHeight;
    using cnahouse::player::PlayerState;
    using cnahouse::util::Id;
    using cnahouse::util::IdRegistry;
    using cnahouse::visibility::CameraSide;
    using cnahouse::visibility::CameraView;
    using cnahouse::visibility::ClipFrustum;
    using cnahouse::visibility::MaxDepthFor;
    using cnahouse::visibility::VisibilitySystem;
    using cnahouse::visibility::VisibleCell;
    using Microsoft::Xna::Framework::Vector3;

    namespace world = cnahouse::world;

    bool ContentIsBuilt()
    {
        return std::filesystem::exists("content/world/layout.cells.json");
    }

    world::WorldData LoadWorld()
    {
        world::WorldData::Contents contents;
        EXPECT_TRUE(world::WorldLoader::LoadLevels("content/world", contents).HasValue());
        EXPECT_TRUE(world::WorldLoader::LoadCells("content/world", contents).HasValue());
        EXPECT_TRUE(world::WorldLoader::LoadPortals("content/world", contents).HasValue());
        auto built = world::WorldData::Create(std::move(contents));
        EXPECT_TRUE(built) << built.Error().ToString();
        return std::move(built.Value());
    }

    CameraView Standing(const world::WorldData& data, std::string_view cellName, float yawDegrees)
    {
        CameraView view;
        view.cell = cnahouse::util::Intern(std::string(cellName));
        const world::Cell* cell = data.FindCell(view.cell);
        EXPECT_NE(cell, nullptr) << cellName;
        if (cell == nullptr)
        {
            return view;
        }
        const world::Level* level = data.FindLevel(cell->level);
        const world::Footprint& box = cell->boxes.front();

        PlayerState state;
        state.position = Vector3((box.minX + box.maxX) * 0.5F,
                                 (level == nullptr ? 0.0F : level->ffl) + state.Rise(),
                                 (box.minZ + box.maxZ) * 0.5F);
        state.yaw = yawDegrees * 3.14159265F / 180.0F;
        FirstPersonCamera camera;
        camera.SetAspect(16.0F / 9.0F);
        camera.Update(state, kPlayerEyeHeight, 0.0F);

        view.eye = camera.Pose().eye;
        view.viewProjection = camera.View() * camera.Projection();
        view.frustum = ClipFrustum(camera.Frustum());
        view.nearPlane = camera.Frustum().getNearProperty();
        view.farPlane = camera.Frustum().getFarProperty();
        return view;
    }

    FrameContext Frame(std::uint64_t index)
    {
        FrameContext frame;
        frame.frameIndex = index;
        return frame;
    }

    bool IsExterior(const world::WorldData& data, Id cell)
    {
        const world::Cell* row = data.FindCell(cell);
        return row != nullptr && row->kind == world::CellKind::Exterior;
    }

    /// §25.2's own words for "a window onto the outdoors", re-derived here rather than asked of
    /// `MaxDepthFor`: a test that read the answer off the thing under test would say nothing.
    bool IsGlazedToOutside(const world::WorldData& data, const world::Portal& portal)
    {
        const bool glazed = portal.kind == world::PortalKind::Window ||
                            portal.opacity == world::PortalOpacity::Glass ||
                            portal.opacity == world::PortalOpacity::Translucent;
        return glazed && (IsExterior(data, portal.cellA) || IsExterior(data, portal.cellB));
    }

    /// Cells within @p hops of @p from over portals that are NOT glazed onto the outdoors, and
    /// that light can pass at all. The graph the depth-2 rows of §25.2's table describe.
    std::set<std::uint32_t> ReachableWithoutGlazing(const world::WorldData& data, Id from, int hops)
    {
        std::set<std::uint32_t> seen{from.Value()};
        std::deque<std::pair<Id, int>> queue{{from, 0}};
        while (!queue.empty())
        {
            const auto [cell, depth] = queue.front();
            queue.pop_front();
            if (depth >= hops)
            {
                continue;
            }
            for (const std::uint32_t index : data.PortalsOf(cell))
            {
                const world::Portal& portal = data.Portals()[index];
                if (IsGlazedToOutside(data, portal))
                {
                    continue;
                }
                const Id other = portal.cellA == cell ? portal.cellB : portal.cellA;
                if (seen.insert(other.Value()).second)
                {
                    queue.push_back({other, depth + 1});
                }
            }
        }
        return seen;
    }

} // namespace

TEST(OutdoorDepthTests, RoadEyeSeesRoomsBehindFrontGlazing)
{
    // HOUSE-00702: outdoor cells are movement partitions, not a wall across the eye. The camera
    // sees these four front windows from the road even when the portal walk does not first enter
    // the narrow yard cell that owns each one's exterior side. Without those rooms their glazed
    // openings show the sky/background instead of a wall and furnished interior.
    IdRegistry::ResetForTesting();
    if (!ContentIsBuilt())
    {
        GTEST_SKIP() << "no deployed world; run tools/ci/build_content.py --only world";
    }
    const world::WorldData data = LoadWorld();
    VisibilitySystem system(data);

    PlayerState state;
    state.position = Vector3(0.0F, state.Rise(), 5.20F);
    state.yaw = 0.0F;
    FirstPersonCamera camera;
    camera.SetAspect(16.0F / 9.0F);
    camera.Update(state, kPlayerEyeHeight, 0.0F);
    CameraView view;
    view.cell = cnahouse::util::Intern("EXT_ROAD");
    view.eye = camera.Pose().eye;
    view.viewProjection = camera.View() * camera.Projection();
    view.frustum = ClipFrustum(camera.Frustum());
    view.nearPlane = camera.Frustum().getNearProperty();
    view.farPlane = camera.Frustum().getFarProperty();
    system.SetCamera(view);
    system.Update(Frame(1));

    std::set<std::string> visible;
    for (const VisibleCell& cell : system.Visible())
    {
        visible.insert(std::string(IdRegistry::NameOf(cell.cell)));
        if (cell.cell != view.cell && !IsExterior(data, cell.cell))
        {
            EXPECT_EQ(cell.depth, 1) << IdRegistry::NameOf(cell.cell)
                                     << " is deeper than the exterior glazing allowance";
        }
    }
    for (const std::string_view room : {"L0_LIVING", "L0_STAIR_MAIN", "L1_STAIR_MAIN", "L2_STAIR_MAIN"})
    {
        EXPECT_TRUE(visible.contains(std::string(room))) << room << " is missing behind its front window";
    }
    EXPECT_LE(system.Visible().size(), cnahouse::visibility::kMaxVisibleCells);
}

TEST(OutdoorDepthTests, TheCapBelongsToTheChainAndNotToOnePortal)
{
    // The rule itself, over every exterior pose: a cell is never reached deeper than the allowance
    // its chain carried, and the allowance never grows as the chain lengthens.
    IdRegistry::ResetForTesting();
    if (!ContentIsBuilt())
    {
        GTEST_SKIP() << "no deployed world; run tools/ci/build_content.py --only world";
    }
    const world::WorldData data = LoadWorld();
    VisibilitySystem system(data);
    // Every door in the house open, which is the arrangement that makes over-reaching possible at
    // all: with them shut there is nothing behind the room for a window to leak into.
    for (const world::Portal& portal : data.Portals())
    {
        system.SetAperture(portal.id, 1.0F);
    }

    int poses = 0;
    int deepest = 0;
    std::size_t widest = 0;
    int interiorCells = 0;
    for (const char* yard : {"EXT_BACKYARD", "EXT_TERRACE", "EXT_FRONTYARD_W", "EXT_DRIVEWAY"})
    {
        for (const float yaw : {0.0F, 45.0F, 90.0F, 135.0F, 180.0F, 225.0F, 270.0F, 315.0F})
        {
            system.SetCamera(Standing(data, yard, yaw));
            system.Update(Frame(1));
            ++poses;
            widest = std::max(widest, system.Visible().size());
            for (const VisibleCell& cell : system.Visible())
            {
                EXPECT_LE(cell.depth, cell.allowance)
                    << cnahouse::util::IdRegistry::NameOf(cell.cell) << " from " << yard;
                deepest = std::max(deepest, cell.depth);
                interiorCells += IsExterior(data, cell.cell) ? 0 : 1;
            }
        }
    }
    EXPECT_EQ(poses, 32);
    // §25.2's exterior column: two for a door or a garage door, one for glazing. Nothing outdoors
    // may reach further than the deepest row of it.
    EXPECT_LE(deepest, 2) << "an exterior camera reached deeper than §25.2's table allows";
    std::printf("  32 garden poses with every door open: deepest chain %d, widest visible set %zu, "
                "%d room(s) seen in all (§71.2 budgets 9 typical, 22 worst, 30 hard)\n",
                deepest,
                widest,
                interiorCells);
    // §71.2's hard fail is 30 visible cells; the garden is where a house full of windows could
    // reach it, and this is the number that says it does not.
    EXPECT_LT(widest, 30U);
}

TEST(OutdoorDepthTests, ARoomSeenThroughAWindowStopsAtThatRoom)
{
    // The acceptance criterion. A cell admitted by glazing carries an allowance of 1, so its own
    // doors -- whose cap is 2 -- cannot carry the chain on: it is at its allowance already.
    IdRegistry::ResetForTesting();
    if (!ContentIsBuilt())
    {
        GTEST_SKIP() << "no deployed world";
    }
    const world::WorldData data = LoadWorld();
    VisibilitySystem system(data);
    for (const world::Portal& portal : data.Portals())
    {
        system.SetAperture(portal.id, 1.0F);
    }

    int throughGlass = 0;
    for (const char* yard : {"EXT_BACKYARD", "EXT_TERRACE", "EXT_FRONTYARD_W", "EXT_SIDEYARD_W"})
    {
        for (const float yaw : {0.0F, 90.0F, 180.0F, 270.0F})
        {
            const CameraView view = Standing(data, yard, yaw);
            system.SetCamera(view);
            system.Update(Frame(2));

            // What §25.2's table allows at depth 2 from outside is a chain of doors, so every cell
            // the walk reached at depth 2 must be two door-hops away. A cell that is only
            // reachable through glazing has no business being there.
            const std::set<std::uint32_t> withoutGlass = ReachableWithoutGlazing(data, view.cell, 2);
            for (const VisibleCell& cell : system.Visible())
            {
                if (cell.depth < 2)
                {
                    if (cell.allowance == 1)
                    {
                        ++throughGlass;
                    }
                    continue;
                }
                EXPECT_TRUE(withoutGlass.contains(cell.cell.Value()))
                    << IdRegistry::NameOf(cell.cell) << " is visible at depth " << cell.depth << " from "
                    << yard << ", and the only way to it is through glazing";
            }
        }
    }
    EXPECT_GT(throughGlass, 0) << "no window ever admitted a chain, so nothing was proved";
    std::printf("  %d cell(s) reached through glazing over 16 garden poses, none of them "
                "carrying the chain on\n",
                throughGlass);
}

TEST(OutdoorDepthTests, TheWindowsOwnCapIsWhatStopsIt)
{
    // The arithmetic, stated: a glazed portal onto the outdoors caps an exterior camera at 1 and an
    // interior one at 3, and a door at 2 and 6. Read from the table rather than from the walk, so
    // that a change to either is a failure here and not a silent change of behaviour.
    IdRegistry::ResetForTesting();
    if (!ContentIsBuilt())
    {
        GTEST_SKIP() << "no deployed world";
    }
    const world::WorldData data = LoadWorld();

    int glazed = 0;
    int doors = 0;
    for (const world::Portal& portal : data.Portals())
    {
        if (IsGlazedToOutside(data, portal))
        {
            ++glazed;
            EXPECT_EQ(MaxDepthFor(portal, data, CameraSide::Exterior), 1) << IdRegistry::NameOf(portal.id);
            EXPECT_EQ(MaxDepthFor(portal, data, CameraSide::Interior), 3);
        }
        else if (portal.kind != world::PortalKind::GarageDoor)
        {
            ++doors;
            EXPECT_EQ(MaxDepthFor(portal, data, CameraSide::Exterior), 2);
            EXPECT_EQ(MaxDepthFor(portal, data, CameraSide::Interior), 6);
        }
    }
    std::printf(
        "  the house has %d glazed opening(s) onto the outdoors and %d other portal(s)\n", glazed, doors);
    EXPECT_GT(glazed, 0) << "no portal in the house is glazed onto the outdoors";
    EXPECT_GT(doors, 0);
}

TEST(OutdoorDepthTests, TheAllowanceIsTheBestOfTwoWaysIn)
{
    // A room seen BOTH through a window and through an open door from the yard is the door's chain
    // from there on: the walk keeps the larger allowance, because refusing to continue down a
    // chain that is allowed to continue would be over-culling.
    IdRegistry::ResetForTesting();
    if (!ContentIsBuilt())
    {
        GTEST_SKIP() << "no deployed world";
    }
    const world::WorldData data = LoadWorld();
    VisibilitySystem system(data);
    for (const world::Portal& portal : data.Portals())
    {
        system.SetAperture(portal.id, 1.0F);
    }

    bool sawBoth = false;
    for (const char* yard : {"EXT_BACKYARD", "EXT_TERRACE", "L0_PORCH", "EXT_DRIVEWAY"})
    {
        for (const float yaw : {0.0F, 45.0F, 90.0F, 135.0F, 180.0F, 225.0F, 270.0F, 315.0F})
        {
            const CameraView view = Standing(data, yard, yaw);
            system.SetCamera(view);
            system.Update(Frame(3));
            for (const VisibleCell& cell : system.Visible())
            {
                if (IsExterior(data, cell.cell) || cell.depth != 1)
                {
                    continue;
                }
                // Is there a NON-glazed portal straight from the camera's cell to this one?
                bool byDoor = false;
                for (const std::uint32_t index : data.PortalsOf(view.cell))
                {
                    const world::Portal& portal = data.Portals()[index];
                    const Id other = portal.cellA == view.cell ? portal.cellB : portal.cellA;
                    byDoor = byDoor || (other == cell.cell && !IsGlazedToOutside(data, portal));
                }
                if (byDoor)
                {
                    sawBoth = true;
                    EXPECT_EQ(cell.allowance, 2) << IdRegistry::NameOf(cell.cell)
                                                 << " is reachable by a door but kept a window's allowance";
                }
                else
                {
                    EXPECT_EQ(cell.allowance, 1)
                        << IdRegistry::NameOf(cell.cell) << " came through glazing only";
                }
            }
        }
    }
    EXPECT_TRUE(sawBoth) << "no room in the house is entered directly from a yard, so the "
                            "better-of-two rule was never exercised";
}

TEST(OutdoorDepthTests, FromInsideTheHouseTheChainIsStillSixDoorsDeep)
{
    // The other direction, unchanged: the chain rule takes the MINIMUM, and indoors the minimum of
    // a run of doors is a door's own six. A rule that had tightened the interior walk would show
    // up here as a house that has gone dark from its own hall.
    IdRegistry::ResetForTesting();
    if (!ContentIsBuilt())
    {
        GTEST_SKIP() << "no deployed world";
    }
    const world::WorldData data = LoadWorld();
    VisibilitySystem system(data);
    for (const world::Portal& portal : data.Portals())
    {
        system.SetAperture(portal.id, 1.0F);
    }

    int deepest = 0;
    std::size_t widest = 0;
    for (const char* room : {"L0_HALL", "L0_FOYER", "L1_LANDING", "B1_HALL"})
    {
        for (const float yaw : {0.0F, 90.0F, 180.0F, 270.0F})
        {
            system.SetCamera(Standing(data, room, yaw));
            system.Update(Frame(4));
            widest = std::max(widest, system.Visible().size());
            for (const VisibleCell& cell : system.Visible())
            {
                deepest = std::max(deepest, cell.depth);
                EXPECT_LE(cell.depth, cell.allowance);
                // Indoors the allowance is a door's six until a window narrows it to three.
                EXPECT_TRUE(cell.allowance == cnahouse::visibility::kNoLimit || cell.allowance == 6 ||
                            cell.allowance == 4 || cell.allowance == 3)
                    << IdRegistry::NameOf(cell.cell) << " carries an allowance of " << cell.allowance;
            }
        }
    }
    std::printf("  16 interior poses with every door open: deepest chain %d, widest visible set %zu\n",
                deepest,
                widest);
    EXPECT_GT(deepest, 2) << "the interior walk no longer chains past two rooms";
    EXPECT_LE(deepest, 6);
}
