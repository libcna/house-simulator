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

        /// @brief The effective normalised transition row at @p season.
        [[nodiscard]] util::Result<std::vector<WeatherChoice>>
        Distribution(util::Id current, const environment::SeasonPhase& season) const;

        /// @brief Draws one destination from `Distribution`, in authored target order.
        [[nodiscard]] util::Result<util::Id>
        SampleNext(util::Id current, const environment::SeasonPhase& season, util::Rng& rng) const;

    private:
        [[nodiscard]] const WeatherArchetype* FindArchetype(util::Id id) const noexcept;
        [[nodiscard]] const WeatherTransitionRow* FindRow(util::Id id) const noexcept;

        std::vector<WeatherArchetype> archetypes_;
        std::vector<WeatherTransitionRow> transitions_;
    };

} // namespace cnahouse::weather
