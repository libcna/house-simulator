// SPDX-License-Identifier: MIT
#pragma once

#include "cnahouse/util/Result.hpp"
#include "cnahouse/weather/WeatherState.hpp"

namespace cnahouse::weather
{

    inline constexpr float kWetnessAccumulationPerMinute = 0.045F;
    inline constexpr float kWetnessDryingPerMinute = 0.006F;

    /// @brief §37.4's dimensionless drying driver for the current weather.
    ///
    /// Temperature and humidity supply the evaporative potential, wind can at most double it,
    /// and shelter retains one quarter of the exposed rate. `sheltered` describes the sampled
    /// surface, not a second weather state.
    [[nodiscard]] float SurfaceDryingRate(const WeatherState& state, bool sheltered) noexcept;

    /// @brief Integrates `surfaceWetness` for a constant weather sample over simulated minutes.
    ///
    /// The result is clamped to [0, 1], and every other weather field is preserved exactly.
    [[nodiscard]] util::Result<WeatherState>
    IntegrateSurfaceWetness(const WeatherState& state, float simulatedMinutes, bool sheltered);

} // namespace cnahouse::weather
