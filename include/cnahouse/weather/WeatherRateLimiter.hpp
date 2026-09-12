// SPDX-License-Identifier: MIT
#pragma once

#include "cnahouse/util/Result.hpp"
#include "cnahouse/weather/WeatherState.hpp"

namespace cnahouse::weather
{

    /// @brief §42.2's per-simulated-minute anti-absurdity guarantee.
    class WeatherRateLimiter
    {
    public:
        explicit WeatherRateLimiter(WeatherRates rates) noexcept;

        /// @brief Moves each archetype-driven channel toward @p desired without overshooting.
        ///
        /// Precipitation type, integrated surfaces and RNG state remain owned by their dedicated
        /// systems. Wind direction takes the shortest wrapped path.
        [[nodiscard]] util::Result<WeatherState>
        Advance(const WeatherState& current, const WeatherState& desired, float simulatedMinutes) const;

        [[nodiscard]] const WeatherRates& Rates() const noexcept
        {
            return rates_;
        }

    private:
        WeatherRates rates_;
    };

} // namespace cnahouse::weather
