// SPDX-License-Identifier: MIT
#include "cnahouse/weather/PrecipitationTransition.hpp"

#include <algorithm>
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

        return AdvanceTowardPhase(current, desired, desired.precipType, simulatedMinutes);
    }

    util::Result<WeatherState> PrecipitationTransition::AdvanceDerived(const WeatherState& current,
                                                                       const WeatherState& desired,
                                                                       PrecipType nominalType,
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

        const util::Result<WeatherState> preview = limiter_.Advance(current, desired, simulatedMinutes);
        if (!preview)
        {
            return preview.Error().WithContext("weather/precipitation");
        }
        const util::Result<PrecipType> targetPhase =
            PrecipTypeAtTemperature(nominalType, preview.Value().temperatureC);
        if (!targetPhase)
        {
            return targetPhase.Error().WithContext("weather/precipitation");
        }
        return AdvanceTowardPhase(current, desired, targetPhase.Value(), simulatedMinutes);
    }

    util::Result<WeatherState> PrecipitationTransition::AdvanceTowardPhase(const WeatherState& current,
                                                                           const WeatherState& desired,
                                                                           PrecipType targetPhase,
                                                                           float simulatedMinutes) const
    {
        const bool phaseChange = current.precipType != targetPhase;
        WeatherState protectedDesired = desired;
        if (phaseChange || targetPhase == PrecipType::None)
        {
            protectedDesired.precipIntensity =
                std::min(protectedDesired.precipIntensity, kPrecipTypeChangeIntensity);
        }
        if (current.precipType == PrecipType::Hail && targetPhase != PrecipType::Hail)
        {
            protectedDesired.thunderIntensity = std::max(protectedDesired.thunderIntensity,
                                                         std::nextafter(kMinimumHailThunderIntensity, 1.0F));
        }

        util::Result<WeatherState> advanced = limiter_.Advance(current, protectedDesired, simulatedMinutes);
        if (!advanced)
        {
            return advanced.Error().WithContext("weather/precipitation");
        }
        if (phaseChange && advanced.Value().precipIntensity <= kPrecipTypeChangeIntensity &&
            (targetPhase != PrecipType::Hail ||
             advanced.Value().thunderIntensity > kMinimumHailThunderIntensity))
        {
            advanced.Value().precipType = targetPhase;
        }
        return advanced;
    }

} // namespace cnahouse::weather
