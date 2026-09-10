// SPDX-License-Identifier: MIT
//
// `HOUSE-01542`. §35.2b's compressed year: one simulated day advances the calendar by 24 days, so
// **a full year takes 365 real minutes** while the sun still rises once every 24.
//
// The decision is the project owner's and its reason is in §35.2b in one line: without it four
// seasons need 365 simulated days, which at 24 real minutes each is 146 real hours of play, and
// *"a player would never witness autumn"*. The two rates are decoupled on purpose -- the diurnal
// one for how the sun should look, the annual one for how long a player waits for winter -- and
// what is proved here is that they really are two rates and that setting the field to 1.0 gives
// back exactly the clock `HOUSE-01531` built.
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>

#include <gtest/gtest.h>

#include "cnahouse/environment/SimClock.hpp"

namespace
{
    using cnahouse::environment::CivilTime;
    using cnahouse::environment::kDefaultCalendarDaysPerSimDay;
    using cnahouse::environment::kDefaultTimeScale;
    using cnahouse::environment::kSecondsPerDay;
    using cnahouse::environment::SimClock;

    /// Real seconds of play, as `Advance` takes them.
    constexpr double kRealMinute = 60.0;

} // namespace

TEST(CompressedYearTests, TheDefaultIsSection35Point2bsTwentyFour)
{
    const SimClock clock;
    EXPECT_DOUBLE_EQ(clock.calendarDaysPerSimDay, 24.0);
    EXPECT_DOUBLE_EQ(kDefaultCalendarDaysPerSimDay, 24.0);
    // The table's other three numbers follow from that one and from `timeScale`, and each is a
    // claim §35.2b makes in its own words rather than an arithmetic identity restated.
    const double simDaysPerYear = 365.0 / clock.calendarDaysPerSimDay;
    EXPECT_NEAR(simDaysPerYear, 15.208333, 1e-6) << "§35.2b's 15.208333 simulated days a year";
    const double realMinutesPerSimDay = kSecondsPerDay / kDefaultTimeScale / 60.0;
    EXPECT_DOUBLE_EQ(realMinutesPerSimDay, 24.0) << "the 24-real-minute day of §35.2";
    EXPECT_NEAR(simDaysPerYear * realMinutesPerSimDay, 365.0, 1e-4)
        << "a year is not 365 real minutes, which is the number the whole decision is for";
    EXPECT_NEAR(365.0 / 4.0, 91.25, 1e-9) << "and a season is 91.25 real minutes";
}

TEST(CompressedYearTests, AYearOfPlayIsThreeHundredAndSixtyFiveRealMinutes)
{
    // Measured rather than derived: run the clock for 365 real minutes a minute at a time and ask
    // what date it is. Not `365 * 60` in one call, because that is the arithmetic being checked.
    SimClock clock;
    for (int minute = 0; minute < 365; ++minute)
    {
        clock.Advance(kRealMinute);
    }
    const CivilTime after = clock.Standard();
    EXPECT_EQ(after.year, 2032) << "365 real minutes did not turn the year over";
    EXPECT_EQ(after.month, 1);
    EXPECT_EQ(after.day, 1) << "a year of play is not a year of calendar";
    EXPECT_EQ(clock.DayIndex(), 365);
    // ...and the sun came up once every 24 of those minutes, which is the rate that did NOT
    // change. 365 / 24 = 15.2, so fifteen sunrises.
    EXPECT_EQ(clock.SimDayIndex(), 15);
}

TEST(CompressedYearTests, TheTimeOfDayIsUntouchedByTheCompression)
{
    // The decoupling, stated as the thing a player sees: the clock face still takes 24 real
    // minutes to go round, whatever the calendar is doing. Twelve real minutes is noon.
    SimClock clock;
    for (int minute = 0; minute < 12; ++minute)
    {
        clock.Advance(kRealMinute);
    }
    EXPECT_DOUBLE_EQ(clock.SecondsOfDay(), 12.0 * 3600.0);
    const CivilTime noon = clock.Standard();
    EXPECT_EQ(noon.hour, 12);
    EXPECT_EQ(noon.minute, 0);
    // ...and half a year of calendar has gone by underneath it: twelve real minutes is half a
    // simulated day, which is twelve calendar days.
    EXPECT_EQ(clock.DayIndex(), 12);
    EXPECT_EQ(noon.day, 13) << "twelve calendar days past 1 January is the 13th";
    EXPECT_EQ(clock.SimDayIndex(), 0) << "the sun has not set yet";
}

TEST(CompressedYearTests, TheCalendarIsContinuousBecauseSection36Point3SaysItMustBe)
{
    // *"Season is a continuous phase, never an enum... a matrix that switched at an instant would
    // be visible as a glitch."* So `CalendarDays` may not step: over a simulated day it has to
    // pass through every value between one date and the next, not jump 24 at midnight.
    SimClock clock;
    double previous = clock.CalendarDays();
    int distinctDates = 0;
    double largestJump = 0.0;
    CivilTime last = clock.Standard();
    for (int step = 0; step < 24 * 60; ++step)
    {
        clock.Advance(1.0); // one real second, which is one simulated minute
        const double now = clock.CalendarDays();
        largestJump = std::max(largestJump, now - previous);
        EXPECT_GT(now, previous) << "the calendar stood still for a second at step " << step;
        previous = now;
        const CivilTime today = clock.Standard();
        distinctDates += today.day != last.day ? 1 : 0;
        last = today;
    }
    // A simulated day of play: 24 calendar days, reached in 1 440 continuous steps rather than in
    // one jump of 24 at midnight.
    EXPECT_NEAR(clock.CalendarDays(), 24.0, 1e-9);
    EXPECT_EQ(distinctDates, 24) << "the date did not advance one day at a time";
    EXPECT_LT(largestJump, 0.02) << "the calendar moved in a step, which §36.3 forbids";
}

TEST(CompressedYearTests, AtOnePointZeroItIsExactlyTheClockItWasBefore)
{
    // §35.2b: *"can be set to 1.0 for a realistic-calendar debug run"*. That has to mean the
    // IDENTITY and not merely something close: every derived quantity collapses, including the
    // ones that go through a floor and could have picked up an off-by-one.
    for (const double seconds : {0.0, 1.0, 86399.0, 86400.0, 86400.5, -1.0, -86400.0, 12345678.0})
    {
        SimClock plain;
        plain.calendarDaysPerSimDay = 1.0;
        plain.epochSeconds = seconds;
        EXPECT_DOUBLE_EQ(plain.CivilEpochSeconds(),
                         std::floor(seconds / kSecondsPerDay) * kSecondsPerDay + plain.SecondsOfDay())
            << seconds;
        EXPECT_NEAR(plain.CivilEpochSeconds(), seconds, 1e-6) << seconds;
        EXPECT_EQ(plain.DayIndex(), plain.SimDayIndex()) << seconds;
    }

    // ...and a date set on an uncompressed clock reads back exactly, which is the property
    // `SetStandard` can only promise at 1.0.
    CivilTime wanted;
    wanted.year = 2031;
    wanted.month = 6;
    wanted.day = 14;
    wanted.hour = 8;
    wanted.minute = 20;
    wanted.second = 5;
    SimClock plain;
    plain.calendarDaysPerSimDay = 1.0;
    plain.SetStandard(wanted);
    const CivilTime back = plain.Standard();
    EXPECT_EQ(back.year, wanted.year);
    EXPECT_EQ(back.month, wanted.month);
    EXPECT_EQ(back.day, wanted.day);
    EXPECT_EQ(back.hour, wanted.hour);
    EXPECT_EQ(back.minute, wanted.minute);
    EXPECT_EQ(back.second, wanted.second);
}

TEST(CompressedYearTests, SetCalendarPutsTheClockExactlyWhereItSays)
{
    // One quantity in, one quantity set, nothing to round. The time of day is not a second input:
    // under compression a calendar position IS a moment in a simulated day, and pretending
    // otherwise is what makes `SetStandard` coarse.
    SimClock clock;
    clock.SetCalendar(172.0);
    EXPECT_DOUBLE_EQ(clock.CalendarDays(), 172.0);
    EXPECT_EQ(clock.DayIndex(), 172);
    // 172 days past 1 January 2031 is 22 June, and 172/24 = 7.1667 simulated days puts the sun at
    // 04:00 of the eighth.
    const CivilTime summer = clock.Standard();
    EXPECT_EQ(summer.month, 6);
    EXPECT_EQ(summer.day, 22);
    EXPECT_EQ(summer.hour, 4);
    EXPECT_EQ(clock.SimDayIndex(), 7);
    EXPECT_TRUE(clock.IsDaylightSaving()) << "late June is not in daylight saving";

    // ...and it is exact for every position asked of it, not for the one this test picked.
    for (const double days : {0.0, 1.0, 91.25, 182.5, 273.75, 365.0, -30.0, 1000.5})
    {
        SimClock probe;
        probe.SetCalendar(days);
        EXPECT_NEAR(probe.CalendarDays(), days, 1e-9) << days;
    }
}

TEST(CompressedYearTests, SetStandardIsCoarseUnderCompressionAndSaysSoRatherThanLying)
{
    // The honest half. A civil reading is a DATE and a TIME OF DAY running at different rates, so
    // most (date, time) pairs are instants the clock never passes through: at 24, a given time of
    // day falls on one date in 24. `SetStandard` lands on the nearest instant it does pass
    // through, and `Standard()` then says where that is.
    CivilTime wanted;
    wanted.year = 2031;
    wanted.month = 6;
    wanted.day = 14;
    wanted.hour = 8;
    wanted.minute = 20;

    SimClock clock;
    clock.SetStandard(wanted);
    const CivilTime landed = clock.Standard();
    EXPECT_EQ(landed.hour, 8) << "the time of day is the half that is always honoured";
    EXPECT_EQ(landed.minute, 20);
    EXPECT_EQ(landed.month, 6) << "and the month is right even when the day cannot be";

    // **The nearest reachable instant, not merely a near one, and over 672 requests rather than
    // over the one this test happens to name.** Stepping either way by a whole simulated day --
    // the only move that keeps the time of day -- must never land closer to the date asked for.
    // The sweep matters: the two candidates either side straddle `rate` calendar days, so which
    // of them is nearer flips at the halfway mark, and a single date only ever exercises one side.
    int worst = 0;
    for (int day = 1; day <= 28; ++day)
    {
        for (int hour = 0; hour < 24; ++hour)
        {
            CivilTime asked = wanted;
            asked.day = day;
            asked.hour = hour;

            SimClock reference;
            reference.calendarDaysPerSimDay = 1.0;
            reference.SetStandard(asked);
            const std::int64_t target = reference.DayIndex();

            SimClock probe;
            probe.SetStandard(asked);
            EXPECT_EQ(probe.Standard().hour, hour) << day << " at " << hour;
            const std::int64_t here = std::abs(probe.DayIndex() - target);
            worst = std::max(worst, static_cast<int>(here));
            for (const double step : {-1.0, 1.0})
            {
                SimClock moved = probe;
                moved.epochSeconds += step * kSecondsPerDay;
                EXPECT_GE(std::abs(moved.DayIndex() - target), here)
                    << "a simulated day " << step << " away lands nearer " << day << " June at " << hour
                    << ":00 than the clock did";
            }
        }
    }
    // Half of `calendarDaysPerSimDay`, because the nearest of two candidates a rate apart is never
    // more than half a rate away. 12 at the default, and the number says the search is doing its
    // job rather than that the tolerance was chosen to fit.
    EXPECT_LE(worst, 12) << "the worst of the 672 requests landed " << worst << " calendar days out";
    EXPECT_GT(worst, 0) << "every request was exact, so this is not testing the compressed case";

    // The coarseness is the compression's and not a bug in the search: at 1.0 the same request is
    // exact, and it is exact for EVERY time of day rather than for the one this test picked.
    for (int hour = 0; hour < 24; ++hour)
    {
        CivilTime each = wanted;
        each.hour = hour;
        SimClock plain;
        plain.calendarDaysPerSimDay = 1.0;
        plain.SetStandard(each);
        EXPECT_EQ(plain.Standard().day, each.day) << "hour " << hour;
        EXPECT_EQ(plain.Standard().hour, hour);
    }
}

TEST(CompressedYearTests, ARateThatIsNotANumberDoesNotTakeTheCalendarWithIt)
{
    // A settings file, a console command or a save from a future build can put anything in this
    // field, and a NaN in it would make every date in the game NaN with no way back. The clock
    // falls back to a realistic calendar instead, which is wrong in a way a person can see.
    for (const double rate : {0.0, -1.0, std::nan(""), -std::numeric_limits<double>::infinity()})
    {
        SimClock clock;
        clock.calendarDaysPerSimDay = rate;
        clock.epochSeconds = 86400.0 * 3.0 + 3600.0;
        EXPECT_TRUE(std::isfinite(clock.CalendarDays())) << rate;
        EXPECT_EQ(clock.DayIndex(), 3) << rate;
        EXPECT_EQ(clock.Standard().day, 4) << rate;
        EXPECT_EQ(clock.Standard().hour, 1) << rate;
    }
    // An infinite rate is refused the same way, and is worth its own line because it is the one a
    // "make the year go faster" slider reaches by division.
    SimClock huge;
    huge.calendarDaysPerSimDay = std::numeric_limits<double>::infinity();
    huge.epochSeconds = 86400.0;
    EXPECT_TRUE(std::isfinite(huge.CalendarDays()));
    EXPECT_EQ(huge.DayIndex(), 1);
}
