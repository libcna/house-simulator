// SPDX-License-Identifier: MIT
//
// `HOUSE-01546`. This is the player-facing line, not §71's diagnostic F8 panel.
#include <array>
#include <string>
#include <string_view>

#include <gtest/gtest.h>

#include "cnahouse/environment/Season.hpp"
#include "cnahouse/environment/SimClock.hpp"
#include "cnahouse/ui/EnvironmentReadout.hpp"

namespace
{
    using cnahouse::environment::CivilTime;
    using cnahouse::environment::kMeanYearDays;
    using cnahouse::environment::kNewGameCalendarDays;
    using cnahouse::environment::SimClock;
    using cnahouse::ui::EnvironmentReadout;

    [[nodiscard]] bool Mentions(std::string_view line, std::string_view text)
    {
        return line.find(text) != std::string_view::npos;
    }

} // namespace

TEST(EnvironmentReadoutTests, OnePlayerFacingLineContainsAllFourRequiredValues)
{
    SimClock clock;
    clock.calendarDaysPerSimDay = 1.0;
    CivilTime summer;
    summer.year = 2031;
    summer.month = 7;
    summer.day = 20;
    summer.hour = 15;
    clock.SetStandard(summer);

    const std::string line = EnvironmentReadout{}.Line(clock);
    EXPECT_TRUE(Mentions(line, "16:00")) << line << " (wall time includes daylight saving)";
    EXPECT_TRUE(Mentions(line, "Summer")) << line;
    EXPECT_TRUE(Mentions(line, "Year 33%")) << line;
    EXPECT_TRUE(Mentions(line, "29.0 °C")) << line;
    EXPECT_FALSE(Mentions(line, "F8")) << "a player readout became a debug overlay: " << line;
    EXPECT_FALSE(Mentions(line, "epoch")) << line;
}

TEST(EnvironmentReadoutTests, AFullYearAlwaysNamesASeasonAndKeepsTheLineCompact)
{
    constexpr std::array<std::string_view, 4> kNames{"Spring", "Summer", "Autumn", "Winter"};
    int seen[4] = {0, 0, 0, 0};
    EnvironmentReadout readout;
    SimClock clock;
    clock.calendarDaysPerSimDay = 1.0;

    // One line per calendar day is the requested manual read-through made repeatable: all 365
    // presentations have a valid clock, the primary season (and its neighbour while blending),
    // year progress and a finite °C value.
    for (int day = 0; day < 365; ++day)
    {
        clock.SetCalendar(kNewGameCalendarDays + static_cast<double>(day));
        const std::string line = readout.Line(clock);
        int namesInLine = 0;
        for (std::size_t index = 0; index < kNames.size(); ++index)
        {
            if (Mentions(line, kNames[index]))
            {
                ++seen[index];
                ++namesInLine;
            }
        }
        const auto phase = clock.Season();
        EXPECT_TRUE(Mentions(line, kNames[static_cast<std::size_t>(phase.primary)]))
            << "day " << day << ": " << line;
        EXPECT_EQ(namesInLine, phase.blend > 0.0F ? 2 : 1) << "day " << day << ": " << line;
        EXPECT_TRUE(Mentions(line, "Year ")) << line;
        EXPECT_TRUE(Mentions(line, "%")) << line;
        EXPECT_TRUE(Mentions(line, "°C")) << line;
        EXPECT_LT(line.size(), 80U) << "the compact HUD line stopped being compact: " << line;
    }
    for (std::size_t index = 0; index < kNames.size(); ++index)
    {
        EXPECT_GT(seen[index], 0) << kNames[index] << " never appears during a year";
    }
}

TEST(EnvironmentReadoutTests, YearProgressIsZeroThroughNinetyNineAndWrapsCleanly)
{
    EnvironmentReadout readout;
    SimClock clock;
    clock.calendarDaysPerSimDay = 1.0;

    clock.SetCalendar(kNewGameCalendarDays);
    EXPECT_TRUE(Mentions(readout.Line(clock), "Year 0%"));
    clock.SetCalendar(kNewGameCalendarDays + kMeanYearDays * 0.999);
    EXPECT_TRUE(Mentions(readout.Line(clock), "Year 99%")) << readout.Line(clock);
    EXPECT_FALSE(Mentions(readout.Line(clock), "Year 100%"));
    clock.SetCalendar(kNewGameCalendarDays + kMeanYearDays);
    EXPECT_TRUE(Mentions(readout.Line(clock), "Year 0%")) << readout.Line(clock);
}
