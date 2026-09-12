// SPDX-License-Identifier: MIT
#include "cnahouse/weather/PrecipitationTransition.hpp"

#include <cmath>

namespace cnahouse::weather
{
    PrecipitationTransition::PrecipitationTransition(WeatherRates rates) noexcept
        : limiter_(rates)
    {
    }

    util::Result<WeatherState> PrecipitationTransition::Advance(const WeatherState& current,
                                                                const WeatherState& desired,
                                                                float simulatedMinutes) const
    {
        if (!std::isfinite(simulatedMinutes) || simulatedMinutes < 0.0F)
        {
            return util::Err(util::ErrorCode::OutOfRange,
                             "precipitation elapsed time must be finite and non-negative",
                             "weather/precipitation/simulatedMinutes");
        }
        if (const util::Result<void> valid = current.Validate(); !valid)
        {
            return valid.Error().WithContext("weather/precipitation/current");
        }
        if (const util::Result<void> valid = desired.Validate(); !valid)
        {
            return valid.Error().WithContext("weather/precipitation/desired");
        }
        if (simulatedMinutes == 0.0F)
        {
            return current;
        }

        const bool phaseChange = current.precipType != desired.precipType;
        WeatherState protectedDesired = desired;
        if (phaseChange && current.precipIntensity > kPrecipTypeChangeIntensity)
        {
            protectedDesired.precipIntensity = kPrecipTypeChangeIntensity;
        }

        util::Result<WeatherState> advanced = limiter_.Advance(current, protectedDesired, simulatedMinutes);
        if (!advanced)
        {
            return advanced.Error().WithContext("weather/precipitation");
        }
        if (phaseChange && advanced.Value().precipIntensity <= kPrecipTypeChangeIntensity)
        {
            advanced.Value().precipType = desired.precipType;
        }
        return advanced;
    }

} // namespace cnahouse::weather
