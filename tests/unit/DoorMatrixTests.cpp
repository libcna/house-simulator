// SPDX-License-Identifier: MIT
//
// `HOUSE-00687`. §70.3: *"Open the kitchen door from the hall → `L0_KITCHEN` enters the visible
// set; close it → it leaves. Repeated for all 62 doors, in both directions, from both sides."*
//
// **The poses are derived, not authored.** A door's own rectangle says where it is and which way
// it faces, so standing in front of one is arithmetic: a stride and a half back along the inward
// normal, at eye height, looking at the middle of it. Sixty-odd hand-authored poses would be
// sixty-odd chances to stand in a wall, and every one of them would have to move when a door does.
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <set>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "cnahouse/player/FirstPersonCamera.hpp"
#include "cnahouse/visibility/VisibilitySystem.hpp"
#include "cnahouse/world/WorldData.hpp"
#include "cnahouse/world/WorldLoader.hpp"

namespace
{
    namespace world = cnahouse::world;
    using cnahouse::app::FrameContext;
    using cnahouse::player::FirstPersonCamera;
    using cnahouse::player::kPlayerEyeHeight;
    using cnahouse::player::PlayerState;
    using cnahouse::util::Id;
    using cnahouse::util::IdRegistry;
    using cnahouse::visibility::CameraView;
    using cnahouse::visibility::ClipFrustum;
    using cnahouse::visibility::VisibilitySystem;
    using cnahouse::visibility::VisibleCell;
    using Microsoft::Xna::Framework::Vector3;

    /// How far back from a door a body stands to look at it. A stride and a half: far enough that
    /// §44's lens sees the whole opening, near enough to be in the room the door is in.
    constexpr float kStandOff = 1.5F;

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

    /// Where the middle of @p portal is, in world space.
    Vector3 PortalCentre(const world::Portal& portal)
    {
        const float u = (portal.minU + portal.maxU) * 0.5F;
        const float v = (portal.minV + portal.maxV) * 0.5F;
        return portal.axis == world::PlaneAxis::X ? Vector3(portal.planeValue, v, u)
                                                  : Vector3(u, v, portal.planeValue);
    }

    struct Stand
    {
        bool valid = false;
        Vector3 feet;
        float yawDegrees = 0.0F;
    };

    /// A body in @p cell, a stride and a half back from @p portal, looking at it.
    Stand StandBefore(const world::WorldData& data, const world::Portal& portal, const world::Cell& cell)
    {
        const Vector3 centre = PortalCentre(portal);
        // The cell's OWN floor and not its level's: §12's yards, porch and balconies carry a
        // `yOverride`, and a body stood at the level's `ffl` outside is a body under the lawn --
        // whose eye is below the door it is meant to be looking at.
        const cnahouse::util::Result<world::Extent> extent = data.ExtentOf(cell);
        if (!extent || cell.boxes.empty())
        {
            return {};
        }
        const float floorY = extent->floorY;
        // Inward is whichever way the cell is: its own middle tells us, and a cell is never
        // centred on its own doorway.
        float middleX = 0.0F;
        float middleZ = 0.0F;
        for (const world::Footprint& box : cell.boxes)
        {
            middleX += (box.minX + box.maxX) * 0.5F / static_cast<float>(cell.boxes.size());
            middleZ += (box.minZ + box.maxZ) * 0.5F / static_cast<float>(cell.boxes.size());
        }
        Stand stand;
        if (portal.axis == world::PlaneAxis::X)
        {
            const float inward = middleX > centre.X ? 1.0F : -1.0F;
            stand.feet = Vector3(centre.X + inward * kStandOff, floorY, centre.Z);
            // §14: yaw 0 north, positive east. Looking back at the door is +X or -X.
            stand.yawDegrees = inward > 0.0F ? 270.0F : 90.0F;
        }
        else
        {
            const float inward = middleZ > centre.Z ? 1.0F : -1.0F;
            stand.feet = Vector3(centre.X, floorY, centre.Z + inward * kStandOff);
            stand.yawDegrees = inward > 0.0F ? 0.0F : 180.0F;
        }
        stand.valid = std::any_of(cell.boxes.begin(),
                                  cell.boxes.end(),
                                  [&stand](const world::Footprint& box)
                                  { return box.Contains(stand.feet.X, stand.feet.Z); });
        return stand;
    }

    std::set<std::string> WalkFrom(
        const world::WorldData& data, VisibilitySystem& system, const Stand& stand, Id cell, Id openPortal)
    {
        for (const world::Portal& portal : data.Portals())
        {
            system.SetAperture(portal.id, portal.id == openPortal ? 1.0F : 0.0F);
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

        std::set<std::string> names;
        for (const VisibleCell& seen : system.Visible())
        {
            names.insert(std::string(IdRegistry::NameOf(seen.cell)));
        }
        return names;
    }

} // namespace

TEST(DoorMatrixTests, EveryLeafedDoorLetsItsOwnRoomInWhenItOpensAndOutWhenItShuts)
{
    if (!ContentIsBuilt())
    {
        GTEST_SKIP() << "no deployed world; run tools/ci/build_content.py --only world";
    }
    IdRegistry::ResetForTesting();
    const world::WorldData data = LoadWorld();
    VisibilitySystem system(data);

    int checked = 0;
    int skippedNoStand = 0;
    int skippedGlass = 0;
    int alsoGlazed = 0;
    for (const world::Portal& portal : data.Portals())
    {
        // A door is a portal with a LEAF. A cased opening has none and is a doorway you walk
        // through; §13.1's "cased openings are not doors" is about counting doors.
        if (!portal.aperture.IsValid())
        {
            continue;
        }
        // Glass passes light shut, so opening it changes nothing to assert on -- and §25.3 says
        // so. Counted, because a house whose doors were all glass would make this test vacuous.
        if (portal.opacity != world::PortalOpacity::OpaqueWhenClosed)
        {
            ++skippedGlass;
            continue;
        }

        for (const Id side : {portal.cellA, portal.cellB})
        {
            const world::Cell* here = data.FindCell(side);
            const Id beyond = side == portal.cellA ? portal.cellB : portal.cellA;
            if (here == nullptr || data.FindCell(beyond) == nullptr)
            {
                continue;
            }
            const Stand stand = StandBefore(data, portal, *here);
            if (!stand.valid)
            {
                ++skippedNoStand;
                continue;
            }
            ++checked;

            const std::set<std::string> shut = WalkFrom(data, system, stand, side, Id{});
            const std::set<std::string> open = WalkFrom(data, system, stand, side, portal.id);
            const std::string name = std::string(IdRegistry::NameOf(portal.id));
            const std::string other = std::string(IdRegistry::NameOf(beyond));

            // Both directions, which is what §70.3 asks for: it enters when the door opens and it
            // leaves when the door shuts. Standing in front of a door, the room behind it is
            // exactly what the door decides.
            EXPECT_TRUE(open.contains(other))
                << name << " opened and " << other << " did not enter the set from "
                << IdRegistry::NameOf(side) << " [tested " << system.Stats().portalsTested << " crossed "
                << system.Stats().portalsCrossed << " closed " << system.Stats().skippedClosed << " facing "
                << system.Stats().skippedFacing << " clipped " << system.Stats().skippedClipped << " small "
                << system.Stats().skippedArea << "]";
            // ...unless the two rooms are joined by something ELSE that passes light. §12 does
            // this four times: the front door has sidelights beside it, the landing has two
            // windows onto the balcony its door opens onto, and the garage and the shed each have
            // a window onto the yard their door leads to. A door is not the only way light gets
            // between two rooms, and a test that assumed it was would be asserting the layout it
            // wished for.
            const bool glazedToo = std::any_of(
                data.Portals().begin(),
                data.Portals().end(),
                [&](const world::Portal& sibling)
                {
                    if (sibling.id == portal.id)
                    {
                        return false;
                    }
                    const bool joinsTheSameTwo =
                        (sibling.cellA == portal.cellA && sibling.cellB == portal.cellB) ||
                        (sibling.cellA == portal.cellB && sibling.cellB == portal.cellA);
                    return joinsTheSameTwo && sibling.opacity != world::PortalOpacity::OpaqueWhenClosed;
                });
            if (glazedToo)
            {
                ++alsoGlazed;
            }
            else
            {
                EXPECT_FALSE(shut.contains(other)) << name << " is shut and " << other
                                                   << " is visible anyway from " << IdRegistry::NameOf(side);
            }
            // ...and opening one door never takes a cell AWAY.
            EXPECT_TRUE(std::includes(open.begin(), open.end(), shut.begin(), shut.end()))
                << name << " opened and the set lost a cell";
        }
    }
    std::printf("  %d door/side pair(s) checked, %d of them glazed to the same room another way, "
                "%d glass doors (shut is still see-through), %d with nowhere to stand\n",
                checked,
                alsoGlazed,
                skippedGlass,
                skippedNoStand);
    // Four of §12's door pairs have a window or a sidelight onto the same room. More than a
    // handful would mean the "it leaves when the door shuts" half of §70.3 is barely being made.
    EXPECT_LE(alsoGlazed, 12) << "most doors have a second way through, so shutting one proves "
                                 "little";
    // §13's house has 62 doors; two sides each, less the ones whose room is too small to stand a
    // body in front of the door in.
    EXPECT_GT(checked, 80) << "most of the house's doors were skipped, so this proves little";
}

TEST(DoorMatrixTests, OpeningADoorNobodyIsLookingAtChangesNothing)
{
    // The other half of §25.3, and the one a test of the door in front of you cannot make: a door
    // behind the camera is not in any cone, so its state cannot matter. A system that rebuilt the
    // set from apertures rather than from the FRUSTUM would fail this and pass everything above.
    if (!ContentIsBuilt())
    {
        GTEST_SKIP() << "no deployed world";
    }
    IdRegistry::ResetForTesting();
    const world::WorldData data = LoadWorld();
    VisibilitySystem system(data);

    int checked = 0;
    for (const world::Portal& portal : data.Portals())
    {
        if (!portal.aperture.IsValid() || portal.opacity != world::PortalOpacity::OpaqueWhenClosed)
        {
            continue;
        }
        const world::Cell* here = data.FindCell(portal.cellA);
        if (here == nullptr)
        {
            continue;
        }
        Stand stand = StandBefore(data, portal, *here);
        if (!stand.valid)
        {
            continue;
        }
        // Turned round: the door is now behind the body.
        stand.yawDegrees = std::fmod(stand.yawDegrees + 180.0F, 360.0F);
        const std::set<std::string> shut = WalkFrom(data, system, stand, portal.cellA, Id{});
        const std::set<std::string> open = WalkFrom(data, system, stand, portal.cellA, portal.id);
        ++checked;
        EXPECT_EQ(shut, open) << IdRegistry::NameOf(portal.id)
                              << " changed the visible set while it was behind the camera";
    }
    std::printf("  %d door(s) opened behind the camera, none of them changing the set\n", checked);
    EXPECT_GT(checked, 20);
}
