// SPDX-License-Identifier: MIT
//
// `HOUSE-01544`, `HOUSE-01617`. Four frames at the edges of the longest and shortest days,
// including the production sky behind each solar state.
//
// These are deliberately reached through §71's production `time` command. Under §35.2b's
// compressed year a calendar date and a clock face are not independent, so each fixture names one
// exact reachable calendar position. The summer pair is 06:00 and 20:00 daylight time; the winter
// pair is the same local clock faces in standard time. Summer has the sun above the horizon at
// both; winter does not.
#include <array>
#include <cstdio>
#include <format>
#include <string>

#include <gtest/gtest.h>

#include "cnahouse/app/CnaHouseGame.hpp"
#include "cnahouse/app/CommandLine.hpp"
#include "cnahouse/app/Settings.hpp"
#include "cnahouse/debug/Console.hpp"
#include "cnahouse/debug/Counters.hpp"
#include "cnahouse/environment/SunModel.hpp"
#include "cnahouse/player/IInputSource.hpp"

#include "render/ImageCompare.hpp"
#include "render/RenderHarness.hpp"

namespace
{
    using cnahouse::app::CnaHouseGame;
    using cnahouse::app::Options;
    using cnahouse::app::QualityPreset;
    using cnahouse::app::RenderTier;
    using cnahouse::app::Settings;
    using cnahouse::testsupport::Compare;
    using cnahouse::testsupport::Image;
    using cnahouse::testsupport::RenderHarness;

    constexpr int kWidth = 640;
    constexpr int kHeight = 360;

    struct Scene
    {
        const char* name;
        const char* moment;
        double calendarDays;
        /// Feet x/y/z, yaw and pitch. Both pairs stand on §65.6's road and frame the house and sun.
        std::array<float, 5> player;
        bool sunVisible;
    };

    // Calendar day 173 is 2031-06-23; 187 is 2031-07-07; 356 is 2031-12-23; and
    // 366 is 2032-01-02. Their remainders modulo §35.2b's 24 calendar days per simulated day are
    // respectively 05:00, 19:00, 20:00 and 06:00 standard time. Daylight saving makes the first
    // pair the same 06:00/20:00 wall-clock comparison as the second. The small date offsets keep
    // the instants exact rather than pretending compression can represent any date/time pair.
    constexpr Scene kScenes[] = {
        {"sun-season-01", "longest-day morning", 173.0, {0.0F, 0.0F, 5.2F, 30.0F, 3.0F}, true},
        {"sun-season-02", "longest-day evening", 187.0, {0.0F, 0.0F, 5.2F, 330.0F, 3.0F}, true},
        {"sun-season-03", "shortest-day morning", 366.0, {0.0F, 0.0F, 5.2F, 30.0F, 3.0F}, false},
        {"sun-season-04", "shortest-day evening", 356.0, {0.0F, 0.0F, 5.2F, 330.0F, 3.0F}, false},
    };

    class SeasonClockDriver final : public cnahouse::player::IInputSource
    {
    public:
        SeasonClockDriver(CnaHouseGame& game, double calendarDays)
            : game_(&game)
            , calendarDays_(calendarDays)
        {
        }

        void Update(float) override
        {
            if (ran_)
            {
                return;
            }
            ran_ = true;
            const double advance = calendarDays_ - game_->ClockForTesting().CalendarDays();
            // Round-trip the double. Nine decimal places can land one ULP below an integer
            // calendar day, which makes the displayed date read 23:59:59 on the previous day even
            // though the solar answer is effectively identical.
            advanceResult_ = game_->ConsoleForTesting().Execute(std::format("time advance {:.17g}", advance));
            freezeResult_ = game_->ConsoleForTesting().Execute("time scale 0");
        }

        [[nodiscard]] const cnahouse::player::InputState& Current() const noexcept override
        {
            return state_;
        }

        [[nodiscard]] bool LookAvailable() const noexcept override
        {
            return false;
        }

        [[nodiscard]] bool Succeeded() const noexcept
        {
            return ran_ && advanceResult_.ok && freezeResult_.ok;
        }

    private:
        CnaHouseGame* game_ = nullptr;
        double calendarDays_ = 0.0;
        cnahouse::player::InputState state_;
        cnahouse::debug::CommandResult advanceResult_;
        cnahouse::debug::CommandResult freezeResult_;
        bool ran_ = false;
    };

    std::string ActualPath(const Scene& scene)
    {
        return std::string(CNAHOUSE_TEST_OUTPUT_DIR) + "/" + scene.name + "-actual.png";
    }

    std::string ReferencePath(const Scene& scene)
    {
        return RenderHarness::ReferenceDirectory() + "/" + scene.name + ".png";
    }

    Options OptionsFor(const Scene& scene, const std::string& path)
    {
        Options options;
        options.tier = RenderTier::S;
        options.quality = QualityPreset::Low;
        options.noAudio = true;
        options.contentRoot = CNAHOUSE_TEST_CONTENT_ROOT;
        options.scene = "walk";
        options.player = scene.player;
        options.seed = 0x5EEDC0DEC0FFEE01ULL;
        options.screenshot = path;
        return options;
    }

    void Capture(const Scene& scene, const std::string& path)
    {
        Settings settings = Settings::Defaults();
        settings.backBufferWidth = kWidth;
        settings.backBufferHeight = kHeight;
        settings.verticalSync = false;
        // The environment readout has its own fixture. These references isolate the sky and sun.
        settings.showEnvironmentReadout = false;

        CnaHouseGame game(OptionsFor(scene, path), settings);
        SeasonClockDriver driver(game, scene.calendarDays);
        game.SetInputSourceForTesting(&driver);
        game.SetFrameLimit(20);
        game.Run();

        EXPECT_EQ(game.ExitCode(), 0) << scene.name;
        EXPECT_TRUE(driver.Succeeded()) << scene.name << " did not set and freeze its clock";
        EXPECT_NEAR(game.ClockForTesting().CalendarDays(), scene.calendarDays, 1e-7) << scene.name;
        EXPECT_DOUBLE_EQ(game.ClockForTesting().timeScale, 0.0) << scene.name;

        const auto* lighting = game.LightingForTesting();
        EXPECT_NE(lighting, nullptr) << scene.name;
        if (lighting != nullptr)
        {
            const bool aboveHorizon =
                lighting->Sun().altitudeDeg >= cnahouse::environment::kRefractedHorizonDeg;
            EXPECT_EQ(aboveHorizon, scene.sunVisible) << scene.name << " (" << scene.moment << ')';
            const auto wall = game.ClockForTesting().Wall();
            std::printf("  %s: %04d-%02d-%02d %02d:%02d, sun %.2f deg at %.2f deg\n",
                        scene.name,
                        wall.year,
                        wall.month,
                        wall.day,
                        wall.hour,
                        wall.minute,
                        lighting->Sun().altitudeDeg,
                        lighting->Sun().azimuthDeg);
        }

        const auto* disc = game.CountersForTesting().Find("sun.disc.draws");
        if (scene.sunVisible)
        {
            EXPECT_NE(disc, nullptr) << scene.name << " did not draw its visible sun";
            if (disc != nullptr)
            {
                EXPECT_EQ(disc->current, 1);
            }
        }
        else
        {
            EXPECT_EQ(disc, nullptr) << scene.name << " drew a sun below the horizon";
        }
    }

    void VerifyScene(std::size_t index)
    {
        const Scene& scene = kScenes[index];
        SCOPED_TRACE(scene.name);
        Capture(scene, ActualPath(scene));
        const auto actual = RenderHarness::LoadPng(ActualPath(scene));
        ASSERT_TRUE(actual.HasValue()) << scene.name << ": " << actual.Error().ToString();
        if (RenderHarness::RenderingInSoftware())
        {
            const auto reference = RenderHarness::LoadPng(ReferencePath(scene));
            ASSERT_TRUE(reference.HasValue()) << scene.name << ": " << reference.Error().ToString();
            const auto diff = Compare(*actual, *reference, 2);
            EXPECT_FALSE(diff.sizeMismatch) << scene.name;
            EXPECT_LT(diff.DifferingFraction(), 0.002) << scene.name << ": " << diff.ToString();
        }
    }

    // Separate cases on purpose. GoogleTest discovery gives each one its own process under ctest,
    // just as the other render scenes do, so a scene is attributable and independently runnable.
    TEST(SunSeasonRenderTests, SunSeason01MatchesItsReference)
    {
        VerifyScene(0);
    }

    TEST(SunSeasonRenderTests, SunSeason02MatchesItsReference)
    {
        VerifyScene(1);
    }

    TEST(SunSeasonRenderTests, SunSeason03MatchesItsReference)
    {
        VerifyScene(2);
    }

    TEST(SunSeasonRenderTests, SunSeason04MatchesItsReference)
    {
        VerifyScene(3);
    }

    void GenerateReference(std::size_t index)
    {
        ASSERT_TRUE(RenderHarness::RenderingInSoftware())
            << "references are accepted only from LIBGL_ALWAYS_SOFTWARE=1";
        const Scene& scene = kScenes[index];
        Capture(scene, ReferencePath(scene));
        std::printf("  wrote %s\n", ReferencePath(scene).c_str());
    }

    TEST(SunSeasonRenderTests, DISABLED_RegenerateReference01)
    {
        GenerateReference(0);
    }

    TEST(SunSeasonRenderTests, DISABLED_RegenerateReference02)
    {
        GenerateReference(1);
    }

    TEST(SunSeasonRenderTests, DISABLED_RegenerateReference03)
    {
        GenerateReference(2);
    }

    TEST(SunSeasonRenderTests, DISABLED_RegenerateReference04)
    {
        GenerateReference(3);
    }

} // namespace
