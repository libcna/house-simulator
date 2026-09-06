// SPDX-License-Identifier: MIT
#include "cnahouse/app/Settings.hpp"

#include <algorithm>
#include <format>

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
            return fallback;
        }

    } // namespace

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

        auto invert = root.OptionalBool("invertY", settings.invertY);
        if (!invert)
        {
            return invert.Error();
        }
        settings.invertY = *invert;

        auto fov = root.OptionalFloat("fieldOfView", settings.fieldOfView);
        if (!fov)
        {
            return fov.Error();
        }
        settings.fieldOfView = *fov;

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
        if (mouseSensitivity < 0.05f || mouseSensitivity > 10.0f)
        {
            mouseSensitivity = std::clamp(mouseSensitivity, 0.05f, 10.0f);
            note("mouseSensitivity");
        }
        if (fieldOfView < 50.0f || fieldOfView > 110.0f)
        {
            fieldOfView = std::clamp(fieldOfView, 50.0f, 110.0f);
            note("fieldOfView");
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
                           "  \"fieldOfView\": {}\n"
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
                           fieldOfView);
    }

} // namespace cnahouse::app
