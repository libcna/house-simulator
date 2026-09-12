// SPDX-License-Identifier: MIT
#include "cnahouse/app/Settings.hpp"

#include <algorithm>
#include <cmath>
#include <format>

#include "cnahouse/player/FirstPersonCamera.hpp"
#include "cnahouse/util/Json.hpp"

namespace cnahouse::app
{
    namespace
    {

        QualityPreset QualityFromName(std::string_view name, QualityPreset fallback)
        {
            if (name == "low")
            {
                return QualityPreset::Low;
            }
            if (name == "medium")
            {
                return QualityPreset::Medium;
            }
            if (name == "high")
            {
                return QualityPreset::High;
            }
            if (name == "ultra")
            {
                return QualityPreset::Ultra;
            }
            return fallback;
        }

    } // namespace

    WeatherMode WeatherModeFromName(std::string_view name, WeatherMode fallback) noexcept
    {
        if (name == "on")
        {
            return WeatherMode::On;
        }
        if (name == "fixed")
        {
            return WeatherMode::Fixed;
        }
        if (name == "off")
        {
            return WeatherMode::Off;
        }
        return fallback;
    }

    util::Result<Settings> Settings::FromJson(std::string_view text, std::string_view name)
    {
        auto document = util::JsonDocument::Parse(text, std::string(name));
        if (!document)
        {
            return document.Error();
        }
        const util::JsonValue& root = document->Root();

        Settings settings = Defaults();

        auto version = root.OptionalInt("version", 1);
        if (!version)
        {
            return version.Error();
        }
        settings.version = static_cast<std::int32_t>(*version);

        auto width = root.OptionalInt("backBufferWidth", settings.backBufferWidth);
        if (!width)
        {
            return width.Error();
        }
        settings.backBufferWidth = static_cast<int>(*width);

        auto height = root.OptionalInt("backBufferHeight", settings.backBufferHeight);
        if (!height)
        {
            return height.Error();
        }
        settings.backBufferHeight = static_cast<int>(*height);

        auto fullscreen = root.OptionalBool("fullscreen", settings.fullscreen);
        if (!fullscreen)
        {
            return fullscreen.Error();
        }
        settings.fullscreen = *fullscreen;

        auto vsync = root.OptionalBool("verticalSync", settings.verticalSync);
        if (!vsync)
        {
            return vsync.Error();
        }
        settings.verticalSync = *vsync;

        auto quality = root.OptionalString("quality", std::string(QualityPresetName(settings.quality)));
        if (!quality)
        {
            return quality.Error();
        }
        settings.quality = QualityFromName(*quality, settings.quality);

        auto master = root.OptionalFloat("masterVolume", settings.masterVolume);
        if (!master)
        {
            return master.Error();
        }
        settings.masterVolume = *master;

        auto effects = root.OptionalFloat("effectsVolume", settings.effectsVolume);
        if (!effects)
        {
            return effects.Error();
        }
        settings.effectsVolume = *effects;

        auto ambience = root.OptionalFloat("ambienceVolume", settings.ambienceVolume);
        if (!ambience)
        {
            return ambience.Error();
        }
        settings.ambienceVolume = *ambience;

        auto sensitivity = root.OptionalFloat("mouseSensitivity", settings.mouseSensitivity);
        if (!sensitivity)
        {
            return sensitivity.Error();
        }
        settings.mouseSensitivity = *sensitivity;

        auto smoothing = root.OptionalBool("lookSmoothing", settings.lookSmoothing);
        if (!smoothing)
        {
            return smoothing.Error();
        }
        settings.lookSmoothing = *smoothing;

        auto invert = root.OptionalBool("invertY", settings.invertY);
        if (!invert)
        {
            return invert.Error();
        }
        settings.invertY = *invert;

        auto fast = root.OptionalBool("fastWalk", settings.fastWalk);
        if (!fast)
        {
            return fast.Error();
        }
        settings.fastWalk = *fast;

        auto headBob =
            root.OptionalString("headBob", std::string(player::HeadBobLevelName(settings.headBob)));
        if (!headBob)
        {
            return headBob.Error();
        }
        // An unknown level falls back rather than failing, the same rule the quality preset
        // follows: a settings file from a build with a fourth level must still load in this one.
        settings.headBob = player::HeadBobLevelFromName(*headBob, settings.headBob);

        auto fov = root.OptionalFloat("fieldOfView", settings.fieldOfView);
        if (!fov)
        {
            return fov.Error();
        }
        settings.fieldOfView = *fov;

        auto dayLength = root.OptionalFloat("dayLengthRealMinutes", settings.dayLengthRealMinutes);
        if (!dayLength)
        {
            return dayLength.Error();
        }
        settings.dayLengthRealMinutes = *dayLength;

        auto environmentReadout =
            root.OptionalBool("showEnvironmentReadout", settings.showEnvironmentReadout);
        if (!environmentReadout)
        {
            return environmentReadout.Error();
        }
        settings.showEnvironmentReadout = *environmentReadout;

        auto weatherMode =
            root.OptionalString("weatherMode", std::string(WeatherModeName(settings.weatherMode)));
        if (!weatherMode)
        {
            return weatherMode.Error();
        }
        settings.weatherMode = WeatherModeFromName(*weatherMode, settings.weatherMode);

        auto fixedWeather = root.OptionalString("fixedWeatherArchetype", settings.fixedWeatherArchetype);
        if (!fixedWeather)
        {
            return fixedWeather.Error();
        }
        settings.fixedWeatherArchetype = *fixedWeather;

        Migrate(settings);
        return settings;
    }

    void Settings::Migrate(Settings& settings)
    {
        // v1 -> v2: `ambienceVolume` was added. A v1 file has no value for it, and `FromJson` has
        // already left the default in place, so the migration is only the version bump -- but it is
        // written out explicitly so the NEXT migration has an obvious place to go and a reader can see
        // that v1 was considered rather than forgotten.
        if (settings.version < 2)
        {
            settings.version = 2;
        }
        if (settings.version < 3)
        {
            // Version 3 added §43.2's walk mode (D-09). A file written before it has no opinion,
            // and the default -- the normal walk -- is the right one to give it: a player who has
            // never pressed `Shift` has not chosen the fast walk.
            settings.fastWalk = false;
            settings.version = 3;
        }
        if (settings.version < 4)
        {
            // Version 4 added §68's head-bob level. A file written before it has no opinion, and
            // §68's own default -- `Subtle` -- is the right one to give it: §44 says the motion is
            // on by default, and a player who has never seen the setting has not turned it off.
            settings.headBob = player::HeadBobLevel::Subtle;
            settings.version = 4;
        }
        if (settings.version < 5)
        {
            // Version 5 added §35.2's day length. A file written before it has no opinion, and the
            // default -- 24 real minutes a simulated day -- is the one §35.2 chose; a player who
            // has never opened the setting has not asked for a slower sun.
            settings.dayLengthRealMinutes = static_cast<float>(environment::kDefaultDayLengthRealMinutes);
            settings.version = 5;
        }
        if (settings.version < 6)
        {
            // Version 6 added HOUSE-01546's player-facing time/season/temperature line. Existing
            // players get the documented visible default; they can then hide it in settings.
            settings.showEnvironmentReadout = true;
            settings.version = 6;
        }
        if (settings.version < 7)
        {
            // Version 7 added §68's weather policy. An older file has never disabled or fixed the
            // simulation, so it receives the documented live-weather default and its canonical
            // fixed selection.
            settings.weatherMode = WeatherMode::On;
            settings.fixedWeatherArchetype = "W_PARTLY";
            settings.version = 7;
        }
        settings.version = kCurrentVersion;
    }

    std::string Settings::ClampToSupportedRanges()
    {
        std::string changed;
        auto note = [&changed](std::string_view what)
        {
            if (!changed.empty())
            {
                changed += ", ";
            }
            changed += what;
        };

        // A settings file is user-editable text. A resolution of 0 reaches the device as a division by
        // zero and a volume of 40 reaches the mixer as clipping, so both are corrected here rather than
        // trusted -- and the correction is reported so the user is told, not silently overruled.
        if (backBufferWidth < 640 || backBufferWidth > 7680)
        {
            backBufferWidth = std::clamp(backBufferWidth, 640, 7680);
            note("backBufferWidth");
        }
        if (backBufferHeight < 480 || backBufferHeight > 4320)
        {
            backBufferHeight = std::clamp(backBufferHeight, 480, 4320);
            note("backBufferHeight");
        }
        for (auto* volume : {&masterVolume, &effectsVolume, &ambienceVolume})
        {
            if (*volume < 0.0f || *volume > 1.0f)
            {
                *volume = std::clamp(*volume, 0.0f, 1.0f);
                note("a volume");
            }
        }
        // §44's band, and not a wider one "to be safe": 0.05x is a mouse that cannot turn round
        // and 10x is one that spins on a twitch, and a settings file that accepts both has told
        // the player those are supported.
        if (mouseSensitivity < 0.2f || mouseSensitivity > 4.0f)
        {
            mouseSensitivity = std::clamp(mouseSensitivity, 0.2f, 4.0f);
            note("mouseSensitivity");
        }
        // §44's band again, and this one is the CAMERA's own constants rather than a copy of
        // them: `player::FirstPersonCamera` clamps whatever it is handed to 55-95 too, so a file
        // outside the band used to be corrected silently by the camera after being accepted here.
        // The file is what the player edits, so the file is where they have to be told.
        if (fieldOfView < player::kMinFovDegrees || fieldOfView > player::kMaxFovDegrees)
        {
            fieldOfView = std::clamp(fieldOfView, player::kMinFovDegrees, player::kMaxFovDegrees);
            note("fieldOfView");
        }
        // §35.2's band, with ZERO left alone because zero is the frozen clock rather than a day
        // length out of range -- it is a preset in the table and the state §35.2 calls essential
        // for pixel-regression tests. Anything else outside the band is corrected: a day of
        // 0.001 real minutes is a sun that strobes, and a negative one is a clock that runs
        // backwards through `TimeScaleForDayLength`'s divide.
        const auto lowest = static_cast<float>(environment::kMinDayLengthRealMinutes);
        const auto highest = static_cast<float>(environment::kMaxDayLengthRealMinutes);
        if (!std::isfinite(dayLengthRealMinutes) ||
            (dayLengthRealMinutes != 0.0f &&
             (dayLengthRealMinutes < lowest || dayLengthRealMinutes > highest)))
        {
            dayLengthRealMinutes = std::isfinite(dayLengthRealMinutes)
                                       ? std::clamp(dayLengthRealMinutes, lowest, highest)
                                       : static_cast<float>(environment::kDefaultDayLengthRealMinutes);
            note("dayLengthRealMinutes");
        }
        if (fixedWeatherArchetype.empty())
        {
            fixedWeatherArchetype = "W_PARTLY";
            note("fixedWeatherArchetype");
        }
        return changed;
    }

    std::string Settings::ToJson() const
    {
        // Written by hand rather than through a serialiser: the file is meant to be read and edited by
        // a person, so field order and grouping are part of its job.
        return std::format("{{\n"
                           "  \"version\": {},\n"
                           "  \"backBufferWidth\": {},\n"
                           "  \"backBufferHeight\": {},\n"
                           "  \"fullscreen\": {},\n"
                           "  \"verticalSync\": {},\n"
                           "  \"quality\": \"{}\",\n"
                           "  \"masterVolume\": {},\n"
                           "  \"effectsVolume\": {},\n"
                           "  \"ambienceVolume\": {},\n"
                           "  \"mouseSensitivity\": {},\n"
                           "  \"invertY\": {},\n"
                           "  \"lookSmoothing\": {},\n"
                           "  \"headBob\": \"{}\",\n"
                           "  \"fieldOfView\": {},\n"
                           "  \"fastWalk\": {},\n"
                           "  \"dayLengthRealMinutes\": {},\n"
                           "  \"showEnvironmentReadout\": {},\n"
                           "  \"weatherMode\": \"{}\",\n"
                           "  \"fixedWeatherArchetype\": \"{}\"\n"
                           "}}\n",
                           version,
                           backBufferWidth,
                           backBufferHeight,
                           fullscreen ? "true" : "false",
                           verticalSync ? "true" : "false",
                           QualityPresetName(quality),
                           masterVolume,
                           effectsVolume,
                           ambienceVolume,
                           mouseSensitivity,
                           invertY ? "true" : "false",
                           lookSmoothing ? "true" : "false",
                           HeadBobLevelName(headBob),
                           fieldOfView,
                           fastWalk ? "true" : "false",
                           dayLengthRealMinutes,
                           showEnvironmentReadout ? "true" : "false",
                           WeatherModeName(weatherMode),
                           fixedWeatherArchetype);
    }

} // namespace cnahouse::app
