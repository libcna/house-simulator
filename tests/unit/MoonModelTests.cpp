// SPDX-License-Identifier: MIT
//
// `HOUSE-01601`. Sixty moonrise times published by the United States Naval Observatory against
// §33.1's six-term lunar model. The USNO uses a full ephemeris; the committed table is fetched once
// by `tools/ci/moontimes_table.py`, not calculated from this implementation.
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
    using cnahouse::environment::kMoonRefractedHorizonDeg;
    using cnahouse::environment::MoonDay;
    using cnahouse::environment::MoonDayFor;
    using cnahouse::environment::MoonPosition;
    using cnahouse::environment::MoonPositionAt;
    using cnahouse::environment::MoonPositionFor;
    using cnahouse::environment::SimClock;
    using cnahouse::environment::SunObserver;

    constexpr const char* kTable = "tests/unit/reference/moontimes.usno.txt";
    constexpr double kToleranceMinutes = 8.0;

    struct Row
    {
        int year = 0;
        int month = 0;
        int day = 0;
        int rise = 0;
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
            std::istringstream fields(line);
            Row row;
            fields >> row.year >> row.month >> row.day >> row.rise;
            if (fields)
            {
                rows.push_back(row);
            }
        }
        return rows;
    }

    CivilTime DateOf(const Row& row)
    {
        CivilTime date;
        date.year = row.year;
        date.month = row.month;
        date.day = row.day;
        return date;
    }

    std::string Describe(const Row& row)
    {
        char text[32];
        std::snprintf(text, sizeof(text), "%04d-%02d-%02d", row.year, row.month, row.day);
        return text;
    }
} // namespace

TEST(MoonModelTests, ReferenceTableContainsExactlySixtyPublishedMoonrisesAcrossTheYear)
{
    ASSERT_TRUE(std::filesystem::exists(kTable));
    const std::vector<Row> rows = ReadTable();
    ASSERT_EQ(rows.size(), 60U);
    EXPECT_EQ(rows.front().year, 2031);
    EXPECT_EQ(rows.back().year, 2032);

    int earliest = 24 * 60;
    int latest = 0;
    for (const Row& row : rows)
    {
        earliest = std::min(earliest, row.rise);
        latest = std::max(latest, row.rise);
    }
    EXPECT_LT(earliest, 3 * 60);
    EXPECT_GT(latest, 21 * 60);
}

TEST(MoonModelTests, EveryMoonriseIsWithinEightMinutesOfTheUsnos)
{
    const std::vector<Row> rows = ReadTable();
    ASSERT_EQ(rows.size(), 60U);
    const SunObserver observer{};
    double worstError = 0.0;
    double absoluteErrorSum = 0.0;
    std::string worstDay;

    for (const Row& row : rows)
    {
        const MoonDay model = MoonDayFor(DateOf(row), observer);
        ASSERT_TRUE(model.rise.occurs) << Describe(row) << ": the compact model predicts no rise";
        const double error = model.rise.minutesOfDay - static_cast<double>(row.rise);
        EXPECT_LT(std::abs(error), kToleranceMinutes)
            << Describe(row) << ": model " << model.rise.minutesOfDay << " min, USNO " << row.rise;
        absoluteErrorSum += std::abs(error);
        if (std::abs(error) > std::abs(worstError))
        {
            worstError = error;
            worstDay = Describe(row);
        }
    }
    std::printf("  60 published moonrises; mean |error| %.2f min; worst %+.2f min on %s\n",
                absoluteErrorSum / static_cast<double>(rows.size()),
                worstError,
                worstDay.c_str());
}

TEST(MoonModelTests, PositionOutputsAreFiniteNormalisedAndMoveAgainstTheStars)
{
    CivilTime instant;
    instant.year = 2031;
    instant.month = 1;
    instant.day = 1;
    instant.hour = 12;
    const double start = DaysSinceJ2000(instant);
    const MoonPosition first = MoonPositionAt(start, SunObserver{});
    const MoonPosition next = MoonPositionAt(start + 1.0, SunObserver{});

    EXPECT_TRUE(std::isfinite(first.declinationDeg));
    EXPECT_GE(first.hourAngleDeg, -180.0);
    EXPECT_LT(first.hourAngleDeg, 180.0);
    EXPECT_GE(first.azimuthDeg, 0.0);
    EXPECT_LT(first.azimuthDeg, 360.0);
    EXPECT_GE(first.rightAscensionDeg, 0.0);
    EXPECT_LT(first.rightAscensionDeg, 360.0);
    EXPECT_GE(first.eclipticLongitudeDeg, 0.0);
    EXPECT_LT(first.eclipticLongitudeDeg, 360.0);
    EXPECT_LE(std::abs(first.eclipticLatitudeDeg), 6.0);

    double dailyMotion = next.eclipticLongitudeDeg - first.eclipticLongitudeDeg;
    if (dailyMotion < 0.0)
    {
        dailyMotion += 360.0;
    }
    EXPECT_GT(dailyMotion, 10.0);
    EXPECT_LT(dailyMotion, 16.0);
}

TEST(MoonModelTests, ClockConvenienceUsesTheCompressedCivilInstant)
{
    SimClock clock;
    clock.epochSeconds = 1234567.0;
    clock.calendarDaysPerSimDay = 24.0;
    const MoonPosition fromClock = MoonPositionFor(clock);
    const MoonPosition direct =
        MoonPositionAt(cnahouse::environment::DaysSinceJ2000ForEpochSeconds(clock.CivilEpochSeconds(),
                                                                            clock.utcOffsetMinutes),
                       SunObserver{clock.latitudeDeg, clock.longitudeDeg, clock.utcOffsetMinutes});
    EXPECT_DOUBLE_EQ(fromClock.eclipticLongitudeDeg, direct.eclipticLongitudeDeg);
    EXPECT_DOUBLE_EQ(fromClock.altitudeDeg, direct.altitudeDeg);
}

TEST(MoonModelTests, BadInputsFailClosedAndARealDateHasCrossings)
{
    const double nan = std::numeric_limits<double>::quiet_NaN();
    const MoonPosition badTime = MoonPositionAt(nan, SunObserver{});
    EXPECT_DOUBLE_EQ(badTime.altitudeDeg, 0.0);
    EXPECT_DOUBLE_EQ(badTime.eclipticLongitudeDeg, 0.0);

    SunObserver badObserver;
    badObserver.latitudeDeg = nan;
    EXPECT_DOUBLE_EQ(MoonPositionAt(0.0, badObserver).azimuthDeg, 0.0);

    CivilTime day;
    day.year = 2031;
    day.month = 1;
    day.day = 1;
    const MoonDay crossings = MoonDayFor(day, SunObserver{}, kMoonRefractedHorizonDeg);
    EXPECT_TRUE(crossings.rise.occurs);
    EXPECT_TRUE(crossings.set.occurs);
    EXPECT_FALSE(MoonDayFor(day, SunObserver{}, nan).rise.occurs);
}
