// SPDX-License-Identifier: MIT
#pragma once

#include "cnahouse/util/Result.hpp"
#include "cnahouse/weather/WeatherState.hpp"

namespace cnahouse::weather
{

    /// @brief The immutable snapshot-to-target transition described by §42.1.
    ///
    /// This produces the desired continuous weather vector. `WeatherRateLimiter` then moves live
    /// state toward that desired vector, so a short authored transition cannot bypass §42.2.
    class WeatherBlend
    {
    public:
        WeatherBlend(WeatherState snapshot, WeatherState target, float durationMinutes) noexcept;

        /// @brief Evaluates the clamped smoothstep curve after @p elapsedMinutes.
        ///
        /// Only the ten archetype-driven continuous channels are blended. Precipitation phase,
        /// integrated surfaces and RNG state remain the snapshot's responsibility until their
        /// dedicated systems apply them. Wind direction follows the shortest wrapped arc.
        [[nodiscard]] util::Result<WeatherState> DesiredAt(float elapsedMinutes) const;

        [[nodiscard]] const WeatherState& Snapshot() const noexcept
        {
            return snapshot_;
        }

        [[nodiscard]] const WeatherState& Target() const noexcept
        {
            return target_;
        }

        [[nodiscard]] float DurationMinutes() const noexcept
        {
            return durationMinutes_;
        }

    private:
        WeatherState snapshot_;
        WeatherState target_;
        float durationMinutes_ = 0.0F;
    };

} // namespace cnahouse::weather
