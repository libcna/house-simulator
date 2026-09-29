// SPDX-License-Identifier: MIT
//
// `HOUSE-02402`: the reduced plan's eight fixed performance scenarios, run under each shipped
// preset since `HOUSE-02405`. This harness reports measurements; `HOUSE-02403` records the
// reference baseline and `HOUSE-02404` owns any optimisation demanded by it. Keeping those decisions out of
// this file is what prevents a busy CI worker from turning a useful measurement into a flaky release gate.
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <map>
#include <ostream>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include "cnahouse/app/CnaHouseGame.hpp"
#include "cnahouse/app/CommandLine.hpp"
#include "cnahouse/app/Settings.hpp"
#include "cnahouse/debug/Counters.hpp"
#include "cnahouse/player/IInputSource.hpp"
#include "cnahouse/visibility/RenderList.hpp"
#include "cnahouse/world/ChunkData.hpp"
#include "cnahouse/world/ChunkReader.hpp"

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

    /// `HOUSE-02405`: each shipped preset at the resolution its platform renders, with the budget
    /// `cna-house.md` §71 gives it. Draw calls and triangles are deterministic for a fixed camera.
    /// High's are the §71.6 hard limits and are asserted; the Web and Android counts are reported
    /// with a verdict, because fitting them is the content work of `HOUSE-02898` and `HOUSE-03037`.
    /// Frame costs are always reported, never asserted: timing on a shared machine is a
    /// measurement to record, not a gate that fails on someone else's compiler.
    struct Preset
    {
        const char* id;
        QualityPreset quality;
        int width;
        int height;
        double cpuBudgetMs;
        double gpuBudgetMs;
        double drawBudget;
        double triangleBudget;
        bool hardCountLimits;
    };

    constexpr std::array<Preset, 3> kPresets{{
        {"High", QualityPreset::High, 1920, 1080, 9.5, 14.0, 1800.0, 3'400'000.0, true},
        {"Web", QualityPreset::Medium, 1280, 720, 33.0, 33.0, 500.0, 900'000.0, false},
        {"Android", QualityPreset::Low, 1280, 720, 33.0, 33.0, 400.0, 700'000.0, false},
    }};

    void PrintTo(const Preset& preset, std::ostream* output)
    {
        *output << preset.id;
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

    using ScenarioAtPreset = std::tuple<Scenario, Preset>;

    class RepresentativeScenarioTests : public testing::TestWithParam<ScenarioAtPreset>
    {
    };

    /// The last frame's static draws grouped by `cell:material`, largest first: where a preset's
    /// triangles actually go, so a miss names its cause rather than only its size.
    void PrintHeaviest(const cnahouse::visibility::RenderList& list,
                       const cnahouse::world::ChunkLibrary& library)
    {
        std::map<std::string, std::pair<double, int>> groups;
        std::map<std::string, int> cells;
        for (const cnahouse::visibility::RenderItem& item : list.Items())
        {
            if (item.geometry >= library.chunks.size())
            {
                continue;
            }
            const cnahouse::world::Chunk& chunk = library.chunks[item.geometry];
            ++cells[library.cells[chunk.cell]];
            auto& group = groups[library.cells[chunk.cell] + ":" + library.materials[chunk.material]];
            group.first += static_cast<double>(chunk.indexCount) / 3.0;
            ++group.second;
        }
        std::vector<std::pair<std::string, std::pair<double, int>>> sorted(groups.begin(), groups.end());
        std::sort(sorted.begin(),
                  sorted.end(),
                  [](const auto& a, const auto& b) { return a.second.first > b.second.first; });
        for (std::size_t i = 0; i < std::min<std::size_t>(sorted.size(), 8u); ++i)
        {
            std::printf("[ heaviest ] %9.0f triangles in %2d draw(s)  %s\n",
                        sorted[i].second.first,
                        sorted[i].second.second,
                        sorted[i].first.c_str());
        }
        std::vector<std::pair<std::string, int>> byCell(cells.begin(), cells.end());
        std::sort(
            byCell.begin(), byCell.end(), [](const auto& a, const auto& b) { return a.second > b.second; });
        std::string busiest;
        for (std::size_t i = 0; i < std::min<std::size_t>(byCell.size(), 10u); ++i)
        {
            busiest += " " + byCell[i].first + "=" + std::to_string(byCell[i].second);
        }
        std::printf("[ heaviest ] draws by cell:%s\n", busiest.c_str());
    }

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

    std::string ScenarioName(const testing::TestParamInfo<ScenarioAtPreset>& parameter)
    {
        return std::string(std::get<0>(parameter.param).id) + "_" + std::get<1>(parameter.param).id;
    }

    TEST_P(RepresentativeScenarioTests, MeetsItsPresetBudget)
    {
        const Scenario& scenario = std::get<0>(GetParam());
        const Preset& preset = std::get<1>(GetParam());

        Options options;
        options.scene = "walk";
        options.player = scenario.player;
        options.seed = kSeed;
        options.timeOfDay = scenario.timeOfDay;
        options.freezeTime = true;
        options.weather = scenario.weather;
        options.noAudio = true;
        options.quality = preset.quality;
        options.tier = RenderTier::S;
        options.contentRoot = CNAHOUSE_TEST_CONTENT_ROOT;
        options.effectRoot = CNAHOUSE_TEST_EFFECT_ROOT;

        Settings settings = Settings::Defaults();
        settings.backBufferWidth = preset.width;
        settings.backBufferHeight = preset.height;
        settings.verticalSync = false;
        settings.quality = preset.quality;

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

        std::printf(
            "[ scenario ] %s — %s\n"
            "[ scenario ] %s\n"
            "[ scenario ] %dx%d / Tier S / %s preset / vsync off / %zu warm-up + %zu samples\n"
            "[ scenario ] draw calls %.0f average (budget %.0f) %s; triangles %.0f average (budget %.0f) %s\n"
            "[ scenario ] CPU %.3f ms (updates %.3f + render submit %.3f, submit p95 %.3f); "
            "budget %.1f ms %s\n"
            "[ scenario ] GPU completion %.3f ms median, %.3f ms p95; budget %.1f ms %s\n",
            scenario.id,
            scenario.description,
            game.GetPlatform().Summary().c_str(),
            preset.width,
            preset.height,
            preset.id,
            kWarmUp,
            kMeasured,
            drawCalls,
            preset.drawBudget,
            drawCalls <= preset.drawBudget ? "within" : "OVER",
            triangles,
            preset.triangleBudget,
            triangles <= preset.triangleBudget ? "within" : "OVER",
            cpuTotal,
            cpuUpdate,
            submitMedian,
            submitP95,
            preset.cpuBudgetMs,
            cpuTotal <= preset.cpuBudgetMs ? "within" : "OVER",
            gpuMedian,
            gpuP95,
            preset.gpuBudgetMs,
            gpuMedian <= preset.gpuBudgetMs ? "within" : "OVER");
        const auto library = cnahouse::world::ChunkReader::ReadFromTitle("content/world/chunks.bin");
        if (library)
        {
            PrintHeaviest(game.RenderListForTesting(), library.Value());
        }
        if (preset.hardCountLimits)
        {
            EXPECT_LE(drawCalls, preset.drawBudget) << preset.id << " draw-call budget";
            EXPECT_LE(triangles, preset.triangleBudget) << preset.id << " triangle budget";
        }
    }

    INSTANTIATE_TEST_SUITE_P(EightFixedScenarios,
                             RepresentativeScenarioTests,
                             testing::Combine(testing::ValuesIn(kScenarios), testing::ValuesIn(kPresets)),
                             ScenarioName);
} // namespace
