// SPDX-License-Identifier: MIT
#pragma once

#include <cstddef>
#include <string>
#include <vector>

#include "cnahouse/environment/MoonModel.hpp"
#include "cnahouse/environment/SimClock.hpp"
#include "cnahouse/environment/SunModel.hpp"
#include "cnahouse/rendering/StarField.hpp"

namespace cnahouse::weather
{
    class WeatherSystem;
}

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

    /// @brief The one retained celestial answer that §71's F8 presents.
    ///
    /// `CnaHouseGame` copies this from the current lighting and sky systems after their update. The
    /// overlay therefore cannot accidentally run a second ephemeris or invent a catalogue cutoff
    /// that differs from the frame being drawn.
    struct CelestialOverlayState
    {
        environment::SunPosition sun;
        environment::MoonPosition moon;
        environment::MoonPhase moonPhase;
        rendering::StarVisibility starVisibility;
        std::size_t visibleStarCount = 0;
        std::size_t catalogueStarCount = 0;
        bool starFieldAvailable = false;
    };

    /// @brief §71's `F8` environment overlay (`HOUSE-01537`, `HOUSE-01616`, `HOUSE-01695`).
    ///
    /// §71 lists `F8` as *"simulated date/time, sun/moon altitude and azimuth, moon phase and name,
    /// the full weather state vector, the current archetype and time to the next transition, RNG
    /// state"*. The weather rows read `WeatherSystem`'s one live state and the celestial rows read
    /// the one retained lighting/sky answer for the current frame.
    ///
    /// A **presenter**, like §69's `F2` and §71's `F1`: it owns no measurement and takes no queries
    /// of its own, so what it says is asserted in a unit test rather than looked at. It reads a
    /// `SimClock`, `WeatherSystem` and `CelestialOverlayState` and owns none of them.
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
        [[nodiscard]] std::vector<std::string> Lines(const environment::SimClock& clock,
                                                     const weather::WeatherSystem* weather = nullptr,
                                                     const CelestialOverlayState* celestial = nullptr) const;

        void Draw(Microsoft::Xna::Framework::Graphics::SpriteBatch& batch,
                  const ui::TextRenderer& text,
                  const environment::SimClock& clock,
                  const weather::WeatherSystem* weather,
                  const CelestialOverlayState* celestial) const;

    private:
        bool visible_ = false;
    };

    /// @brief §36.3's season as a person reads it: `spring`, `summer`, `autumn`, `winter`.
    [[nodiscard]] std::string_view SeasonName(int season) noexcept;

} // namespace cnahouse::debug
