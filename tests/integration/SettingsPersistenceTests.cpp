// SPDX-License-Identifier: MIT
#include <string_view>
#include <gtest/gtest.h>

#include <array>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>

#include "cnahouse/app/CnaHouseGame.hpp"
#include "cnahouse/persistence/DesktopSaveStore.hpp"

namespace
{
    class EditSettings final : public cnahouse::player::IInputSource
    {
    public:
        void Update(float) override
        {
            state_ = {};
            state_.anyPressed = frame_ == 0U;
            state_.uiDownPressed = frame_ == 1U;
            state_.uiAcceptPressed = frame_ == 2U;
            state_.uiRightPressed = frame_ == 3U;
            state_.cancelPressed = frame_ == 4U;
            ++frame_;
        }

        const cnahouse::player::InputState& Current() const noexcept override
        {
            return state_;
        }

        bool LookAvailable() const noexcept override
        {
            return false;
        }

    private:
        cnahouse::player::InputState state_;
        unsigned int frame_ = 0U;
    };

    TEST(SettingsPersistenceTests, NormalSettingsEditSurvivesASecondApplicationProcess)
    {
        if (std::string_view(CNAHOUSE_RENDERER_NAME) == "HEADLESS")
        {
            // AM4-318: this test reads a rendered frame back, and HEADLESS rasterises nothing and
            // refuses render-target readback. It runs in full on every drawing renderer.
            GTEST_SKIP() << "needs render-target readback, which HEADLESS does not have";
        }
#if defined(__linux__) && !defined(__ANDROID__)
        using namespace cnahouse;
        auto store = persistence::DesktopSaveStore::Open();
        ASSERT_TRUE(store) << store.Error().ToString();
        ASSERT_NE((*store)->Location().find("/cnahouse-integration-"), std::string::npos);
        ASSERT_TRUE((*store)->Delete("settings.json"));
        app::Options options;
        options.contentRoot = CNAHOUSE_TEST_CONTENT_ROOT;
        options.noAudio = true;
        options.effectRoot =
            (std::filesystem::path(CNAHOUSE_TEST_CONTENT_ROOT).parent_path() / "content-fx").string();
        const auto directory = std::filesystem::path(CNAHOUSE_TEST_CONTENT_ROOT).parent_path();
        const auto output = directory / "test-output/house-03645";
        std::filesystem::create_directories(output);
        options.screenshotFrame = 4;
        options.screenshot = (output / "settings-page.png").string();
        app::Settings initial = app::Settings::Defaults();
        initial.backBufferWidth = 640;
        initial.backBufferHeight = 480;
        initial.verticalSync = false;
        EditSettings input;
        app::CnaHouseGame game(options, initial);
        game.SetInputSourceForTesting(&input);
        game.SetFrameLimit(8U);
        game.Run();
        ASSERT_EQ(game.ExitCode(), 0);
        ASSERT_TRUE((*store)->Exists("settings.json"));
        const auto saved = (*store)->Read("settings.json");
        ASSERT_TRUE(saved);
        const auto settings = app::Settings::FromJson(*saved, "settings.json");
        ASSERT_TRUE(settings);
        EXPECT_EQ(settings->quality, game.UserSettings().quality);
        EXPECT_EQ(settings->backBufferWidth, 640);
        EXPECT_EQ(settings->backBufferHeight, 480);
        EXPECT_FALSE(settings->verticalSync);

        ASSERT_EQ(directory.string().find('\''), std::string::npos);
        for (const bool overrideQuality : {false, true})
        {
            const std::string suffix = overrideQuality ? "override" : "restart";
            const std::string log = (output / (suffix + ".log")).string();
            const std::string png = (output / (suffix + ".png")).string();
            const std::string command = "'" + (directory / "cna-house").string() +
                                        "' --no-audio --screenshot-frame=3 --screenshot='" + png + "'" +
                                        (overrideQuality ? " --quality=low" : "") + " > '" + log + "' 2>&1";
            ASSERT_EQ(std::system(command.c_str()), 0);
            std::ifstream logfile(log);
            std::ostringstream text;
            text << logfile.rdbuf();
            const auto expected = overrideQuality ? app::QualityPreset::Low : settings->quality;
            EXPECT_NE(
                text.str().find("quality " + std::string(app::QualityPresetName(expected)) + " (requested)"),
                std::string::npos)
                << text.str();
            EXPECT_NE(text.str().find("loaded settings from"), std::string::npos);
            std::ifstream image(png, std::ios::binary);
            std::array<unsigned char, 24> header{};
            image.read(reinterpret_cast<char*>(header.data()), static_cast<std::streamsize>(header.size()));
            ASSERT_EQ(image.gcount(), static_cast<std::streamsize>(header.size()));
            const auto dimension = [&header](std::size_t offset)
            {
                return (static_cast<unsigned int>(header[offset]) << 24U) |
                       (static_cast<unsigned int>(header[offset + 1U]) << 16U) |
                       (static_cast<unsigned int>(header[offset + 2U]) << 8U) |
                       static_cast<unsigned int>(header[offset + 3U]);
            };
            EXPECT_EQ(dimension(16U), 640U);
            EXPECT_EQ(dimension(20U), 480U);
        }
        EXPECT_EQ(*(*store)->Read("settings.json"), *saved)
            << "a diagnostic CLI override must not overwrite the saved preset";

        ASSERT_TRUE((*store)->Write("settings.json", "{broken"));
        const auto checkFallback =
            [&](const std::string& prefix, const std::string& suffix, const std::string& warning)
        {
            const std::string log = (output / (suffix + ".log")).string();
            const std::string command = prefix + "'" + (directory / "cna-house").string() +
                                        "' --no-audio --screenshot-frame=3 --screenshot='" +
                                        (output / (suffix + ".png")).string() + "' > '" + log + "' 2>&1";
            EXPECT_EQ(std::system(command.c_str()), 0);
            std::ifstream logfile(log);
            std::ostringstream text;
            text << logfile.rdbuf();
            EXPECT_NE(text.str().find(warning), std::string::npos) << text.str();
        };
        checkFallback("", "corrupt", "settings invalid; using defaults");
        EXPECT_EQ(*(*store)->Read("settings.json"), "{broken")
            << "invalid preferences are preserved until an intentional edit";
        const auto blockedRoot = output / "not-a-directory";
        {
            std::ofstream blocker(blockedRoot);
            blocker << "private test storage obstruction";
        }
        checkFallback(
            "XDG_DATA_HOME='" + blockedRoot.string() + "' ", "unwritable", "settings store unavailable");
        ASSERT_TRUE((*store)->Write("settings.json", *saved));
#else
        GTEST_SKIP() << "desktop Linux settings/restart regression";
#endif
    }
} // namespace
