// SPDX-License-Identifier: MIT
#pragma once

#include <cstdint>

#include "cnahouse/app/CommandLine.hpp"
#include "cnahouse/visibility/PortalTraversal.hpp"

namespace cnahouse::visibility
{

    /// @brief §26.4's three sets.
    enum class DetailSet : std::uint8_t
    {
        /// @brief Shell, doors, windows, lights, interactables, large furniture. Never dropped.
        Essential,
        /// @brief Books, cushions, ornaments, kitchenware, papers, cables.
        Dressing,
        /// @brief Crumbs, small stains, tiny labels, individual pens.
        Micro,
    };

    /// @brief §26.4: dressing is dropped beyond this...
    inline constexpr float kDressingDistance = 18.0F;
    /// @brief ...and the micro set beyond this.
    inline constexpr float kMicroDistance = 8.0F;

    /// @brief What a cell draws, given how it was reached and how far away it is.
    struct CellDetail
    {
        bool dressing = true;
        bool micro = true;
        /// @brief The quality's own bias, plus §15.4's +1 for a cell seen through frosted glass.
        int lodBias = 0;

        [[nodiscard]] bool Draws(DetailSet set) const noexcept
        {
            switch (set)
            {
                case DetailSet::Essential:
                    // *"Never"* is the whole row: a room with no shell, no doors and no lights is
                    // not a cheaper room, it is a hole in the house.
                    return true;
                case DetailSet::Dressing:
                    return dressing;
                case DetailSet::Micro:
                    return micro;
            }
            return true;
        }
    };

    /// @brief §26.4's table, and §15.4's `translucent` row of it (`HOUSE-00669`).
    ///
    /// | Set | Dropped when |
    /// |---|---|
    /// | `essential` | never |
    /// | `dressing` | reached through a `translucent` portal, or quality `low`, or beyond 18 m |
    /// | `micro` | quality below `high`, or beyond 8 m |
    ///
    /// *"This is a large, cheap win: a room seen through a frosted door renders its shell and
    /// furniture but none of its 60 small dressing props."*
    ///
    /// @param flags the cell's, from `PortalTraversal` -- `Diffuse` only when EVERY cone into it
    ///        came through frosted glass.
    /// @param distance metres from the eye to the NEAREST point of the cell, not to its centre: a
    ///        room whose near corner is five metres away has dressing the player can read, whatever
    ///        its far corner is doing.
    /// @param lodBias §68's setting, which a diffuse cell adds one to.
    [[nodiscard]] CellDetail
    DetailFor(ConeFlags flags, app::QualityPreset quality, float distance, int lodBias) noexcept;

} // namespace cnahouse::visibility
