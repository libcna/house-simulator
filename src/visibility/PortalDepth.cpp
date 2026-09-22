// SPDX-License-Identifier: MIT
#include "cnahouse/visibility/PortalDepth.hpp"

namespace cnahouse::visibility
{
    namespace
    {
        /// §25.2's four rows, named so the table below reads like the table above.
        constexpr int kDoorFromInside = 6;
        // Static-open entrance doors can expose the same long interior sightline from a porch as
        // from the foyer. The old value of two only matched the former all-doors-shut start.
        constexpr int kDoorFromOutside = 6;
        constexpr int kGlazedFromInside = 3;
        constexpr int kGlazedFromOutside = 1;
        constexpr int kGarageFromInside = 4;
        constexpr int kGarageFromOutside = 2;

        [[nodiscard]] bool TouchesOutside(const world::Portal& portal, const world::WorldData& world) noexcept
        {
            for (const util::Id side : {portal.cellA, portal.cellB})
            {
                const world::Cell* cell = world.FindCell(side);
                if (cell != nullptr && cell->kind == world::CellKind::Exterior)
                {
                    return true;
                }
            }
            return false;
        }
    } // namespace

    int MaxDepthFor(const world::Portal& portal, const world::WorldData& world, CameraSide camera) noexcept
    {
        const bool inside = camera == CameraSide::Interior;
        if (portal.kind == world::PortalKind::GarageDoor)
        {
            return inside ? kGarageFromInside : kGarageFromOutside;
        }
        // Glazed onto the outdoors: a window, or anything with glass in it between a room and an
        // exterior cell. The kind alone is not enough -- the sunroom's slider is a `slider`.
        const bool glazed = portal.kind == world::PortalKind::Window ||
                            portal.opacity == world::PortalOpacity::Glass ||
                            portal.opacity == world::PortalOpacity::Translucent;
        if (glazed && TouchesOutside(portal, world))
        {
            return inside ? kGlazedFromInside : kGlazedFromOutside;
        }
        return inside ? kDoorFromInside : kDoorFromOutside;
    }
} // namespace cnahouse::visibility
