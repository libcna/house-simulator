// SPDX-License-Identifier: MIT
#pragma once

#include "cnahouse/debug/Console.hpp"
#include "cnahouse/environment/SimClock.hpp"

namespace cnahouse::debug
{

    /// @brief What §71's `time` commands reach (`HOUSE-01536`).
    ///
    /// A pointer and not a captured reference, for the reason `PlayerCommands` gives: the command
    /// is registered once at start-up against state that outlives it, and a test can drive it with
    /// no game at all.
    struct TimeCommandContext
    {
        environment::SimClock* clock = nullptr;
    };

    /// @brief Registers §71's `time set <hh:mm>`, `time scale <x>` and `time advance <days>`.
    ///
    /// **One command with three verbs, not three commands**, because that is how §71 lists them
    /// and because `time` with no verb is then the reading -- which is what somebody about to
    /// change the clock wants first.
    void RegisterTimeCommands(Console& console, TimeCommandContext context);

} // namespace cnahouse::debug
