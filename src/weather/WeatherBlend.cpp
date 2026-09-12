// SPDX-License-Identifier: MIT
#include "cnahouse/weather/WeatherBlend.hpp"

#include <algorithm>
#include <cmath>

namespace cnahouse::weather
{
    namespace
    {
        [[nodiscard]] float Smoothstep(float value) noexcept
        {
            const float t = std::clamp(value, 0.0F, 1.0F);
            return t * t * (3.0F - 2.0F * t);
        }

        [[nodiscard]] float Lerp(float start, float target, float amount) noexcept
        {
            return start + (target - start) * amount;
        }

        [[nodiscard]] float WrapDegrees(float degrees) noexcept
        {
            const float wrapped = std::fmod(degrees, 360.0F);
            return wrapped < 0.0F ? wrapped + 360.0F : wrapped;
        }

        [[nodiscard]] float LerpDirection(float start, float target, float amount) noexcept
        {
            const float wrappedStart = WrapDegrees(start);
            float delta = WrapDegrees(target) - wrappedStart;
            if (delta > 180.0F)
            {
                delta -= 360.0F;
            }
            else if (delta < -180.0F)
            {
                delta += 360.0F;
            }
            return WrapDegrees(wrappedStart + delta * amount);
        }
    } // namespace

    WeatherBlend::WeatherBlend(WeatherState snapshot, WeatherState target, float durationMinutes) noexcept
        : snapshot_(snapshot)
        , target_(target)
        , durationMinutes_(durationMinutes)
    {
    }

    util::Result<WeatherState> WeatherBlend::DesiredAt(float elapsedMinutes) const
    {
        if (!std::isfinite(durationMinutes_) || !(durationMinutes_ > 0.0F))
        {
            return util::Err(util::ErrorCode::OutOfRange,
                             "weather blend duration must be finite and positive",
                             "weather/blend/durationMinutes");
        }
        if (!std::isfinite(elapsedMinutes) || elapsedMinutes < 0.0F)
        {
            return util::Err(util::ErrorCode::OutOfRange,
                             "weather blend elapsed time must be finite and non-negative",
                             "weather/blend/elapsedMinutes");
        }
        if (const util::Result<void> valid = snapshot_.Validate(); !valid)
        {
            return valid.Error().WithContext("weather/blend/snapshot");
        }
        if (const util::Result<void> valid = target_.Validate(); !valid)
        {
            return valid.Error().WithContext("weather/blend/target");
        }

        const float amount = Smoothstep(elapsedMinutes / durationMinutes_);
        WeatherState desired = snapshot_;
        desired.cloudCover = Lerp(snapshot_.cloudCover, target_.cloudCover, amount);
        desired.cloudCumuliform = Lerp(snapshot_.cloudCumuliform, target_.cloudCumuliform, amount);
        desired.precipIntensity = Lerp(snapshot_.precipIntensity, target_.precipIntensity, amount);
        desired.windSpeed = Lerp(snapshot_.windSpeed, target_.windSpeed, amount);
        desired.windDirectionDeg =
            LerpDirection(snapshot_.windDirectionDeg, target_.windDirectionDeg, amount);
        desired.gustFactor = Lerp(snapshot_.gustFactor, target_.gustFactor, amount);
        desired.fogDensity = Lerp(snapshot_.fogDensity, target_.fogDensity, amount);
        desired.thunderIntensity = Lerp(snapshot_.thunderIntensity, target_.thunderIntensity, amount);
        desired.temperatureC = Lerp(snapshot_.temperatureC, target_.temperatureC, amount);
        desired.humidity = Lerp(snapshot_.humidity, target_.humidity, amount);
        return desired;
    }

} // namespace cnahouse::weather
