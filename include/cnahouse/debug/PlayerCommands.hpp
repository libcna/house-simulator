// SPDX-License-Identifier: MIT
#pragma once

#include "cnahouse/debug/Console.hpp"
#include "cnahouse/player/CellTracker.hpp"
#include "cnahouse/player/PlayerController.hpp"
#include "cnahouse/world/SpatialIndex.hpp"
#include "cnahouse/world/WorldData.hpp"

namespace cnahouse::debug
{

    /// @brief What `teleport` and `noclip` need to reach (`HOUSE-00563`).
    ///
    /// Passed by pointer rather than captured, so the commands can be registered once at start-up
    /// against state that outlives them, and so a test can drive them without a game.
    struct PlayerCommandContext
    {
        player::PlayerState* player = nullptr;
        player::CellTracker* tracker = nullptr;
        const world::WorldData* world = nullptr;
        const world::SpatialIndex* index = nullptr;
    };

    /// @brief Registers §71's `teleport <cellId>` and `noclip` on @p console.
    ///
    /// **`teleport` puts the body at the cell's floor, not at its centre.** A cell is a volume and
    /// the middle of one is in the air; a body dropped there falls, which turns a debugging aid
    /// into a fall and a landing sound. The height comes from the cell's own `ffl`.
    ///
    /// It also FORGETS the tracked cell, so the next lookup goes to §16.4's grid: the incremental
    /// test would answer a query about the attic with the kitchen the player teleported out of,
    /// expanded by 5 cm.
    void RegisterPlayerCommands(Console& console, PlayerCommandContext context);

} // namespace cnahouse::debug
