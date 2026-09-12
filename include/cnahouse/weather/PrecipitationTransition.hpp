// SPDX-License-Identifier: MIT
#pragma once

#include "cnahouse/util/Result.hpp"
#include "cnahouse/weather/WeatherRateLimiter.hpp"
#include "cnahouse/weather/WeatherState.hpp"

namespace cnahouse::weather
{

    inline constexpr float kPrecipTypeChangeIntensity = 0.05F;
    inline constexpr float kMinimumHailThunderIntensity = 0.3F;

    /// @brief §42.3's protected precipitation-phase transition.
    ///
    /// Continuous channels still use `WeatherRateLimiter`; this layer prevents its intensity
    /// target and discrete phase from disagreeing while rain becomes sleet or snow, and enforces
    /// §40's rule that an observable hail phase always has thunder above 0.3.
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

        /// @brief Advances while deriving rain/sleet/snow from the next rate-limited temperature.
        ///
        /// @p nominalType is the archetype's unmodified water phase. Hail and None remain
        /// themselves; the three water phases follow §36.2 after temperature has advanced, so an
        /// observable low-intensity state cannot say rain below freezing or snow above 2.5 C.
        [[nodiscard]] util::Result<WeatherState> AdvanceDerived(const WeatherState& current,
                                                                const WeatherState& desired,
                                                                PrecipType nominalType,
                                                                float simulatedMinutes) const;

    private:
        [[nodiscard]] util::Result<WeatherState> AdvanceTowardPhase(const WeatherState& current,
                                                                    const WeatherState& desired,
                                                                    PrecipType targetPhase,
                                                                    float simulatedMinutes) const;

        WeatherRateLimiter limiter_;
    };

} // namespace cnahouse::weather
