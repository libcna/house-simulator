// SPDX-License-Identifier: MIT
#include "cnahouse/weather/WeatherSystem.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <format>

#include "cnahouse/weather/SnowAccumulation.hpp"
#include "cnahouse/weather/SurfaceWetness.hpp"

namespace cnahouse::weather
{
    namespace
    {
        [[nodiscard]] bool ValidRates(const WeatherRates& rates) noexcept
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

        [[nodiscard]] bool HasStateArchetype(std::span<const WeatherArchetype> archetypes,
                                             util::Id id) noexcept
        {
            return std::ranges::any_of(archetypes,
                                       [id](const WeatherArchetype& archetype)
                                       { return archetype.id == id && !archetype.modifier; });
        }
    } // namespace

    WeatherSystem::WeatherSystem(std::span<const WeatherArchetype> archetypes,
                                 std::span<const WeatherTransitionRow> transitions,
                                 WeatherRates rates,
                                 WeatherState initialState,
                                 util::Id initialArchetype,
                                 float targetExpiryMinutes)
        : sampler_(archetypes, transitions)
        , precipitation_(rates)
        , state_(initialState)
        , targetArchetype_(initialArchetype)
        , requestedArchetype_(initialArchetype)
        , targetExpiryMinutes_(targetExpiryMinutes)
        , nominalPrecipType_(initialState.precipType)
    {
    }

    util::Result<WeatherSystem> WeatherSystem::Create(std::span<const WeatherArchetype> archetypes,
                                                      std::span<const WeatherTransitionRow> transitions,
                                                      WeatherRates rates,
                                                      WeatherState initialState,
                                                      util::Id initialArchetype,
                                                      float targetExpiryMinutes)
    {
        if (const util::Result<void> valid = initialState.Validate(); !valid)
        {
            return valid.Error().WithContext("weather/system/initialState");
        }
        if (!HasStateArchetype(archetypes, initialArchetype))
        {
            return util::Err(util::ErrorCode::NotFound,
                             "the initial weather target is not a complete archetype",
                             std::format("weather/system/{}", initialArchetype.Value()));
        }
        if (!std::isfinite(targetExpiryMinutes) || targetExpiryMinutes < 0.0F || targetExpiryMinutes > 380.0F)
        {
            return util::Err(util::ErrorCode::OutOfRange,
                             "weather target expiry must be in 0..380 simulated minutes",
                             "weather/system/targetExpiryMinutes");
        }
        if (!ValidRates(rates))
        {
            return util::Err(util::ErrorCode::InvalidData,
                             "every weather rate must be finite and positive",
                             "weather/system/rates");
        }
        return WeatherSystem(
            archetypes, transitions, rates, initialState, initialArchetype, targetExpiryMinutes);
    }

    float WeatherSystem::TransitionRemainingMinutes() const noexcept
    {
        return blend_.has_value() ? std::max(0.0F, blend_->DurationMinutes() - transitionElapsedMinutes_)
                                  : 0.0F;
    }

    util::Result<void> WeatherSystem::BeginTransition(util::Id next,
                                                      const environment::SeasonPhase& season,
                                                      float baseTemperatureC)
    {
        // All draws happen against a copy. A malformed target cannot partially advance the one
        // persisted weather stream.
        WeatherState sampledState = state_;
        const util::Result<WeatherTiming> timing =
            sampler_.SampleTiming(targetArchetype_, next, season, sampledState);
        if (!timing)
        {
            return timing.Error().WithContext("weather/system/transition");
        }
        const util::Result<WeatherTarget> target =
            sampler_.SampleTarget(next, baseTemperatureC, 0.0F, sampledState);
        if (!target)
        {
            return target.Error().WithContext("weather/system/transition");
        }

        state_.rngState = sampledState.rngState;
        WeatherState snapshot = state_;
        WeatherState targetState = target.Value().state;
        targetState.surfaceWetness = state_.surfaceWetness;
        targetState.snowDepth = state_.snowDepth;
        targetState.rngState = state_.rngState;
        blend_.emplace(snapshot, targetState, timing.Value().transitionMinutes);
        nominalPrecipType_ = target.Value().nominalPrecipType;
        targetTemperatureOffsetC_ = target.Value().temperatureOffsetC;
        transitionElapsedMinutes_ = 0.0F;
        targetTracksBaseTemperature_ = true;
        targetArchetype_ = next;
        requestedArchetype_ = next;
        targetExpiryMinutes_ = timing.Value().dwellMinutes;
        return util::Ok();
    }

    util::Result<void>
    WeatherSystem::AdvanceStep(float simulatedMinutes, float baseTemperatureC, float sunlight)
    {
        WeatherState desired = state_;
        if (blend_.has_value())
        {
            WeatherState movingTarget = blend_->Target();
            if (targetTracksBaseTemperature_)
            {
                movingTarget.temperatureC = baseTemperatureC + targetTemperatureOffsetC_;
                const util::Result<PrecipType> phase =
                    PrecipTypeAtTemperature(nominalPrecipType_, movingTarget.temperatureC);
                if (!phase)
                {
                    return phase.Error().WithContext("weather/system/target");
                }
                movingTarget.precipType = phase.Value();
            }
            const WeatherBlend currentBlend(blend_->Snapshot(), movingTarget, blend_->DurationMinutes());
            const util::Result<WeatherState> blended =
                currentBlend.DesiredAt(transitionElapsedMinutes_ + simulatedMinutes);
            if (!blended)
            {
                return blended.Error().WithContext("weather/system");
            }
            desired = blended.Value();
            desired.precipType = movingTarget.precipType;
        }

        const util::Result<WeatherState> advanced = precipitation_.Advance(state_, desired, simulatedMinutes);
        if (!advanced)
        {
            return advanced.Error().WithContext("weather/system");
        }
        const util::Result<WeatherState> wet =
            IntegrateSurfaceWetness(advanced.Value(), simulatedMinutes, false);
        if (!wet)
        {
            return wet.Error().WithContext("weather/system");
        }
        const util::Result<WeatherState> snow = IntegrateSnowDepth(wet.Value(), sunlight, simulatedMinutes);
        if (!snow)
        {
            return snow.Error().WithContext("weather/system");
        }
        state_ = snow.Value();
        if (blend_.has_value())
        {
            transitionElapsedMinutes_ =
                std::min(blend_->DurationMinutes(), transitionElapsedMinutes_ + simulatedMinutes);
        }
        return util::Ok();
    }

    util::Result<void> WeatherSystem::Advance(float simulatedMinutes,
                                              const environment::SeasonPhase& season,
                                              float baseTemperatureC,
                                              float sunlight)
    {
        if (!std::isfinite(simulatedMinutes) || simulatedMinutes < 0.0F)
        {
            return util::Err(util::ErrorCode::OutOfRange,
                             "weather elapsed time must be finite and non-negative",
                             "weather/system/simulatedMinutes");
        }
        if (!std::isfinite(baseTemperatureC) || baseTemperatureC < -18.0F || baseTemperatureC > 38.0F)
        {
            return util::Err(util::ErrorCode::OutOfRange,
                             "weather base temperature must be in -18..38 C",
                             "weather/system/baseTemperatureC");
        }
        if (!std::isfinite(sunlight) || sunlight < 0.0F || sunlight > 1.0F)
        {
            return util::Err(
                util::ErrorCode::OutOfRange, "weather sunlight must be in [0, 1]", "weather/system/sunlight");
        }
        if (const util::Result<void> valid = state_.Validate(); !valid)
        {
            return valid.Error().WithContext("weather/system/state");
        }
        if (simulatedMinutes == 0.0F || transitionsPaused_)
        {
            return util::Ok();
        }

        float remaining = simulatedMinutes;
        while (remaining > 0.0F)
        {
            if (requestedArchetype_ != targetArchetype_)
            {
                if (const util::Result<void> begun =
                        BeginTransition(requestedArchetype_, season, baseTemperatureC);
                    !begun)
                {
                    return begun.Error();
                }
            }
            else if (automaticTransitions_ && targetExpiryMinutes_ <= 0.0F)
            {
                WeatherState sampledState = state_;
                const util::Result<util::Id> next =
                    sampler_.SampleNext(targetArchetype_, season, baseTemperatureC, sampledState);
                if (!next)
                {
                    return next.Error().WithContext("weather/system");
                }
                // `BeginTransition` is transactional on its own; commit SampleNext's single draw
                // only through the same temporary stream used for all subsequent target draws.
                const auto originalRng = state_.rngState;
                state_.rngState = sampledState.rngState;
                if (const util::Result<void> begun = BeginTransition(next.Value(), season, baseTemperatureC);
                    !begun)
                {
                    state_.rngState = originalRng;
                    return begun.Error();
                }
            }

            float step = std::min(remaining, 1.0F);
            if (automaticTransitions_ && targetExpiryMinutes_ > 0.0F)
            {
                step = std::min(step, targetExpiryMinutes_);
            }
            if (!(step > 0.0F))
            {
                return util::Err(
                    util::ErrorCode::InvalidData, "weather advance made no progress", "weather/system");
            }
            if (const util::Result<void> advanced = AdvanceStep(step, baseTemperatureC, sunlight); !advanced)
            {
                return advanced.Error();
            }
            targetExpiryMinutes_ = std::max(0.0F, targetExpiryMinutes_ - step);
            remaining -= step;
        }
        return util::Ok();
    }

} // namespace cnahouse::weather
