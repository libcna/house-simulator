// SPDX-License-Identifier: MIT
#pragma once

#include "cnahouse/debug/Console.hpp"

namespace cnahouse::debug
{

    /// @brief What §71's `cull` reaches (`HOUSE-00684`).
    ///
    /// A pointer and not a captured reference, for `PlayerCommands`' reason: the command is
    /// registered once at start-up against state that outlives it, and a test can drive it with no
    /// game at all.
    struct VisibilityCommandContext
    {
        /// @brief Whether the frame's draw list is built from §25's visible set.
        ///
        /// **Off does not mean "do not walk".** The walk keeps running and `F3` keeps reporting
        /// it; what stops is the draw list being built from its answer. That is the only way
        /// `HOUSE-00688`'s comparison can work -- two frames from one pose, one culled and one
        /// not, with everything else about them identical.
        bool* cullingEnabled = nullptr;
    };

    /// @brief Registers §71's `cull off|on` on @p console.
    void RegisterVisibilityCommands(Console& console, VisibilityCommandContext context);

} // namespace cnahouse::debug
