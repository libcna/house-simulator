// SPDX-License-Identifier: MIT
//
// `HOUSE-01545`. The live outdoor temperature composes `HOUSE-01535`'s accepted annual and
// diurnal curves with weather's continuously interpolated offset. The annual curve stays analytic
// across the integer labels exposed by `SeasonPhase`, so no season boundary can switch it.
#include <algorithm>
#include <cmath>
#include <limits>

#include <gtest/gtest.h>

#include "cnahouse/environment/Season.hpp"
#include "cnahouse/environment/SimClock.hpp"
#include "cnahouse/environment/Temperature.hpp"

namespace
{
    using cnahouse::environment::DiurnalTemperatureDeltaC;
    using cnahouse::environment::kAnnualMeanC;
    using cnahouse::environment::kAnnualPeriodDays;
    using cnahouse::environment::kDiurnalAmplitudeC;
    using cnahouse::environment::kNewGameCalendarDays;
    using cnahouse::environment::kVernalEquinoxDayOfYear;
    using cnahouse::environment::kWarmestDayOfYear;
    using cnahouse::environment::kWarmestHourOfDay;
    using cnahouse::environment::OutdoorTemperatureC;
    using cnahouse::environment::Season;
    using cnahouse::environment::SeasonAt;
    using cnahouse::environment::SimClock;

} // namespace

TEST(TemperatureTests, TheLiveValueAddsWeatherToTheSeasonalAndDiurnalCurvesExactlyOnce)
{
    EXPECT_DOUBLE_EQ(DiurnalTemperatureDeltaC(kWarmestHourOfDay), kDiurnalAmplitudeC);
    EXPECT_DOUBLE_EQ(OutdoorTemperatureC(kWarmestDayOfYear, kWarmestHourOfDay, -5.0), 24.0);
    EXPECT_DOUBLE_EQ(OutdoorTemperatureC(kWarmestDayOfYear, kWarmestHourOfDay, 2.0), 31.0);
}

TEST(TemperatureTests, TheAnnualMinimumAndMaximumFallInWinterAndSummer)
{
    double minimum = std::numeric_limits<double>::infinity();
    double maximum = -std::numeric_limits<double>::infinity();
    Season minimumSeason = Season::Spring;
    Season maximumSeason = Season::Spring;

    // One sample an hour for a whole year. Holding the clock face at 15:00 isolates the annual
    // location while retaining the same complete sampling grid the live model sees.
    constexpr int kSamples = 365 * 24;
    for (int sample = 0; sample < kSamples; ++sample)
    {
        const auto phase = SeasonAt(static_cast<double>(sample) / kSamples);
        const double dayOfYear = static_cast<double>(kVernalEquinoxDayOfYear) +
                                 static_cast<double>(phase.yearFraction) * kAnnualPeriodDays;
        const double temperature = OutdoorTemperatureC(dayOfYear, kWarmestHourOfDay, 0.0);
        if (temperature < minimum)
        {
            minimum = temperature;
            minimumSeason = static_cast<Season>(phase.primary);
        }
        if (temperature > maximum)
        {
            maximum = temperature;
            maximumSeason = static_cast<Season>(phase.primary);
        }
    }

    EXPECT_EQ(minimumSeason, Season::Winter);
    EXPECT_EQ(maximumSeason, Season::Summer);
    EXPECT_NEAR(minimum, 5.0, 0.01);
    EXPECT_NEAR(maximum, 29.0, 0.01);
}

TEST(TemperatureTests, TheDiurnalMinimumIsPreDawnAndTheMaximumIsMidAfternoon)
{
    double minimum = std::numeric_limits<double>::infinity();
    double maximum = -std::numeric_limits<double>::infinity();
    double minimumHour = -1.0;
    double maximumHour = -1.0;
    const auto phase = SeasonAt(0.1);
    const double dayOfYear = static_cast<double>(kVernalEquinoxDayOfYear) +
                             static_cast<double>(phase.yearFraction) * kAnnualPeriodDays;

    for (int quarterHour = 0; quarterHour < 24 * 4; ++quarterHour)
    {
        const double hour = static_cast<double>(quarterHour) / 4.0;
        const double temperature = OutdoorTemperatureC(dayOfYear, hour, 0.0);
        if (temperature < minimum)
        {
            minimum = temperature;
            minimumHour = hour;
        }
        if (temperature > maximum)
        {
            maximum = temperature;
            maximumHour = hour;
        }
    }

    EXPECT_DOUBLE_EQ(minimumHour, 3.0);
    EXPECT_DOUBLE_EQ(maximumHour, 15.0);
    EXPECT_NEAR(maximum - minimum, 2.0 * kDiurnalAmplitudeC, 1e-9);
}

TEST(TemperatureTests, NoSeasonBoundarySwitchesTheCurve)
{
    constexpr double kEpsilon = 1e-7;
    for (const double boundary : {0.0, 0.25, 0.5, 0.75, 1.0})
    {
        const auto beforePhase = SeasonAt(boundary - kEpsilon);
        const auto afterPhase = SeasonAt(boundary + kEpsilon);
        const double beforeDay = static_cast<double>(kVernalEquinoxDayOfYear) +
                                 static_cast<double>(beforePhase.yearFraction) * kAnnualPeriodDays;
        const double afterDay = static_cast<double>(kVernalEquinoxDayOfYear) +
                                static_cast<double>(afterPhase.yearFraction) * kAnnualPeriodDays;
        const double before = OutdoorTemperatureC(beforeDay, 9.0, -2.0);
        const double after = OutdoorTemperatureC(afterDay, 9.0, -2.0);
        EXPECT_NEAR(before, after, 0.0001) << "temperature switched at year fraction " << boundary;
    }
}

TEST(TemperatureTests, AFullClockYearIsContinuousAndReturnsToItsStartingTemperature)
{
    SimClock clock;
    clock.calendarDaysPerSimDay = 1.0;
    clock.SetCalendar(kNewGameCalendarDays);

    const double start = clock.OutdoorTemperatureC();
    double previous = start;
    double largestHourlyStep = 0.0;
    constexpr int kHours = 365 * 24;
    for (int hour = 0; hour < kHours; ++hour)
    {
        clock.epochSeconds += 3600.0;
        const double current = clock.OutdoorTemperatureC();
        largestHourlyStep = std::max(largestHourlyStep, std::abs(current - previous));
        previous = current;
    }

    EXPECT_LT(largestHourlyStep, 1.7) << "the curve jumped rather than moving through an hour";
    EXPECT_NEAR(previous, start, 0.1) << "one year does not return to the same outdoor temperature";
}

TEST(TemperatureTests, TheClockPublishesTheSingleValueFutureSnowAndStormGatesRead)
{
    SimClock clock;
    clock.SetCalendar(kNewGameCalendarDays + 140.0);
    EXPECT_DOUBLE_EQ(clock.OutdoorTemperatureC(-4.0), clock.OutdoorBaseTemperatureC() - 4.0);
    EXPECT_DOUBLE_EQ(clock.OutdoorTemperatureC(-4.0) - clock.OutdoorTemperatureC(0.0), -4.0);

    // Corrupt weather input falls back to the base instead of poisoning every downstream gate.
    EXPECT_DOUBLE_EQ(clock.OutdoorTemperatureC(std::nan("")), clock.OutdoorTemperatureC(0.0));
    EXPECT_TRUE(std::isfinite(clock.OutdoorTemperatureC(std::numeric_limits<double>::infinity())));
    EXPECT_DOUBLE_EQ(OutdoorTemperatureC(std::nan(""), 12.0, -4.0), kAnnualMeanC - 4.0);
    EXPECT_NE(kAnnualMeanC, clock.OutdoorTemperatureC(-4.0));
}
