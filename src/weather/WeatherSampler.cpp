// SPDX-License-Identifier: MIT
#include "cnahouse/weather/WeatherSampler.hpp"

#include <algorithm>
#include <cmath>
#include <format>

namespace cnahouse::weather
{
    namespace
    {
        [[nodiscard]] bool ValidWindRange(const WeatherRange& range, float maximum) noexcept
        {
            return std::isfinite(range.minimum) && std::isfinite(range.maximum) && range.minimum >= 0.0F &&
                   range.minimum <= range.maximum && range.maximum <= maximum;
        }

        [[nodiscard]] float SampleRange(const WeatherRange& range, util::Rng& rng) noexcept
        {
            return range.minimum + (range.maximum - range.minimum) * rng.NextFloat();
        }
    } // namespace

    WeatherSampler::WeatherSampler(std::span<const WeatherArchetype> archetypes,
                                   std::span<const WeatherTransitionRow> transitions)
        : archetypes_(archetypes.begin(), archetypes.end())
        , transitions_(transitions.begin(), transitions.end())
    {
    }

    std::vector<util::Id> WeatherSampler::StateArchetypes() const
    {
        std::vector<util::Id> result;
        result.reserve(archetypes_.size());
        for (const WeatherArchetype& archetype : archetypes_)
        {
            if (!archetype.modifier)
            {
                result.push_back(archetype.id);
            }
        }
        return result;
    }

    const WeatherArchetype* WeatherSampler::FindArchetype(util::Id id) const noexcept
    {
        const auto found = std::ranges::find(archetypes_, id, &WeatherArchetype::id);
        return found == archetypes_.end() ? nullptr : &*found;
    }

    const WeatherTransitionRow* WeatherSampler::FindRow(util::Id id) const noexcept
    {
        const auto found = std::ranges::find(transitions_, id, &WeatherTransitionRow::source);
        return found == transitions_.end() ? nullptr : &*found;
    }

    util::Result<std::vector<WeatherChoice>>
    WeatherSampler::Distribution(util::Id current, const environment::SeasonPhase& season) const
    {
        const WeatherTransitionRow* row = FindRow(current);
        if (row == nullptr)
        {
            return util::Err(util::ErrorCode::NotFound,
                             "the current archetype has no transition row",
                             std::format("weather/{}", current.Value()));
        }

        std::vector<WeatherChoice> choices;
        choices.reserve(row->targets.size());
        double total = 0.0;
        for (const WeatherTransition& transition : row->targets)
        {
            const WeatherArchetype* target = FindArchetype(transition.target);
            if (target == nullptr || target->modifier)
            {
                return util::Err(util::ErrorCode::InvalidData,
                                 "a transition target is missing or is a modifier",
                                 std::format("weather/{}/{}", current.Value(), transition.target.Value()));
            }
            const float perSeason[4]{target->seasonalWeights[0],
                                     target->seasonalWeights[1],
                                     target->seasonalWeights[2],
                                     target->seasonalWeights[3]};
            const float seasonalWeight = environment::MixBySeason(season, perSeason);
            const float effective = transition.probability * target->weight * seasonalWeight;
            if (!std::isfinite(effective) || !(effective >= 0.0F))
            {
                return util::Err(util::ErrorCode::InvalidData,
                                 "an effective transition weight is negative or non-finite",
                                 std::format("weather/{}/{}", current.Value(), transition.target.Value()));
            }
            choices.push_back({transition.target, effective});
            total += static_cast<double>(effective);
        }
        if (!(total > 0.0))
        {
            return util::Err(util::ErrorCode::InvalidData,
                             "seasonal weighting leaves the transition row with zero probability",
                             std::format("weather/{}", current.Value()));
        }
        for (WeatherChoice& choice : choices)
        {
            choice.probability = static_cast<float>(static_cast<double>(choice.probability) / total);
        }
        return choices;
    }

    util::Result<util::Id>
    WeatherSampler::SampleNext(util::Id current, const environment::SeasonPhase& season, util::Rng& rng) const
    {
        const util::Result<std::vector<WeatherChoice>> choices = Distribution(current, season);
        if (!choices)
        {
            return choices.Error();
        }

        const float draw = rng.NextFloat();
        float cumulative = 0.0F;
        util::Id lastPositive;
        for (const WeatherChoice& choice : choices.Value())
        {
            if (choice.probability > 0.0F)
            {
                lastPositive = choice.archetype;
            }
            cumulative += choice.probability;
            if (draw < cumulative)
            {
                return choice.archetype;
            }
        }
        // Float normalisation may sum to one ULP below 1. The final positive target owns that
        // sliver; returning no state would turn a harmless rounding detail into a stuck sky.
        return lastPositive;
    }

    util::Result<util::Id> WeatherSampler::SampleNext(util::Id current,
                                                      const environment::SeasonPhase& season,
                                                      WeatherState& state) const
    {
        if (const util::Result<void> valid = state.Validate(); !valid)
        {
            return valid.Error().WithContext("weather/rng/state");
        }
        util::Rng rng(state.rngState);
        util::Result<util::Id> sampled = SampleNext(current, season, rng);
        if (sampled)
        {
            state.rngState = rng.GetState();
        }
        return sampled;
    }

    util::Result<WeatherTiming> WeatherSampler::SampleTiming(util::Id current,
                                                             util::Id next,
                                                             const environment::SeasonPhase& season,
                                                             util::Rng& rng) const
    {
        const WeatherArchetype* source = FindArchetype(current);
        const WeatherArchetype* target = FindArchetype(next);
        if (source == nullptr || source->modifier)
        {
            return util::Err(util::ErrorCode::NotFound,
                             "the current archetype has no timing distribution",
                             std::format("weather/{}", current.Value()));
        }
        if (target == nullptr || target->modifier)
        {
            return util::Err(util::ErrorCode::NotFound,
                             "the next archetype has no timing distribution",
                             std::format("weather/{}", next.Value()));
        }

        const float dwellScales[4]{target->seasonalDwellScales[0],
                                   target->seasonalDwellScales[1],
                                   target->seasonalDwellScales[2],
                                   target->seasonalDwellScales[3]};
        const float dwellScale = environment::MixBySeason(season, dwellScales);
        const float dwellMinimum = target->dwellMinutes.minimum * dwellScale;
        const float dwellMaximum = target->dwellMinutes.maximum * dwellScale;
        const float transitionMinimum =
            0.5F * (source->transitionMinutes.minimum + target->transitionMinutes.minimum);
        const float transitionMaximum =
            0.5F * (source->transitionMinutes.maximum + target->transitionMinutes.maximum);
        if (!std::isfinite(dwellMinimum) || !std::isfinite(dwellMaximum) ||
            !(dwellMinimum >= 25.0F && dwellMinimum <= dwellMaximum && dwellMaximum <= 380.0F))
        {
            return util::Err(util::ErrorCode::InvalidData,
                             "the seasonal dwell distribution is outside 25..380 simulated minutes",
                             std::format("weather/{}", next.Value()));
        }
        if (!std::isfinite(transitionMinimum) || !std::isfinite(transitionMaximum) ||
            !(transitionMinimum >= 5.0F && transitionMinimum <= transitionMaximum &&
              transitionMaximum <= 45.0F))
        {
            return util::Err(util::ErrorCode::InvalidData,
                             "the transition distribution is outside 5..45 simulated minutes",
                             std::format("weather/{}/{}", current.Value(), next.Value()));
        }

        const float dwellDraw = rng.NextFloat();
        const float transitionDraw = rng.NextFloat();
        return WeatherTiming{
            dwellMinimum + (dwellMaximum - dwellMinimum) * dwellDraw,
            transitionMinimum + (transitionMaximum - transitionMinimum) * transitionDraw,
        };
    }

    util::Result<WeatherTiming> WeatherSampler::SampleTiming(util::Id current,
                                                             util::Id next,
                                                             const environment::SeasonPhase& season,
                                                             WeatherState& state) const
    {
        if (const util::Result<void> valid = state.Validate(); !valid)
        {
            return valid.Error().WithContext("weather/rng/state");
        }
        util::Rng rng(state.rngState);
        util::Result<WeatherTiming> sampled = SampleTiming(current, next, season, rng);
        if (sampled)
        {
            state.rngState = rng.GetState();
        }
        return sampled;
    }

    util::Result<WeatherWindTarget>
    WeatherSampler::SampleWind(util::Id archetypeId, float windyModifierAmount, util::Rng& rng) const
    {
        const WeatherArchetype* archetype = FindArchetype(archetypeId);
        if (archetype == nullptr || archetype->modifier)
        {
            return util::Err(util::ErrorCode::NotFound,
                             "wind needs a weather-state archetype",
                             std::format("weather/{}", archetypeId.Value()));
        }
        const WeatherArchetype* modifier = FindArchetype(util::Id::Of("W_WINDY"));
        if (modifier == nullptr || !modifier->modifier)
        {
            return util::Err(util::ErrorCode::NotFound, "the W_WINDY modifier is missing", "weather/W_WINDY");
        }
        if (!std::isfinite(windyModifierAmount) || windyModifierAmount < 0.0F || windyModifierAmount > 1.0F)
        {
            return util::Err(util::ErrorCode::OutOfRange,
                             "the W_WINDY modifier amount must be finite and in [0, 1]",
                             "weather/W_WINDY/amount");
        }
        if (!ValidWindRange(archetype->windSpeed, 30.0F) || !ValidWindRange(archetype->gustFactor, 1.0F) ||
            (windyModifierAmount > 0.0F &&
             (!ValidWindRange(modifier->windSpeed, 30.0F) || !ValidWindRange(modifier->gustFactor, 1.0F))))
        {
            return util::Err(util::ErrorCode::InvalidData,
                             "wind speed and gust bands must be finite, ordered and in range",
                             std::format("weather/{}", archetypeId.Value()));
        }

        WeatherWindTarget target;
        target.speed = SampleRange(archetype->windSpeed, rng);
        target.gustFactor = SampleRange(archetype->gustFactor, rng);
        target.modifierAmount = windyModifierAmount;
        if (windyModifierAmount > 0.0F)
        {
            const float boostedSpeed = std::max(target.speed, SampleRange(modifier->windSpeed, rng));
            const float boostedGust = std::max(target.gustFactor, SampleRange(modifier->gustFactor, rng));
            target.speed += (boostedSpeed - target.speed) * windyModifierAmount;
            target.gustFactor += (boostedGust - target.gustFactor) * windyModifierAmount;
        }
        return target;
    }

    util::Result<WeatherWindTarget>
    WeatherSampler::SampleWind(util::Id archetype, float windyModifierAmount, WeatherState& state) const
    {
        if (const util::Result<void> valid = state.Validate(); !valid)
        {
            return valid.Error().WithContext("weather/rng/state");
        }
        util::Rng rng(state.rngState);
        util::Result<WeatherWindTarget> sampled = SampleWind(archetype, windyModifierAmount, rng);
        if (sampled)
        {
            state.rngState = rng.GetState();
        }
        return sampled;
    }

} // namespace cnahouse::weather
