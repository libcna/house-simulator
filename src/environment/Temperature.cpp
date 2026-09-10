// SPDX-License-Identifier: MIT
#include "cnahouse/environment/Temperature.hpp"

#include <cmath>

namespace cnahouse::environment
{
    namespace
    {
        constexpr double kTwoPi = 6.283185307179586476925286766559;
    } // namespace

    double BaseTemperatureC(double dayOfYear, double hourOfDay) noexcept
    {
        if (!std::isfinite(dayOfYear) || !std::isfinite(hourOfDay))
        {
            return kAnnualMeanC;
        }
        const double annual = std::cos(kTwoPi * (dayOfYear - kWarmestDayOfYear) / kAnnualPeriodDays);
        const double diurnal = std::cos(kTwoPi * (hourOfDay - kWarmestHourOfDay) / 24.0);
        return kAnnualMeanC + kAnnualAmplitudeC * annual + kDiurnalAmplitudeC * diurnal;
    }

} // namespace cnahouse::environment
