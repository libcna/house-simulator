// SPDX-License-Identifier: MIT
//
// `HOUSE-01547`: one uninterrupted compressed real-time year through the production clock.
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>

#include <gtest/gtest.h>

#include "cnahouse/environment/Season.hpp"
#include "cnahouse/environment/SimClock.hpp"

namespace
{
    using cnahouse::environment::kMeanYearDays;
    using cnahouse::environment::kNewGameCalendarDays;
    using cnahouse::environment::Season;
    using cnahouse::environment::SeasonPhase;
    using cnahouse::environment::SimClock;

    constexpr double kCalendarDaysPerSimDay = 24.0;
    constexpr double kLargeTimeScale = 60.0;
    constexpr double kRealSecondsPerCalendarDay =
        cnahouse::environment::kSecondsPerDay / (kLargeTimeScale * kCalendarDaysPerSimDay);

    TEST(SeasonCycleTests, FullYear)
    {
        static_assert(kRealSecondsPerCalendarDay == 60.0);

        SimClock clock;
        clock.calendarDaysPerSimDay = kCalendarDaysPerSimDay;
        clock.timeScale = kLargeTimeScale;
        clock.SetCalendar(kNewGameCalendarDays);

        const std::array<int, 5> expected{
            static_cast<int>(Season::Spring),
            static_cast<int>(Season::Summer),
            static_cast<int>(Season::Autumn),
            static_cast<int>(Season::Winter),
            static_cast<int>(Season::Spring),
        };
        std::array<int, 5> visited{};
        std::size_t visitedCount = 1;
        visited[0] = clock.Season().primary;

        SeasonPhase previous = clock.Season();
        double previousTemperature = clock.OutdoorTemperatureC();
        double largestBlend = static_cast<double>(previous.blend);
        double largestBlendStep = 0.0;
        double largestTemperatureStep = 0.0;

        auto observe = [&]
        {
            const SeasonPhase phase = clock.Season();
            const double temperature = clock.OutdoorTemperatureC();
            ASSERT_TRUE(std::isfinite(temperature));

            largestBlend = std::max(largestBlend, static_cast<double>(phase.blend));
            largestBlendStep =
                std::max(largestBlendStep, std::abs(static_cast<double>(phase.blend - previous.blend)));
            largestTemperatureStep =
                std::max(largestTemperatureStep, std::abs(temperature - previousTemperature));

            if (phase.primary != previous.primary)
            {
                ASSERT_LT(visitedCount, visited.size()) << "more than one seasonal cycle was crossed";
                visited[visitedCount++] = phase.primary;
            }
            previous = phase;
            previousTemperature = temperature;
        };

        // One production clock update per virtual real second. At 60x diurnal time and 24x
        // calendar compression this is one sixtieth of a calendar day, so the loop represents an
        // uninterrupted 365.2425-real-minute year while completing in milliseconds in CI.
        const double realSecondsInYear = kMeanYearDays * kRealSecondsPerCalendarDay;
        const int wholeSeconds = static_cast<int>(std::floor(realSecondsInYear));
        for (int second = 0; second < wholeSeconds; ++second)
        {
            clock.Advance(1.0);
            observe();
        }
        clock.Advance(realSecondsInYear - static_cast<double>(wholeSeconds));
        observe();

        EXPECT_EQ(visitedCount, expected.size());
        EXPECT_EQ(visited, expected);
        EXPECT_EQ(clock.Season().primary, static_cast<int>(Season::Spring));
        EXPECT_NEAR(clock.CalendarDays() - kNewGameCalendarDays, kMeanYearDays, 1e-9);
        EXPECT_NEAR(clock.YearFraction(), 0.0, 1e-12);

        // At one-second sampling the blend ramp moves by 1/2191.455. A discontinuous label flip
        // would jump from 0.5 to 0; keep ample floating-point headroom while still catching it.
        EXPECT_GT(largestBlend, 0.49) << "the year never entered a seasonal blend";
        EXPECT_LT(largestBlendStep, 0.001);
        // The live annual + diurnal temperature curve moved at most about 0.03 °C per sampled
        // second when measured. A switch between seasonal constants would exceed this by orders
        // of magnitude at one of the four boundaries.
        EXPECT_LT(largestTemperatureStep, 0.05);

        std::printf("  %.4f virtual real minutes, %d one-second samples; "
                    "season sequence Spring/Summer/Autumn/Winter/Spring; "
                    "largest blend step %.6f, temperature step %.6f C\n",
                    realSecondsInYear / 60.0,
                    wholeSeconds + 1,
                    largestBlendStep,
                    largestTemperatureStep);
    }

} // namespace
