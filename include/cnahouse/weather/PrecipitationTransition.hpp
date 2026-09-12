// SPDX-License-Identifier: MIT
#pragma once

#include "cnahouse/util/Result.hpp"
#include "cnahouse/weather/WeatherRateLimiter.hpp"
#include "cnahouse/weather/WeatherState.hpp"

namespace cnahouse::weather
{

    inline constexpr float kPrecipTypeChangeIntensity = 0.05F;

    /// @brief §42.3's protected precipitation-phase transition.
    ///
    /// Continuous channels still use `WeatherRateLimiter`; this layer only prevents its intensity
    /// target and discrete phase from disagreeing while rain becomes sleet or snow.
    class PrecipitationTransition
    {
    public:
        explicit PrecipitationTransition(WeatherRates rates) noexcept;

        /// @brief Advances all rate-limited channels and applies the protected phase switch.
        ///
        /// When @p desired asks for another phase above intensity 0.05, intensity first targets
        /// exactly 0.05 while the old phase remains. The first result at or below that boundary
        /// carries the new phase; later calls ramp toward the original desired intensity.
        [[nodiscard]] util::Result<WeatherState>
        Advance(const WeatherState& current, const WeatherState& desired, float simulatedMinutes) const;

    private:
        WeatherRateLimiter limiter_;
    };

} // namespace cnahouse::weather
