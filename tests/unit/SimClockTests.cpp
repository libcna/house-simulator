// SPDX-License-Identifier: MIT
//
// `HOUSE-01531`. §35's clock: epoch seconds, time scale, location, UTC offset and the US daylight
// saving rule. `HOUSE-01532` is the full calendar conversion and its 500-case table; what is
// proved here is the clock itself and the arithmetic the DST rule needs to exist at all.
//
// The strongest fixture is `cna-house.md` §31.4's own screenshot scene: **Saturday 14 June 2031,
// 09:20 local, UTC−4 DST**. Three independent facts in one line -- a weekday, a date and the DST
// verdict -- authored years before this code and by a different hand.
#include <cmath>
#include <cstdint>
#include <limits>
#include <tuple>
#include <utility>

#include <gtest/gtest.h>

#include "cnahouse/environment/SimClock.hpp"

namespace
{
    using cnahouse::environment::CivilFromDays;
    using cnahouse::environment::CivilTime;
    using cnahouse::environment::DaysFromCivil;
    using cnahouse::environment::kDefaultTimeScale;
    using cnahouse::environment::kDefaultUtcOffsetMinutes;
    using cnahouse::environment::NthWeekdayOfMonth;
    using cnahouse::environment::SimClock;

    SimClock At(int year, int month, int day, int hour = 0, int minute = 0, int second = 0)
    {
        SimClock clock;
        CivilTime time;
        time.year = year;
        time.month = month;
        time.day = day;
        time.hour = hour;
        time.minute = minute;
        time.second = second;
        clock.SetStandard(time);
        return clock;
    }

} // namespace

TEST(SimClockTests, TheEpochIsMidnightOnTheFirstOf2031)
{
    const SimClock clock;
    EXPECT_DOUBLE_EQ(clock.epochSeconds, 0.0);
    const CivilTime start = clock.Standard();
    EXPECT_EQ(start.year, 2031);
    EXPECT_EQ(start.month, 1);
    EXPECT_EQ(start.day, 1);
    EXPECT_EQ(start.hour, 0);
    EXPECT_EQ(start.dayOfYear, 1);
    // 2031-01-01 was a Wednesday. Independent of everything else here, and the seed the DST rule's
    // weekday arithmetic is built on.
    EXPECT_EQ(start.weekday, 3);
}

TEST(SimClockTests, OneRealSecondIsExactlyOneSimulatedMinute)
{
    // §35.2's whole reason for choosing 60x: *"1 s = 1 min removes an entire class of arithmetic
    // mistakes from every test, log and settings dialogue in the project."* This is that test.
    SimClock clock;
    EXPECT_DOUBLE_EQ(clock.timeScale, kDefaultTimeScale);
    clock.Advance(1.0);
    EXPECT_DOUBLE_EQ(clock.epochSeconds, 60.0);
    clock.Advance(3.0);
    EXPECT_DOUBLE_EQ(clock.epochSeconds, 240.0);
    EXPECT_EQ(clock.Standard().minute, 4);
    // A simulated day is 24 real minutes, which is the other half of the same decision.
    SimClock day;
    day.Advance(24.0 * 60.0);
    EXPECT_DOUBLE_EQ(day.epochSeconds, 86400.0);
    EXPECT_EQ(day.Standard().day, 2);
}

TEST(SimClockTests, AHitchAdvancesByTheTimeThatReallyPassed)
{
    // `HOUSE-01540`'s claim in the clock's own terms: a 250 ms frame is 250 ms of real time and
    // 15 simulated seconds, not one frame's worth of anything.
    SimClock clock;
    clock.Advance(0.250);
    EXPECT_DOUBLE_EQ(clock.epochSeconds, 15.0);

    // ...and time does not run backwards because a platform timer glitched. A NaN would put the
    // calendar beyond recovery in one frame, which is worse than a frame that did not advance.
    const double before = clock.epochSeconds;
    clock.Advance(-5.0);
    clock.Advance(0.0);
    clock.Advance(std::nan(""));
    clock.Advance(std::numeric_limits<double>::infinity());
    EXPECT_DOUBLE_EQ(clock.epochSeconds, before);
}

TEST(SimClockTests, TheCalendarRoundTripsAndKnowsItsLeapYears)
{
    for (const auto& [year, month, day] : {std::tuple{2031, 1, 1},
                                           std::tuple{2031, 12, 31},
                                           std::tuple{2032, 2, 29},  // a leap year
                                           std::tuple{2100, 3, 1},   // not one: divisible by 100
                                           std::tuple{2000, 2, 29},  // but 400 is
                                           std::tuple{1969, 7, 20}}) // before the 1970 pivot
    {
        const CivilTime back = CivilFromDays(DaysFromCivil(year, month, day));
        EXPECT_EQ(back.year, year) << year << "-" << month << "-" << day;
        EXPECT_EQ(back.month, month) << year << "-" << month << "-" << day;
        EXPECT_EQ(back.day, day) << year << "-" << month << "-" << day;
    }
    // 2100 is not a leap year, so 29 February does not exist and 1 March is day 60.
    EXPECT_EQ(CivilFromDays(DaysFromCivil(2100, 3, 1)).dayOfYear, 60);
    EXPECT_EQ(CivilFromDays(DaysFromCivil(2032, 3, 1)).dayOfYear, 61) << "2032 IS a leap year";

    // The weekday anchors: 1970-01-01 was a Thursday and 2000-01-01 a Saturday.
    EXPECT_EQ(CivilFromDays(DaysFromCivil(1970, 1, 1)).weekday, 4);
    EXPECT_EQ(CivilFromDays(DaysFromCivil(2000, 1, 1)).weekday, 6);
}

TEST(SimClockTests, TheScreenshotSceneOfSection31IsSaturdayInDaylightSaving)
{
    // `cna-house.md`: "Saturday 14 June 2031, 09:20 local (UTC−4 DST)". Authored as a screenshot
    // scene long before this clock existed, which is what makes it worth testing against: three
    // facts that were not derived from this code.
    const SimClock clock = At(2031, 6, 14, 8, 20); // 09:20 WALL is 08:20 standard in June
    EXPECT_EQ(clock.Standard().weekday, 6) << "14 June 2031 is not a Saturday";
    EXPECT_TRUE(clock.IsDaylightSaving()) << "June is not in daylight saving";
    EXPECT_EQ(clock.EffectiveUtcOffsetMinutes(), -240) << "the scene says UTC-4";
    EXPECT_EQ(kDefaultUtcOffsetMinutes, -300) << "...and standard time is UTC-5";

    const CivilTime wall = clock.Wall();
    EXPECT_EQ(wall.hour, 9);
    EXPECT_EQ(wall.minute, 20) << "the wall clock does not read 09:20";
    EXPECT_EQ(wall.day, 14);
}

TEST(SimClockTests, DaylightSavingRunsFromMarchsSecondSundayToNovembersFirst)
{
    // 2031: 9 March and 2 November. Derived, not typed -- the rule is what is being tested.
    EXPECT_EQ(NthWeekdayOfMonth(2031, 3, 0, 2), 9);
    EXPECT_EQ(NthWeekdayOfMonth(2031, 11, 0, 1), 2);

    EXPECT_FALSE(At(2031, 3, 9, 1, 59, 59).IsDaylightSaving()) << "a minute before it begins";
    EXPECT_TRUE(At(2031, 3, 9, 2, 0, 0).IsDaylightSaving()) << "02:00 standard, on the second";
    EXPECT_TRUE(At(2031, 11, 2, 0, 59, 59).IsDaylightSaving()) << "a minute before it ends";
    EXPECT_FALSE(At(2031, 11, 2, 1, 0, 0).IsDaylightSaving())
        << "01:00 standard is 02:00 daylight, which is the reading the clock goes back from";

    // Winter at both ends of the year, summer in the middle.
    EXPECT_FALSE(At(2031, 1, 15).IsDaylightSaving());
    EXPECT_FALSE(At(2031, 12, 15).IsDaylightSaving());
    EXPECT_TRUE(At(2031, 7, 4).IsDaylightSaving());

    // And a different year, so the rule is a rule and not 2031's two dates.
    EXPECT_EQ(NthWeekdayOfMonth(2032, 3, 0, 2), 14);
    EXPECT_EQ(NthWeekdayOfMonth(2032, 11, 0, 1), 7);
    EXPECT_FALSE(At(2032, 3, 13, 12).IsDaylightSaving());
    EXPECT_TRUE(At(2032, 3, 14, 12).IsDaylightSaving());
}

TEST(SimClockTests, TheRuleCanBeTurnedOffAndThenTheOffsetNeverMoves)
{
    SimClock clock = At(2031, 7, 4, 12);
    EXPECT_TRUE(clock.IsDaylightSaving());
    clock.dstRulesUS = false;
    EXPECT_FALSE(clock.IsDaylightSaving());
    EXPECT_EQ(clock.EffectiveUtcOffsetMinutes(), kDefaultUtcOffsetMinutes);
    // With the rule off the wall clock IS the standard clock, in July as in January.
    EXPECT_EQ(clock.Wall().hour, clock.Standard().hour);
}

TEST(SimClockTests, TheEpochLineIsMonotoneThroughBothTransitions)
{
    // The reason `epochSeconds` is local STANDARD time and not wall-clock time. Stepping a minute
    // at a time across November's transition, the STANDARD reading never repeats and never skips;
    // a wall-clock epoch would show 01:00-01:59 twice and there would be no number to order them
    // by. Checked across both transitions, an hour either side.
    for (const auto& [month, day] : {std::pair{3, 9}, std::pair{11, 2}})
    {
        SimClock clock = At(2031, month, day, 0, 0, 0);
        double previous = clock.epochSeconds - 1.0;
        int minutes = 0;
        for (int step = 0; step < 240; ++step)
        {
            EXPECT_GT(clock.epochSeconds, previous) << month << "/" << day << " step " << step;
            previous = clock.epochSeconds;
            const CivilTime standard = clock.Standard();
            EXPECT_EQ(standard.day, day) << "the standard date wandered";
            minutes += standard.minute == 30 ? 1 : 0;
            clock.epochSeconds += 60.0;
        }
        EXPECT_EQ(minutes, 4) << "four hours were stepped, so :30 must occur exactly four times";
    }
}

TEST(SimClockTests, SecondsOfDayAndDayIndexAgreeWithTheCalendar)
{
    const SimClock clock = At(2031, 3, 9, 13, 45, 30);
    EXPECT_DOUBLE_EQ(clock.SecondsOfDay(), 13 * 3600 + 45 * 60 + 30);
    EXPECT_EQ(clock.DayIndex(), DaysFromCivil(2031, 3, 9) - DaysFromCivil(2031, 1, 1));

    // Before the epoch the arithmetic is a FLOOR and not a truncation: -1 s is the last second of
    // 2030-12-31, not the first of 2031-01-01 counted backwards.
    SimClock before;
    before.epochSeconds = -1.0;
    EXPECT_EQ(before.DayIndex(), -1);
    EXPECT_DOUBLE_EQ(before.SecondsOfDay(), 86399.0);
    const CivilTime last = before.Standard();
    EXPECT_EQ(last.year, 2030);
    EXPECT_EQ(last.month, 12);
    EXPECT_EQ(last.day, 31);
    EXPECT_EQ(last.hour, 23);
    EXPECT_EQ(last.minute, 59);
    EXPECT_EQ(last.second, 59);
}

TEST(SimClockTests, TheLocationIsSection33s)
{
    const SimClock clock;
    EXPECT_DOUBLE_EQ(clock.latitudeDeg, 40.05);
    EXPECT_DOUBLE_EQ(clock.longitudeDeg, -75.30);
    EXPECT_EQ(clock.utcOffsetMinutes, -300);
    EXPECT_TRUE(clock.dstRulesUS);
}
