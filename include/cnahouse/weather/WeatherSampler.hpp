// SPDX-License-Identifier: MIT
#pragma once

#include <span>
#include <vector>

#include "cnahouse/environment/Season.hpp"
#include "cnahouse/util/Result.hpp"
#include "cnahouse/util/Rng.hpp"
#include "cnahouse/weather/WeatherState.hpp"

namespace cnahouse::weather
{

    /// @brief One normalised entry in the effective seasonal transition row.
    struct WeatherChoice
    {
        util::Id archetype;
        float probability = 0.0F;

        [[nodiscard]] friend bool operator==(const WeatherChoice&, const WeatherChoice&) = default;
    };

    struct WeatherTiming
    {
        float dwellMinutes = 0.0F;
        float transitionMinutes = 0.0F;

        [[nodiscard]] friend bool operator==(const WeatherTiming&, const WeatherTiming&) = default;
    };

    /// @brief The independently sampled continuous wind part of an archetype target.
    struct WeatherWindTarget
    {
        float speed = 0.0F;
        float gustFactor = 0.0F;
        float modifierAmount = 0.0F;

        [[nodiscard]] friend bool operator==(const WeatherWindTarget&, const WeatherWindTarget&) = default;
    };

    /// @brief One fully sampled archetype target plus the values needed to follow a moving base
    /// temperature without drawing again.
    struct WeatherTarget
    {
        WeatherState state;
        PrecipType nominalPrecipType = PrecipType::None;
        float temperatureOffsetC = 0.0F;
    };

    /// @brief §36.3's deterministic, continuously season-weighted archetype selector.
    ///
    /// The authored file stores one base transition row per state and four sparse seasonal weight
    /// vectors. A missing seasonal entry means 1. `Distribution` multiplies the base probability
    /// by the destination's blended seasonal weight and normalises the row; `SampleNext` consumes
    /// exactly one draw from the caller-owned weather RNG.
    class WeatherSampler
    {
    public:
        WeatherSampler(std::span<const WeatherArchetype> archetypes,
                       std::span<const WeatherTransitionRow> transitions);

        /// @brief Fixed-weather selector entries in authored order.
        ///
        /// Modifier rows such as `W_WINDY` are deliberately absent: they do not define a complete
        /// weather target and therefore cannot stand alone in §68's selector.
        [[nodiscard]] std::vector<util::Id> StateArchetypes() const;

        /// @brief The effective normalised transition row at @p season.
        ///
        /// @p outdoorTemperatureC is the measured `HOUSE-01545` value. Frozen-water targets have zero
        /// probability at and above freezing, while thunderstorm weight rises continuously with
        /// heat; neither rule inspects a month or a discrete season.
        [[nodiscard]] util::Result<std::vector<WeatherChoice>> Distribution(
            util::Id current, const environment::SeasonPhase& season, float outdoorTemperatureC) const;

        /// @brief Draws one destination from `Distribution`, in authored target order.
        [[nodiscard]] util::Result<util::Id> SampleNext(util::Id current,
                                                        const environment::SeasonPhase& season,
                                                        float outdoorTemperatureC,
                                                        util::Rng& rng) const;

        /// @brief Samples from and immediately persists the RNG owned by @p state.
        ///
        /// This is the runtime/save path. The explicit-`Rng` overload remains the low-level path
        /// for tests and tools. A failed decision leaves the stored stream untouched.
        [[nodiscard]] util::Result<util::Id> SampleNext(util::Id current,
                                                        const environment::SeasonPhase& season,
                                                        float outdoorTemperatureC,
                                                        WeatherState& state) const;

        /// @brief Draws the destination dwell and the source-to-destination transition duration.
        ///
        /// Both are uniform authored distributions. The dwell range is scaled by the continuously
        /// blended season; the transition range is the component-wise mean of the two
        /// archetypes' characteristic ranges. Exactly two RNG draws are consumed on success.
        [[nodiscard]] util::Result<WeatherTiming> SampleTiming(util::Id current,
                                                               util::Id next,
                                                               const environment::SeasonPhase& season,
                                                               util::Rng& rng) const;

        /// @brief Samples timing from and persists @p state's weather RNG.
        [[nodiscard]] util::Result<WeatherTiming> SampleTiming(util::Id current,
                                                               util::Id next,
                                                               const environment::SeasonPhase& season,
                                                               WeatherState& state) const;

        /// @brief Draws wind separately from the precipitation archetype's other channels.
        ///
        /// The caller owns `W_WINDY`'s independent continuous amount: neither the architecture nor
        /// authored data defines an activation distribution. A non-zero amount uses two further
        /// draws and blends toward a boost that can never reduce the base speed or gust. Success
        /// consumes exactly two RNG draws at zero and four above zero.
        [[nodiscard]] util::Result<WeatherWindTarget>
        SampleWind(util::Id archetype, float windyModifierAmount, util::Rng& rng) const;

        /// @brief Samples wind from and persists @p state's weather RNG.
        [[nodiscard]] util::Result<WeatherWindTarget>
        SampleWind(util::Id archetype, float windyModifierAmount, WeatherState& state) const;

        /// @brief Samples every continuous channel of one archetype target atomically.
        ///
        /// The caller's stored RNG advances only if every authored band and the derived
        /// temperature are valid. Persistent wetness and snow depth are copied, never sampled.
        [[nodiscard]] util::Result<WeatherTarget> SampleTarget(util::Id archetype,
                                                               float baseTemperatureC,
                                                               float windyModifierAmount,
                                                               WeatherState& state) const;

    private:
        [[nodiscard]] const WeatherArchetype* FindArchetype(util::Id id) const noexcept;
        [[nodiscard]] const WeatherTransitionRow* FindRow(util::Id id) const noexcept;

        std::vector<WeatherArchetype> archetypes_;
        std::vector<WeatherTransitionRow> transitions_;
    };

} // namespace cnahouse::weather
