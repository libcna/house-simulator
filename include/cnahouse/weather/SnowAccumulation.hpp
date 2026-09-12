// SPDX-License-Identifier: MIT
#pragma once

#include "cnahouse/util/Result.hpp"
#include "cnahouse/weather/WeatherState.hpp"

namespace cnahouse::weather
{

    inline constexpr float kSnowAccumulationMetresPerMinute = 0.0009F;
    inline constexpr float kSnowMeltMetresPerDegreeMinute = 0.00006F;
    inline constexpr float kMaximumSnowDepthMetres = 0.35F;

    /// @brief §38's melt rate in metres per simulated minute.
    ///
    /// Above-freezing temperature supplies the heat and cloud-attenuated direct sunlight in
    /// `[0, 1]` can double it. At or below freezing the rate is exactly zero.
    [[nodiscard]] float SnowMeltRate(float temperatureC, float sunlight) noexcept;

    /// @brief Integrates §38's persistent snow depth for one constant weather sample.
    ///
    /// Only `PrecipType::Snow` accumulates. Rain, sleet and hail can still melt existing cover but
    /// cannot create it. Every field except `snowDepth` is preserved exactly.
    [[nodiscard]] util::Result<WeatherState>
    IntegrateSnowDepth(const WeatherState& state, float sunlight, float simulatedMinutes);

} // namespace cnahouse::weather
