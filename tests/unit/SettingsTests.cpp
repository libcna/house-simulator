// SPDX-License-Identifier: MIT
//
// `HOUSE-00131`'s acceptance, including a v1 -> v2 migration fixture.
#include <gtest/gtest.h>

#include "cnahouse/app/Settings.hpp"

namespace
{
    using cnahouse::app::QualityPreset;
    using cnahouse::app::Settings;

    TEST(SettingsTests, DefaultsAreTheDocumentedOnes)
    {
        const Settings settings = Settings::Defaults();
        EXPECT_EQ(settings.version, Settings::kCurrentVersion);
        EXPECT_EQ(settings.backBufferWidth, 1600);
        EXPECT_EQ(settings.backBufferHeight, 900);
        EXPECT_EQ(settings.quality, QualityPreset::High);
        EXPECT_FLOAT_EQ(settings.masterVolume, 1.0f);
    }

    TEST(SettingsTests, RoundTripsThroughItsOwnJson)
    {
        Settings written = Settings::Defaults();
        written.backBufferWidth = 1920;
        written.backBufferHeight = 1080;
        written.fullscreen = true;
        written.quality = QualityPreset::Medium;
        written.masterVolume = 0.75f;
        written.mouseSensitivity = 1.5f;
        written.invertY = true;
        written.fieldOfView = 90.0f;

        auto read = Settings::FromJson(written.ToJson(), "settings.json");
        ASSERT_TRUE(read) << read.Error().ToString();
        EXPECT_EQ(read->backBufferWidth, 1920);
        EXPECT_EQ(read->backBufferHeight, 1080);
        EXPECT_TRUE(read->fullscreen);
        EXPECT_EQ(read->quality, QualityPreset::Medium);
        EXPECT_FLOAT_EQ(read->masterVolume, 0.75f);
        EXPECT_FLOAT_EQ(read->mouseSensitivity, 1.5f);
        EXPECT_TRUE(read->invertY);
        EXPECT_FLOAT_EQ(read->fieldOfView, 90.0f);
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
        EXPECT_FLOAT_EQ(settings->effectsVolume, 0.25f);
        EXPECT_TRUE(settings->invertY);
        EXPECT_FLOAT_EQ(settings->ambienceVolume, 1.0f)
            << "the field v1 did not have takes its default rather than zero";
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

        const std::string changed = settings.ClampToSupportedRanges();
        EXPECT_FALSE(changed.empty()) << "the user is told, not silently overruled";
        EXPECT_EQ(settings.backBufferWidth, 640);
        EXPECT_EQ(settings.backBufferHeight, 4320);
        EXPECT_FLOAT_EQ(settings.masterVolume, 1.0f);
        // §44's band, tightened from the 0.05-10 this file accepted until `HOUSE-00625`: 0.05x
        // is a mouse that cannot turn round and 10x is one that spins on a twitch, and a settings
        // file that accepted both had told the player those were supported.
        EXPECT_FLOAT_EQ(settings.mouseSensitivity, 0.2f);
        EXPECT_FLOAT_EQ(settings.fieldOfView, 110.0f);
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
