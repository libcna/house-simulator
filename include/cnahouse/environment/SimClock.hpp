// SPDX-License-Identifier: MIT
#pragma once

#include <cstdint>

#include "cnahouse/environment/Season.hpp"
#include "cnahouse/environment/Temperature.hpp"

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

    /// @brief §35.2b's compression: how many CALENDAR days one SIMULATED day advances the date by.
    ///
    /// *"Decision (project owner, 2026-09-06): the calendar is compressed so that one simulated day
    /// advances the calendar by 24 calendar days."* The diurnal rate and the annual rate are
    /// deliberately decoupled -- the first is chosen for how the sun should look, the second for
    /// how long a player should have to wait to see winter. At 24 the sun still rises once every
    /// 24 real minutes and **a full year takes 365 real minutes**, so one long session shows the
    /// house in all four seasons; without it four seasons would need 146 real hours of play and
    /// nobody would ever witness autumn.
    ///
    /// Set it to 1.0 for a realistic-calendar debug run, which is what §35.2b says it is for and
    /// what every calendar test in the project uses.
    inline constexpr double kDefaultCalendarDaysPerSimDay = 24.0;

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
    /// **The compressed year IS here** (`HOUSE-01542`). `epochSeconds` is the DIURNAL clock and
    /// nothing else: the sun's hour angle, the time on a wall clock's face, `SecondsOfDay`. The
    /// DATE is derived from it through `calendarDaysPerSimDay`, so it runs 24 times faster, and
    /// `CivilEpochSeconds` is where the two are put back together into one civil instant. With the
    /// field at 1.0 every one of them collapses to the identity and the clock is the uncompressed
    /// one `HOUSE-01531` built.
    struct SimClock
    {
        double epochSeconds = 0.0;
        double timeScale = kDefaultTimeScale;
        double latitudeDeg = kDefaultLatitudeDeg;
        double longitudeDeg = kDefaultLongitudeDeg;
        int utcOffsetMinutes = kDefaultUtcOffsetMinutes;
        bool dstRulesUS = true;
        /// @brief §35.2b's compression. 1.0 is a realistic calendar; the default is 24.0.
        double calendarDaysPerSimDay = kDefaultCalendarDaysPerSimDay;

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

        /// @brief Calendar days since the epoch, fractional and CONTINUOUS (`HOUSE-01542`).
        ///
        /// The quantity §36.3's season phase and §35.3's solar declination read. Continuous
        /// because §36.3 requires it in terms: *"a boundary is crossed every 91 real minutes, so a
        /// matrix that switched at an instant would be visible as a glitch"*.
        [[nodiscard]] double CalendarDays() const noexcept;

        /// @brief The civil instant this clock is at, in uncompressed epoch seconds.
        ///
        /// The compressed DATE's midnight plus the diurnal time of day, which is the one number
        /// the calendar, the daylight-saving rule and the wall clock all have to agree about.
        /// Identical to `epochSeconds` when `calendarDaysPerSimDay` is 1.0.
        [[nodiscard]] double CivilEpochSeconds() const noexcept;

        /// @brief Local STANDARD time -- the number, with no daylight saving in it.
        ///
        /// Under §35.2b's compression the date advances 24 times faster than the clock face, so a
        /// reading a real minute later is an hour later AND a day later. That is the decoupling
        /// §35.2b asks for and not a defect: *"the diurnal rate is chosen for how the sun should
        /// look, the annual rate for how long a player should have to wait to see winter."*
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

        /// @brief Whole CALENDAR days since the 2031-01-01 epoch. Negative before it.
        ///
        /// The date's day index and not the simulated one: this is what says which day of the year
        /// it is, so it moves 24 times faster than `SimDayIndex`.
        [[nodiscard]] std::int64_t DayIndex() const noexcept;

        /// @brief Whole SIMULATED days since the epoch -- how many times the sun has come up.
        [[nodiscard]] std::int64_t SimDayIndex() const noexcept;

        /// @brief Where the year is, `[0, 1)`, with 0 at the vernal equinox (`HOUSE-01534`).
        ///
        /// Continuous, and it completes exactly once per calendar year: the divisor is the length
        /// of the year the clock is IN, so a leap year's 366 days still come to one turn and the
        /// phase does not drift by a day every four years.
        [[nodiscard]] double YearFraction() const noexcept;

        /// @brief §36.3's season phase for `YearFraction()`.
        [[nodiscard]] SeasonPhase Season() const noexcept;

        /// @brief §36.2's base outdoor temperature for where and when the clock is, °C.
        ///
        /// The ANNUAL term reads the compressed calendar and the DIURNAL term reads the clock
        /// face, which is §35.2b's decoupling arriving where it can be felt: over one simulated
        /// day of play the sun rises and sets once while the season moves by 24 days, so the
        /// afternoon warms and the year cools underneath it at the same time.
        [[nodiscard]] double OutdoorBaseTemperatureC() const noexcept;

        /// @brief The clock set as close to @p time, read as local STANDARD time, as it can get.
        ///
        /// **Exact when `calendarDaysPerSimDay` is 1.0, and coarse otherwise, by construction.**
        /// A civil reading is a DATE and a TIME OF DAY, and under compression those are two
        /// quantities running at different rates: with the default 24, a given time of day only
        /// ever falls on one date in 24, so most (date, time) pairs are instants the clock never
        /// passes through. This lands on the nearest one it does pass through and `Standard()`
        /// then says where that is, rather than pretending the request was honoured.
        void SetStandard(const CivilTime& time) noexcept;

        /// @brief The clock set to @p calendarDays past the epoch, EXACTLY.
        ///
        /// One quantity in and one quantity set, so there is nothing to round: `CalendarDays()`
        /// reads back the number given. **The time of day falls out of it** and is not a second
        /// input, because under §35.2b's compression the two are not independent -- a calendar
        /// position IS a moment in a simulated day. This is what `time set` wants when it is asked
        /// for a season rather than for a date, and what `SetStandard` cannot promise.
        void SetCalendar(double calendarDays) noexcept;
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
