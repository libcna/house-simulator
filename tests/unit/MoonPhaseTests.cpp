// SPDX-License-Identifier: MIT
//
// `HOUSE-01602`. The comparison table is the USNO's independently published illuminated fraction
// and waxing/waning phase at local noon, cached by tools/ci/moonphases_table.py.
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

#include "cnahouse/environment/MoonModel.hpp"

namespace
{
    using cnahouse::environment::CivilTime;
    using cnahouse::environment::DaysSinceJ2000;
    using cnahouse::environment::MoonPhase;
    using cnahouse::environment::MoonPhaseAt;
    using cnahouse::environment::MoonPhaseFor;
    using cnahouse::environment::MoonPhaseFromPositions;
    using cnahouse::environment::MoonPosition;
    using cnahouse::environment::SimClock;
    using cnahouse::environment::SunObserver;
    using cnahouse::environment::SunPosition;

    constexpr const char* kTable = "tests/unit/reference/moonphases.usno.txt";
    constexpr double kTolerance = 0.02;

    struct Row
    {
        int year = 0;
        int month = 0;
        int day = 0;
        double illuminated = 0.0;
        bool waxing = false;
        double phase = 0.0;
    };

    std::vector<Row> ReadTable()
    {
        std::vector<Row> rows;
        std::ifstream file(kTable);
        std::string line;
        while (std::getline(file, line))
        {
            if (line.empty() || line.front() == '#')
            {
                continue;
            }
            int waxing = 0;
            Row row;
            std::istringstream fields(line);
            fields >> row.year >> row.month >> row.day >> row.illuminated >> waxing >> row.phase;
            if (fields)
            {
                row.waxing = waxing != 0;
                rows.push_back(row);
            }
        }
        return rows;
    }

    double CircularDistance(double a, double b)
    {
        const double direct = std::abs(a - b);
        return std::min(direct, 1.0 - direct);
    }

    double J2000AtLocalNoon(const Row& row)
    {
        // The fixture is fixed UTC-5 standard time, so its documented local noon is 17:00 UT.
        CivilTime utc;
        utc.year = row.year;
        utc.month = row.month;
        utc.day = row.day;
        utc.hour = 17;
        return DaysSinceJ2000(utc);
    }

    std::string Describe(const Row& row)
    {
        char text[32];
        std::snprintf(text, sizeof(text), "%04d-%02d-%02d", row.year, row.month, row.day);
        return text;
    }
} // namespace

TEST(MoonPhaseTests, CardinalDirectionsProduceTheFourCardinalPhases)
{
    SunPosition sun;
    sun.eclipticLongitudeDeg = 0.0;
    MoonPosition moon;

    moon.eclipticLongitudeDeg = 0.0;
    MoonPhase result = MoonPhaseFromPositions(moon, sun);
    EXPECT_DOUBLE_EQ(result.elongationDeg, 0.0);
    EXPECT_DOUBLE_EQ(result.illuminatedFraction, 0.0);
    EXPECT_TRUE(result.waxing);
    EXPECT_DOUBLE_EQ(result.phase, 0.0);

    moon.eclipticLongitudeDeg = 90.0;
    result = MoonPhaseFromPositions(moon, sun);
    EXPECT_NEAR(result.elongationDeg, 90.0, 1e-12);
    EXPECT_NEAR(result.illuminatedFraction, 0.5, 1e-12);
    EXPECT_TRUE(result.waxing);
    EXPECT_NEAR(result.phase, 0.25, 1e-12);

    moon.eclipticLongitudeDeg = 180.0;
    result = MoonPhaseFromPositions(moon, sun);
    EXPECT_NEAR(result.illuminatedFraction, 1.0, 1e-12);
    EXPECT_FALSE(result.waxing);
    EXPECT_NEAR(result.phase, 0.5, 1e-12);

    moon.eclipticLongitudeDeg = 270.0;
    result = MoonPhaseFromPositions(moon, sun);
    EXPECT_NEAR(result.illuminatedFraction, 0.5, 1e-12);
    EXPECT_FALSE(result.waxing);
    EXPECT_NEAR(result.phase, 0.75, 1e-12);
}

TEST(MoonPhaseTests, ElongationUsesTheDirectionsLatitudeAndNotOnlyLongitude)
{
    SunPosition sun;
    MoonPosition moon;
    moon.eclipticLatitudeDeg = 60.0;
    const MoonPhase result = MoonPhaseFromPositions(moon, sun);
    EXPECT_NEAR(result.elongationDeg, 60.0, 1e-12);
    EXPECT_NEAR(result.illuminatedFraction, 0.25, 1e-12);
}

TEST(MoonPhaseTests, PhaseIsContinuousAcrossNewMoonAsACircularQuantity)
{
    SunPosition sun;
    MoonPosition moon;
    moon.eclipticLongitudeDeg = 359.99;
    const MoonPhase before = MoonPhaseFromPositions(moon, sun);
    moon.eclipticLongitudeDeg = 0.01;
    const MoonPhase after = MoonPhaseFromPositions(moon, sun);
    EXPECT_FALSE(before.waxing);
    EXPECT_TRUE(after.waxing);
    EXPECT_LT(CircularDistance(before.phase, after.phase), 1e-6);
    EXPECT_GE(before.phase, 0.0);
    EXPECT_LT(before.phase, 1.0);
    EXPECT_GE(after.phase, 0.0);
    EXPECT_LT(after.phase, 1.0);
}

TEST(MoonPhaseTests, SixtyPublishedNoonPhasesAreWithinPointZeroTwo)
{
    ASSERT_TRUE(std::filesystem::exists(kTable));
    const std::vector<Row> rows = ReadTable();
    ASSERT_EQ(rows.size(), 60U);
    int waxingRows = 0;
    int waningRows = 0;
    int directionChecks = 0;
    double worstPhaseError = 0.0;
    double worstIlluminationError = 0.0;
    double phaseErrorSum = 0.0;
    std::string worstDay;

    for (const Row& row : rows)
    {
        const MoonPhase model = MoonPhaseAt(J2000AtLocalNoon(row), SunObserver{});
        const double phaseError = CircularDistance(model.phase, row.phase);
        const double illuminationError = std::abs(model.illuminatedFraction - row.illuminated);
        EXPECT_LT(phaseError, kTolerance)
            << Describe(row) << ": model phase " << model.phase << ", USNO-derived " << row.phase;
        EXPECT_LT(illuminationError, kTolerance) << Describe(row) << ": model illumination "
                                                 << model.illuminatedFraction << ", USNO " << row.illuminated;
        // At an integer-published 0% or 100%, the primary phase can occur on either side of noon;
        // the six-term model and the USNO full ephemeris may put that instant hours apart while
        // agreeing on phase to three decimals. Waxing/waning is observable everywhere else.
        if (row.illuminated > 0.01 && row.illuminated < 0.99)
        {
            EXPECT_EQ(model.waxing, row.waxing) << Describe(row);
            ++directionChecks;
        }
        waxingRows += row.waxing ? 1 : 0;
        waningRows += row.waxing ? 0 : 1;
        phaseErrorSum += phaseError;
        if (phaseError > worstPhaseError)
        {
            worstPhaseError = phaseError;
            worstDay = Describe(row);
        }
        worstIlluminationError = std::max(worstIlluminationError, illuminationError);
    }
    EXPECT_GT(waxingRows, 20);
    EXPECT_GT(waningRows, 20);
    EXPECT_GT(directionChecks, 40);
    std::printf("  60 published noon phases; mean error %.4f; worst %.4f on %s; illum worst %.4f\n",
                phaseErrorSum / static_cast<double>(rows.size()),
                worstPhaseError,
                worstDay.c_str(),
                worstIlluminationError);
}

TEST(MoonPhaseTests, ClockConvenienceUsesTheCompressedCivilInstantAndBadInputFailsClosed)
{
    SimClock clock;
    clock.epochSeconds = 7654321.0;
    clock.calendarDaysPerSimDay = 24.0;
    const MoonPhase fromClock = MoonPhaseFor(clock);
    const MoonPhase direct =
        MoonPhaseAt(cnahouse::environment::DaysSinceJ2000ForEpochSeconds(clock.CivilEpochSeconds(),
                                                                         clock.utcOffsetMinutes),
                    SunObserver{clock.latitudeDeg, clock.longitudeDeg, clock.utcOffsetMinutes});
    EXPECT_DOUBLE_EQ(fromClock.phase, direct.phase);
    EXPECT_DOUBLE_EQ(fromClock.illuminatedFraction, direct.illuminatedFraction);

    MoonPosition moon;
    moon.eclipticLongitudeDeg = std::numeric_limits<double>::quiet_NaN();
    const MoonPhase bad = MoonPhaseFromPositions(moon, SunPosition{});
    EXPECT_DOUBLE_EQ(bad.phase, 0.0);
    EXPECT_DOUBLE_EQ(bad.illuminatedFraction, 0.0);
    EXPECT_DOUBLE_EQ(MoonPhaseAt(std::numeric_limits<double>::infinity(), SunObserver{}).phase, 0.0);
}
