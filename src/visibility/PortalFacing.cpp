// SPDX-License-Identifier: MIT
#include "cnahouse/visibility/PortalFacing.hpp"

#include <algorithm>
#include <cmath>

namespace cnahouse::visibility
{
    using Microsoft::Xna::Framework::Vector3;

    namespace
    {
        /// The component of @p point along the portal's plane axis.
        [[nodiscard]] float Along(const world::Portal& portal, const Vector3& point) noexcept
        {
            switch (portal.axis)
            {
                case world::PlaneAxis::X:
                    return point.X;
                case world::PlaneAxis::Y:
                    return point.Y;
                case world::PlaneAxis::Z:
                default:
                    return point.Z;
            }
        }

        /// The centre of the cell's own footprint AT this portal, at mid-height.
        ///
        /// **Not the cell's bounding box** (`HOUSE-00687`). §12's cells are one or more boxes and
        /// several of them wrap round something: `EXT_SIDEYARD_E` runs up the east side of the
        /// house and along the back of the garage, so the middle of its bounding box is INSIDE the
        /// garage -- on the far side of the garage's own east door. Asked which side of that door
        /// the yard is on, a bounding box answers "the garage's", and the walk then refuses to look
        /// through a door the player is standing in front of. That is over-culling, which is the
        /// one failure §25 has no tolerance for.
        ///
        /// The box that TOUCHES the portal is the room the doorway is in, so that is the one asked.
        /// A cell with one box -- most of the house -- is unchanged by this.
        [[nodiscard]] bool CentreOf(const world::WorldData& world,
                                    const world::Cell& cell,
                                    const world::Portal& portal,
                                    Vector3& out) noexcept
        {
            if (cell.boxes.empty())
            {
                return false;
            }
            constexpr float kTouching = 1e-3F;
            const world::Footprint* chosen = nullptr;
            for (const world::Footprint& box : cell.boxes)
            {
                const bool touches = portal.axis == world::PlaneAxis::X
                                         ? (std::abs(box.minX - portal.planeValue) < kTouching ||
                                            std::abs(box.maxX - portal.planeValue) < kTouching)
                                         : (std::abs(box.minZ - portal.planeValue) < kTouching ||
                                            std::abs(box.maxZ - portal.planeValue) < kTouching);
                if (!touches)
                {
                    continue;
                }
                // ...and ACROSS the doorway, not merely on its plane: a long wall can meet the
                // same plane a room away from the opening in it.
                const float lo = portal.axis == world::PlaneAxis::X ? box.minZ : box.minX;
                const float hi = portal.axis == world::PlaneAxis::X ? box.maxZ : box.maxX;
                if (std::min(hi, portal.maxU) - std::max(lo, portal.minU) <= 0.0F)
                {
                    continue;
                }
                chosen = &box;
                break;
            }
            // A `Y` portal is a stairwell in a floor and no footprint edge meets it; so is a cell
            // whose boxes do not reach their own portal. The bounding box is the fallback, which is
            // what this used to be for everything.
            float minX = cell.boxes.front().minX;
            float maxX = cell.boxes.front().maxX;
            float minZ = cell.boxes.front().minZ;
            float maxZ = cell.boxes.front().maxZ;
            for (const world::Footprint& box : cell.boxes)
            {
                minX = std::min(minX, box.minX);
                maxX = std::max(maxX, box.maxX);
                minZ = std::min(minZ, box.minZ);
                maxZ = std::max(maxZ, box.maxZ);
            }
            if (chosen != nullptr)
            {
                minX = chosen->minX;
                maxX = chosen->maxX;
                minZ = chosen->minZ;
                maxZ = chosen->maxZ;
            }
            const util::Result<world::Extent> extent = world.ExtentOf(cell);
            const float low = extent ? extent.Value().floorY : 0.0F;
            const float high = extent ? extent.Value().ceilingY : 0.0F;
            out = Vector3{(minX + maxX) * 0.5F, (low + high) * 0.5F, (minZ + maxZ) * 0.5F};
            return true;
        }
    } // namespace

    float SignedDistanceToPlane(const world::Portal& portal, const Vector3& point) noexcept
    {
        return Along(portal, point) - portal.planeValue;
    }

    bool PlaneFacesAway(const world::Portal& portal,
                        const world::WorldData& world,
                        util::Id from,
                        const Vector3& eye) noexcept
    {
        const world::Cell* cell = world.FindCell(from);
        if (cell == nullptr)
        {
            return false;
        }
        Vector3 centre{};
        if (!CentreOf(world, *cell, portal, centre))
        {
            return false;
        }

        const float cellSide = SignedDistanceToPlane(portal, centre);
        const float eyeSide = SignedDistanceToPlane(portal, eye);
        if (cellSide == 0.0F || eyeSide == 0.0F)
        {
            // A cell whose centre is in the plane cannot say which side it is on, and an eye in
            // the doorway has nothing to be behind. Both face towards, which is the conservative
            // answer: the walk looks through a portal it might not have needed to.
            return false;
        }
        return (cellSide > 0.0F) != (eyeSide > 0.0F);
    }
} // namespace cnahouse::visibility
