// SPDX-License-Identifier: MIT
#pragma once

#include <string>
#include <vector>

#include "cnahouse/environment/SimClock.hpp"

namespace Microsoft::Xna::Framework::Graphics
{
    class SpriteBatch;
}

namespace cnahouse::ui
{
    class TextRenderer;
}

namespace cnahouse::debug
{

    /// @brief §71's `F8` environment overlay -- its TIME section (`HOUSE-01537`).
    ///
    /// §71 lists `F8` as *"simulated date/time, sun/moon altitude and azimuth, moon phase and name,
    /// the full weather state vector, the current archetype and time to the next transition, RNG
    /// state"*. Everything after the first clause belongs to §35.3's sun and §36's weather, which
    /// are later phases. **The overlay says so on its own last line** rather than looking finished:
    /// a debug panel that is silently missing half its rows teaches its reader that the missing
    /// rows do not exist.
    ///
    /// A **presenter**, like §69's `F2` and §71's `F1`: it owns no measurement and takes no queries
    /// of its own, so what it says is asserted in a unit test rather than looked at. It reads a
    /// `SimClock` and nothing else, because §35.1 says everything time-dependent reads that one
    /// clock and nothing keeps its own.
    class EnvironmentOverlay
    {
    public:
        void SetVisible(bool visible) noexcept
        {
            visible_ = visible;
        }

        void Toggle() noexcept
        {
            visible_ = !visible_;
        }

        [[nodiscard]] bool Visible() const noexcept
        {
            return visible_;
        }

        /// @brief The lines the overlay would draw, top to bottom.
        [[nodiscard]] std::vector<std::string> Lines(const environment::SimClock& clock) const;

        void Draw(Microsoft::Xna::Framework::Graphics::SpriteBatch& batch,
                  const ui::TextRenderer& text,
                  const environment::SimClock& clock) const;

    private:
        bool visible_ = false;
    };

    /// @brief §36.3's season as a person reads it: `spring`, `summer`, `autumn`, `winter`.
    [[nodiscard]] std::string_view SeasonName(int season) noexcept;

} // namespace cnahouse::debug
