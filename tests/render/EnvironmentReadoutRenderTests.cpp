// SPDX-License-Identifier: MIT
//
// `HOUSE-01546`: `hud-season-01`, the compact player-facing environment line in the real walk HUD.
#include <array>
#include <cstddef>
#include <string>
#include <string_view>

#include <gtest/gtest.h>

#include "cnahouse/app/CnaHouseGame.hpp"
#include "cnahouse/app/CommandLine.hpp"
#include "cnahouse/app/Settings.hpp"
#include "cnahouse/player/IInputSource.hpp"
#include "cnahouse/ui/EnvironmentReadout.hpp"

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
    using cnahouse::testsupport::Region;
    using cnahouse::testsupport::RenderHarness;

    constexpr int kWidth = 640;
    constexpr int kHeight = 360;
    constexpr Region kReadoutBand{80, 24, 480, 28};

    class FreezeClock final : public cnahouse::player::IInputSource
    {
    public:
        explicit FreezeClock(CnaHouseGame& game)
            : game_(&game)
        {
        }

        void Update(float) override
        {
            if (!ran_)
            {
                ran_ = true;
                result_ = game_->ConsoleForTesting().Execute("time scale 0");
            }
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
            return ran_ && result_.ok;
        }

    private:
        CnaHouseGame* game_ = nullptr;
        cnahouse::player::InputState state_;
        cnahouse::debug::CommandResult result_;
        bool ran_ = false;
    };

    [[nodiscard]] Options FixtureOptions(const std::string& path)
    {
        Options options;
        options.tier = RenderTier::S;
        options.quality = QualityPreset::Low;
        options.noAudio = true;
        options.contentRoot = CNAHOUSE_TEST_CONTENT_ROOT;
        options.scene = "walk";
        options.player = std::array<float, 5>{0.0F, 0.0F, 5.2F, 0.0F, 2.0F};
        options.seed = 0x5EEDC0DEC0FFEE01ULL;
        options.screenshot = path;
        return options;
    }

    void Capture(bool visible, const std::string& path)
    {
        Settings settings = Settings::Defaults();
        settings.backBufferWidth = kWidth;
        settings.backBufferHeight = kHeight;
        settings.verticalSync = false;
        settings.showEnvironmentReadout = visible;

        CnaHouseGame game(FixtureOptions(path), settings);
        FreezeClock driver(game);
        game.SetInputSourceForTesting(&driver);
        game.SetFrameLimit(20);
        game.Run();

        EXPECT_EQ(game.ExitCode(), 0);
        EXPECT_TRUE(driver.Succeeded());
        EXPECT_DOUBLE_EQ(game.ClockForTesting().timeScale, 0.0);
        const std::string line = cnahouse::ui::EnvironmentReadout{}.Line(game.ClockForTesting());
        EXPECT_NE(line.find("Spring"), std::string::npos) << line;
        EXPECT_NE(line.find("Year 0%"), std::string::npos) << line;
        EXPECT_NE(line.find("°C"), std::string::npos) << line;
    }

    [[nodiscard]] std::size_t DifferentPixelsIn(const Image& first, const Image& second, const Region& region)
    {
        if (first.width != second.width || first.height != second.height)
        {
            return 0;
        }
        std::size_t count = 0;
        for (int y = region.y; y < region.y + region.height; ++y)
        {
            for (int x = region.x; x < region.x + region.width; ++x)
            {
                const std::size_t index =
                    static_cast<std::size_t>(y) * static_cast<std::size_t>(first.width) +
                    static_cast<std::size_t>(x);
                const auto& a = first.pixels[index];
                const auto& b = second.pixels[index];
                if (a.getRProperty() != b.getRProperty() || a.getGProperty() != b.getGProperty() ||
                    a.getBProperty() != b.getBProperty() || a.getAProperty() != b.getAProperty())
                {
                    ++count;
                }
            }
        }
        return count;
    }

    [[nodiscard]] std::string OutputPath(std::string_view suffix)
    {
        return std::string(CNAHOUSE_TEST_OUTPUT_DIR) + "/hud-season-01-" + std::string(suffix) + ".png";
    }

    [[nodiscard]] std::string ReferencePath()
    {
        return RenderHarness::ReferenceDirectory() + "/hud-season-01.png";
    }

    TEST(EnvironmentReadoutRenderTests, HudSeason01IsLegibleAndTheSettingHidesIt)
    {
        const std::string visiblePath = OutputPath("actual");
        const std::string hiddenPath = OutputPath("hidden");
        Capture(true, visiblePath);
        Capture(false, hiddenPath);

        const auto visible = RenderHarness::LoadPng(visiblePath);
        const auto hidden = RenderHarness::LoadPng(hiddenPath);
        ASSERT_TRUE(visible.HasValue()) << visible.Error().ToString();
        ASSERT_TRUE(hidden.HasValue()) << hidden.Error().ToString();
        EXPECT_GT(DifferentPixelsIn(*visible, *hidden, kReadoutBand), 250U)
            << "showEnvironmentReadout changed no readable pixels in the top-centre HUD band";

        if (RenderHarness::RenderingInSoftware())
        {
            const auto reference = RenderHarness::LoadPng(ReferencePath());
            ASSERT_TRUE(reference.HasValue()) << reference.Error().ToString();
            const auto diff =
                Compare(*visible, *reference, 2, RenderHarness::NonDeterministicRegions(kWidth, kHeight));
            EXPECT_FALSE(diff.sizeMismatch);
            EXPECT_LT(diff.DifferingFraction(), 0.002) << diff.ToString();
        }
    }

    TEST(EnvironmentReadoutRenderTests, DISABLED_RegenerateHudSeason01)
    {
        ASSERT_TRUE(RenderHarness::RenderingInSoftware());
        Capture(true, ReferencePath());
    }

} // namespace
