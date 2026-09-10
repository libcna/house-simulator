// SPDX-License-Identifier: MIT
#pragma once

#include <cstddef>

namespace cnahouse::environment
{

    /// @brief §35.2's day-length presets, in REAL MINUTES per simulated day (`HOUSE-01533`).
    ///
    /// The setting is a NUMBER and the presets are named values of it, rather than an enum with a
    /// number beside it. An enum plus a value is two places that can disagree -- a file saying
    /// `preset: slow, minutes: 24` has to be resolved by somebody -- and §35.2 asks for *"the
    /// presets above and a free numeric entry"*, which is exactly a number with some named points
    /// on it.
    ///
    /// §35.2's own table, in order: the brief's original 20, the chosen 24, a slow preset for
    /// screenshots, a very slow one, real time, and frozen. **Zero is frozen**, which is the table
    /// row written as ∞ real minutes a day: `timeScale` 0 is the state §35.2 calls *"essential for
    /// pixel-regression tests"*, and a caller that divides by this number has to notice it.
    inline constexpr double kDayLengthPresetsRealMinutes[] = {20.0, 24.0, 48.0, 96.0, 1440.0, 0.0};

    inline constexpr std::size_t kDayLengthPresetCount =
        sizeof(kDayLengthPresetsRealMinutes) / sizeof(kDayLengthPresetsRealMinutes[0]);

    /// @brief §35.2's chosen default: 24 real minutes a simulated day, which is `timeScale` 60.
    inline constexpr double kDefaultDayLengthRealMinutes = 24.0;

    /// @brief The fastest a day may be set to run. Under a real minute the sun crosses the sky
    ///        faster than the eye follows and §26's LOD popping becomes the whole picture.
    inline constexpr double kMinDayLengthRealMinutes = 1.0;

    /// @brief The slowest: §35.2's real-time row. Slower than real time is what frozen is for.
    inline constexpr double kMaxDayLengthRealMinutes = 1440.0;

    /// @brief The `SimClock::timeScale` for @p realMinutesPerSimDay.
    ///
    /// A simulated day is 1 440 simulated minutes, so the scale is `1440 / realMinutes` -- 60 at
    /// the default, which is the *"1 real second = 1 simulated minute"* §35.2 chose the number
    /// for. Zero, negative and non-finite all give **0**, the frozen clock: a settings file is
    /// user-editable text and a `timeScale` of infinity is a calendar with no way back.
    [[nodiscard]] double TimeScaleForDayLength(double realMinutesPerSimDay) noexcept;

    /// @brief The inverse, for a settings dialogue that has a `timeScale` and wants to show a day
    ///        length. A frozen clock is 0 rather than infinity, for the same reason.
    [[nodiscard]] double DayLengthForTimeScale(double timeScale) noexcept;

    /// @brief The index into `kDayLengthPresetsRealMinutes` that @p realMinutesPerSimDay is, or
    ///        `kDayLengthPresetCount` when it is the free numeric entry.
    [[nodiscard]] std::size_t DayLengthPresetIndex(double realMinutesPerSimDay) noexcept;

} // namespace cnahouse::environment
