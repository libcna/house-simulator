// SPDX-License-Identifier: MIT
//
// `HOUSE-01536`. §71's `time set <hh:mm>`, `time scale <x>` and `time advance <days>`.
//
// One command with three verbs rather than three commands, which is how §71 lists them and what
// makes `time` with no verb the READING -- the thing somebody about to change the clock wants
// first. The console is the only way to reach §35's clock from inside a running game, so the
// interesting claims here are the refusals: a console takes typing, and a typo that silently
// becomes a valid clock setting is worse than an error message.
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include <gtest/gtest.h>

#include "cnahouse/debug/Console.hpp"
#include "cnahouse/debug/TimeCommands.hpp"
#include "cnahouse/environment/DayLength.hpp"
#include "cnahouse/environment/SimClock.hpp"

namespace
{
    using cnahouse::debug::Console;
    using cnahouse::debug::RegisterTimeCommands;
    using cnahouse::debug::TimeCommandContext;
    using cnahouse::environment::kSecondsPerDay;
    using cnahouse::environment::SimClock;
    using cnahouse::environment::TimeScaleForDayLength;

    struct Fixture
    {
        SimClock clock;
        Console console;

        Fixture()
        {
            clock.timeScale = TimeScaleForDayLength(24.0);
            RegisterTimeCommands(console, TimeCommandContext{&clock});
        }

        [[nodiscard]] auto Run(std::string_view line)
        {
            return console.Execute(line);
        }
    };

} // namespace

TEST(TimeCommandTests, TimeWithNoVerbReportsWithoutChangingAnything)
{
    Fixture fixture;
    fixture.clock.SetCalendar(100.0);
    const double before = fixture.clock.epochSeconds;

    const auto result = fixture.Run("time");
    EXPECT_TRUE(result.ok) << result.message;
    EXPECT_DOUBLE_EQ(fixture.clock.epochSeconds, before) << "reporting moved the clock";
    // The reading carries everything a person needs to know they are where they think they are:
    // the date, the wall clock, whether daylight saving is in force, the rate, and -- because §36
    // derives precipitation from it -- the temperature.
    EXPECT_NE(result.message.find("2031-04-11"), std::string::npos) << result.message;
    EXPECT_NE(result.message.find("scale 60x"), std::string::npos) << result.message;
    EXPECT_NE(result.message.find("24 real min/day"), std::string::npos) << result.message;
    EXPECT_NE(result.message.find(" C"), std::string::npos) << result.message;
}

TEST(TimeCommandTests, SetPutsTheClockFaceWhereItIsTold)
{
    Fixture fixture;
    fixture.clock.SetCalendar(100.0);

    EXPECT_TRUE(fixture.Run("time set 09:20").ok);
    EXPECT_EQ(fixture.clock.Standard().hour, 9);
    EXPECT_EQ(fixture.clock.Standard().minute, 20);
    EXPECT_DOUBLE_EQ(fixture.clock.SecondsOfDay(), 9.0 * 3600.0 + 20.0 * 60.0);

    EXPECT_TRUE(fixture.Run("time set 00:00").ok);
    EXPECT_DOUBLE_EQ(fixture.clock.SecondsOfDay(), 0.0);
    EXPECT_TRUE(fixture.Run("time set 23:59").ok);
    EXPECT_DOUBLE_EQ(fixture.clock.SecondsOfDay(), 23.0 * 3600.0 + 59.0 * 60.0);

    // It stays inside the SAME simulated day, which is what makes it a clock-face command rather
    // than a jump: the number of sunrises does not change.
    fixture.clock.SetCalendar(100.0);
    const std::int64_t sunrises = fixture.clock.SimDayIndex();
    EXPECT_TRUE(fixture.Run("time set 06:00").ok);
    EXPECT_EQ(fixture.clock.SimDayIndex(), sunrises);
}

TEST(TimeCommandTests, SetRefusesWhatIsNotAClockFace)
{
    Fixture fixture;
    fixture.clock.SetCalendar(100.0);
    const double before = fixture.clock.epochSeconds;
    for (const char* bad : {"time set 24:00",
                            "time set 12:60",
                            "time set -1:00",
                            "time set 9",
                            "time set 9:5x",
                            "time set noon",
                            "time set 9:30:15",
                            "time set 9.5:30",
                            "time set"})
    {
        const auto result = fixture.Run(bad);
        EXPECT_FALSE(result.ok) << bad << " was accepted: " << result.message;
        EXPECT_DOUBLE_EQ(fixture.clock.epochSeconds, before) << bad << " moved the clock anyway";
    }
    // 12:60 is a typo for 13:00 and 24:00 for 00:00. Wrapping them would be guessing at what
    // somebody meant while telling them they were right.
    EXPECT_TRUE(fixture.Run("time set 13:00").ok);
    EXPECT_EQ(fixture.clock.Standard().hour, 13);
}

TEST(TimeCommandTests, ScaleTakesSection35Point2sColumnIncludingFrozen)
{
    Fixture fixture;
    EXPECT_TRUE(fixture.Run("time scale 30").ok);
    EXPECT_DOUBLE_EQ(fixture.clock.timeScale, 30.0);
    fixture.clock.Advance(48.0 * 60.0);
    EXPECT_DOUBLE_EQ(fixture.clock.epochSeconds, kSecondsPerDay) << "48 real minutes is not a day at 30x";

    // Zero is §35.2's frozen row and is accepted deliberately -- the state the design calls
    // "essential for pixel-regression tests".
    const auto frozen = fixture.Run("time scale 0");
    EXPECT_TRUE(frozen.ok) << frozen.message;
    EXPECT_NE(frozen.message.find("frozen"), std::string::npos) << frozen.message;
    const double stopped = fixture.clock.epochSeconds;
    fixture.clock.Advance(3600.0);
    EXPECT_DOUBLE_EQ(fixture.clock.epochSeconds, stopped);

    // A negative scale is a clock that runs backwards, which `Advance` would refuse anyway -- so
    // the command refuses it where a person can see why.
    for (const char* bad : {"time scale -1", "time scale fast", "time scale", "time scale 1x"})
    {
        const auto result = fixture.Run(bad);
        EXPECT_FALSE(result.ok) << bad << " was accepted: " << result.message;
    }
    EXPECT_DOUBLE_EQ(fixture.clock.timeScale, 0.0) << "a refused scale changed the rate";
}

TEST(TimeCommandTests, AdvanceMovesTheCalendarByTheDaysItIsGiven)
{
    Fixture fixture;
    fixture.clock.SetCalendar(0.0);
    EXPECT_TRUE(fixture.Run("time advance 30").ok);
    EXPECT_NEAR(fixture.clock.CalendarDays(), 30.0, 1e-9) << "advance 30 is a month later";
    EXPECT_EQ(fixture.clock.Standard().month, 1);
    EXPECT_EQ(fixture.clock.Standard().day, 31);

    // CALENDAR days and not sunrises: under §35.2b's compression 30 days is a day and a quarter of
    // play, and a person typing "advance 30" means the date.
    EXPECT_EQ(fixture.clock.SimDayIndex(), 1);

    // Fractional and negative both work, because a debug command that could only go forwards in
    // whole days would be no use for reproducing a bug at dusk yesterday.
    EXPECT_TRUE(fixture.Run("time advance -0.5").ok);
    EXPECT_NEAR(fixture.clock.CalendarDays(), 29.5, 1e-9);
    EXPECT_TRUE(fixture.Run("time advance 365").ok);
    EXPECT_NEAR(fixture.clock.CalendarDays(), 394.5, 1e-9);
    EXPECT_EQ(fixture.clock.Standard().year, 2032) << "a year of advancing did not turn the year";

    for (const char* bad : {"time advance", "time advance soon", "time advance 1d"})
    {
        EXPECT_FALSE(fixture.Run(bad).ok) << bad;
    }
}

TEST(TimeCommandTests, AnUnknownVerbIsRefusedByName)
{
    Fixture fixture;
    const auto result = fixture.Run("time stop now");
    EXPECT_FALSE(result.ok);
    EXPECT_NE(result.message.find("stop"), std::string::npos)
        << "the message does not say what was not understood: " << result.message;

    // ...and a command registered against no clock says so rather than dereferencing nothing.
    Console orphan;
    RegisterTimeCommands(orphan, TimeCommandContext{});
    const auto none = orphan.Execute("time");
    EXPECT_FALSE(none.ok);
    EXPECT_NE(none.message.find("no clock"), std::string::npos) << none.message;
}
