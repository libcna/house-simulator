// SPDX-License-Identifier: MIT
#include "cnahouse/environment/DayLength.hpp"

#include <cmath>

#include "cnahouse/environment/SimClock.hpp"

namespace cnahouse::environment
{
    namespace
    {
        constexpr double kSimMinutesPerDay = 1440.0;
        /// Half a second of simulated day, expressed in real minutes at the default scale. Two
        /// day lengths closer together than this are the same setting as far as a person is
        /// concerned, and a preset stored as text and read back must still match itself.
        constexpr double kSameSetting = 1e-6;
    } // namespace

    double TimeScaleForDayLength(double realMinutesPerSimDay) noexcept
    {
        if (!std::isfinite(realMinutesPerSimDay) || realMinutesPerSimDay <= 0.0)
        {
            return 0.0;
        }
        return kSimMinutesPerDay / realMinutesPerSimDay;
    }

    double DayLengthForTimeScale(double timeScale) noexcept
    {
        if (!std::isfinite(timeScale) || timeScale <= 0.0)
        {
            return 0.0;
        }
        return kSimMinutesPerDay / timeScale;
    }

    std::size_t DayLengthPresetIndex(double realMinutesPerSimDay) noexcept
    {
        for (std::size_t index = 0; index < kDayLengthPresetCount; ++index)
        {
            if (std::abs(kDayLengthPresetsRealMinutes[index] - realMinutesPerSimDay) < kSameSetting)
            {
                return index;
            }
        }
        return kDayLengthPresetCount;
    }

} // namespace cnahouse::environment
