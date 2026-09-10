// SPDX-License-Identifier: MIT
#pragma once

#include <cstdint>

namespace cnahouse::environment
{

    /// @brief A civil date and time of day, as a person would read it off a clock.
    ///
    /// Not a `std::tm` and not a `chrono` calendar: this project targets a Web build through
    /// Emscripten and a `time_t` there is whatever the host says it is. §35's clock is a number
    /// this project owns, and reading it has to give the same answer everywhere.
    struct CivilTime
    {
        int year = 2031;
        /// @brief 1-12.
        int month = 1;
        /// @brief 1-31.
        int day = 1;
        int hour = 0;
        int minute = 0;
        int second = 0;
        /// @brief 0 = Sunday … 6 = Saturday, which is what §35's DST rule counts in.
        int weekday = 0;
        /// @brief 1-366.
        int dayOfYear = 1;
    };

    /// @brief §35.2's chosen rate: 60 simulated seconds per real second, so 1 real second is
    ///        exactly 1 simulated minute and a simulated day is 24 real minutes.
    ///
    /// *"The reason 24 wins is not aesthetic -- it is that `1 s = 1 min` removes an entire class
    /// of arithmetic mistakes from every test, log and settings dialogue in the project."*
    inline constexpr double kDefaultTimeScale = 60.0;

    /// @brief §33's location: *"latitude 40.05° N, longitude −75.30°, UTC−5 with US DST rules"* --
    ///        suburban Philadelphia, which is what the sun, the seasons and the temperature curve
    ///        are all computed for.
    inline constexpr double kDefaultLatitudeDeg = 40.05;
    inline constexpr double kDefaultLongitudeDeg = -75.30;
    inline constexpr int kDefaultUtcOffsetMinutes = -300;

    inline constexpr double kSecondsPerDay = 86400.0;

    /// @brief §35.1's clock. Everything time-dependent reads it; nothing else keeps its own.
    ///
    /// **`epochSeconds` is local STANDARD time, and that is a decision.** §35.1 says *"seconds
    /// since 2031-01-01T00:00:00 local"*, and "local" is ambiguous exactly where it matters: on
    /// the first Sunday in November the local wall clock runs from 01:00 to 02:00 twice, so an
    /// epoch counted in wall-clock seconds has two seconds with the same reading and one hour that
    /// never happens in March. Counted in local STANDARD seconds the line is monotone and
    /// unbroken, and the wall clock is derived from it by adding an hour while daylight saving is
    /// in force -- which is what a wall clock is. `Wall()` is the reading; `Standard()` is the
    /// number.
    ///
    /// **The compressed year is not here.** §35.2b's `calendarDaysPerSimDay` is `HOUSE-01542`, and
    /// putting it in now would mean this clock's own tests could not say what date a given number
    /// of seconds is.
    struct SimClock
    {
        double epochSeconds = 0.0;
        double timeScale = kDefaultTimeScale;
        double latitudeDeg = kDefaultLatitudeDeg;
        double longitudeDeg = kDefaultLongitudeDeg;
        int utcOffsetMinutes = kDefaultUtcOffsetMinutes;
        bool dstRulesUS = true;

        /// @brief §35.1: *"`Update` accumulates `gameTime.ElapsedGameTime · timeScale`"*.
        ///
        /// Takes `FrameContext::realDeltaSeconds` -- the UNCLAMPED one -- and never a frame
        /// count. §35 and `HOUSE-01540` are explicit that a hitch advances the clock by *"the real
        /// elapsed time"*, and `FrameContext` carries two deltas that differ exactly then: a
        /// 250 ms hitch is 0.250 s real and `FrameTimer::kMaxDeltaSeconds` = 0.100 s clamped, so
        /// the clock gains 15 simulated seconds where the physics integrates 6. **That divergence
        /// is the design**: the simulation runs slow for a frame rather than wrong, and the wall
        /// clock does not stop because the machine stuttered. Feeding this the clamped delta makes
        /// a hitchy session's clock quietly lose time, which is the failure `HOUSE-01540` names.
        ///
        /// A negative or non-finite `dt` advances nothing; time in this house does not run
        /// backwards because a platform timer glitched.
        void Advance(double realSeconds) noexcept;

        /// @brief Local STANDARD time -- the number, with no daylight saving in it.
        [[nodiscard]] CivilTime Standard() const noexcept;

        /// @brief The local WALL clock: `Standard()` plus an hour while daylight saving is on.
        [[nodiscard]] CivilTime Wall() const noexcept;

        /// @brief Whether §35's US rule has daylight saving in force at this instant.
        ///
        /// Second Sunday in March at 02:00 standard, to first Sunday in November at 01:00
        /// standard -- which is 02:00 daylight, the reading the clock shows when it goes back.
        /// Both bounds are expressed in STANDARD time on purpose: in wall-clock terms the end is
        /// an hour that happens twice, and a comparison against it has no single answer.
        [[nodiscard]] bool IsDaylightSaving() const noexcept;

        /// @brief `utcOffsetMinutes`, plus 60 while daylight saving is in force.
        [[nodiscard]] int EffectiveUtcOffsetMinutes() const noexcept;

        /// @brief Seconds since local standard midnight, `[0, 86400)`.
        [[nodiscard]] double SecondsOfDay() const noexcept;

        /// @brief Whole local standard days since the 2031-01-01 epoch. Negative before it.
        [[nodiscard]] std::int64_t DayIndex() const noexcept;

        /// @brief The clock set to @p time, read as local STANDARD time.
        void SetStandard(const CivilTime& time) noexcept;
    };

    /// @brief Days from 1970-01-01 to @p year-@p month-@p day, proleptic Gregorian.
    ///
    /// Howard Hinnant's `days_from_civil`, which is exact for every year a `std::int64_t` can
    /// hold and has no table, no leap-year special case at the call site and no locale.
    [[nodiscard]] std::int64_t DaysFromCivil(int year, int month, int day) noexcept;

    /// @brief The inverse.
    [[nodiscard]] CivilTime CivilFromDays(std::int64_t days) noexcept;

    /// @brief The day of the month of the @p nth @p weekday of @p month in @p year.
    ///
    /// `NthWeekdayOfMonth(2031, 3, 0, 2)` is the second Sunday in March 2031, which is when §35's
    /// daylight saving begins. `n` counts from 1.
    [[nodiscard]] int NthWeekdayOfMonth(int year, int month, int weekday, int nth) noexcept;

    /// @brief §35.1's epoch seconds as a civil date and time (`HOUSE-01532`).
    ///
    /// The reading is local STANDARD time, `weekday` and `dayOfYear` filled in. Before the 2031
    /// epoch the arithmetic is a FLOOR and not a truncation: −1 s is the last second of
    /// 2030-12-31, not the first of 2031-01-01 counted backwards. A fractional second reads as the
    /// second it is inside, so 0.9 s is second 0.
    [[nodiscard]] CivilTime CivilFromEpochSeconds(double epochSeconds) noexcept;

    /// @brief The inverse: @p time, read as local STANDARD time, as §35.1's epoch seconds.
    ///
    /// `weekday` and `dayOfYear` are OUTPUTS of the conversion and are ignored here, so a caller
    /// may fill in the five fields it knows and get the right answer.
    [[nodiscard]] double EpochSecondsFor(const CivilTime& time) noexcept;

    /// @brief Whether §35's US rule has daylight saving in force at @p epochSeconds.
    ///
    /// The rule without a clock to hang it on, so a caller with a timestamp can ask directly.
    /// Second Sunday in March at 02:00 standard to first Sunday in November at 01:00 standard --
    /// **the rule in force since 2007**, applied to every year. Before that the United States
    /// began in April and ended in October; this house is set in 2031 and the project does not
    /// carry a history of legislation.
    [[nodiscard]] bool DaylightSavingAt(double epochSeconds) noexcept;

} // namespace cnahouse::environment
