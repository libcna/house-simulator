// SPDX-License-Identifier: MIT
#include "cnahouse/environment/Temperature.hpp"

#include <cmath>

namespace cnahouse::environment
{
    namespace
    {
        constexpr double kTwoPi = 6.283185307179586476925286766559;

        [[nodiscard]] double AnnualTemperatureC(double dayOfYear) noexcept
        {
            const double annual = std::cos(kTwoPi * (dayOfYear - kWarmestDayOfYear) / kAnnualPeriodDays);
            return kAnnualMeanC + kAnnualAmplitudeC * annual;
        }

    } // namespace

    double BaseTemperatureC(double dayOfYear, double hourOfDay) noexcept
    {
        if (!std::isfinite(dayOfYear) || !std::isfinite(hourOfDay))
        {
            return kAnnualMeanC;
        }
        return AnnualTemperatureC(dayOfYear) + DiurnalTemperatureDeltaC(hourOfDay);
    }

    double DiurnalTemperatureDeltaC(double hourOfDay) noexcept
    {
        if (!std::isfinite(hourOfDay))
        {
            return 0.0;
        }
        return kDiurnalAmplitudeC * std::cos(kTwoPi * (hourOfDay - kWarmestHourOfDay) / 24.0);
    }

    double OutdoorTemperatureC(double dayOfYear, double hourOfDay, double weatherDeltaC) noexcept
    {
        const double finiteWeatherDelta = std::isfinite(weatherDeltaC) ? weatherDeltaC : 0.0;
        return BaseTemperatureC(dayOfYear, hourOfDay) + finiteWeatherDelta;
    }

} // namespace cnahouse::environment
