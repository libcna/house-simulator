// SPDX-License-Identifier: MIT
#pragma once

#include "cnahouse/debug/Console.hpp"
#include "cnahouse/util/Ids.hpp"
#include "cnahouse/weather/WeatherState.hpp"

namespace cnahouse::debug
{

    /// @brief The live weather control plane reached by §71's console (`HOUSE-01694`).
    ///
    /// These are pointers because the console is registered once against game-owned state that
    /// outlives it. The weather simulation remains the sole owner of advancing the continuous
    /// vector; the console changes its named target or pauses that advancement.
    struct WeatherCommandContext
    {
        std::span<const weather::WeatherArchetype> archetypes;
        util::Id* targetArchetype = nullptr;
        bool* transitionsPaused = nullptr;
    };

    /// @brief Registers `weather set <archetype>` and the toggling `weather freeze` command.
    void RegisterWeatherCommands(Console& console, WeatherCommandContext context);

} // namespace cnahouse::debug
