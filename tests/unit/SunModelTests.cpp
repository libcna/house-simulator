// SPDX-License-Identifier: MIT
//
// `HOUSE-01561`. §32.1's solar-position model, against **390 rise and set times this project did
// not compute**.
//
// The `verify` line asks for *"200 published sunrise/sunset times, ± 3 minutes"*, and the word that
// does the work is PUBLISHED. §32.1 is a simplified NOAA-family algorithm, and a NOAA algorithm
// checked against a second NOAA algorithm written by the same hand proves the arithmetic is
// self-consistent and says nothing about whether it is right -- which is the exact mistake
// `HOUSE-01532` spent a whole task removing from the calendar.
//
// So `tests/unit/reference/suntimes.usno.txt` comes from the **United States Naval Observatory,
// Astronomical Applications Department** -- the institution that publishes the Astronomical
// Almanac -- retrieved once by `tools/ci/suntimes_table.py` and committed so CI needs no network.
// Their times come from a full ephemeris. Ours come from about forty flops. They are not the same
// computation and are not expected to agree exactly; the claim is that they agree to within
// minutes, and this file is where that claim is made.
//
// **Sunrise here is solved from the position model and from nothing else**: the crossing of
// `kRefractedHorizonDeg` is bracketed and bisected on `SunPositionAt`'s own altitude. A closed-form
// hour-angle solution would be a second approximation, and then a disagreement with the USNO could
// not be attributed to either one.
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <limits>
#include <sstream>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "cnahouse/environment/SimClock.hpp"
#include "cnahouse/environment/SunModel.hpp"

namespace
{
    using cnahouse::environment::CivilTime;
    using cnahouse::environment::DaysSinceJ2000;
    using cnahouse::environment::DaysSinceJ2000ForEpochSeconds;
    using cnahouse::environment::kAstronomicalTwilightDeg;
    using cnahouse::environment::kCivilTwilightDeg;
    using cnahouse::environment::kRefractedHorizonDeg;
    using cnahouse::environment::SimClock;
    using cnahouse::environment::SunDay;
    using cnahouse::environment::SunDayFor;
    using cnahouse::environment::SunObserver;
    using cnahouse::environment::SunPosition;
    using cnahouse::environment::SunPositionAt;
    using cnahouse::environment::SunPositionFor;

    constexpr const char* kTable = "tests/unit/reference/suntimes.usno.txt";

    /// §32.1's verify line. Three minutes against a full ephemeris, for a forty-flop model.
    constexpr double kToleranceMinutes = 3.0;

    struct Row
    {
        int year = 0;
        int month = 0;
        int day = 0;
        /// @brief Minutes after local standard midnight. The USNO publishes whole minutes.
        int rise = 0;
        int set = 0;
        int transit = 0;
    };

    std::vector<Row> ReadTable()
    {
        std::vector<Row> rows;
        std::ifstream file(kTable);
        if (!file)
        {
            return rows;
        }
        std::string line;
        while (std::getline(file, line))
        {
            if (line.empty() || line.front() == '#')
            {
                continue;
            }
            std::istringstream fields(line);
            Row row;
            fields >> row.year >> row.month >> row.day >> row.rise >> row.set >> row.transit;
            if (!fields)
            {
                continue;
            }
            rows.push_back(row);
        }
        return rows;
    }

    std::string Describe(const Row& row)
    {
        char text[64];
        std::snprintf(text, sizeof(text), "%04d-%02d-%02d", row.year, row.month, row.day);
        return text;
    }

    CivilTime DateOf(const Row& row)
    {
        CivilTime date;
        date.year = row.year;
        date.month = row.month;
        date.day = row.day;
        return date;
    }

    /// §33's location, which is what the USNO table was requested for.
    SunObserver DefaultObserver()
    {
        return SunObserver{};
    }

} // namespace

TEST(SunModelTests, TheTableIsThereAndCarriesTheCriterionsTwoHundredTimes)
{
    // A missing table would make every claim in this file vacuously true, which is the one way it
    // could pass while proving nothing at all.
    ASSERT_TRUE(std::filesystem::exists(kTable))
        << kTable << " is missing; tests run from the repository root and this file is committed";
    const std::vector<Row> rows = ReadTable();
    ASSERT_FALSE(rows.empty());
    EXPECT_GE(rows.size() * 2U, 200U)
        << "the criterion is 200 published rise/set times and this table has " << rows.size() * 2U;

    // ...and they are not 195 copies of one day. A table with no seasonal spread would pass a
    // model that returned a constant.
    int shortestDay = 24 * 60;
    int longestDay = 0;
    int earliestYear = rows.front().year;
    int latestYear = rows.front().year;
    for (const Row& row : rows)
    {
        const int length = row.set - row.rise;
        shortestDay = std::min(shortestDay, length);
        longestDay = std::max(longestDay, length);
        earliestYear = std::min(earliestYear, row.year);
        latestYear = std::max(latestYear, row.year);
    }
    // §32.1: *"day length runs from 9h17m to 15h03m"*. The published extremes are what that claim
    // was made about, so they belong here rather than in a comment.
    EXPECT_LT(shortestDay, 10 * 60) << "the table has no winter";
    EXPECT_GT(longestDay, 14 * 60) << "the table has no summer";
    EXPECT_GE(latestYear - earliestYear, 10)
        << "every row is from one year, so an error growing with days-since-J2000 is invisible";
    std::printf("  %zu day(s), %zu published rise/set times, %d-%d, day length %d-%d min\n",
                rows.size(),
                rows.size() * 2U,
                earliestYear,
                latestYear,
                shortestDay,
                longestDay);
}

TEST(SunModelTests, EverySunriseAndSunsetIsWithinThreeMinutesOfTheUsnos)
{
    const std::vector<Row> rows = ReadTable();
    ASSERT_FALSE(rows.empty()) << kTable;
    const SunObserver observer = DefaultObserver();

    int checked = 0;
    double worstRise = 0.0;
    double worstSet = 0.0;
    std::string worstRiseDay;
    std::string worstSetDay;
    double sumAbs = 0.0;

    for (const Row& row : rows)
    {
        const SunDay day = SunDayFor(DateOf(row), observer);
        ASSERT_TRUE(day.rise.occurs) << Describe(row) << ": the model says the sun does not rise";
        ASSERT_TRUE(day.set.occurs) << Describe(row) << ": the model says the sun does not set";

        // The USNO publishes whole minutes, so a model answer of 06:04:29 and a published 06:04
        // are the same answer. Comparing the model's fractional minute against the published
        // integer is the honest way round: it never credits the model with a rounding it did not
        // earn, and it costs at most half a minute of the three.
        const double riseError = day.rise.minutesOfDay - static_cast<double>(row.rise);
        const double setError = day.set.minutesOfDay - static_cast<double>(row.set);

        EXPECT_LT(std::abs(riseError), kToleranceMinutes)
            << Describe(row) << ": sunrise -- model " << day.rise.minutesOfDay << " min, USNO " << row.rise
            << " min";
        EXPECT_LT(std::abs(setError), kToleranceMinutes)
            << Describe(row) << ": sunset -- model " << day.set.minutesOfDay << " min, USNO " << row.set
            << " min";

        if (std::abs(riseError) > std::abs(worstRise))
        {
            worstRise = riseError;
            worstRiseDay = Describe(row);
        }
        if (std::abs(setError) > std::abs(worstSet))
        {
            worstSet = setError;
            worstSetDay = Describe(row);
        }
        sumAbs += std::abs(riseError) + std::abs(setError);
        checked += 2;
    }

    std::printf("  %d published time(s) checked against the USNO\n"
                "  worst sunrise %+.2f min (%s), worst sunset %+.2f min (%s), mean |error| %.2f min\n"
                "  tolerance %.1f min\n",
                checked,
                worstRise,
                worstRiseDay.c_str(),
                worstSet,
                worstSetDay.c_str(),
                sumAbs / static_cast<double>(checked),
                kToleranceMinutes);
    EXPECT_GE(checked, 200) << "the criterion is 200 published times";
}

TEST(SunModelTests, EveryUpperTransitIsWithinThreeMinutesOfTheUsnos)
{
    // The transit is the equation of time on its own -- declination cancels out of it entirely --
    // so a model that got the right rise and set by two errors cancelling fails here.
    const std::vector<Row> rows = ReadTable();
    ASSERT_FALSE(rows.empty()) << kTable;
    const SunObserver observer = DefaultObserver();
    double worst = 0.0;
    std::string worstDay;
    for (const Row& row : rows)
    {
        const SunDay day = SunDayFor(DateOf(row), observer);
        ASSERT_TRUE(day.transit.occurs) << Describe(row);
        const double error = day.transit.minutesOfDay - static_cast<double>(row.transit);
        EXPECT_LT(std::abs(error), kToleranceMinutes)
            << Describe(row) << ": transit -- model " << day.transit.minutesOfDay << " min, USNO "
            << row.transit << " min";
        if (std::abs(error) > std::abs(worst))
        {
            worst = error;
            worstDay = Describe(row);
        }
    }
    std::printf("  worst transit %+.2f min (%s)\n", worst, worstDay.c_str());
}

TEST(SunModelTests, TheNoonAltitudeRunsBetweenTheTwoFiguresSectionThirtyTwoStates)
{
    // §32.1: *"the noon altitude runs from 26.4° to 73.4°"*. That is 90 − latitude ∓ obliquity and
    // is an independent consequence of the model, checked here because a declination with the wrong
    // amplitude would still give plausible sunrise times near the equinoxes.
    const SunObserver observer = DefaultObserver();
    double lowest = 90.0;
    double highest = -90.0;
    CivilTime date;
    date.year = 2031;
    for (int month = 1; month <= 12; ++month)
    {
        for (int day = 1; day <= 28; ++day)
        {
            date.month = month;
            date.day = day;
            const double altitude = SunDayFor(date, observer).transitAltitudeDeg;
            lowest = std::min(lowest, altitude);
            highest = std::max(highest, altitude);
        }
    }
    EXPECT_NEAR(lowest, 26.4, 0.4) << "the winter noon altitude";
    EXPECT_NEAR(highest, 73.4, 0.4) << "the summer noon altitude";
    std::printf("  noon altitude over 2031: %.2f° to %.2f° (§32.1 says 26.4° to 73.4°)\n", lowest, highest);
}

TEST(SunModelTests, TheSunRisesNorthOfEastInJuneAndSouthOfEastInDecember)
{
    // §32.1's own stated consequence: *"the sun rises in the north-east in June and the south-east
    // in December"*. Azimuth is the one output the USNO table cannot check, so it is checked
    // against the geometry instead -- at 40° N the solstice rise azimuths are about 58° and 122°.
    const SunObserver observer = DefaultObserver();

    struct Case
    {
        int month;
        int day;
        double expectedAzimuthDeg;
    };

    for (const Case& probe : {Case{6, 21, 58.0}, Case{12, 21, 122.0}, Case{3, 20, 90.0}})
    {
        CivilTime date;
        date.year = 2031;
        date.month = probe.month;
        date.day = probe.day;
        const SunDay day = SunDayFor(date, observer);
        ASSERT_TRUE(day.rise.occurs);
        const double dayStart =
            DaysSinceJ2000ForEpochSeconds(0.0, observer.utcOffsetMinutes) +
            static_cast<double>(cnahouse::environment::DaysFromCivil(date.year, date.month, date.day) -
                                cnahouse::environment::DaysFromCivil(2031, 1, 1));
        const SunPosition atRise = SunPositionAt(dayStart + day.rise.minutesOfDay / 1440.0, observer);
        // Refraction is why this is not exactly the geometric figure: the sun is 0.83° below the
        // horizon at the published moment, so its azimuth is a degree or so short of the extreme.
        EXPECT_NEAR(atRise.azimuthDeg, probe.expectedAzimuthDeg, 2.5)
            << "2031-" << probe.month << "-" << probe.day << " rise azimuth";
        // Whatever the exact number, the sun is in the EAST at sunrise, which is the claim a
        // north-clockwise azimuth convention is easiest to get backwards about.
        EXPECT_GT(atRise.azimuthDeg, 0.0);
        EXPECT_LT(atRise.azimuthDeg, 180.0);
    }
}

TEST(SunModelTests, AtNoonTheSunIsDueSouthAndTheHourAngleIsZero)
{
    // At a northern latitude the sun transits to the SOUTH, azimuth 180. Getting the azimuth
    // formula's sign wrong puts it due north and every shadow in the house points the wrong way.
    const SunObserver observer = DefaultObserver();
    CivilTime date;
    date.year = 2031;
    date.month = 6;
    date.day = 21;
    const SunDay day = SunDayFor(date, observer);
    const double dayStart = DaysSinceJ2000ForEpochSeconds(0.0, observer.utcOffsetMinutes) +
                            static_cast<double>(cnahouse::environment::DaysFromCivil(2031, 6, 21) -
                                                cnahouse::environment::DaysFromCivil(2031, 1, 1));
    const SunPosition atNoon = SunPositionAt(dayStart + day.transit.minutesOfDay / 1440.0, observer);
    EXPECT_NEAR(atNoon.azimuthDeg, 180.0, 0.5);
    EXPECT_NEAR(atNoon.hourAngleDeg, 0.0, 0.05) << "the transit is where the hour angle is zero";
    EXPECT_NEAR(atNoon.declinationDeg, 23.44, 0.1) << "the June solstice declination";
}

TEST(SunModelTests, TwilightIsLongerThanDaybreakAndTheThresholdsNest)
{
    // Civil twilight begins before nautical, which begins before astronomical -- an ordering that
    // follows from nothing but the thresholds, and that a sign error in the bisection would break.
    const SunObserver observer = DefaultObserver();
    CivilTime date;
    date.year = 2031;
    date.month = 3;
    date.day = 20;
    const SunDay sunrise = SunDayFor(date, observer, kRefractedHorizonDeg);
    const SunDay civil = SunDayFor(date, observer, kCivilTwilightDeg);
    const SunDay astronomical = SunDayFor(date, observer, kAstronomicalTwilightDeg);
    ASSERT_TRUE(sunrise.rise.occurs);
    ASSERT_TRUE(civil.rise.occurs);
    ASSERT_TRUE(astronomical.rise.occurs);
    EXPECT_LT(astronomical.rise.minutesOfDay, civil.rise.minutesOfDay);
    EXPECT_LT(civil.rise.minutesOfDay, sunrise.rise.minutesOfDay);
    EXPECT_GT(astronomical.set.minutesOfDay, civil.set.minutesOfDay);
    EXPECT_GT(civil.set.minutesOfDay, sunrise.set.minutesOfDay);

    // The USNO published civil twilight for this very date as 05:38 and 18:40 -- 338 and 1 120
    // minutes. It is in the raw cache rather than the table, so it is quoted here as a second
    // anchor rather than read from a file.
    EXPECT_NEAR(civil.rise.minutesOfDay, 338.0, kToleranceMinutes);
    EXPECT_NEAR(civil.set.minutesOfDay, 1120.0, kToleranceMinutes);
}

TEST(SunModelTests, AnAltitudeThresholdTheSunNeverReachesDoesNotOccur)
{
    // **This test was first written against a wrong premise and the model corrected it.** The
    // premise was that at 40° N in June the sun never gets 18° below the horizon, so astronomical
    // twilight would last all night. It does not: the sun's LOWER transit altitude on the June
    // solstice is `latitude + 23.44 - 90`, which at 40.05° N is −26.5°, comfortably past −18°. The
    // latitude where astronomical night first disappears is 90 − 18 − 23.44 = **48.56° N**, and
    // §33's house is eight and a half degrees south of it.
    CivilTime date;
    date.year = 2031;
    date.month = 6;
    date.day = 21;
    const SunDay atTheHouse = SunDayFor(date, DefaultObserver(), kAstronomicalTwilightDeg);
    EXPECT_TRUE(atTheHouse.rise.occurs) << "40° N does have astronomical night in June";
    EXPECT_TRUE(atTheHouse.set.occurs);

    // §32.1's Reykjavik preset is where the branch is actually reachable: 64.13° N puts the lower
    // transit at −2.4°, so the sun sets but never reaches even CIVIL twilight.
    const SunObserver reykjavik = cnahouse::environment::ReykjavikObserver();
    const SunDay iceland = SunDayFor(date, reykjavik, kCivilTwilightDeg);
    EXPECT_FALSE(iceland.rise.occurs) << "the sun reached -6° at Reykjavik on the June solstice";
    EXPECT_FALSE(iceland.set.occurs);
    // ...and the transit still happens, because the sun crosses the meridian every day.
    EXPECT_TRUE(iceland.transit.occurs);
    // The sun does still rise and set there, which is what makes the twilight result the
    // interesting one rather than a degenerate midnight-sun case.
    const SunDay icelandDaylight = SunDayFor(date, reykjavik, kRefractedHorizonDeg);
    EXPECT_TRUE(icelandDaylight.rise.occurs);
    EXPECT_TRUE(icelandDaylight.set.occurs);
}

TEST(SunModelTests, InsideTheArcticCircleInJuneTheSunDoesNotSetAtAll)
{
    // The degenerate case the `occurs` flag exists for. At 70° N the June lower transit is +3.4°,
    // so there is no horizon crossing to find -- and a bisection asked for one anyway would return
    // an arbitrary minute of the day that a caller could not tell from a real sunrise.
    const SunObserver arctic{70.0, 25.0, 120};
    CivilTime date;
    date.year = 2031;
    date.month = 6;
    date.day = 21;
    const SunDay midnightSun = SunDayFor(date, arctic, kRefractedHorizonDeg);
    EXPECT_FALSE(midnightSun.rise.occurs);
    EXPECT_FALSE(midnightSun.set.occurs);
    EXPECT_GT(midnightSun.transitAltitudeDeg, 40.0);

    // ...and the same place in December, where it is the other degenerate case: polar night.
    date.month = 12;
    date.day = 21;
    const SunDay polarNight = SunDayFor(date, arctic, kRefractedHorizonDeg);
    EXPECT_FALSE(polarNight.rise.occurs);
    EXPECT_FALSE(polarNight.set.occurs);
    EXPECT_LT(polarNight.transitAltitudeDeg, 0.0) << "the sun did not clear the horizon all day";
}

TEST(SunModelTests, AtTheEquatorEveryDayIsTwelveHoursLongAllYearRound)
{
    // §32.1's other preset, and a claim that follows from geometry rather than from this model:
    // on the equator the sun is up for half of every rotation whatever the declination. A seasonal
    // term with the wrong SIGN still gives plausible days at 40° N -- summer and winter merely
    // swap -- and cannot hide here, because here the answer is the same either way only if the
    // amplitude is right too.
    const SunObserver equator = cnahouse::environment::EquatorObserver();
    double shortest = 1440.0;
    double longest = 0.0;
    CivilTime date;
    date.year = 2031;
    for (int month = 1; month <= 12; ++month)
    {
        date.month = month;
        date.day = 15;
        const SunDay day = SunDayFor(date, equator, kRefractedHorizonDeg);
        ASSERT_TRUE(day.rise.occurs) << "month " << month;
        ASSERT_TRUE(day.set.occurs) << "month " << month;
        const double length = day.set.minutesOfDay - day.rise.minutesOfDay;
        shortest = std::min(shortest, length);
        longest = std::max(longest, length);
    }
    // Slightly OVER twelve hours every day, and that is correct rather than sloppy: the refracted
    // horizon is 0.83° below the geometric one, which adds about seven minutes at the equator.
    EXPECT_GT(shortest, 12.0 * 60.0);
    EXPECT_LT(longest, 12.0 * 60.0 + 12.0);
    std::printf("  equator day length over 2031: %.1f-%.1f min\n", shortest, longest);
}

TEST(SunModelTests, TheEpochConversionAgreesWithTheCivilOne)
{
    // §35.1's epoch is 2031-01-01T00:00:00 local STANDARD, which is 05:00 UT. Two routes to the
    // same J2000 day, and they have to meet: one goes through the epoch seconds, the other names
    // the universal instant directly.
    CivilTime utc;
    utc.year = 2031;
    utc.month = 1;
    utc.day = 1;
    utc.hour = 5;
    EXPECT_NEAR(DaysSinceJ2000ForEpochSeconds(0.0, -300), DaysSinceJ2000(utc), 1e-9);

    // J2000.0 itself is 2000-01-01 12:00 UT and is day zero by definition.
    CivilTime j2000;
    j2000.year = 2000;
    j2000.month = 1;
    j2000.day = 1;
    j2000.hour = 12;
    EXPECT_NEAR(DaysSinceJ2000(j2000), 0.0, 1e-12);

    // An hour of epoch seconds is an hour of J2000 days, in the right direction.
    EXPECT_NEAR(DaysSinceJ2000ForEpochSeconds(3600.0, -300) - DaysSinceJ2000ForEpochSeconds(0.0, -300),
                1.0 / 24.0,
                1e-12);
}

TEST(SunModelTests, TheClockOverloadReadsTheCompressedCalendarAndNotTheSimulatedDayCount)
{
    // §35.2b's compression is the thing most likely to be wired in wrong here: the DATE runs 24×
    // faster than the clock face, and the declination belongs to the date. A sun that read
    // `epochSeconds` directly would take 365 simulated days to see a year instead of 365 minutes.
    SimClock clock;
    clock.SetCalendar(0.0);
    const SunPosition atEpoch = SunPositionFor(clock);

    // Half a calendar year on. Declination must have swung to the other side of the equator.
    clock.SetCalendar(182.5);
    const SunPosition halfAYearOn = SunPositionFor(clock);
    EXPECT_GT(std::abs(halfAYearOn.declinationDeg - atEpoch.declinationDeg), 20.0)
        << "half a compressed year moved the declination by "
        << halfAYearOn.declinationDeg - atEpoch.declinationDeg
        << "°, so the sun is not reading the compressed calendar";

    // And a clock with the compression off is the same sun at the same civil instant: the two
    // agree wherever the compression is the identity, which is what `CivilEpochSeconds` promises.
    SimClock uncompressed;
    uncompressed.calendarDaysPerSimDay = 1.0;
    uncompressed.epochSeconds = 182.5 * 86400.0;
    const SunPosition plain = SunPositionFor(uncompressed);
    EXPECT_NEAR(plain.declinationDeg, halfAYearOn.declinationDeg, 0.5);
}

TEST(SunModelTests, NonFiniteInputsGiveAZeroedPositionRatherThanANaN)
{
    // A NaN altitude reaching §32.2's colour LUT is a black screen with no error message. The
    // clock can produce one only through a corrupt settings file, which is exactly the case that
    // reaches a player rather than a test.
    const SunObserver observer = DefaultObserver();
    for (const double bad : {std::nan(""), std::numeric_limits<double>::infinity()})
    {
        const SunPosition position = SunPositionAt(bad, observer);
        EXPECT_TRUE(std::isfinite(position.altitudeDeg));
        EXPECT_TRUE(std::isfinite(position.azimuthDeg));
        EXPECT_TRUE(std::isfinite(position.declinationDeg));
    }
    SunObserver broken = observer;
    broken.latitudeDeg = std::nan("");
    EXPECT_TRUE(std::isfinite(SunPositionAt(0.0, broken).altitudeDeg));
}
