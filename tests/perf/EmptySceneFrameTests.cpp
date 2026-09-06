// SPDX-License-Identifier: MIT
//
// `HOUSE-00165`: what the empty scene costs per frame, which is the floor every later phase is
// measured against. There is no house yet -- this is the clear, the title screen's two strings and
// the HUD -- so a number here that is already a large fraction of the 16.7 ms budget would be a
// finding about the framework rather than about the game.
//
// Perf tests are NEVER gating (`cna-house.md` §70.4). This one asserts only a ceiling loose enough
// that it can fail for one reason: something became categorically slower.
#include <algorithm>
#include <cstdio>
#include <vector>

#include <gtest/gtest.h>

#include "cnahouse/app/CnaHouseGame.hpp"
#include "cnahouse/app/CommandLine.hpp"
#include "cnahouse/app/Settings.hpp"

namespace
{
    using cnahouse::app::CnaHouseGame;
    using cnahouse::app::Options;
    using cnahouse::app::QualityPreset;
    using cnahouse::app::Settings;

    /// Frames thrown away before measuring. Shader compilation, the first content touch and the
    /// window's first present all land in the first few frames and none of them is a frame cost.
    constexpr std::size_t kWarmUp = 120;
    constexpr std::size_t kMeasured = 600;

    double Percentile(std::vector<float> samples, double fraction)
    {
        std::sort(samples.begin(), samples.end());
        const auto index = static_cast<std::size_t>(fraction * static_cast<double>(samples.size() - 1));
        return static_cast<double>(samples[index]);
    }

    TEST(EmptySceneFrameTests, TheEmptySceneFrameCostIsRecorded)
    {
        Options options;
        options.noAudio = true;
        options.quality = QualityPreset::Low;
        options.contentRoot = CNAHOUSE_TEST_CONTENT_ROOT;

        Settings settings = Settings::Defaults();
        settings.backBufferWidth = 1600;
        settings.backBufferHeight = 900;
        // WITHOUT vsync, or the number measured is the display's refresh rate and nothing else.
        settings.verticalSync = false;

        CnaHouseGame game(options, settings);
        game.SetFrameLimit(kWarmUp + kMeasured);
        game.Run();
        ASSERT_EQ(game.ExitCode(), 0);

        const auto& all = game.FrameTimes();
        ASSERT_GE(all.size(), kWarmUp + kMeasured);
        const std::vector<float> samples(all.begin() + static_cast<std::ptrdiff_t>(kWarmUp),
                                         all.begin() + static_cast<std::ptrdiff_t>(kWarmUp + kMeasured));

        const double median = Percentile(samples, 0.50);
        const double p95 = Percentile(samples, 0.95);
        const double p99 = Percentile(samples, 0.99);
        const double worst = Percentile(samples, 1.0);

        // Printed rather than only asserted, because the NUMBER is the deliverable and it goes into
        // `docs/performance-log.md` by hand. Three digits: the samples are milliseconds from a
        // steady clock on a machine shared with other build jobs, and a fourth would be invented.
        std::printf("empty scene, %zu samples after %zu warm-up frames, 1600x900, vsync off:\n"
                    "  median %.3f ms  p95 %.3f ms  p99 %.3f ms  worst %.3f ms  (%.0f fps median)\n",
                    samples.size(),
                    kWarmUp,
                    median,
                    p95,
                    p99,
                    worst,
                    1000.0 / median);

        // A ceiling, not a target. 8 ms is half the 60 Hz budget on a frame that draws two strings;
        // anything above it means something categorically changed, not that the machine was busy.
        EXPECT_LT(median, 8.0) << "the empty scene should not be a measurable part of the budget";
    }
} // namespace
