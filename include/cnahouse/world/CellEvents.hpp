// SPDX-License-Identifier: MIT
#pragma once

#include "cnahouse/util/Ids.hpp"

namespace cnahouse::world
{

    /// @brief The player's cell changed (`HOUSE-00559`).
    ///
    /// §43.4: *"The controller publishes `CellEntered` whenever `currentCell` changes. That single
    /// event drives the visibility root, the audio listener's cell, the residency request set, the
    /// ambience bed cross-fade and the debug overlay."*
    ///
    /// **One event for five systems, and that is the point.** Each of them needs to know the same
    /// thing at the same moment, and five systems each deciding for themselves which cell the
    /// player is in is five chances for the audio to be in the hall while the lighting is in the
    /// kitchen.
    struct CellEntered
    {
        /// @brief The cell just entered.
        util::Id cell;
        /// @brief The cell left behind. Invalid on the first assignment -- a spawn enters a cell
        ///        without leaving one, and a listener that cross-fades needs to tell that apart.
        util::Id previous;
    };

} // namespace cnahouse::world
