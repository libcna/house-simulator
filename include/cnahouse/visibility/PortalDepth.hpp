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
    /// | cased opening, door, stair well | 6 | 6 |
    /// | window or glass, interior → `EXT_WORLD` | 3 | — |
    /// | window or glass, `EXT_WORLD` → interior | — | **1** |
    /// | garage door | 4 | 2 |
    ///
    /// **The asymmetry is the point.** Standing in the garden you should see *one* room through a
    /// window, not that room plus everything behind its open door; depth 1 from outside delivers
    /// exactly that. An open entrance door is different: its unobstructed view may continue
    /// through the house, so it receives the interior door allowance. From inside, a window onto
    /// the garden is worth three because the garden is where the neighbourhood, terrain and sky
    /// are.
    ///
    /// A portal is "a window onto outside" when one of its cells is an exterior one, which is read
    /// from the world rather than from the portal's own kind: a glazed door between the sunroom and
    /// the terrace is the same case as a window, and its `kind` says `slider`.
    [[nodiscard]] int
    MaxDepthFor(const world::Portal& portal, const world::WorldData& world, CameraSide camera) noexcept;

} // namespace cnahouse::visibility
