// SPDX-License-Identifier: MIT
//
// `HOUSE-00692`. §25.2's exterior column, on the row that matters most:
//
// > | window / glass (`EXT_WORLD` → interior) | — | **1** |
// >
// > The asymmetry matters: standing in the garden you should see *one* room through a window, not
// > that room plus everything behind its open door. Depth 1 from outside delivers exactly that.
//
// **Every glazed opening in the house, from outside, one at a time.** `OutdoorDepthTests` makes the
// claim over garden POSES -- whatever happens to be in view from four yards. This makes it over
// the WINDOWS: sixty-six of them, each with a body stood in front of it looking in.
#include <algorithm>
#include <cstdio>
#include <set>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "cnahouse/player/FirstPersonCamera.hpp"
#include "cnahouse/visibility/PortalDepth.hpp"
#include "cnahouse/visibility/VisibilitySystem.hpp"

#include "unit/DoorStand.hpp"

namespace
{
    namespace world = cnahouse::world;
    using cnahouse::app::FrameContext;
    using cnahouse::player::FirstPersonCamera;
    using cnahouse::player::kPlayerEyeHeight;
    using cnahouse::player::PlayerState;
    using cnahouse::testsupport::ContentIsBuilt;
    using cnahouse::testsupport::LoadWorld;
    using cnahouse::testsupport::Stand;
    using cnahouse::testsupport::StandBefore;
    using cnahouse::util::Id;
    using cnahouse::util::IdRegistry;
    using cnahouse::visibility::CameraSide;
    using cnahouse::visibility::CameraView;
    using cnahouse::visibility::ClipFrustum;
    using cnahouse::visibility::MaxDepthFor;
    using cnahouse::visibility::VisibilitySystem;
    using cnahouse::visibility::VisibleCell;
    using Microsoft::Xna::Framework::Vector3;

    bool IsExterior(const world::WorldData& data, Id cell)
    {
        const world::Cell* row = data.FindCell(cell);
        return row != nullptr && row->kind == world::CellKind::Exterior;
    }

    /// A glazed opening between the outdoors and a room: a window, or anything with glass in it.
    bool GlazedToOutside(const world::WorldData& data, const world::Portal& portal)
    {
        const bool glazed = portal.kind == world::PortalKind::Window ||
                            portal.opacity == world::PortalOpacity::Glass ||
                            portal.opacity == world::PortalOpacity::Translucent;
        return glazed && (IsExterior(data, portal.cellA) != IsExterior(data, portal.cellB));
    }

    void WalkFrom(
        const world::WorldData& data, VisibilitySystem& system, const Stand& stand, Id cell, bool doorsOpen)
    {
        for (const world::Portal& portal : data.Portals())
        {
            system.SetAperture(portal.id, doorsOpen ? 1.0F : 0.0F);
        }
        PlayerState state;
        state.position = Vector3(stand.feet.X, stand.feet.Y + state.Rise(), stand.feet.Z);
        state.yaw = stand.yawDegrees * 3.14159265F / 180.0F;
        FirstPersonCamera camera;
        camera.SetAspect(16.0F / 9.0F);
        camera.Update(state, kPlayerEyeHeight, 0.0F);

        CameraView view;
        view.cell = cell;
        view.eye = camera.Pose().eye;
        view.viewProjection = camera.View() * camera.Projection();
        view.frustum = ClipFrustum(camera.Frustum());
        view.nearPlane = camera.Frustum().getNearProperty();
        view.farPlane = camera.Frustum().getFarProperty();
        system.SetCamera(view);
        FrameContext frame;
        frame.frameIndex = 1;
        system.Update(frame);
    }

} // namespace

TEST(WindowDepthTests, TheTableSaysOneFromOutsideAndThreeFromInside)
{
    // §25.2's two glazed rows, read off the house's own openings rather than off the constants.
    if (!ContentIsBuilt())
    {
        GTEST_SKIP() << "no deployed world; run tools/ci/build_content.py --only world";
    }
    IdRegistry::ResetForTesting();
    const world::WorldData data = LoadWorld();

    int glazed = 0;
    for (const world::Portal& portal : data.Portals())
    {
        if (!GlazedToOutside(data, portal))
        {
            continue;
        }
        ++glazed;
        const bool clearDoor =
            world::IsPassable(portal.kind) && portal.opacity == world::PortalOpacity::Glass;
        EXPECT_EQ(MaxDepthFor(portal, data, CameraSide::Exterior), clearDoor ? 6 : 1)
            << IdRegistry::NameOf(portal.id);
        EXPECT_EQ(MaxDepthFor(portal, data, CameraSide::Interior), clearDoor ? 6 : 3)
            << IdRegistry::NameOf(portal.id);
    }
    std::printf("  %d glazed opening(s) between the outdoors and a room\n", glazed);
    EXPECT_GT(glazed, 40) << "§12.6 schedules 66 windows and the house has almost none";
}

TEST(WindowDepthTests, StandingOutsideAWindowShowsOneRoomAndNothingBehindIt)
{
    // The claim itself, over every glazed opening, with EVERY DOOR IN THE HOUSE OPEN -- which is
    // the only arrangement in which the failure is possible: a window that admitted a chain would
    // then show the room, its doorway, and whatever is through it.
    if (!ContentIsBuilt())
    {
        GTEST_SKIP() << "no deployed world";
    }
    IdRegistry::ResetForTesting();
    const world::WorldData data = LoadWorld();
    VisibilitySystem system(data);

    int checked = 0;
    int roomsSeen = 0;
    for (const world::Portal& portal : data.Portals())
    {
        if (!GlazedToOutside(data, portal))
        {
            continue;
        }
        // Stand OUTSIDE it: the exterior side is the one that is an exterior cell.
        const Id outside = IsExterior(data, portal.cellA) ? portal.cellA : portal.cellB;
        const Id inside = outside == portal.cellA ? portal.cellB : portal.cellA;
        const world::Cell* yard = data.FindCell(outside);
        if (yard == nullptr)
        {
            continue;
        }
        const Stand stand = StandBefore(data, portal, *yard);
        if (!stand.valid)
        {
            continue;
        }
        ++checked;

        const std::string window = std::string(IdRegistry::NameOf(portal.id));
        const std::string room = std::string(IdRegistry::NameOf(inside));

        // With every door SHUT, window/frosted chains stay one room deep. Clear glazed doors
        // can keep a longer sightline; their door allowance must not be confused with a window.
        WalkFrom(data, system, stand, outside, false);
        for (const VisibleCell& cell : system.Visible())
        {
            if (IsExterior(data, cell.cell) || cell.allowance != 1)
            {
                continue;
            }
            ++roomsSeen;
            EXPECT_EQ(cell.depth, 1) << window << ": " << IdRegistry::NameOf(cell.cell)
                                     << " is visible at depth " << cell.depth
                                     << " from outside, past §25.2's cap of 1";
            EXPECT_EQ(cell.allowance, 1)
                << window << ": " << IdRegistry::NameOf(cell.cell) << " carries an allowance of "
                << cell.allowance << " and not glazing's 1";
        }

        // ...and with every door OPEN, a front door beside the window is worth 2 (§25.2's other
        // exterior row), so cells DO appear at depth 2 -- but only ones whose chain crossed a
        // door. A cell that came through GLAZING still carries an allowance of 1 and still stops
        // at one room, which is the claim the two arrangements make together.
        WalkFrom(data, system, stand, outside, true);
        for (const VisibleCell& cell : system.Visible())
        {
            if (IsExterior(data, cell.cell) || cell.allowance != 1)
            {
                continue;
            }
            EXPECT_EQ(cell.depth, 1) << window << ": " << IdRegistry::NameOf(cell.cell)
                                     << " came through glazing and is " << cell.depth << " rooms deep";
        }
        WalkFrom(data, system, stand, outside, false);
        // ...and the room the window looks into is one of them, or the pose is looking at a wall.
        if (system.IsVisible(inside) && system.Find(inside)->allowance == 1)
        {
            EXPECT_EQ(system.Find(inside)->depth, 1)
                << window << " reached " << room << " at the wrong depth";
        }
    }
    std::printf("  %d window(s) stood in front of from outside; %d room sighting(s), every one "
                "of them at depth 1\n",
                checked,
                roomsSeen);
    EXPECT_GT(checked, 30) << "too few windows had a yard to stand in";
    EXPECT_GT(roomsSeen, 20) << "no window showed a room, so the cap was never exercised";
}

TEST(WindowDepthTests, FromInsideTheSameWindowIsWorthThree)
{
    // The other half of the asymmetry, and the reason it exists: from a room, a window onto the
    // garden is worth three because the garden is where the neighbourhood, the terrain and the sky
    // are. A cap applied symmetrically would either blind the house or open it.
    if (!ContentIsBuilt())
    {
        GTEST_SKIP() << "no deployed world";
    }
    IdRegistry::ResetForTesting();
    const world::WorldData data = LoadWorld();
    VisibilitySystem system(data);

    int deeperThanOne = 0;
    int checked = 0;
    for (const world::Portal& portal : data.Portals())
    {
        if (!GlazedToOutside(data, portal))
        {
            continue;
        }
        const Id outside = IsExterior(data, portal.cellA) ? portal.cellA : portal.cellB;
        const Id inside = outside == portal.cellA ? portal.cellB : portal.cellA;
        const world::Cell* room = data.FindCell(inside);
        if (room == nullptr)
        {
            continue;
        }
        const Stand stand = StandBefore(data, portal, *room);
        if (!stand.valid)
        {
            continue;
        }
        ++checked;
        WalkFrom(data, system, stand, inside, true);
        for (const VisibleCell& cell : system.Visible())
        {
            // The cells whose chain crossed GLAZING: their allowance is the 3 of §25.2's row, and
            // three is what they are allowed. A cell with a larger allowance came through a DOOR
            // -- worth six from inside -- and this room has one of those beside its window: from
            // `L0_FOYER` the front door reaches `EXT_WORLD` at depth 4, which the table permits
            // and this claim is not about.
            if (cell.allowance != 3)
            {
                continue;
            }
            EXPECT_LE(cell.depth, 3) << IdRegistry::NameOf(portal.id) << ": " << IdRegistry::NameOf(cell.cell)
                                     << " at depth " << cell.depth;
            deeperThanOne += cell.depth > 1 ? 1 : 0;
        }
    }
    std::printf("  %d window(s) looked out of from inside; %d glazed sighting(s) deeper than the 1 "
                "the same window allows from outside\n",
                checked,
                deeperThanOne);
    EXPECT_GT(checked, 30);
    EXPECT_GT(deeperThanOne, 0) << "no window reaches past depth 1 from inside either, so the "
                                   "asymmetry is doing nothing in this house";
}
