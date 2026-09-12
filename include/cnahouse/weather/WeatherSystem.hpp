// SPDX-License-Identifier: MIT
#pragma once

#include <optional>
#include <span>

#include "cnahouse/environment/Season.hpp"
#include "cnahouse/util/Result.hpp"
#include "cnahouse/weather/PrecipitationTransition.hpp"
#include "cnahouse/weather/WeatherBlend.hpp"
#include "cnahouse/weather/WeatherSampler.hpp"

namespace cnahouse::weather
{

    /// @brief The one owner of §36's live weather vector and §42's transition state.
    ///
    /// The sampler chooses discrete targets, `WeatherBlend` owns an immutable snapshot curve and
    /// `PrecipitationTransition` applies the rate limits. This class composes those pieces once per
    /// frame and is deliberately independent of rendering and XNA.
    class WeatherSystem
    {
    public:
        /// @brief Builds a reached initial state without consuming its persisted RNG stream.
        [[nodiscard]] static util::Result<WeatherSystem>
        Create(std::span<const WeatherArchetype> archetypes,
               std::span<const WeatherTransitionRow> transitions,
               WeatherRates rates,
               WeatherState initialState,
               util::Id initialArchetype,
               float targetExpiryMinutes);

        /// @brief Advances the live state by simulated minutes.
        ///
        /// Work is divided at minute and target-expiry boundaries so a hitch cannot skip a weather
        /// decision. A paused system is an exact identity, including wetness, snow and RNG state.
        [[nodiscard]] util::Result<void> Advance(float simulatedMinutes,
                                                 const environment::SeasonPhase& season,
                                                 float baseTemperatureC,
                                                 float sunlight);

        [[nodiscard]] const WeatherState& State() const noexcept
        {
            return state_;
        }

        [[nodiscard]] util::Id TargetArchetype() const noexcept
        {
            return targetArchetype_;
        }

        [[nodiscard]] float TargetExpiryMinutes() const noexcept
        {
            return targetExpiryMinutes_;
        }

        [[nodiscard]] float TransitionRemainingMinutes() const noexcept;

        [[nodiscard]] const WeatherState& TransitionSnapshot() const noexcept
        {
            return blend_.has_value() ? blend_->Snapshot() : state_;
        }

        [[nodiscard]] bool TransitionsPaused() const noexcept
        {
            return transitionsPaused_;
        }

        void SetAutomaticTransitions(bool enabled) noexcept
        {
            automaticTransitions_ = enabled;
        }

        [[nodiscard]] bool AutomaticTransitions() const noexcept
        {
            return automaticTransitions_;
        }

        /// @brief Stable control addresses used by the console registered for the game's lifetime.
        [[nodiscard]] util::Id& TargetArchetypeControl() noexcept
        {
            return requestedArchetype_;
        }

        [[nodiscard]] bool& TransitionsPausedControl() noexcept
        {
            return transitionsPaused_;
        }

    private:
        WeatherSystem(std::span<const WeatherArchetype> archetypes,
                      std::span<const WeatherTransitionRow> transitions,
                      WeatherRates rates,
                      WeatherState initialState,
                      util::Id initialArchetype,
                      float targetExpiryMinutes);

        [[nodiscard]] util::Result<void>
        BeginTransition(util::Id next, const environment::SeasonPhase& season, float baseTemperatureC);
        [[nodiscard]] util::Result<void>
        AdvanceStep(float simulatedMinutes, float baseTemperatureC, float sunlight);

        WeatherSampler sampler_;
        PrecipitationTransition precipitation_;
        WeatherState state_;
        util::Id targetArchetype_;
        util::Id requestedArchetype_;
        float targetExpiryMinutes_ = 0.0F;
        std::optional<WeatherBlend> blend_;
        PrecipType nominalPrecipType_ = PrecipType::None;
        float targetTemperatureOffsetC_ = 0.0F;
        float transitionElapsedMinutes_ = 0.0F;
        bool targetTracksBaseTemperature_ = false;
        bool transitionsPaused_ = false;
        bool automaticTransitions_ = true;
    };

} // namespace cnahouse::weather
