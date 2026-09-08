// SPDX-License-Identifier: MIT
#pragma once

#include "Microsoft/Xna/Framework/Vector3.hpp"

#include "cnahouse/app/EventQueue.hpp"
#include "cnahouse/world/CellEvents.hpp"
#include "cnahouse/world/SpatialIndex.hpp"
#include "cnahouse/world/WorldData.hpp"

namespace cnahouse::player
{

    /// @brief Which cell the player is in, kept across steps (`HOUSE-00559`).
    ///
    /// **The state is why this is a class and not a function.** §16.4's lookup is incremental: it
    /// answers in O(1) by testing the cell the caller was in LAST, expanded by 5 cm, and only walks
    /// the portals or the grid when that fails. Something has to remember the last answer, and
    /// remembering it in one place is what makes the hysteresis mean anything -- two systems each
    /// keeping their own `currentCell` would each get their own idea of when the player crossed a
    /// doorway.
    ///
    /// The 5 cm itself lives in `SpatialIndex::kHysteresis` and is applied there, on the
    /// incremental test alone: staying put is sticky, arriving is not.
    class CellTracker
    {
    public:
        /// @brief The cell the player was last found in, or an invalid id before the first update.
        [[nodiscard]] util::Id Current() const noexcept
        {
            return current_;
        }

        /// @brief Forgets where the player was, so the next update starts from the GRID.
        ///
        /// A spawn, a teleport and a save load are all "the player is now somewhere, and where
        /// they were tells you nothing about it" -- and the incremental test would answer such a
        /// query with the room they were in before the load.
        void Forget() noexcept
        {
            current_ = {};
        }

        /// @brief Finds the cell containing @p point and publishes `CellEntered` if it changed.
        ///
        /// @param queue optional: nothing is published without one, which is what lets a test or a
        ///        tool track cells without standing up an event system.
        /// @return true if the cell changed this update.
        bool Update(const world::WorldData& world,
                    const world::SpatialIndex& index,
                    const Microsoft::Xna::Framework::Vector3& point,
                    app::EventQueue* queue = nullptr);

        /// @brief Which of §16.4's four steps answered the last update. For the overlay, and for
        ///        the test that proves the incremental step is the one doing the work.
        [[nodiscard]] world::SpatialIndex::Step LastStep() const noexcept
        {
            return step_;
        }

    private:
        util::Id current_{};
        world::SpatialIndex::Step step_ = world::SpatialIndex::Step::None;
    };

} // namespace cnahouse::player
