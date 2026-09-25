// SPDX-License-Identifier: MIT
#pragma once

#include <cstdint>
#include <string>

#include "cnahouse/app/CommandLine.hpp"
#include "cnahouse/environment/DayLength.hpp"
#include "cnahouse/environment/SimClock.hpp"
#include "cnahouse/player/HeadBob.hpp"
#include "cnahouse/util/Result.hpp"

namespace cnahouse::app
{

    enum class WeatherMode : std::uint8_t
    {
        On,
        Fixed,
        Off,
    };

    [[nodiscard]] constexpr std::string_view WeatherModeName(WeatherMode mode) noexcept
    {
        switch (mode)
        {
            case WeatherMode::On:
                return "on";
            case WeatherMode::Fixed:
                return "fixed";
            case WeatherMode::Off:
                return "off";
        }
        return "on";
    }

    [[nodiscard]] WeatherMode WeatherModeFromName(std::string_view name, WeatherMode fallback) noexcept;

    /// @brief The persisted user settings, versioned so an old file can be migrated rather than lost.
    ///
    /// **Why a version and a migration chain from day one.** `R-15` names save-format churn during
    /// development as a high-probability risk, and settings are the first file that will churn. A
    /// settings file that is silently discarded on a schema change re-teaches every developer's
    /// resolution and volume once a week; one that is migrated does not. The chain is built now,
    /// before anything depends on it, which is the only time it is cheap.
    struct Settings
    {
        /// @brief Bumped whenever a field changes meaning. `Migrate` handles every older value.
        static constexpr std::int32_t kCurrentVersion = 10;

        std::int32_t version = kCurrentVersion;

        int backBufferWidth = 1600;
        int backBufferHeight = 900;
        bool fullscreen = false;
        bool verticalSync = true;

        QualityPreset quality = QualityPreset::High;

        /// @brief M8's retained compact mix, persisted with the same category names the UI shows.
        float masterVolume = 0.80f;
        float footstepsVolume = 0.85f;
        float ambienceVolume = 0.75f;
        float weatherVolume = 0.75f;

        /// @brief §44's mouse sensitivity multiplier, 0.2x to 4x.
        ///
        /// A MULTIPLIER and not an angle: the rad/px is `InputConfig::kRadiansPerPixel`, and
        /// exposing both would let a player find two ways to mean the same thing.
        float mouseSensitivity = 1.0f;
        bool invertY = false;

        /// @brief §44's *"optional raw-ish smoothing over 2 frames, default off"*.
        ///
        /// Off by default because smoothing is latency: it trades a millisecond of aim for a
        /// millisecond of lag, and a player who wants it knows they do. Two frames and not a
        /// filter with a time constant -- at 60 Hz that is 8 ms of averaging, which takes the
        /// jitter off a cheap mouse without becoming a mouse that arrives late.
        bool lookSmoothing = false;

        /// @brief §44's field of view in degrees, VERTICAL, 55-95.
        ///
        /// The band and the default are `player::kMinFovDegrees`, `kMaxFovDegrees` and
        /// `kDefaultFovDegrees`; `ClampToSupportedRanges` uses those constants and a test asserts
        /// this default is that one. The number is repeated here rather than included because
        /// this header is small and widely included and the camera's is neither.
        float fieldOfView = 70.0f;

        /// @brief §68's head-bob level, defaulting to §44's *"on but low"*.
        ///
        /// Three levels and not a slider, because the two numbers §44 gives move together: a
        /// player choosing between "a little" and "a bit more" is not choosing an amplitude in
        /// millimetres, and a slider would offer them one that makes them ill.
        player::HeadBobLevel headBob = player::HeadBobLevel::Subtle;

        /// @brief §43.2's walk mode: false is the 1.35 m/s walk, true the 2.05 m/s one.
        ///
        /// **A preference, not world state** (D-09, §77). `Shift` toggles it rather than holding
        /// it, so it has to be remembered somewhere, and remembering it here means it survives a
        /// save/load AND a *Reset House*. The save's player block records it too, so a save is a
        /// self-consistent snapshot -- and settings wins on conflict, which is what makes this
        /// the one place that decides.
        bool fastWalk = false;

        /// @brief §35.2's day length, in REAL MINUTES per simulated day. 0 is the frozen clock.
        ///
        /// A number with named points on it rather than an enum plus a number: §35.2 asks for
        /// *"the presets above and a free numeric entry"*, and storing both a preset name and a
        /// value would be two places that can disagree. `environment::kDayLengthPresetsRealMinutes`
        /// is the list a settings dialogue offers, `TimeScaleForDayLength` turns this into
        /// `SimClock::timeScale`, and the default 24 is the one §35.2 chose because it makes
        /// 1 real second exactly 1 simulated minute.
        float dayLengthRealMinutes = static_cast<float>(environment::kDefaultDayLengthRealMinutes);

        /// @brief §33.5's phase-only acceleration, 1x astronomical through 8x.
        ///
        /// This never speeds the civil clock or the moon's path across the sky. It only makes the
        /// illuminated phase progress faster, with 1x preserving the real ephemeris exactly.
        float moonPhaseSpeedMultiplier = static_cast<float>(environment::kDefaultMoonPhaseSpeedMultiplier);

        /// @brief Whether §67's compact player-facing environment readout is visible in play.
        bool showEnvironmentReadout = true;

        /// @brief §68's weather simulation policy.
        WeatherMode weatherMode = WeatherMode::On;

        /// @brief Stable id of the state archetype used while `weatherMode == Fixed`.
        ///
        /// The settings layer preserves the string; the weather system resolves it against the
        /// thirteen loaded state archetypes. `W_WINDY` is a modifier and cannot be selected alone.
        std::string fixedWeatherArchetype = "W_PARTLY";

        [[nodiscard]] static Settings Defaults()
        {
            return Settings{};
        }

        /// @brief Parses settings from JSON text. Unknown fields are ignored; wrong types are errors.
        ///
        /// Ignoring unknown fields is deliberate and is the other half of the migration story: a file
        /// written by a NEWER build must still load in an older one rather than being rejected, or a
        /// developer who switches branches loses their settings in the direction the version number
        /// cannot help with.
        [[nodiscard]] static util::Result<Settings> FromJson(std::string_view text, std::string_view name);

        [[nodiscard]] std::string ToJson() const;

        /// @brief Brings a parsed older version up to `kCurrentVersion`.
        ///
        /// Each step is separate and does one thing, so a later reader can see what changed and when.
        static void Migrate(Settings& settings);

        /// @brief Clamps every field into its supported range, reporting what it changed.
        ///
        /// Applied after loading, because a settings file is user-editable and a resolution of 0 or a
        /// volume of 40 must not reach the device. Returns a human-readable list of what was clamped so
        /// the caller can log it once rather than silently correcting the user (§5.4).
        [[nodiscard]] std::string ClampToSupportedRanges();
    };

} // namespace cnahouse::app
