// SPDX-License-Identifier: MIT
//
// `HOUSE-00154`'s acceptance, run for real: "`--no-audio` and a missing device both leave the game
// fully playable". A unit test cannot make that claim -- it is about the whole `Game` starting,
// drawing frames and exiting cleanly with no sound anywhere in it.
#include <gtest/gtest.h>

#include <fstream>
#include <sstream>

#include "cnahouse/app/CnaHouseGame.hpp"
#include "cnahouse/app/CommandLine.hpp"
#include "cnahouse/app/Settings.hpp"
#include "cnahouse/audio/AudioSystem.hpp"
#include "cnahouse/content/ContentRegistry.hpp"
#include "cnahouse/ui/MenuStack.hpp"
#include "cnahouse/util/Log.hpp"
#include "cnahouse/world/WorldData.hpp"
#include "cnahouse/world/WorldLoader.hpp"

namespace
{
    using cnahouse::app::CnaHouseGame;
    using cnahouse::app::Options;
    using cnahouse::app::Settings;
    using cnahouse::audio::AudioState;

    Settings SmallSettings()
    {
        Settings settings = Settings::Defaults();
        settings.backBufferWidth = 320;
        settings.backBufferHeight = 240;
        settings.verticalSync = false;
        return settings;
    }

    TEST(AudioGateTests, NoAudioRunsAFullSessionAndNeverOpensTheDevice)
    {
        Options options;
        options.headless = true;
        options.noAudio = true;
        options.contentRoot = CNAHOUSE_TEST_CONTENT_ROOT;

        CnaHouseGame game(options, SmallSettings());
        game.SetFrameLimit(30);
        game.Run();

        EXPECT_EQ(game.ExitCode(), 0) << "silence is a supported way to run, not an error path";
        EXPECT_GE(game.FramesDrawn(), 30u);
        EXPECT_EQ(game.Audio().State(), AudioState::Silent);
        EXPECT_NE(game.Audio().SilentReason().find("--no-audio"), std::string::npos);
    }

    TEST(AudioGateTests, AudioStaysClosedUntilAGestureAndTheGameRunsAnyway)
    {
        // No input arrives in a headless run, so no gesture arrives either -- which is exactly the
        // state a browser tab is in before the player clicks. The game must be fully playable there,
        // because that is where the title screen lives.
        Options options;
        options.headless = true;
        options.contentRoot = CNAHOUSE_TEST_CONTENT_ROOT;

        CnaHouseGame game(options, SmallSettings());
        game.SetFrameLimit(30);
        game.Run();

        EXPECT_EQ(game.ExitCode(), 0);
        EXPECT_GE(game.FramesDrawn(), 30u);
        EXPECT_EQ(game.Audio().State(), AudioState::Waiting)
            << "the device must not be opened by anything other than a user gesture";
        EXPECT_FLOAT_EQ(game.Audio().EffectiveVolume(cnahouse::audio::Category::Footsteps), 0.0f);
    }

    TEST(AudioGateTests, TheTitleScreenIsUpForTheWholeSessionUntilSomethingIsPressed)
    {
        // `HOUSE-00155` / `HOUSE-00156` end to end. No input arrives in a headless run, so the
        // title screen must still be on the stack after 30 real frames -- which is exactly the
        // browser-tab state, and the frames prove the game is drawing rather than blocked.
        Options options;
        options.headless = true;
        options.contentRoot = CNAHOUSE_TEST_CONTENT_ROOT;

        CnaHouseGame game(options, SmallSettings());
        game.SetFrameLimit(30);
        game.Run();

        EXPECT_EQ(game.ExitCode(), 0);
        EXPECT_GE(game.FramesDrawn(), 30u);
        ASSERT_FALSE(game.Menus().Empty()) << "the title screen dismissed with nothing pressed";
        ASSERT_NE(game.Menus().Top(), nullptr);
        EXPECT_EQ(game.Menus().Top()->Id(), cnahouse::ui::ScreenId::Loading);
        EXPECT_TRUE(game.Menus().WorldIsPaused())
            << "the house has not been built yet, so there is nothing to keep running";
        EXPECT_EQ(game.Audio().State(), AudioState::Waiting)
            << "and the audio device is still closed, which is the point of the gate";
    }

    TEST(AudioGateTests, TheStartupLogSaysWhatTheAudioStateIs)
    {
        // It belongs in the header for the same reason the renderer and tier do: "there was no
        // sound" is one of the most commonly reported symptoms and the least self-explanatory.
        cnahouse::util::Log::ClearRing();

        Options options;
        options.headless = true;
        options.noAudio = true;
        options.contentRoot = CNAHOUSE_TEST_CONTENT_ROOT;

        CnaHouseGame game(options, SmallSettings());
        game.SetFrameLimit(3);
        game.Run();

        bool found = false;
        for (const auto& record : cnahouse::util::Log::Ring())
        {
            if (record.message.find("audio") != std::string::npos)
            {
                found = true;
            }
        }
        EXPECT_TRUE(found) << "no line in the startup log mentions audio at all";
    }

    TEST(AudioGateTests, EveryDeployedBankResolvesAgainstTheDeployedManifest)
    {
        const std::string worldDirectory = std::string(CNAHOUSE_TEST_CONTENT_ROOT) + "/world";
        cnahouse::world::WorldData::Contents contents;
        const auto layout = cnahouse::world::WorldLoader::LoadAudio(worldDirectory, contents);
        ASSERT_TRUE(layout) << layout.Error().ToString();

        const std::string manifestPath = worldDirectory + "/assets.manifest.json";
        std::ifstream file(manifestPath);
        ASSERT_TRUE(file.is_open()) << manifestPath;
        std::ostringstream text;
        text << file.rdbuf();

        cnahouse::content::ContentRegistry registry;
        const auto manifest = registry.LoadFromJson(text.str(), manifestPath);
        ASSERT_TRUE(manifest) << manifest.Error().ToString();

        cnahouse::audio::AudioSystem audio(false);
        audio.LoadBanks(contents.audioBanks, registry);
        EXPECT_EQ(audio.BankCount(), 13U);
        EXPECT_TRUE(audio.BankProblems().empty());
        EXPECT_EQ(audio.State(), AudioState::Silent)
            << "resolving bank metadata must not open the audio device";
    }
} // namespace
