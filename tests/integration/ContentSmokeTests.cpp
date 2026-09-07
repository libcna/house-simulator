// SPDX-License-Identifier: MIT
//
// `HOUSE-00201`. The content smoke scene, checked for the things a picture cannot show: that the
// sound reached the mixer, that the video's play position actually moved, that the six names
// resolved to the six intended assets, and that no stale `.xnb` is shadowing a `.cnb`.
//
// It runs the whole game, because every one of those happens inside `LoadContent` or `Update` and
// nothing outside a frame can observe them (the same reason `TierFallbackTests` exists).
#include <filesystem>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "cnahouse/app/CnaHouseGame.hpp"
#include "cnahouse/app/CommandLine.hpp"
#include "cnahouse/app/Settings.hpp"
#include "cnahouse/content/SmokeScene.hpp"

namespace
{
    using cnahouse::app::CnaHouseGame;
    using cnahouse::app::Options;
    using cnahouse::app::RenderTier;
    using cnahouse::app::Settings;
    using cnahouse::content::SmokeScene;

    Settings SmallSettings()
    {
        Settings settings = Settings::Defaults();
        settings.backBufferWidth = 320;
        settings.backBufferHeight = 240;
        settings.verticalSync = false;
        return settings;
    }

    Options SmokeOptions()
    {
        Options options;
        options.headless = true;
        options.scene = SmokeScene::kSceneName;
        options.contentRoot = CNAHOUSE_TEST_CONTENT_ROOT;
        options.effectRoot = CNAHOUSE_TEST_EFFECT_ROOT;
        options.tier = RenderTier::E;
        // Audio ON, which is the point of the sound half of this fixture. Every other integration
        // test disables it; a smoke test that disabled it could not tell a sound that failed to
        // load from one that was never asked to play.
        options.noAudio = false;
        return options;
    }

    /// Runs the scene for `frames` and returns what it recorded. A copy, because the game -- and
    /// the report inside it -- is destroyed when this returns.
    cnahouse::content::SmokeReport RunSmoke(Options options, std::uint64_t frames)
    {
        CnaHouseGame game(options, SmallSettings());
        game.SetFrameLimit(frames);
        game.Run();
        const cnahouse::content::SmokeReport* report = game.SmokeReport();
        return report == nullptr ? cnahouse::content::SmokeReport{} : *report;
    }

    TEST(ContentSmokeTests, AllSixContentTypesLoad)
    {
        const auto report = RunSmoke(SmokeOptions(), 90);

        // Reported one at a time, not as a single `AllRequiredLoaded`, because the whole reason
        // this scene exists is to say WHICH of the six is broken.
        EXPECT_TRUE(report.model.loaded) << report.model.contentName << ": " << report.model.error;
        EXPECT_TRUE(report.texture.loaded) << report.texture.contentName << ": " << report.texture.error;
        EXPECT_TRUE(report.font.loaded) << report.font.contentName << ": " << report.font.error;
        EXPECT_TRUE(report.sound.loaded) << report.sound.contentName << ": " << report.sound.error;
        EXPECT_TRUE(report.video.loaded) << report.video.contentName << ": " << report.video.error;
        EXPECT_TRUE(report.effect.loaded) << report.effect.contentName << ": " << report.effect.error
                                          << "\n(this build has Tier E, so the compiled effect must load)\n"
                                          << report.ToString();
        EXPECT_TRUE(report.AllRequiredLoaded()) << report.ToString();
    }

    TEST(ContentSmokeTests, TheNamesResolveToTheIntendedAssets)
    {
        const auto report = RunSmoke(SmokeOptions(), 30);

        // The names, and what was found behind them. A `Model` that loaded is not evidence that the
        // RIGHT model loaded: `Caches` falls back to `Models/Fallback/box` when a load fails, and
        // the fallback box is a cube too. The marker is 0.8 m where the fallback is 1.0 m, and the
        // texture is 64x64 where the fallback grey is 4x4 -- so the dimensions are the evidence.
        EXPECT_EQ(report.model.contentName, std::string("Models/Smoke/marker"));
        EXPECT_EQ(report.texture.contentName, std::string("Textures/Smoke/quadrants"));
        EXPECT_EQ(report.font.contentName, std::string("Fonts/ui-22"));
        EXPECT_EQ(report.sound.contentName, std::string("Audio/Smoke/chime"));
        EXPECT_EQ(report.effect.contentName, std::string("Effects/P1Probe"));
        EXPECT_EQ(report.video.contentName, std::string("Video/smoke_clip"));

        EXPECT_NE(report.texture.detail.find("64x64"), std::string::npos)
            << "the texture is " << report.texture.detail
            << ", not the 64x64 quadrant fixture -- a 4x4 here is the grey fallback";
        EXPECT_NE(report.video.detail.find("64x64"), std::string::npos) << report.video.detail;
        EXPECT_NE(report.video.detail.find("2.00 s"), std::string::npos) << report.video.detail;
        EXPECT_NE(report.sound.detail.find("0.300 s"), std::string::npos)
            << "the sound is " << report.sound.detail
            << ", not the 0.3 s chime -- 0.250 s here is the silent fallback";
        // 190 glyphs is the Latin-1 repertoire minus soft hyphen that `check_fonts.py` enforces.
        EXPECT_NE(report.font.detail.find("190 glyph"), std::string::npos) << report.font.detail;
    }

    TEST(ContentSmokeTests, TheSoundReachesTheMixer)
    {
        const auto report = RunSmoke(SmokeOptions(), 60);
        ASSERT_TRUE(report.sound.loaded) << report.sound.error;
        // Started, not merely loaded. `HOUSE-00155` keeps the device silent until a user gesture,
        // so a `Play` before the gate opens is a sound that never happens -- and the scene opens
        // the gate itself precisely because `--scene` has no player to press a key.
        EXPECT_TRUE(report.soundStarted) << "the chime loaded but the mixer reports it as '"
                                         << report.soundState << "' rather than playing\n"
                                         << report.ToString();
        EXPECT_EQ(report.soundState, std::string("playing")) << report.ToString();
    }

    TEST(ContentSmokeTests, TheVideoAdvancesRatherThanShowingOneFrameForever)
    {
        const auto report = RunSmoke(SmokeOptions(), 240);
        ASSERT_TRUE(report.video.loaded) << report.video.error;
        EXPECT_GT(report.videoFramesShown, 0) << "VideoPlayer::GetTexture never returned a frame\n"
                                              << report.ToString();
        // The claim that matters. A player that hands back frame 0 forever satisfies every check
        // that only asks whether a texture came back, and that is exactly the bug this fixture is
        // here to catch.
        EXPECT_TRUE(report.videoAdvanced)
            << "the play position never moved: the decoder produced a frame and then stopped\n"
            << report.ToString();
        EXPECT_GT(report.videoPositionSeconds, 0.0) << report.ToString();
    }

    TEST(ContentSmokeTests, TheCompiledEffectReachesTheDraw)
    {
        const auto report = RunSmoke(SmokeOptions(), 30);
        ASSERT_TRUE(report.effect.loaded) << report.effect.error;
        // Which effect drew, not merely that something did. `Model::Draw` would produce a picture
        // too -- a picture in which the compiled effect had never been used.
        EXPECT_EQ(report.techniqueDrawn, std::string(SmokeScene::kTechnique)) << report.ToString();
        EXPECT_GT(report.effectDrawCalls, 0)
            << "the technique was selected but no draw call went through it\n"
            << report.ToString();
    }

    TEST(ContentSmokeTests, TierSIsCompleteWithoutTheCompiledEffect)
    {
        // ADR-0003: Tier S is COMPLETE. The smoke scene must therefore report five of six loaded
        // and still say the session is sound -- if it called Tier S a failure, it would contradict
        // the architecture it is smoke-testing.
        Options options = SmokeOptions();
        options.tier = RenderTier::S;
        const auto report = RunSmoke(options, 30);

        EXPECT_FALSE(report.tierEAvailable);
        EXPECT_FALSE(report.effect.loaded);
        EXPECT_NE(report.effect.error.find("Tier S"), std::string::npos) << report.effect.error;
        EXPECT_TRUE(report.AllRequiredLoaded()) << "Tier S must be a complete session, not a failed one\n"
                                                << report.ToString();
        EXPECT_EQ(report.techniqueDrawn, std::string(SmokeScene::kStockTechnique)) << report.ToString();
        // The other five are unaffected by the tier.
        EXPECT_TRUE(report.model.loaded && report.texture.loaded && report.font.loaded &&
                    report.sound.loaded && report.video.loaded)
            << report.ToString();
    }

    TEST(ContentSmokeTests, NoStaleXnbShadowsTheCnbTree)
    {
        // `HOUSE-00064` measured that **`.xnb` wins** the content resolution order, so one left
        // behind in the `.cnb` tree silently shadows whatever the build just produced -- and the
        // symptom is an asset that is simply the wrong version, with nothing in any log. The two
        // roots exist to make that impossible; this is the check that they still are two.
        const std::filesystem::path root(CNAHOUSE_TEST_CONTENT_ROOT);
        ASSERT_TRUE(std::filesystem::is_directory(root)) << root.string();

        std::vector<std::string> shadows;
        for (const auto& entry : std::filesystem::recursive_directory_iterator(root))
        {
            if (entry.is_regular_file() && entry.path().extension() == ".xnb")
            {
                shadows.push_back(entry.path().string());
            }
        }
        std::string names;
        for (const auto& shadow : shadows)
        {
            names += "\n  " + shadow;
        }
        EXPECT_TRUE(shadows.empty())
            << "the .cnb content tree holds " << shadows.size()
            << " .xnb file(s), which win the resolution order and shadow the .cnb beside them:" << names;

        // ...and the effect tree is the mirror image: `.xnb` only, no `.cnb`.
        const std::filesystem::path effects(CNAHOUSE_TEST_EFFECT_ROOT);
        if (std::filesystem::is_directory(effects))
        {
            std::vector<std::string> strays;
            for (const auto& entry : std::filesystem::recursive_directory_iterator(effects))
            {
                if (entry.is_regular_file() && entry.path().extension() == ".cnb")
                {
                    strays.push_back(entry.path().string());
                }
            }
            EXPECT_TRUE(strays.empty()) << "the effect tree holds " << strays.size() << " .cnb file(s)";
        }
    }

    TEST(ContentSmokeTests, TheStreamedVideoSitsWhereTheRuntimeResolvesIt)
    {
        // MEASURED, `HOUSE-00201`. A `Video`'s `.cnb` is metadata plus a reference to a media file
        // that the runtime resolves through `ContentManager::BuildAssetPath` -- relative to the
        // CONTENT ROOT, not to the `.cnb` beside it. That is why `assets-src/Media/` is built with
        // the content root as its output directory while every other tree is built one
        // subdirectory at a time. If someone "tidies" that back, the video stops loading and the
        // reason is invisible; this is the check that says why.
        const std::filesystem::path root(CNAHOUSE_TEST_CONTENT_ROOT);
        EXPECT_TRUE(std::filesystem::exists(root / "Video" / "smoke_clip.cnb"));
        EXPECT_TRUE(std::filesystem::exists(root / "Video" / "smoke_clip.ogv"))
            << "the streamed media is not beside its metadata at the path the content name "
               "resolves to; see the CNAHOUSE_CNB_MEDIA_DIR comment in CMakeLists.txt";
        EXPECT_FALSE(std::filesystem::exists(root / "Video" / "Video" / "smoke_clip.ogv"))
            << "the stream was deployed one directory too deep, which is what happens when the "
               "media tree is built into content/Video instead of content";
    }

} // namespace
