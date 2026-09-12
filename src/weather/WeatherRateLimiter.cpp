// SPDX-License-Identifier: MIT
#include "cnahouse/weather/WeatherRateLimiter.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>

namespace cnahouse::weather
{
    namespace
    {
        [[nodiscard]] float MoveTowards(float current, float desired, float maximumDelta) noexcept
        {
            const float delta = desired - current;
            if (std::abs(delta) <= maximumDelta)
            {
                return desired;
            }
            return current + std::clamp(delta, -maximumDelta, maximumDelta);
        }

        [[nodiscard]] float WrapDegrees(float degrees) noexcept
        {
            const float wrapped = std::fmod(degrees, 360.0F);
            return wrapped < 0.0F ? wrapped + 360.0F : wrapped;
        }

        [[nodiscard]] float MoveDirection(float current, float desired, float maximumDelta) noexcept
        {
            const float wrappedCurrent = WrapDegrees(current);
            float delta = WrapDegrees(desired) - wrappedCurrent;
            if (delta > 180.0F)
            {
                delta -= 360.0F;
            }
            else if (delta < -180.0F)
            {
                delta += 360.0F;
            }
            const float step = std::clamp(delta, -maximumDelta, maximumDelta);
            float result = WrapDegrees(wrappedCurrent + step);
            const auto distance = [wrappedCurrent](float value)
            {
                float difference = std::fmod(std::abs(value - wrappedCurrent), 360.0F);
                return difference > 180.0F ? 360.0F - difference : difference;
            };
            // Adding a 4.5-degree float step to a value near 360 can round the observable
            // difference a few ULP above 4.5. Walk the result inward until the represented state
            // itself honours the limit, not merely the pre-addition delta.
            while (distance(result) > maximumDelta)
            {
                result = std::nextafter(result,
                                        step >= 0.0F ? -std::numeric_limits<float>::infinity()
                                                     : std::numeric_limits<float>::infinity());
            }
            return result;
        }

        [[nodiscard]] bool Finite(const WeatherState& state) noexcept
        {
            return std::isfinite(state.cloudCover) && std::isfinite(state.cloudCumuliform) &&
                   std::isfinite(state.precipIntensity) && std::isfinite(state.windSpeed) &&
                   std::isfinite(state.windDirectionDeg) && std::isfinite(state.gustFactor) &&
                   std::isfinite(state.fogDensity) && std::isfinite(state.thunderIntensity) &&
                   std::isfinite(state.temperatureC) && std::isfinite(state.humidity);
        }

        [[nodiscard]] bool Valid(const WeatherRates& rates) noexcept
        {
            const std::array values{rates.cloudCover,
                                    rates.cloudCumuliform,
                                    rates.precipIntensity,
                                    rates.windSpeed,
                                    rates.windDirectionDeg,
                                    rates.gustFactor,
                                    rates.fogDensity,
                                    rates.thunderIntensity,
                                    rates.temperatureC,
                                    rates.humidity};
            return std::ranges::all_of(values,
                                       [](float value) { return std::isfinite(value) && value > 0.0F; });
        }
    } // namespace

    WeatherRateLimiter::WeatherRateLimiter(WeatherRates rates) noexcept
        : rates_(rates)
    {
    }

    util::Result<WeatherState> WeatherRateLimiter::Advance(const WeatherState& current,
                                                           const WeatherState& desired,
                                                           float simulatedMinutes) const
    {
        if (!std::isfinite(simulatedMinutes) || simulatedMinutes < 0.0F)
        {
            return util::Err(util::ErrorCode::OutOfRange,
                             "weather elapsed time must be finite and non-negative",
                             "weather/rate-limiter");
        }
        if (!Finite(current) || !Finite(desired) || !Valid(rates_))
        {
            return util::Err(util::ErrorCode::InvalidData,
                             "weather state and rate channels must be finite and rates positive",
                             "weather/rate-limiter");
        }

        WeatherState next = current;
        next.cloudCover =
            MoveTowards(current.cloudCover, desired.cloudCover, rates_.cloudCover * simulatedMinutes);
        next.cloudCumuliform = MoveTowards(
            current.cloudCumuliform, desired.cloudCumuliform, rates_.cloudCumuliform * simulatedMinutes);
        next.precipIntensity = MoveTowards(
            current.precipIntensity, desired.precipIntensity, rates_.precipIntensity * simulatedMinutes);
        next.windSpeed =
            MoveTowards(current.windSpeed, desired.windSpeed, rates_.windSpeed * simulatedMinutes);
        next.windDirectionDeg = MoveDirection(
            current.windDirectionDeg, desired.windDirectionDeg, rates_.windDirectionDeg * simulatedMinutes);
        next.gustFactor =
            MoveTowards(current.gustFactor, desired.gustFactor, rates_.gustFactor * simulatedMinutes);
        next.fogDensity =
            MoveTowards(current.fogDensity, desired.fogDensity, rates_.fogDensity * simulatedMinutes);
        next.thunderIntensity = MoveTowards(
            current.thunderIntensity, desired.thunderIntensity, rates_.thunderIntensity * simulatedMinutes);
        next.temperatureC =
            MoveTowards(current.temperatureC, desired.temperatureC, rates_.temperatureC * simulatedMinutes);
        next.humidity = MoveTowards(current.humidity, desired.humidity, rates_.humidity * simulatedMinutes);
        return next;
    }

} // namespace cnahouse::weather
