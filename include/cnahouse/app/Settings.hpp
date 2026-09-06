// SPDX-License-Identifier: MIT
#pragma once

#include <cstdint>
#include <string>

#include "cnahouse/app/CommandLine.hpp"
#include "cnahouse/util/Result.hpp"

namespace cnahouse::app
{

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
        static constexpr std::int32_t kCurrentVersion = 2;

        std::int32_t version = kCurrentVersion;

        int backBufferWidth = 1600;
        int backBufferHeight = 900;
        bool fullscreen = false;
        bool verticalSync = true;

        QualityPreset quality = QualityPreset::High;

        float masterVolume = 1.0f;
        float effectsVolume = 1.0f;
        float ambienceVolume = 1.0f;

        float mouseSensitivity = 1.0f;
        bool invertY = false;

        /// @brief Field of view in degrees, vertical.
        float fieldOfView = 70.0f;

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
