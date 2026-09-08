// SPDX-License-Identifier: MIT
#include "cnahouse/visibility/PortalFacing.hpp"

#include <algorithm>

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

        /// The centre of a cell's footprint at mid-height. Enough to say which side of a plane the
        /// cell is on, which is all this file asks of it.
        [[nodiscard]] bool
        CentreOf(const world::WorldData& world, const world::Cell& cell, Vector3& out) noexcept
        {
            if (cell.boxes.empty())
            {
                return false;
            }
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
        if (!CentreOf(world, *cell, centre))
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
