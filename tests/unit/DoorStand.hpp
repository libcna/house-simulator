// SPDX-License-Identifier: MIT
#pragma once

#include <algorithm>
#include <filesystem>
#include <string>

#include "Microsoft/Xna/Framework/Vector3.hpp"

#include "cnahouse/world/WorldData.hpp"
#include "cnahouse/world/WorldLoader.hpp"

/// @file
/// Standing a body in front of a door, derived from the door (`HOUSE-00687`, `HOUSE-00691`).
///
/// Shared because two tests need the same arithmetic and a second copy of it is a second thing to
/// move when a door does: `DoorMatrixTests` opens and shuts every door from both sides, and
/// `AjarDoorTests` opens one of them a crack.

namespace cnahouse::testsupport
{
    namespace world = cnahouse::world;
    using Microsoft::Xna::Framework::Vector3;

    /// How far back from a door a body stands to look at it. A stride and a half: far enough that
    /// §44's lens sees the whole opening, near enough to be in the room the door is in.
    constexpr float kStandOff = 1.5F;

    inline bool ContentIsBuilt()
    {
        return std::filesystem::exists("content/world/layout.cells.json");
    }

    inline world::WorldData LoadWorld()
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
    inline Vector3 PortalCentre(const world::Portal& portal)
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
    inline Stand
    StandBefore(const world::WorldData& data, const world::Portal& portal, const world::Cell& cell)
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

} // namespace cnahouse::testsupport
