// SPDX-License-Identifier: MIT
#pragma once

#include <cstdint>

#include "cnahouse/world/WorldData.hpp"
#include "cnahouse/world/WorldTypes.hpp"

/// @file
/// `maxDepthFor` -- how far the portal walk may chain through one kind of opening
/// (`HOUSE-00667`, `cna-house.md` §25.2).

namespace cnahouse::visibility
{

    /// @brief Where the camera is, which is half of §25.2's depth table.
    enum class CameraSide : std::uint8_t
    {
        Interior,
        Exterior,
    };

    /// @brief §25.2's depth cap for crossing @p portal, given where the camera is standing.
    ///
    /// | Portal kind | From inside | From outside |
    /// |---|---|---|
    /// | cased opening, door, stair well; clear or open glazed door | 6 | 6 |
    /// | window or closed frosted glazing, interior → `EXT_WORLD` | 3 | — |
    /// | window or closed frosted glazing, `EXT_WORLD` → interior | — | **1** |
    /// | garage door | 4 | 2 |
    ///
    /// **The asymmetry is the point.** Standing in the garden you should see *one* room through a
    /// window, not that room plus everything behind its open door; depth 1 from outside delivers
    /// exactly that. An open entrance door is different: its unobstructed view may continue
    /// through the house, so it receives the interior door allowance. From inside, a window onto
    /// the garden is worth three because the garden is where the neighbourhood, terrain and sky
    /// are.
    ///
    /// Clear glazed doors and latched-open doors use the door row: their visible sightline must
    /// not expose the sky instead of a culled room behind an interior opening. Closed frosted
    /// doors retain the glazing row, as do windows regardless of their aperture state.
    [[nodiscard]] int MaxDepthFor(const world::Portal& portal,
                                  const world::WorldData& world,
                                  CameraSide camera,
                                  bool open = false) noexcept;

} // namespace cnahouse::visibility
