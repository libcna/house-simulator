// SPDX-License-Identifier: MIT
#include "cnahouse/weather/SurfaceWetness.hpp"

#include <algorithm>
#include <cmath>

namespace cnahouse::weather
{
    namespace
    {
        constexpr float kMinimumTemperatureC = -18.0F;
        constexpr float kTemperatureRangeC = 56.0F;
        constexpr float kMaximumWindSpeed = 30.0F;
        constexpr float kShelteredDryingScale = 0.25F;
    } // namespace

    float SurfaceDryingRate(const WeatherState& state, bool sheltered) noexcept
    {
        const float temperatureFactor =
            std::clamp((state.temperatureC - kMinimumTemperatureC) / kTemperatureRangeC, 0.0F, 1.0F);
        const float humidityFactor = std::clamp(1.0F - state.humidity, 0.0F, 1.0F);
        const float windFactor = 1.0F + std::clamp(state.windSpeed / kMaximumWindSpeed, 0.0F, 1.0F);
        const float shelterFactor = sheltered ? kShelteredDryingScale : 1.0F;
        return temperatureFactor * humidityFactor * windFactor * shelterFactor;
    }

    util::Result<WeatherState>
    IntegrateSurfaceWetness(const WeatherState& state, float simulatedMinutes, bool sheltered)
    {
        if (!std::isfinite(simulatedMinutes) || simulatedMinutes < 0.0F)
        {
            return util::Err(util::ErrorCode::OutOfRange,
                             "wetness elapsed time must be finite and non-negative",
                             "weather/wetness/simulatedMinutes");
        }
        if (const util::Result<void> valid = state.Validate(); !valid)
        {
            return valid.Error().WithContext("weather/wetness/state");
        }
        if (simulatedMinutes == 0.0F)
        {
            return state;
        }

        const float rate = kWetnessAccumulationPerMinute * state.precipIntensity -
                           kWetnessDryingPerMinute * SurfaceDryingRate(state, sheltered);
        WeatherState next = state;
        next.surfaceWetness = std::clamp(state.surfaceWetness + rate * simulatedMinutes, 0.0F, 1.0F);
        return next;
    }

} // namespace cnahouse::weather
