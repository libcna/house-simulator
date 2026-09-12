// SPDX-License-Identifier: MIT
#include "cnahouse/weather/SnowAccumulation.hpp"

#include <algorithm>
#include <cmath>

namespace cnahouse::weather
{
    float SnowMeltRate(float temperatureC, float sunlight) noexcept
    {
        const float degreesAboveFreezing = std::max(temperatureC, 0.0F);
        return kSnowMeltMetresPerDegreeMinute * degreesAboveFreezing * (1.0F + sunlight);
    }

    util::Result<WeatherState>
    IntegrateSnowDepth(const WeatherState& state, float sunlight, float simulatedMinutes)
    {
        if (!std::isfinite(sunlight) || sunlight < 0.0F || sunlight > 1.0F)
        {
            return util::Err(util::ErrorCode::OutOfRange,
                             "snow sunlight must be finite and in [0, 1]",
                             "weather/snow/sunlight");
        }
        if (!std::isfinite(simulatedMinutes) || simulatedMinutes < 0.0F)
        {
            return util::Err(util::ErrorCode::OutOfRange,
                             "snow elapsed time must be finite and non-negative",
                             "weather/snow/simulatedMinutes");
        }
        if (const util::Result<void> valid = state.Validate(); !valid)
        {
            return valid.Error().WithContext("weather/snow/state");
        }
        if (simulatedMinutes == 0.0F)
        {
            return state;
        }

        const float accumulation = state.precipType == PrecipType::Snow
                                       ? kSnowAccumulationMetresPerMinute * state.precipIntensity
                                       : 0.0F;
        const float rate = accumulation - SnowMeltRate(state.temperatureC, sunlight);
        WeatherState next = state;
        next.snowDepth = std::clamp(state.snowDepth + rate * simulatedMinutes, 0.0F, kMaximumSnowDepthMetres);
        return next;
    }

} // namespace cnahouse::weather
