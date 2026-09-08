// SPDX-License-Identifier: MIT
#pragma once

#include "cnahouse/util/Ids.hpp"
#include "cnahouse/world/WorldData.hpp"
#include "cnahouse/world/WorldTypes.hpp"

/// @file
/// The back-face test of `cna-house.md` §25.2's traversal (`HOUSE-00666`).

namespace cnahouse::visibility
{

    /// @brief How far @p point is from @p portal's plane, positive on the plane axis's `+` side.
    ///
    /// Signed, because the sign is the whole answer: which side of a doorway something is on.
    [[nodiscard]] float SignedDistanceToPlane(const world::Portal& portal,
                                              const Microsoft::Xna::Framework::Vector3& point) noexcept;

    /// @brief §25.2's `if p.plane faces away from the camera: continue`.
    ///
    /// The walk expands a cell it believes the camera can see into. A portal of that cell leads
    /// somewhere on the **other** side of its plane; if the camera is already on that other side,
    /// it is looking at the doorway from behind and cannot see through it into @p from's
    /// neighbour. Skipping those is most of what stops the walk turning round and re-entering the
    /// room it came from.
    ///
    /// Which side @p from is on is read from the cell's own box rather than passed in, because a
    /// caller that had to work it out would work it out differently in two places. A camera
    /// exactly in the doorway plane — standing in the opening — faces **towards**: at zero
    /// distance there is nothing to be behind.
    ///
    /// Returns false when either the cell or its geometry is missing: a portal whose cell is not
    /// there is rule 6's finding, and a visibility walk must not silently drop a room over it.
    [[nodiscard]] bool PlaneFacesAway(const world::Portal& portal,
                                      const world::WorldData& world,
                                      util::Id from,
                                      const Microsoft::Xna::Framework::Vector3& eye) noexcept;

} // namespace cnahouse::visibility
