// SPDX-License-Identifier: MIT
//
// `HOUSE-00131`'s acceptance, including a v1 -> v2 migration fixture.
#include <gtest/gtest.h>

#include "cnahouse/app/Settings.hpp"
#include "cnahouse/player/FirstPersonCamera.hpp"
#include "cnahouse/player/HeadBob.hpp"

namespace
{
    using cnahouse::app::QualityPreset;
    using cnahouse::app::Settings;
    using cnahouse::app::WeatherMode;
    using cnahouse::app::WeatherModeName;
    using cnahouse::player::HeadBobLevelName;

    TEST(SettingsTests, DefaultsAreTheDocumentedOnes)
    {
        const Settings settings = Settings::Defaults();
        EXPECT_EQ(settings.version, Settings::kCurrentVersion);
        EXPECT_EQ(settings.backBufferWidth, 1600);
        EXPECT_EQ(settings.backBufferHeight, 900);
        EXPECT_EQ(settings.quality, QualityPreset::High);
        EXPECT_FLOAT_EQ(settings.masterVolume, 0.80f);
        EXPECT_FLOAT_EQ(settings.footstepsVolume, 0.85f);
        EXPECT_FLOAT_EQ(settings.ambienceVolume, 0.75f);
        EXPECT_FLOAT_EQ(settings.weatherVolume, 0.75f);
        EXPECT_TRUE(settings.showEnvironmentReadout);
        EXPECT_FLOAT_EQ(settings.moonPhaseSpeedMultiplier, 1.0F);
        EXPECT_EQ(settings.weatherMode, WeatherMode::On);
        EXPECT_EQ(settings.fixedWeatherArchetype, "W_PARTLY");
    }

    TEST(SettingsTests, RoundTripsThroughItsOwnJson)
    {
        Settings written = Settings::Defaults();
        written.backBufferWidth = 1920;
        written.backBufferHeight = 1080;
        written.fullscreen = true;
        written.quality = QualityPreset::Medium;
        written.masterVolume = 0.75f;
        written.footstepsVolume = 0.65f;
        written.ambienceVolume = 0.55f;
        written.weatherVolume = 0.45f;
        written.mouseSensitivity = 1.5f;
        written.invertY = true;
        written.fieldOfView = 90.0f;
        written.headBob = cnahouse::player::HeadBobLevel::Off;
        written.showEnvironmentReadout = false;
        written.moonPhaseSpeedMultiplier = 6.5F;
        written.weatherMode = WeatherMode::Fixed;
        written.fixedWeatherArchetype = "W_HEAVY_SNOW";

        auto read = Settings::FromJson(written.ToJson(), "settings.json");
        ASSERT_TRUE(read) << read.Error().ToString();
        EXPECT_EQ(read->backBufferWidth, 1920);
        EXPECT_EQ(read->backBufferHeight, 1080);
        EXPECT_TRUE(read->fullscreen);
        EXPECT_EQ(read->quality, QualityPreset::Medium);
        EXPECT_FLOAT_EQ(read->masterVolume, 0.75f);
        EXPECT_FLOAT_EQ(read->footstepsVolume, 0.65f);
        EXPECT_FLOAT_EQ(read->ambienceVolume, 0.55f);
        EXPECT_FLOAT_EQ(read->weatherVolume, 0.45f);
        EXPECT_FLOAT_EQ(read->mouseSensitivity, 1.5f);
        EXPECT_TRUE(read->invertY);
        EXPECT_FLOAT_EQ(read->fieldOfView, 90.0f);
        EXPECT_EQ(read->headBob, cnahouse::player::HeadBobLevel::Off)
            << "§68's level did not survive the file";
        EXPECT_FALSE(read->showEnvironmentReadout)
            << "the player's choice to hide HOUSE-01546's readout did not survive the file";
        EXPECT_FLOAT_EQ(read->moonPhaseSpeedMultiplier, 6.5F);
        EXPECT_EQ(read->weatherMode, WeatherMode::Fixed);
        EXPECT_EQ(read->fixedWeatherArchetype, "W_HEAVY_SNOW");
    }

    TEST(SettingsTests, EveryWeatherModeHasAStableFileName)
    {
        for (const WeatherMode mode : {WeatherMode::On, WeatherMode::Fixed, WeatherMode::Off})
        {
            Settings written = Settings::Defaults();
            written.weatherMode = mode;
            const auto read = Settings::FromJson(written.ToJson(), "settings.json");
            ASSERT_TRUE(read) << read.Error().ToString();
            EXPECT_EQ(read->weatherMode, mode);
            EXPECT_NE(written.ToJson().find(std::string{"\"weatherMode\": \""} +
                                            std::string{WeatherModeName(mode)} + "\""),
                      std::string::npos);
        }

        const auto future =
            Settings::FromJson(R"({"version":99,"weatherMode":"seasonal-ai"})", "settings.json");
        ASSERT_TRUE(future);
        EXPECT_EQ(future->weatherMode, WeatherMode::On);
    }

    TEST(SettingsTests, TheHeadBobLevelIsAName)
    {
        // §68 gives three levels and the file spells them, because a settings file is text a
        // person edits: `"headBob": 2` would make them count, and counting from a number they
        // cannot see is how a file ends up meaning something nobody intended.
        for (const auto level : {cnahouse::player::HeadBobLevel::Off,
                                 cnahouse::player::HeadBobLevel::Subtle,
                                 cnahouse::player::HeadBobLevel::Normal})
        {
            Settings written = Settings::Defaults();
            written.headBob = level;
            const std::string json = written.ToJson();
            EXPECT_NE(json.find(std::string("\"headBob\": \"") + std::string(HeadBobLevelName(level)) + "\""),
                      std::string::npos)
                << json;
            auto read = Settings::FromJson(json, "settings.json");
            ASSERT_TRUE(read) << read.Error().ToString();
            EXPECT_EQ(read->headBob, level);
        }

        // An unknown level falls back to the default rather than failing, the rule the quality
        // preset already follows: a file from a build with a fourth level must still load here.
        auto odd = Settings::FromJson(R"({"version": 4, "headBob": "violent"})", "settings.json");
        ASSERT_TRUE(odd) << odd.Error().ToString();
        EXPECT_EQ(odd->headBob, cnahouse::player::HeadBobLevel::Subtle);
    }

    TEST(SettingsTests, AVersionOneFileMigratesRatherThanBeingDiscarded)
    {
        // The v1 -> v2 migration fixture. v1 had no `ambienceVolume`; a v1 file must load, keep every
        // value the user did set, take the default for the new field, and come out at the current
        // version. Discarding it instead would re-teach every developer's resolution once a week,
        // which is exactly what `R-15` is about.
        constexpr std::string_view kVersionOne = R"({
      "version": 1,
      "backBufferWidth": 1280,
      "backBufferHeight": 720,
      "fullscreen": false,
      "verticalSync": false,
      "quality": "low",
      "masterVolume": 0.5,
      "effectsVolume": 0.25,
      "mouseSensitivity": 2.0,
      "invertY": true,
      "fieldOfView": 85.0
    })";

        auto settings = Settings::FromJson(kVersionOne, "settings.json");
        ASSERT_TRUE(settings) << settings.Error().ToString();

        EXPECT_EQ(settings->version, Settings::kCurrentVersion) << "migrated, not left at v1";
        EXPECT_EQ(settings->backBufferWidth, 1280) << "the user's own values survive";
        EXPECT_EQ(settings->quality, QualityPreset::Low);
        EXPECT_FLOAT_EQ(settings->masterVolume, 0.5f);
        EXPECT_FLOAT_EQ(settings->footstepsVolume, 0.25f)
            << "the old effects category migrates to the retained footsteps category";
        EXPECT_TRUE(settings->invertY);
        EXPECT_FLOAT_EQ(settings->ambienceVolume, 0.75f)
            << "the field v1 did not have takes its default rather than zero";
        EXPECT_FLOAT_EQ(settings->weatherVolume, 0.75f)
            << "the weather category added in v9 takes the compact-mix default";
        // v3 -> v4 added §68's head bob, and §44 says the motion is on by default: a player who
        // has never seen the setting has not turned it off.
        EXPECT_EQ(settings->headBob, cnahouse::player::HeadBobLevel::Subtle);
        EXPECT_TRUE(settings->showEnvironmentReadout)
            << "a pre-HOUSE-01546 file takes the documented visible default";
        EXPECT_EQ(settings->weatherMode, WeatherMode::On);
        EXPECT_EQ(settings->fixedWeatherArchetype, "W_PARTLY");
        EXPECT_FLOAT_EQ(settings->moonPhaseSpeedMultiplier, 1.0F);
    }

    TEST(SettingsTests, AFileFromANewerBuildStillLoads)
    {
        // The other half of the migration story, and the direction a version number cannot help with:
        // a developer who switches branches must not lose their settings because the newer branch
        // added a field. Unknown fields are ignored, deliberately.
        constexpr std::string_view kFromTheFuture = R"({
      "version": 99,
      "backBufferWidth": 2560,
      "somethingWeHaveNotInventedYet": { "nested": [1, 2, 3] },
      "raytracing": true
    })";
        auto settings = Settings::FromJson(kFromTheFuture, "settings.json");
        ASSERT_TRUE(settings) << settings.Error().ToString();
        EXPECT_EQ(settings->backBufferWidth, 2560);
    }

    TEST(SettingsTests, AWrongTypeIsStillAnErrorEvenThoughFieldsAreOptional)
    {
        // "Optional" means the field may be absent, not that a string may stand in for a number.
        auto settings = Settings::FromJson(R"({"backBufferWidth": "wide"})", "settings.json");
        ASSERT_FALSE(settings);
        EXPECT_EQ(settings.Error().Context(), "backBufferWidth");
    }

    TEST(SettingsTests, MalformedJsonNamesTheFile)
    {
        auto settings = Settings::FromJson("{not json", "~/.config/settings.json");
        ASSERT_FALSE(settings);
        EXPECT_EQ(settings.Error().Context(), "~/.config/settings.json");
    }

    TEST(SettingsTests, OutOfRangeValuesAreClampedAndReported)
    {
        // A settings file is user-editable text. A resolution of 0 reaches the device as a division by
        // zero and a volume of 40 reaches the mixer as clipping.
        Settings settings = Settings::Defaults();
        settings.backBufferWidth = 0;
        settings.backBufferHeight = 99999;
        settings.masterVolume = 40.0f;
        settings.mouseSensitivity = -1.0f;
        settings.fieldOfView = 179.0f;
        settings.fixedWeatherArchetype.clear();
        settings.moonPhaseSpeedMultiplier = 99.0F;

        const std::string changed = settings.ClampToSupportedRanges();
        EXPECT_FALSE(changed.empty()) << "the user is told, not silently overruled";
        EXPECT_EQ(settings.backBufferWidth, 640);
        EXPECT_EQ(settings.backBufferHeight, 4320);
        EXPECT_FLOAT_EQ(settings.masterVolume, 1.0f);
        // §44's band, tightened from the 0.05-10 this file accepted until `HOUSE-00625`: 0.05x
        // is a mouse that cannot turn round and 10x is one that spins on a twitch, and a settings
        // file that accepted both had told the player those were supported.
        EXPECT_FLOAT_EQ(settings.mouseSensitivity, 0.2f);
        // §44's field-of-view band, tightened from the 50-110 this file accepted until
        // `HOUSE-00626`, and taken from the camera's own constants rather than copied: a file
        // outside the band was accepted here and then silently corrected by
        // `FirstPersonCamera::SetFieldOfView`, so the player was overruled in the one place they
        // could not see it.
        EXPECT_FLOAT_EQ(settings.fieldOfView, cnahouse::player::kMaxFovDegrees);
        EXPECT_EQ(settings.fixedWeatherArchetype, "W_PARTLY");
        EXPECT_FLOAT_EQ(settings.moonPhaseSpeedMultiplier, 8.0F);

        settings.moonPhaseSpeedMultiplier = 0.0F;
        EXPECT_NE(settings.ClampToSupportedRanges().find("moonPhaseSpeedMultiplier"), std::string::npos);
        EXPECT_FLOAT_EQ(settings.moonPhaseSpeedMultiplier, 1.0F);

        settings.fieldOfView = 10.0f;
        EXPECT_FALSE(settings.ClampToSupportedRanges().empty());
        EXPECT_FLOAT_EQ(settings.fieldOfView, cnahouse::player::kMinFovDegrees);
    }

    TEST(SettingsTests, TheDefaultsAreTheAnglesTheCameraWasBuiltAround)
    {
        // The default lives in `Settings.hpp` as a literal because that header is small and
        // widely included and the camera's is neither. This is what stops the two drifting.
        EXPECT_FLOAT_EQ(Settings::Defaults().fieldOfView, cnahouse::player::kDefaultFovDegrees);
        EXPECT_GE(Settings::Defaults().fieldOfView, cnahouse::player::kMinFovDegrees);
        EXPECT_LE(Settings::Defaults().fieldOfView, cnahouse::player::kMaxFovDegrees);

        // A settings file at either end of the band survives a round trip through the camera
        // unchanged, which is the property the shared constants exist for.
        for (const float degrees : {cnahouse::player::kMinFovDegrees, cnahouse::player::kMaxFovDegrees})
        {
            Settings settings = Settings::Defaults();
            settings.fieldOfView = degrees;
            EXPECT_TRUE(settings.ClampToSupportedRanges().empty()) << degrees << " is inside the band";
            cnahouse::player::FirstPersonCamera camera;
            camera.SetFieldOfView(settings.fieldOfView);
            EXPECT_FLOAT_EQ(camera.FieldOfViewDegrees(), degrees);
        }
    }

    TEST(SettingsTests, ClampingReportsNothingWhenNothingChanged)
    {
        Settings settings = Settings::Defaults();
        EXPECT_TRUE(settings.ClampToSupportedRanges().empty());
    }

    TEST(SettingsTests, AnUnknownQualityNameFallsBackRatherThanFailing)
    {
        // Deliberately different from the type failures above: a quality name is an enumeration whose
        // members will grow, and an older build reading a newer name should run at its own default
        // rather than refuse to start.
        auto settings = Settings::FromJson(R"({"quality": "cinematic"})", "settings.json");
        ASSERT_TRUE(settings);
        EXPECT_EQ(settings->quality, QualityPreset::High);
    }

} // namespace
