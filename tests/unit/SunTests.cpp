// SPDX-License-Identifier: MIT
//
// `HOUSE-01544`. §35.3's seasonal day-length variation: *"sunrise and sunset drift with the
// declination, so the shortest and longest days are visibly different within one 6-hour year"*.
//
// The task's first criterion is that midsummer and midwinter daylight differ *"by the ANALYTIC
// amount"*, and that word settles how this file is written. The model finds sunrise by bisecting
// `SunPositionAt`'s altitude; the oracle here solves the same geometry in closed form -- the
// sunrise equation, `cos ω₀ = (sin h₀ − sin φ sin δ)/(cos φ cos δ)`, which is the spherical
// right-triangle identity and not a second copy of the model's arithmetic. Where the two agree,
// they agree because the geometry is right and not because the same lines ran twice.
//
// The USNO's own published solstice times are checked as well, because an analytic oracle and a
// numerical model can still share a wrong latitude or a wrong declination amplitude, and a
// published number cannot.
//
// The second criterion is that *"the drift is smooth frame to frame"*, and that one is about
// §35.2b's compression rather than about astronomy: the calendar crosses a whole day every real
// minute of play, so a day length quantised to calendar days would step sixty times an hour.
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "cnahouse/environment/SimClock.hpp"
#include "cnahouse/environment/SunModel.hpp"

namespace
{
    using cnahouse::environment::CivilTime;
    using cnahouse::environment::DaylightMinutes;
    using cnahouse::environment::DaylightMinutesAt;
    using cnahouse::environment::DaylightMinutesFor;
    using cnahouse::environment::kRefractedHorizonDeg;
    using cnahouse::environment::SimClock;
    using cnahouse::environment::SunDayFor;
    using cnahouse::environment::SunObserver;
    using cnahouse::environment::SunPositionAt;

    constexpr double kDegToRad = 0.017453292519943295769236907684886;

    /// @brief The closed-form daylight length, in minutes. **The analytic oracle.**
    ///
    /// From the spherical right triangle whose vertices are the pole, the zenith and the sun at
    /// the threshold altitude: `cos ω₀ = (sin h₀ − sin φ sin δ)/(cos φ cos δ)`, and the sun is
    /// above the threshold for `2ω₀` of the `360°` it turns through in a day. Nothing here is
    /// taken from `SunModel`'s bisection -- only the declination, which is an INPUT to both and
    /// is what the criterion is about.
    ///
    /// Returns 1 440 when the argument leaves `[-1, 1]` upward (midnight sun) and 0 downward.
    [[nodiscard]] double AnalyticDaylightMinutes(double latitudeDeg,
                                                 double declinationDeg,
                                                 double thresholdDeg = kRefractedHorizonDeg)
    {
        const double latitude = latitudeDeg * kDegToRad;
        const double declination = declinationDeg * kDegToRad;
        const double cosOmega =
            (std::sin(thresholdDeg * kDegToRad) - std::sin(latitude) * std::sin(declination)) /
            (std::cos(latitude) * std::cos(declination));
        if (cosOmega <= -1.0)
        {
            return 1440.0;
        }
        if (cosOmega >= 1.0)
        {
            return 0.0;
        }
        const double omegaDeg = std::acos(cosOmega) / kDegToRad;
        return 2.0 * omegaDeg * 4.0; // 4 minutes of clock per degree of rotation.
    }

    /// @brief The model's declination at local apparent noon on a date, which is the declination
    ///        the closed form wants: it treats δ as fixed across the day, and noon is the middle.
    [[nodiscard]] double NoonDeclination(const CivilTime& date, const SunObserver& observer)
    {
        const auto day = SunDayFor(date, observer);
        const double dayStart =
            static_cast<double>(cnahouse::environment::DaysFromCivil(date.year, date.month, date.day) -
                                cnahouse::environment::DaysFromCivil(2000, 1, 1)) -
            static_cast<double>(observer.utcOffsetMinutes) / 1440.0 - 0.5;
        return SunPositionAt(dayStart + day.transit.minutesOfDay / 1440.0, observer).declinationDeg;
    }

    CivilTime Date(int year, int month, int day)
    {
        CivilTime civil;
        civil.year = year;
        civil.month = month;
        civil.day = day;
        return civil;
    }

    struct PublishedDay
    {
        int year = 0;
        int month = 0;
        int day = 0;
        int rise = 0;
        int set = 0;
    };

    /// The USNO table `HOUSE-01561` committed, read again here for its solstice rows.
    std::vector<PublishedDay> ReadPublished()
    {
        std::vector<PublishedDay> rows;
        std::ifstream file("tests/unit/reference/suntimes.usno.txt");
        std::string line;
        while (std::getline(file, line))
        {
            if (line.empty() || line.front() == '#')
            {
                continue;
            }
            std::istringstream fields(line);
            PublishedDay row;
            int transit = 0;
            fields >> row.year >> row.month >> row.day >> row.rise >> row.set >> transit;
            if (fields)
            {
                rows.push_back(row);
            }
        }
        return rows;
    }

} // namespace

TEST(SunTests, SeasonalDayLength)
{
    // The task's criterion, in its own terms: *"at the configured latitude, midsummer and midwinter
    // daylight lengths differ by the analytic amount within 2 simulated minutes"*.
    const SunObserver observer;
    const CivilTime midsummer = Date(2031, 6, 21);
    const CivilTime midwinter = Date(2031, 12, 21);

    const double modelSummer = DaylightMinutes(SunDayFor(midsummer, observer));
    const double modelWinter = DaylightMinutes(SunDayFor(midwinter, observer));
    const double modelDifference = modelSummer - modelWinter;

    const double analyticSummer =
        AnalyticDaylightMinutes(observer.latitudeDeg, NoonDeclination(midsummer, observer));
    const double analyticWinter =
        AnalyticDaylightMinutes(observer.latitudeDeg, NoonDeclination(midwinter, observer));
    const double analyticDifference = analyticSummer - analyticWinter;

    EXPECT_NEAR(modelSummer, analyticSummer, 2.0) << "midsummer daylight";
    EXPECT_NEAR(modelWinter, analyticWinter, 2.0) << "midwinter daylight";
    EXPECT_NEAR(modelDifference, analyticDifference, 2.0)
        << "the seasonal swing: model " << modelDifference << " min, analytic " << analyticDifference
        << " min";

    // ...and the same claim against the published times, which share no arithmetic with either.
    const std::vector<PublishedDay> published = ReadPublished();
    ASSERT_FALSE(published.empty()) << "tests/unit/reference/suntimes.usno.txt";
    double publishedSummer = 0.0;
    double publishedWinter = 0.0;
    for (const PublishedDay& row : published)
    {
        if (row.year == 2031 && row.month == 6 && row.day == 21)
        {
            publishedSummer = static_cast<double>(row.set - row.rise);
        }
        if (row.year == 2031 && row.month == 12 && row.day == 21)
        {
            publishedWinter = static_cast<double>(row.set - row.rise);
        }
    }
    ASSERT_GT(publishedSummer, 0.0) << "the table has no 2031 June solstice";
    ASSERT_GT(publishedWinter, 0.0) << "the table has no 2031 December solstice";
    EXPECT_NEAR(modelDifference, publishedSummer - publishedWinter, 2.0)
        << "the swing against the USNO: model " << modelDifference << " min, published "
        << publishedSummer - publishedWinter << " min";

    std::printf("  midsummer %.1f min, midwinter %.1f min, swing %.1f min\n"
                "  analytic swing %.1f min, USNO swing %.1f min\n",
                modelSummer,
                modelWinter,
                modelDifference,
                analyticDifference,
                publishedSummer - publishedWinter);

    // §32.1's stated range, as a sanity bound on the absolute numbers rather than the difference:
    // *"day length runs from 9h17m to 15h03m"*.
    EXPECT_NEAR(modelWinter, 9.0 * 60.0 + 17.0, 5.0);
    EXPECT_NEAR(modelSummer, 15.0 * 60.0 + 3.0, 5.0);
}

TEST(SunTests, TheAnalyticOracleAgreesWithTheModelOnEveryDayOfTheYear)
{
    // Two solstices could agree by luck. The whole year cannot, and the shoulders -- where the
    // day length changes fastest -- are where an error in the equation of time or the declination
    // amplitude actually shows.
    const SunObserver observer;
    double worst = 0.0;
    std::string worstDay;
    for (int dayOfYear = 0; dayOfYear < 365; ++dayOfYear)
    {
        const CivilTime date = cnahouse::environment::CivilFromDays(
            cnahouse::environment::DaysFromCivil(2031, 1, 1) + dayOfYear);
        const double model = DaylightMinutes(SunDayFor(date, observer));
        const double analytic =
            AnalyticDaylightMinutes(observer.latitudeDeg, NoonDeclination(date, observer));
        if (std::abs(model - analytic) > std::abs(worst))
        {
            worst = model - analytic;
            char text[32];
            std::snprintf(text, sizeof(text), "%04d-%02d-%02d", date.year, date.month, date.day);
            worstDay = text;
        }
    }
    // The two are not identical and should not be: the closed form holds δ fixed across the day,
    // and δ actually moves by up to 0.4° between one sunrise and the next sunset. That is worth
    // about half a minute of day length near the equinoxes.
    std::printf("  worst model-analytic disagreement over 2031: %+.2f min (%s)\n", worst, worstDay.c_str());
    EXPECT_LT(std::abs(worst), 2.0);
}

TEST(SunTests, TheDayLengthDriftsSmoothlyAcrossACompressedYearRatherThanSteppingDaily)
{
    // The task's second criterion. §35.2b's compression means the calendar crosses a whole day
    // every real minute of play, so `SunDayFor`'s per-DATE answer would step by up to three
    // minutes sixty times an hour. `DaylightMinutesAt` takes the fractional calendar position for
    // exactly this reason, and this is the test that would fail if it ever stopped.
    const SunObserver observer;
    SimClock clock;
    clock.SetCalendar(0.0);

    // One frame at 60 fps is 1 simulated minute, which under the default 24× compression is
    // 1/60 of a calendar day. A year is 365 real minutes, so this walks a year in frame steps.
    constexpr double kCalendarDaysPerFrame = 1.0 / 60.0;
    double worstStep = 0.0;
    double worstAt = 0.0;
    double previous = DaylightMinutesAt(0.0, observer);
    for (int frame = 1; frame <= 365 * 60; ++frame)
    {
        const double position = static_cast<double>(frame) * kCalendarDaysPerFrame;
        const double current = DaylightMinutesAt(position, observer);
        const double step = std::abs(current - previous);
        if (step > worstStep)
        {
            worstStep = step;
            worstAt = position;
        }
        previous = current;
    }
    // The day length changes by at most about 3 minutes a calendar day at the equinoxes, so a
    // sixtieth of a day may move it by about 0.05 minutes. A quantised implementation would show
    // 3 minutes here, which is sixty times the bound.
    EXPECT_LT(worstStep, 0.15) << "the worst frame-to-frame step is at calendar day " << worstAt;
    std::printf("  worst frame-to-frame day-length step over a compressed year: %.4f min (day %.1f)\n",
                worstStep,
                worstAt);
}

TEST(SunTests, TheContinuousDayLengthMeetsThePerDateOneAtEveryMidnight)
{
    // Interpolation has to be anchored on the thing it interpolates, or it is a different answer
    // that happens to be smooth.
    const SunObserver observer;
    for (int wholeDay : {0, 1, 79, 172, 265, 355, 364})
    {
        const CivilTime date =
            cnahouse::environment::CivilFromDays(cnahouse::environment::DaysFromCivil(2031, 1, 1) + wholeDay);
        EXPECT_NEAR(DaylightMinutesAt(static_cast<double>(wholeDay), observer),
                    DaylightMinutes(SunDayFor(date, observer)),
                    1e-9)
            << "calendar day " << wholeDay;
    }
}

TEST(SunTests, TheShortestAndLongestDaysOfACompressedYearAreVisiblyDifferent)
{
    // The task's own words: *"so the shortest and longest days are visibly different within one
    // 6-hour year"*. A year is 365 real minutes at the default rate; this walks it at one sample a
    // real minute and asks what a player would actually have seen.
    const SunObserver observer;
    double shortest = 1440.0;
    double longest = 0.0;
    for (int realMinute = 0; realMinute < 365; ++realMinute)
    {
        const double length = DaylightMinutesAt(static_cast<double>(realMinute), observer);
        shortest = std::min(shortest, length);
        longest = std::max(longest, length);
    }
    EXPECT_GT(longest - shortest, 5.0 * 60.0)
        << "a player sitting through one 365-real-minute year saw the day length change by "
        << longest - shortest << " minutes";
    std::printf("  over one 365-real-minute year: %.1f min to %.1f min of daylight\n", shortest, longest);
}

TEST(SunTests, TheClockOverloadReadsTheCompressedCalendarAndNotTheSimulatedDayCount)
{
    // Same trap as `SunModelTests`: a day length read off `epochSeconds` instead of
    // `CalendarDays()` would take 365 simulated days -- 146 real hours -- to complete a cycle.
    SimClock clock;
    clock.SetCalendar(0.0);
    const double atEpoch = DaylightMinutesFor(clock);
    clock.SetCalendar(182.5);
    const double halfAYearOn = DaylightMinutesFor(clock);
    EXPECT_GT(std::abs(halfAYearOn - atEpoch), 4.0 * 60.0)
        << "half a compressed year changed the daylight by only " << halfAYearOn - atEpoch << " minutes";
}

TEST(SunTests, MidnightSunIsAFullDayOfDaylightAndPolarNightIsNone)
{
    // The degenerate answers `DaylightMinutes` exists to give. `set − rise` is 0 for BOTH of them,
    // which is why leaving this to the caller would have been wrong in the one place nobody looks.
    const SunObserver arctic{70.0, 25.0, 120};
    EXPECT_DOUBLE_EQ(DaylightMinutes(SunDayFor(Date(2031, 6, 21), arctic)), 1440.0);
    EXPECT_DOUBLE_EQ(DaylightMinutes(SunDayFor(Date(2031, 12, 21), arctic)), 0.0);

    // The analytic oracle says the same thing, from the other direction: the argument to `acos`
    // leaves [-1, 1].
    EXPECT_DOUBLE_EQ(AnalyticDaylightMinutes(70.0, 23.44), 1440.0);
    EXPECT_DOUBLE_EQ(AnalyticDaylightMinutes(70.0, -23.44), 0.0);
}

TEST(SunTests, AtTheEquatorTheYearBarelyMovesTheDayLength)
{
    // The seasonal swing is `f(latitude)` and vanishes at the equator -- the control case for the
    // whole claim. §32.1's Equator preset exists for this.
    const SunObserver equator = cnahouse::environment::EquatorObserver();
    double shortest = 1440.0;
    double longest = 0.0;
    for (int dayOfYear = 0; dayOfYear < 365; ++dayOfYear)
    {
        const double length = DaylightMinutesAt(static_cast<double>(dayOfYear), equator);
        shortest = std::min(shortest, length);
        longest = std::max(longest, length);
    }
    EXPECT_LT(longest - shortest, 2.0)
        << "the equator's day length swung by " << longest - shortest << " minutes over a year";
    // ...while §33's latitude swings by nearly six hours, which is the contrast the criterion is
    // really about.
    const SunObserver house;
    EXPECT_GT(DaylightMinutesAt(171.0, house) - DaylightMinutesAt(354.0, house), 5.0 * 60.0);
}
