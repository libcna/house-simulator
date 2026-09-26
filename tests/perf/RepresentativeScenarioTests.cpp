// SPDX-License-Identifier: MIT
//
// `HOUSE-02402`: the reduced plan's eight fixed performance scenarios. This harness reports
// measurements; `HOUSE-02403` records the reference baseline and `HOUSE-02404` owns any optimisation
// demanded by it. Keeping those decisions out of this file is what prevents a busy CI worker from
// turning a useful measurement into a flaky release gate.
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <ostream>
#include <string>
#include <string_view>
#include <vector>

#include <gtest/gtest.h>

#include "cnahouse/app/CnaHouseGame.hpp"
#include "cnahouse/app/CommandLine.hpp"
#include "cnahouse/app/Settings.hpp"
#include "cnahouse/debug/Counters.hpp"
#include "cnahouse/player/IInputSource.hpp"

namespace
{
    using cnahouse::app::CnaHouseGame;
    using cnahouse::app::Options;
    using cnahouse::app::QualityPreset;
    using cnahouse::app::RenderTier;
    using cnahouse::app::Settings;

    constexpr std::size_t kWarmUp = 120;
    constexpr std::size_t kMeasured = 600;
    constexpr std::uint64_t kSeed = 6840335469064670721ULL;

    struct Scenario
    {
        const char* id;
        const char* description;
        std::array<float, 5> player;
        float timeOfDay;
        const char* weather;
    };

    void PrintTo(const Scenario& scenario, std::ostream* output)
    {
        *output << scenario.id;
    }

    constexpr std::array<Scenario, 8> kScenarios{{
        {"Kitchen", "kitchen interior", {-1.10F, 0.60F, -25.05F, 90.0F, 0.0F}, 12.0F, "W_CLEAR"},
        {"Library", "library interior", {-7.40F, 6.55F, -16.30F, 90.0F, 0.0F}, 10.5F, "W_CLEAR"},
        {"MainStair", "main stair looking up", {4.10F, 0.60F, -14.70F, 0.0F, 60.0F}, 10.5F, "W_CLEAR"},
        {"StreetApproach",
         "street approach toward house and neighbourhood",
         {0.00F, 0.00F, 5.20F, 0.0F, 3.0F},
         10.5F,
         "W_CLEAR"},
        {"RearGarden", "rear garden toward house", {-8.00F, -0.28F, -42.00F, 135.0F, 0.0F}, 10.5F, "W_CLEAR"},
        {"UpperWindow",
         "upper-floor window looking out",
         {-3.90F, 3.65F, -25.50F, 0.0F, 0.0F},
         10.5F,
         "W_CLEAR"},
        {"HeavyRain", "heavy rain outside", {0.00F, 0.45F, -32.80F, 180.0F, 0.0F}, 18.0F, "W_HEAVY_RAIN"},
        {"NightOutside", "night outside", {0.00F, 0.00F, 5.20F, 0.0F, 3.0F}, 22.0F, "W_CLEAR"},
    }};

    double Percentile(std::vector<float> samples, double fraction)
    {
        std::sort(samples.begin(), samples.end());
        const auto index = static_cast<std::size_t>(fraction * static_cast<double>(samples.size() - 1));
        return static_cast<double>(samples[index]);
    }

    std::vector<float> MeasuredSamples(const std::vector<float>& all)
    {
        if (all.size() < kWarmUp + kMeasured)
        {
            return {};
        }
        return std::vector<float>(all.begin() + static_cast<std::ptrdiff_t>(kWarmUp),
                                  all.begin() + static_cast<std::ptrdiff_t>(kWarmUp + kMeasured));
    }

    double AverageCounters(const cnahouse::debug::Counters& counters,
                           const std::array<std::string_view, 3>& exact,
                           std::string_view suffix)
    {
        double total = 0.0;
        for (const cnahouse::debug::Counter& counter : counters.All())
        {
            const bool exactMatch = std::find(exact.begin(), exact.end(), counter.name) != exact.end();
            if (exactMatch || std::string_view(counter.name).ends_with(suffix))
            {
                total += counter.Average();
            }
        }
        return total;
    }

    class RepresentativeScenarioTests : public testing::TestWithParam<Scenario>
    {
    };

    class NeutralInput final : public cnahouse::player::IInputSource
    {
    public:
        void Update(float) override {}

        [[nodiscard]] const cnahouse::player::InputState& Current() const noexcept override
        {
            return state_;
        }

        [[nodiscard]] bool LookAvailable() const noexcept override
        {
            return false;
        }

    private:
        cnahouse::player::InputState state_;
    };

    std::string ScenarioName(const testing::TestParamInfo<Scenario>& parameter)
    {
        return parameter.param.id;
    }

    TEST_P(RepresentativeScenarioTests, ReportsHighTierFrameCost)
    {
        const Scenario& scenario = GetParam();

        Options options;
        options.scene = "walk";
        options.player = scenario.player;
        options.seed = kSeed;
        options.timeOfDay = scenario.timeOfDay;
        options.freezeTime = true;
        options.weather = scenario.weather;
        options.noAudio = true;
        options.quality = QualityPreset::High;
        options.tier = RenderTier::S;
        options.contentRoot = CNAHOUSE_TEST_CONTENT_ROOT;
        options.effectRoot = CNAHOUSE_TEST_EFFECT_ROOT;

        Settings settings = Settings::Defaults();
        settings.backBufferWidth = 1920;
        settings.backBufferHeight = 1080;
        settings.verticalSync = false;
        settings.quality = QualityPreset::High;

        CnaHouseGame game(options, settings);
        // A fixed camera must not consume the owner's real mouse while this window is visible.
        // Otherwise the measured draw/triangle counts describe wherever the cursor happened to
        // point, not the named scenario, and platform preset comparisons are meaningless.
        NeutralInput input;
        game.SetInputSourceForTesting(&input);
        game.SetFrameLimit(kWarmUp + kMeasured);
        game.EnableGpuCompletionSamplingForTesting();
        game.Run();

        ASSERT_EQ(game.ExitCode(), 0);
        ASSERT_EQ(game.FramesDrawn(), kWarmUp + kMeasured);

        const std::vector<float> submit = MeasuredSamples(game.RenderSubmitTimesForTesting());
        const std::vector<float> completion = MeasuredSamples(game.GpuCompletionTimesForTesting());
        ASSERT_EQ(submit.size(), kMeasured);
        ASSERT_EQ(completion.size(), kMeasured);

        const double submitMedian = Percentile(submit, 0.50);
        const double submitP95 = Percentile(submit, 0.95);
        const double gpuMedian = Percentile(completion, 0.50);
        const double gpuP95 = Percentile(completion, 0.95);
        const double cpuUpdate = game.TimingForTesting().TotalAverageMilliseconds();
        const double cpuTotal = cpuUpdate + submitMedian;

        const std::array<std::string_view, 3> drawCounters{
            "static.chunks", "alpha.chunks", "transparent.chunks"};
        const std::array<std::string_view, 3> noExact{"", "", ""};
        const double drawCalls = AverageCounters(game.CountersForTesting(), drawCounters, ".draws");
        const double triangles = AverageCounters(game.CountersForTesting(), noExact, ".triangles");

        ASSERT_TRUE(std::isfinite(cpuTotal));
        ASSERT_TRUE(std::isfinite(gpuMedian));
        ASSERT_GT(drawCalls, 0.0);
        ASSERT_GT(triangles, 0.0);

        std::printf("[ scenario ] %s — %s\n"
                    "[ scenario ] %s\n"
                    "[ scenario ] 1920x1080 / Tier S / High / vsync off / %zu warm-up + %zu samples\n"
                    "[ scenario ] draw calls %.0f average; triangles %.0f average\n"
                    "[ scenario ] CPU %.3f ms (updates %.3f + render submit %.3f, submit p95 %.3f)\n"
                    "[ scenario ] GPU completion %.3f ms median, %.3f ms p95\n",
                    scenario.id,
                    scenario.description,
                    game.GetPlatform().Summary().c_str(),
                    kWarmUp,
                    kMeasured,
                    drawCalls,
                    triangles,
                    cpuTotal,
                    cpuUpdate,
                    submitMedian,
                    submitP95,
                    gpuMedian,
                    gpuP95);
    }

    INSTANTIATE_TEST_SUITE_P(EightFixedScenarios,
                             RepresentativeScenarioTests,
                             testing::ValuesIn(kScenarios),
                             ScenarioName);
} // namespace
