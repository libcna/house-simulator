// SPDX-License-Identifier: MIT
//
// `HOUSE-00150`. The overlay is a PRESENTER -- it owns no measurement -- which is what lets its
// content be asserted here without a device, and what lets a perf test assert against `Counters`
// and `Timing` directly rather than parsing a screen.
#include <gtest/gtest.h>

#include "cnahouse/app/Platform.hpp"
#include "cnahouse/debug/Overlay.hpp"

namespace
{
    using cnahouse::app::Platform;
    using cnahouse::app::UpdateStage;
    using cnahouse::debug::Counters;
    using cnahouse::debug::Overlay;
    using cnahouse::debug::Timing;

    TEST(OverlayTests, ItStartsHiddenAndTogglesToVisible)
    {
        Overlay overlay;
        EXPECT_FALSE(overlay.Visible()) << "a performance overlay that is on by default is in the way";
        overlay.Toggle();
        EXPECT_TRUE(overlay.Visible());
        overlay.Toggle();
        EXPECT_FALSE(overlay.Visible());
    }

    TEST(OverlayTests, TheGraphIsBoundedToItsWindow)
    {
        Overlay overlay;
        for (std::size_t i = 0; i < Overlay::kGraphFrames * 3; ++i)
        {
            overlay.PushFrameTime(16.6f);
        }
        EXPECT_EQ(overlay.GraphRow().size(), Overlay::kGraphFrames);
    }

    TEST(OverlayTests, TheGraphShowsShapeNotJustMagnitude)
    {
        // The whole reason the graph exists: an average tells you the cost, the SHAPE tells you whether
        // something hitches every two seconds -- which is the bug that actually gets reported and which
        // is invisible in an average.
        Overlay overlay;
        for (int i = 0; i < 10; ++i)
        {
            overlay.PushFrameTime(2.0f);
        }
        overlay.PushFrameTime(32.0f);
        for (int i = 0; i < 10; ++i)
        {
            overlay.PushFrameTime(2.0f);
        }
        const std::string row = overlay.GraphRow();
        ASSERT_EQ(row.size(), 21u);
        EXPECT_EQ(row.front(), row.back()) << "the quiet frames read the same at both ends";
        EXPECT_NE(row[10], row[0]) << "and the spike does not";
        EXPECT_GT(static_cast<unsigned char>(row[10]), 0u);
    }

    TEST(OverlayTests, AFrameOverTheCeilingClampsRatherThanOverflowing)
    {
        Overlay overlay;
        overlay.PushFrameTime(10000.0f);
        const std::string row = overlay.GraphRow();
        ASSERT_EQ(row.size(), 1u);
        EXPECT_EQ(row, "#") << "the tallest bar, not a crash and not an empty cell";
    }

    TEST(OverlayTests, TheCeilingIsAboveTheTargetSoASpikeIsVisible)
    {
        // A graph whose ceiling is the 16.6 ms target clips exactly when something goes wrong, which is
        // the moment the shape matters most.
        EXPECT_GT(Overlay::kGraphCeilingMilliseconds, 16.7f);
    }

    TEST(OverlayTests, TheLinesCarryTheBuildAndTheBudget)
    {
        Overlay overlay;
        Timing timing;
        Counters counters;
        timing.Record(UpdateStage::Visibility, 3.0);
        timing.BeginFrame();

        const Platform platform = Platform::FromBuild();
        const auto lines = overlay.Lines(platform, timing, counters);
        ASSERT_GE(lines.size(), 3u);
        EXPECT_NE(lines[0].find(CNAHOUSE_RENDERER_NAME), std::string::npos)
            << "which build produced this frame, so a screenshot is attributable: " << lines[0];
        EXPECT_NE(lines[1].find("16.67"), std::string::npos)
            << "the budget is on screen beside the number, not in someone's head: " << lines[1];
    }

    TEST(OverlayTests, OnlyStagesThatHaveRunAppear)
    {
        // Twelve rows of 0.00 is a table nobody reads. A stage that HAS run is always shown, even at
        // zero, because its absence would be indistinguishable from it not existing.
        Overlay overlay;
        Timing timing;
        Counters counters;
        timing.Record(UpdateStage::Audio, 1.0);
        timing.BeginFrame();

        const auto lines = overlay.Lines(Platform::FromBuild(), timing, counters);
        bool sawAudio = false;
        bool sawWeather = false;
        for (const std::string& line : lines)
        {
            sawAudio = sawAudio || line.starts_with("audio");
            sawWeather = sawWeather || line.starts_with("weather");
        }
        EXPECT_TRUE(sawAudio);
        EXPECT_FALSE(sawWeather) << "a stage that has never run is noise";
    }

    TEST(OverlayTests, CountersAppearWithTheirWindowStatistics)
    {
        Overlay overlay;
        Timing timing;
        Counters counters;
        const auto handle = counters.Resolve("draws");
        counters.Set(handle, 250);
        counters.BeginFrame();
        counters.Set(handle, 300);

        const auto lines = overlay.Lines(Platform::FromBuild(), timing, counters);
        bool sawDraws = false;
        for (const std::string& line : lines)
        {
            sawDraws = sawDraws || line.starts_with("draws");
        }
        EXPECT_TRUE(sawDraws);
    }

} // namespace
