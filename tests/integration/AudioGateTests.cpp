// SPDX-License-Identifier: MIT
//
// `HOUSE-00154`'s acceptance, run for real: "`--no-audio` and a missing device both leave the game
// fully playable". A unit test cannot make that claim -- it is about the whole `Game` starting,
// drawing frames and exiting cleanly with no sound anywhere in it.
#include <gtest/gtest.h>

#include <filesystem>
#include <format>
#include <fstream>
#include <sstream>

#include "cnahouse/app/CnaHouseGame.hpp"
#include "cnahouse/app/CommandLine.hpp"
#include "cnahouse/app/Settings.hpp"
#include "cnahouse/audio/AudioSystem.hpp"
#include "cnahouse/audio/FootstepDirector.hpp"
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

        SCOPED_TRACE(std::format("exit code {}, frames drawn {}, audio state {}, silent reason '{}'",
                                 game.ExitCode(),
                                 game.FramesDrawn(),
                                 static_cast<int>(game.Audio().State()),
                                 game.Audio().SilentReason()));
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

    TEST(AudioGateTests, SavedMixIsAppliedBeforeTheAudioGestureGateOpens)
    {
        Options options;
        options.headless = true;
        options.noAudio = true;
        options.contentRoot = CNAHOUSE_TEST_CONTENT_ROOT;

        Settings settings = SmallSettings();
        settings.masterVolume = 0.40F;
        settings.footstepsVolume = 0.30F;
        settings.ambienceVolume = 0.20F;
        settings.weatherVolume = 0.10F;

        CnaHouseGame game(options, settings);

        EXPECT_FLOAT_EQ(game.Audio().MasterVolume(), 0.40F);
        EXPECT_FLOAT_EQ(game.Audio().CategoryVolume(cnahouse::audio::Category::Footsteps), 0.30F);
        EXPECT_FLOAT_EQ(game.Audio().CategoryVolume(cnahouse::audio::Category::Ambience), 0.20F);
        EXPECT_FLOAT_EQ(game.Audio().CategoryVolume(cnahouse::audio::Category::Weather), 0.10F);
        EXPECT_EQ(game.Audio().State(), AudioState::Silent)
            << "applying persisted settings must not open an audio device";
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

        cnahouse::audio::FootstepDirector footsteps(audio, 0x01920ULL);
        footsteps.BindBanks(contents.audioBanks);
        std::size_t mappedBanks = 0U;
        std::size_t mappedSurfaces = 0U;
        for (const cnahouse::world::AudioBank& bank : contents.audioBanks)
        {
            if (bank.surfaces.empty())
            {
                continue;
            }
            ++mappedBanks;
            mappedSurfaces += bank.surfaces.size();
            footsteps.Reset();
            cnahouse::audio::FootstepStep step;
            step.surface = bank.surfaces.front();
            step.distanceMeters = cnahouse::audio::FootstepDirector::kWalkStrideMeters;
            step.onGround = true;
            const auto selected = footsteps.Advance(step);
            ASSERT_TRUE(selected.has_value()) << bank.surfaces.front();
            EXPECT_EQ(selected->bank, bank.id) << bank.surfaces.front();
        }
        EXPECT_EQ(mappedBanks, 6U);
        EXPECT_EQ(mappedSurfaces, 22U);
    }

    TEST(AudioGateTests, TheWalkStartsOnlyTheFourRetainedAmbienceVoices)
    {
        const auto original = std::filesystem::current_path();
        std::filesystem::current_path(std::filesystem::path(CNAHOUSE_TEST_CONTENT_ROOT).parent_path());
        Options options;
        options.contentRoot = CNAHOUSE_TEST_CONTENT_ROOT;
        options.scene = "walk";
        options.timeOfDay = 12.0F;
        CnaHouseGame game(options, SmallSettings());
        game.SetFrameLimit(4);
        game.Run();
        std::filesystem::current_path(original);
        ASSERT_EQ(game.ExitCode(), 0);
        ASSERT_EQ(game.Audio().State(), AudioState::Ready) << game.Audio().Summary();
        EXPECT_TRUE(game.Audio().AmbienceStarted());
        EXPECT_EQ(game.Audio().AmbienceVoiceCount(), 4U);
    }

    TEST(AudioGateTests, OrdinaryControllerWalkPlaysFootstepsButStandingDoesNot)
    {
        const auto original = std::filesystem::current_path();
        std::filesystem::current_path(std::filesystem::path(CNAHOUSE_TEST_CONTENT_ROOT).parent_path());
        Options options;
        options.contentRoot = CNAHOUSE_TEST_CONTENT_ROOT;
        options.scene = "walk";
        options.player = std::array<float, 5>{0.0F, -2.30F, -15.0F, 0.0F, 0.0F};
        CnaHouseGame game(options, SmallSettings());

        class WalkInput final : public cnahouse::player::IInputSource
        {
        public:
            explicit WalkInput(CnaHouseGame& game)
                : game_(game)
            {
            }

            void Update(float) override
            {
                state_ = {};
                if (game_.FixedStepsForTesting() >= 120U)
                {
                    state_.move.Y = 1.0F;
                }
                else
                {
                    EXPECT_EQ(game_.Audio().OneShotsPlayed(), 0U);
                }
            }

            const cnahouse::player::InputState& Current() const noexcept override
            {
                return state_;
            }

            bool LookAvailable() const noexcept override
            {
                return false;
            }

            CnaHouseGame& game_;
            cnahouse::player::InputState state_;
        } input(game);

        game.SetInputSourceForTesting(&input);
        game.SetFixedStepLimit(480U);
        game.SetFrameLimit(4000U);
        game.Run();
        std::filesystem::current_path(original);
        ASSERT_EQ(game.ExitCode(), 0);
        ASSERT_EQ(game.Audio().State(), AudioState::Ready) << game.Audio().Summary();
        EXPECT_LT(game.PlayerForTesting().Feet().Z, -18.0F);
        EXPECT_GE(game.Audio().OneShotsPlayed(), 4U);
        EXPECT_LE(game.Audio().OneShotsPlayed(), 6U);
    }
} // namespace
