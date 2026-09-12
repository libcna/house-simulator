// SPDX-License-Identifier: MIT
//
// `HOUSE-01694`: §71's `weather set <archetype>` and `weather freeze`.
#include <array>
#include <string>

#include <gtest/gtest.h>

#include "cnahouse/debug/WeatherCommands.hpp"

namespace
{
    using cnahouse::debug::CommandResult;
    using cnahouse::debug::Console;
    using cnahouse::debug::RegisterWeatherCommands;
    using cnahouse::debug::WeatherCommandContext;
    using cnahouse::util::Id;
    using cnahouse::util::Intern;
    using cnahouse::weather::WeatherArchetype;

    struct Fixture
    {
        Fixture()
        {
            archetypes[0].id = Intern("W_CLEAR");
            archetypes[1].id = Intern("W_RAIN");
            archetypes[2].id = Intern("W_WINDY");
            archetypes[2].modifier = true;
            target = archetypes[0].id;
            RegisterWeatherCommands(console, WeatherCommandContext{archetypes, &target, &paused});
        }

        std::array<WeatherArchetype, 3> archetypes;
        Id target;
        bool paused = false;
        Console console;
    };

    TEST(WeatherCommandTests, SetSelectsAnAuthoredStateWithoutChangingTheFreeze)
    {
        Fixture fixture;
        const CommandResult result = fixture.console.Execute("weather set W_RAIN");
        EXPECT_TRUE(result.ok) << result.message;
        EXPECT_EQ(fixture.target, fixture.archetypes[1].id);
        EXPECT_FALSE(fixture.paused);
        EXPECT_NE(result.message.find("W_RAIN"), std::string::npos) << result.message;
        EXPECT_NE(result.message.find("running"), std::string::npos) << result.message;
    }

    TEST(WeatherCommandTests, UnknownAndModifierTargetsAreRejectedWithoutChangingState)
    {
        Fixture fixture;
        const Id before = fixture.target;

        const CommandResult unknown = fixture.console.Execute("weather set W_NOPE");
        EXPECT_FALSE(unknown.ok);
        EXPECT_EQ(fixture.target, before);
        EXPECT_NE(unknown.message.find("W_NOPE"), std::string::npos) << unknown.message;

        const CommandResult modifier = fixture.console.Execute("weather set W_WINDY");
        EXPECT_FALSE(modifier.ok);
        EXPECT_EQ(fixture.target, before);
        EXPECT_NE(modifier.message.find("modifier"), std::string::npos) << modifier.message;
    }

    TEST(WeatherCommandTests, FreezeTogglesAndReportsTheStateItLeaves)
    {
        Fixture fixture;
        const CommandResult frozen = fixture.console.Execute("weather freeze");
        EXPECT_TRUE(frozen.ok) << frozen.message;
        EXPECT_TRUE(fixture.paused);
        EXPECT_NE(frozen.message.find("frozen"), std::string::npos) << frozen.message;

        const CommandResult resumed = fixture.console.Execute("weather freeze");
        EXPECT_TRUE(resumed.ok) << resumed.message;
        EXPECT_FALSE(fixture.paused);
        EXPECT_NE(resumed.message.find("running"), std::string::npos) << resumed.message;
    }

    TEST(WeatherCommandTests, NoVerbReportsWithoutMutating)
    {
        Fixture fixture;
        fixture.paused = true;
        const CommandResult result = fixture.console.Execute("weather");
        EXPECT_TRUE(result.ok) << result.message;
        EXPECT_TRUE(fixture.paused);
        EXPECT_EQ(fixture.target, fixture.archetypes[0].id);
        EXPECT_NE(result.message.find("W_CLEAR"), std::string::npos) << result.message;
        EXPECT_NE(result.message.find("frozen"), std::string::npos) << result.message;
    }

    TEST(WeatherCommandTests, WrongShapesSayWhatTheCommandWanted)
    {
        Fixture fixture;
        for (const std::string command : {"weather set", "weather set W_RAIN extra", "weather freeze now"})
        {
            const CommandResult result = fixture.console.Execute(command);
            EXPECT_FALSE(result.ok) << command;
            EXPECT_NE(result.message.find(command.starts_with("weather set") ? "set" : "freeze"),
                      std::string::npos)
                << result.message;
        }
        const CommandResult verb = fixture.console.Execute("weather thaw");
        EXPECT_FALSE(verb.ok);
        EXPECT_NE(verb.message.find("thaw"), std::string::npos) << verb.message;
        EXPECT_NE(fixture.console.Usage("weather").find("set <archetype>"), std::string::npos);
    }

    TEST(WeatherCommandTests, MissingRuntimeContextFailsInsteadOfDereferencing)
    {
        Console console;
        RegisterWeatherCommands(console, WeatherCommandContext{});
        const CommandResult result = console.Execute("weather freeze");
        EXPECT_FALSE(result.ok);
        EXPECT_NE(result.message.find("no weather"), std::string::npos) << result.message;
    }

} // namespace
