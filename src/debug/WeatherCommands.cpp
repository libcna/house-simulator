// SPDX-License-Identifier: MIT
#include "cnahouse/debug/WeatherCommands.hpp"

#include <algorithm>
#include <format>

namespace cnahouse::debug
{
    namespace
    {
        [[nodiscard]] std::string_view ArchetypeName(util::Id id) noexcept
        {
            const std::string_view name = util::IdRegistry::NameOf(id);
            return name.empty() ? std::string_view{"<none>"} : name;
        }
    } // namespace

    void RegisterWeatherCommands(Console& console, WeatherCommandContext context)
    {
        console.Register(
            "weather",
            "weather [set <archetype> | freeze]  -- set the target or pause/resume transitions",
            [context](std::span<const std::string_view> arguments) -> CommandResult
            {
                if (context.targetArchetype == nullptr || context.transitionsPaused == nullptr ||
                    context.archetypes.empty())
                {
                    return CommandResult{false, "weather: this session has no weather simulation"};
                }

                if (arguments.empty())
                {
                    return CommandResult{true,
                                         std::format("weather {} ({})",
                                                     ArchetypeName(*context.targetArchetype),
                                                     *context.transitionsPaused ? "frozen" : "running")};
                }

                if (arguments[0] == "set")
                {
                    if (arguments.size() != 2U)
                    {
                        return CommandResult{false, "weather set: needs exactly one archetype id"};
                    }
                    const auto found = std::ranges::find_if(
                        context.archetypes,
                        [&](const weather::WeatherArchetype& archetype)
                        { return util::IdRegistry::NameOf(archetype.id) == arguments[1]; });
                    if (found == context.archetypes.end())
                    {
                        return CommandResult{
                            false,
                            std::format("weather set: '{}' is not an authored archetype", arguments[1])};
                    }
                    if (found->modifier)
                    {
                        return CommandResult{
                            false,
                            std::format("weather set: '{}' is a modifier, not a weather state",
                                        arguments[1])};
                    }
                    *context.targetArchetype = found->id;
                    return CommandResult{true,
                                         std::format("weather set: target {}; transitions are {}",
                                                     arguments[1],
                                                     *context.transitionsPaused ? "frozen" : "running")};
                }

                if (arguments[0] == "freeze")
                {
                    if (arguments.size() != 1U)
                    {
                        return CommandResult{false, "weather freeze: takes no value"};
                    }
                    *context.transitionsPaused = !*context.transitionsPaused;
                    return CommandResult{true,
                                         std::format("weather {} at {}",
                                                     *context.transitionsPaused ? "frozen" : "running",
                                                     ArchetypeName(*context.targetArchetype))};
                }

                return CommandResult{false, std::format("weather: '{}' is not set or freeze", arguments[0])};
            });
    }

} // namespace cnahouse::debug
