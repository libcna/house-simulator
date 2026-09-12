// SPDX-License-Identifier: MIT
#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include "cnahouse/util/Ids.hpp"
#include "cnahouse/util/Result.hpp"
#include "cnahouse/util/Rng.hpp"

namespace cnahouse::weather
{

    /// @brief §36.1's precipitation phase, kept separate from its continuous intensity.
    enum class PrecipType : std::uint8_t
    {
        None,
        Rain,
        Snow,
        Sleet,
        Hail,
    };

    [[nodiscard]] constexpr std::string_view PrecipTypeName(PrecipType type) noexcept
    {
        switch (type)
        {
            case PrecipType::None:
                return "None";
            case PrecipType::Rain:
                return "Rain";
            case PrecipType::Snow:
                return "Snow";
            case PrecipType::Sleet:
                return "Sleet";
            case PrecipType::Hail:
                return "Hail";
        }
        return "None";
    }

    /// @brief Parses the stable JSON spelling used by weather state and authored archetypes.
    [[nodiscard]] util::Result<PrecipType> ParsePrecipType(std::string_view name,
                                                           std::string_view context = "precipType");

    /// @brief One inclusive target band from `layout.weather.json`.
    ///
    /// Equal endpoints are valid: clear weather deliberately carries a precipitation band of
    /// `[0, 0]`. Sampling is owned by `HOUSE-01683`; this type only preserves authored intent.
    struct WeatherRange
    {
        float minimum = 0.0F;
        float maximum = 0.0F;

        [[nodiscard]] friend bool operator==(const WeatherRange&, const WeatherRange&) = default;
    };

    /// @brief One of §36.2's thirteen weather targets or its independent wind modifier.
    struct WeatherArchetype
    {
        util::Id id;
        WeatherRange cloudCover;
        WeatherRange cloudCumuliform;
        PrecipType precipType = PrecipType::None;
        WeatherRange precipIntensity;
        WeatherRange windSpeed;
        WeatherRange gustFactor;
        WeatherRange fogDensity;
        WeatherRange temperatureOffsetC;
        WeatherRange humidity;
        WeatherRange thunderProbability;
        bool modifier = false;
        float weight = 0.0F;
        /// Spring, summer, autumn, winter. An omitted authored entry keeps weight 1.
        std::array<float, 4> seasonalWeights{1.0F, 1.0F, 1.0F, 1.0F};

        [[nodiscard]] friend bool operator==(const WeatherArchetype&, const WeatherArchetype&) = default;
    };

    struct WeatherTransition
    {
        util::Id target;
        float probability = 0.0F;

        [[nodiscard]] friend bool operator==(const WeatherTransition&, const WeatherTransition&) = default;
    };

    /// @brief One base transition-matrix row; seasonal weights turn it into four matrices.
    struct WeatherTransitionRow
    {
        util::Id source;
        std::vector<WeatherTransition> targets;

        [[nodiscard]] friend bool operator==(const WeatherTransitionRow&,
                                             const WeatherTransitionRow&) = default;
    };

    /// @brief The complete continuous weather vector of `cna-house.md` §36.1.
    ///
    /// This is live state, not an archetype. An archetype supplies a target and later systems move
    /// these fields toward it under §42.2's rate limits. Precipitation therefore has both a phase
    /// and an intensity; there is deliberately no boolean shortcut for it.
    struct WeatherState
    {
        float cloudCover = 0.0F;
        float cloudCumuliform = 0.0F;
        PrecipType precipType = PrecipType::None;
        float precipIntensity = 0.0F;
        float windSpeed = 0.0F;
        float windDirectionDeg = 0.0F;
        float gustFactor = 0.0F;
        float fogDensity = 0.0F;
        float thunderIntensity = 0.0F;
        float temperatureC = 11.0F;
        float humidity = 0.0F;
        float surfaceWetness = 0.0F;
        float snowDepth = 0.0F;
        util::Rng::State rngState = util::Rng{}.GetState();

        /// @brief Parses one complete weather-state object; every §36.1 field is required.
        [[nodiscard]] static util::Result<WeatherState> FromJson(std::string_view text,
                                                                 std::string_view name);

        /// @brief Serialises every §36.1 field through `System::Text::Json`.
        ///
        /// The four RNG words are 64 hexadecimal characters, so values above `INT64_MAX` survive
        /// JSON implementations whose numeric model is signed or floating point.
        [[nodiscard]] util::Result<std::string> ToJson() const;

        /// @brief Checks every documented range and rejects non-finite continuous state.
        [[nodiscard]] util::Result<void> Validate() const;

        [[nodiscard]] friend bool operator==(const WeatherState&, const WeatherState&) = default;
    };

} // namespace cnahouse::weather
