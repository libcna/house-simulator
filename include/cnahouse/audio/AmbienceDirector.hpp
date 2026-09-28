// SPDX-License-Identifier: MIT
#pragma once

#include "cnahouse/weather/WeatherState.hpp"
#include "cnahouse/world/WorldTypes.hpp"
#include <array>

namespace cnahouse::audio
{

    /// @brief The three retained ambience beds after cell and sun cross-fades.
    struct AmbienceMix
    {
        float interior = 0.0F;
        float exteriorDay = 0.0F;
        float exteriorNight = 0.0F;
    };

    struct WeatherMix
    {
        /// Rain calm/strong, wind calm/forest. Weights include shelter attenuation.
        std::array<float, 4> layers{};
        float dull = 0.0F;
    };

    /// @brief Selects the one interior bed or the exterior day/night pair.
    ///
    /// This is deliberately not a room-tone matrix: only `CellKind::Exterior` selects outdoors.
    /// The exterior day/night balance follows the shared sun altitude, while crossing the house
    /// boundary uses one short fade so neither bed cuts at a doorway.
    class AmbienceDirector
    {
    public:
        static constexpr float kCellCrossfadeSeconds = 0.8F;
        static constexpr double kNightAltitudeDeg = -6.0;
        static constexpr double kDayAltitudeDeg = 3.0;

        /// @brief Advances the bounded mix, snapping the first observation to its correct side.
        [[nodiscard]] AmbienceMix
        Advance(world::CellKind listenerKind, double sunAltitudeDeg, float deltaSeconds) noexcept;

        /// Roof distance uses eye height; underground depth uses the cell's authored floor.
        [[nodiscard]] WeatherMix AdvanceWeather(world::CellKind listenerKind,
                                                float skyExposure,
                                                float roofDistance,
                                                float undergroundDepth,
                                                const weather::WeatherState& weather,
                                                float deltaSeconds) noexcept;

        void Reset() noexcept;

        [[nodiscard]] static float DayMix(double sunAltitudeDeg) noexcept;

    private:
        bool initialized_ = false;
        float exterior_ = 0.0F;
        bool weatherInitialized_ = false;
        WeatherMix weatherMix_;
    };

} // namespace cnahouse::audio
