// SPDX-License-Identifier: MIT
#pragma once

#include <cstdint>
#include <span>
#include <vector>

#include "Microsoft/Xna/Framework/BoundingSphere.hpp"

#include "cnahouse/util/Ids.hpp"
#include "cnahouse/visibility/PortalTraversal.hpp"

namespace cnahouse::visibility
{

    /// @brief One thing that moves, as the visibility stage needs to see it (`HOUSE-00673`).
    ///
    /// §17.4 batches a cell's static props into chunks -- *"drawing a fully visible kitchen with
    /// 140 props costs 5 draw calls, not 140"* -- and excludes the ones that move: *"a drawer, a
    /// door, a chair the player can nudge"*. Those are submitted one at a time, so each one is
    /// worth a test of its own, and §71.2 budgets 180 of them typically and 700 at the hard limit.
    ///
    /// **A sphere and not a box**, which is §25.1's own choice (*"AABB/sphere vs. frusta"*): the
    /// things in this list rotate -- a door swings, a chair is nudged round -- and a sphere is the
    /// bound that does not have to be rebuilt when they do.
    struct DynamicInstance
    {
        util::Id id;
        /// @brief The cell it is in, from §16.4's lookup. An instance in no visible cell is not
        ///        tested against anything: the room it is in has already been culled.
        util::Id cell;
        Microsoft::Xna::Framework::BoundingSphere bounds;
    };

    /// @brief §25.1's step 3 for the things that move.
    ///
    /// The list is handed over every frame rather than indexed once, because it changes every
    /// frame -- which is what makes these dynamic. `HOUSE-00672`'s chunks are the other half of the
    /// same step and are prepared once, because they do not.
    class InstanceCuller
    {
    public:
        struct Stats
        {
            int tested = 0;
            int drawn = 0;
            /// @brief In a cell the walk never reached. The portal graph did this, not the cones.
            int culledByCell = 0;
            /// @brief In a visible cell, but in none of its cones.
            int culledByCone = 0;
        };

        void Cull(std::span<const VisibleCell> visible, std::span<const DynamicInstance> instances);

        /// @brief Indices into the span passed to `Cull`, in that span's order.
        [[nodiscard]] std::span<const std::uint32_t> Instances() const noexcept
        {
            return drawn_;
        }

        [[nodiscard]] const Stats& Statistics() const noexcept
        {
            return stats_;
        }

    private:
        std::vector<std::uint32_t> drawn_;
        /// @brief The visible set as a flat lookup, rebuilt per frame. Thirty entries at most
        ///        (`kMaxVisibleCells`), so a linear scan is faster than anything with a hash in it.
        std::vector<const VisibleCell*> lookup_;
        Stats stats_;
    };

} // namespace cnahouse::visibility
