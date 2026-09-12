// SPDX-License-Identifier: MIT
#include "cnahouse/environment/SimClock.hpp"

#include <cmath>
#include <initializer_list>

namespace cnahouse::environment
{
    namespace
    {
        /// 2031-01-01, the epoch §35.1 counts from, as days since 1970-01-01.
        const std::int64_t kEpochDay = DaysFromCivil(2031, 1, 1);

        /// Floor division, which is what a calendar needs and what `/` is not: -1 / 86400 is 0 in
        /// C++ and -1 day here, and the difference is the whole of what happens before the epoch.
        [[nodiscard]] std::int64_t FloorDiv(std::int64_t value, std::int64_t divisor) noexcept
        {
            const std::int64_t quotient = value / divisor;
            return (value % divisor != 0 && ((value < 0) != (divisor < 0))) ? quotient - 1 : quotient;
        }

        [[nodiscard]] std::int64_t FloorSeconds(double seconds) noexcept
        {
            return static_cast<std::int64_t>(std::floor(seconds));
        }

        [[nodiscard]] CivilTime FromEpochSeconds(double epochSeconds) noexcept
        {
            const std::int64_t total = FloorSeconds(epochSeconds);
            const std::int64_t day = FloorDiv(total, 86400);
            const std::int64_t rest = total - day * 86400;
            CivilTime time = CivilFromDays(kEpochDay + day);
            time.hour = static_cast<int>(rest / 3600);
            time.minute = static_cast<int>((rest % 3600) / 60);
            time.second = static_cast<int>(rest % 60);
            return time;
        }

        /// The epoch second at which daylight saving begins in @p year, in local STANDARD time:
        /// the second Sunday in March at 02:00.
        [[nodiscard]] std::int64_t DstStart(int year) noexcept
        {
            const int day = NthWeekdayOfMonth(year, 3, 0, 2);
            return (DaysFromCivil(year, 3, day) - kEpochDay) * 86400 + 2 * 3600;
        }

        /// ...and where it ends: the first Sunday in November at 02:00 DAYLIGHT, which is 01:00
        /// standard. Both bounds are standard so the comparison is against a monotone line.
        [[nodiscard]] std::int64_t DstEnd(int year) noexcept
        {
            const int day = NthWeekdayOfMonth(year, 11, 0, 1);
            return (DaysFromCivil(year, 11, day) - kEpochDay) * 86400 + 1 * 3600;
        }
    } // namespace

    std::int64_t DaysFromCivil(int year, int month, int day) noexcept
    {
        // Howard Hinnant's algorithm, unchanged: the era is a 400-year Gregorian cycle, which is
        // what makes leap years arithmetic rather than a table.
        std::int64_t y = year;
        y -= month <= 2 ? 1 : 0;
        const std::int64_t era = (y >= 0 ? y : y - 399) / 400;
        const std::int64_t yoe = y - era * 400;                         // [0, 399]
        const std::int64_t mp = (month + 9) % 12;                       // Mar = 0
        const std::int64_t doy = (153 * mp + 2) / 5 + day - 1;          // [0, 365]
        const std::int64_t doe = yoe * 365 + yoe / 4 - yoe / 100 + doy; // [0, 146096]
        return era * 146097 + doe - 719468;
    }

    CivilTime CivilFromDays(std::int64_t days) noexcept
    {
        std::int64_t z = days + 719468;
        const std::int64_t era = (z >= 0 ? z : z - 146096) / 146097;
        const std::int64_t doe = z - era * 146097; // [0, 146096]
        const std::int64_t yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
        const std::int64_t y = yoe + era * 400;
        const std::int64_t doy = doe - (365 * yoe + yoe / 4 - yoe / 100); // [0, 365]
        const std::int64_t mp = (5 * doy + 2) / 153;                      // [0, 11]
        const std::int64_t d = doy - (153 * mp + 2) / 5 + 1;              // [1, 31]
        const std::int64_t m = mp < 10 ? mp + 3 : mp - 9;                 // [1, 12]

        CivilTime time;
        time.year = static_cast<int>(y + (m <= 2 ? 1 : 0));
        time.month = static_cast<int>(m);
        time.day = static_cast<int>(d);
        // 1970-01-01 was a Thursday, so day 0 is weekday 4. Floor arithmetic again, because the
        // epoch of this project is 2031 and a fixture may well ask about 1969.
        const std::int64_t shifted = days + 4;
        time.weekday = static_cast<int>(shifted - FloorDiv(shifted, 7) * 7);
        time.dayOfYear = static_cast<int>(days - DaysFromCivil(time.year, 1, 1)) + 1;
        return time;
    }

    int NthWeekdayOfMonth(int year, int month, int weekday, int nth) noexcept
    {
        const std::int64_t first = DaysFromCivil(year, month, 1);
        const int firstWeekday = CivilFromDays(first).weekday;
        const int offset = ((weekday - firstWeekday) % 7 + 7) % 7;
        return 1 + offset + (nth - 1) * 7;
    }

    CivilTime CivilFromEpochSeconds(double epochSeconds) noexcept
    {
        return FromEpochSeconds(epochSeconds);
    }

    double EpochSecondsFor(const CivilTime& time) noexcept
    {
        const std::int64_t day = DaysFromCivil(time.year, time.month, time.day) - kEpochDay;
        return static_cast<double>(day * 86400 + time.hour * 3600 + time.minute * 60 + time.second);
    }

    bool DaylightSavingAt(double epochSeconds) noexcept
    {
        const std::int64_t now = FloorSeconds(epochSeconds);
        const int year = FromEpochSeconds(epochSeconds).year;
        return now >= DstStart(year) && now < DstEnd(year);
    }

    void SimClock::Advance(double realSeconds) noexcept
    {
        // A hitch advances the clock by the time that really passed (`HOUSE-01540`), so this takes
        // the frame's own dt and does no clamping of its own -- `FrameContext` has already decided
        // what "really passed" means. What it refuses is time running backwards, and a NaN dt,
        // which would put the whole calendar beyond recovery in one frame.
        if (!(realSeconds > 0.0) || !std::isfinite(realSeconds) || !std::isfinite(timeScale))
        {
            return;
        }
        epochSeconds += realSeconds * timeScale;
    }

    double SimClock::CalendarDays() const noexcept
    {
        // Continuous, because §36.3 requires it: a season boundary is crossed every 91 real
        // minutes and a phase that stepped would read as a glitch. A non-finite or non-positive
        // rate would take the calendar with it, so the field is treated as 1.0 there -- a clock
        // that refuses a bad setting is better than one that becomes NaN because of it.
        const double rate = (std::isfinite(calendarDaysPerSimDay) && calendarDaysPerSimDay > 0.0)
                                ? calendarDaysPerSimDay
                                : 1.0;
        return epochSeconds / kSecondsPerDay * rate;
    }

    double SimClock::CivilEpochSeconds() const noexcept
    {
        // The compressed DATE's midnight plus the diurnal time of day. At a rate of 1.0 the two
        // terms are `floor(t/86400)*86400` and `t - floor(t/86400)*86400`, which is `t`.
        return std::floor(CalendarDays()) * kSecondsPerDay + SecondsOfDay();
    }

    CivilTime SimClock::Standard() const noexcept
    {
        return FromEpochSeconds(CivilEpochSeconds());
    }

    CivilTime SimClock::Wall() const noexcept
    {
        return FromEpochSeconds(CivilEpochSeconds() + (IsDaylightSaving() ? 3600.0 : 0.0));
    }

    bool SimClock::IsDaylightSaving() const noexcept
    {
        // Asked of the CIVIL instant, not of `epochSeconds`: under compression the rule turns over
        // 24 times as often in real time, and it is the date on the calendar that decides.
        return dstRulesUS && DaylightSavingAt(CivilEpochSeconds());
    }

    int SimClock::EffectiveUtcOffsetMinutes() const noexcept
    {
        return utcOffsetMinutes + (IsDaylightSaving() ? 60 : 0);
    }

    double SimClock::SecondsOfDay() const noexcept
    {
        const double days = std::floor(epochSeconds / kSecondsPerDay);
        return epochSeconds - days * kSecondsPerDay;
    }

    std::int64_t SimClock::DayIndex() const noexcept
    {
        return static_cast<std::int64_t>(std::floor(CalendarDays()));
    }

    std::int64_t SimClock::SimDayIndex() const noexcept
    {
        return FloorDiv(FloorSeconds(epochSeconds), 86400);
    }

    double SimClock::YearFraction() const noexcept
    {
        // Counted CONTINUOUSLY from the epoch's own vernal equinox, over a constant year length.
        // The obvious alternative -- the day of the year over the length of the year the clock is
        // in -- is what `HOUSE-01534` did, and it is discontinuous at every 1 January: the offset
        // resets and the divisor changes between 365 and 366 at the same instant, so the phase
        // stepped 0.0007 of a turn in one hour. §36.3 forbids exactly that, and the equinox moving
        // by a fraction of a day between years is what the real one does.
        const double turns = (CalendarDays() - kNewGameCalendarDays) / kMeanYearDays;
        return turns - std::floor(turns);
    }

    SeasonPhase SimClock::Season() const noexcept
    {
        return SeasonAt(YearFraction());
    }

    double SimClock::OutdoorBaseTemperatureC() const noexcept
    {
        return OutdoorTemperatureC(0.0);
    }

    double SimClock::OutdoorTemperatureC(double weatherDeltaC) const noexcept
    {
        // The day of the year CONTINUOUSLY and 1-based, which is what §36.2's `doy` is: the annual
        // term has to move within a simulated day, because under §35.2b's compression a simulated
        // day is 24 of them.
        const CivilTime now = Standard();
        const std::int64_t yearStart = DaysFromCivil(now.year, 1, 1) - kEpochDay;
        const double dayOfYear = CalendarDays() - static_cast<double>(yearStart) + 1.0;
        return cnahouse::environment::OutdoorTemperatureC(dayOfYear, SecondsOfDay() / 3600.0, weatherDeltaC);
    }

    void SimClock::SetCalendar(double calendarDays) noexcept
    {
        const double rate = (std::isfinite(calendarDaysPerSimDay) && calendarDaysPerSimDay > 0.0)
                                ? calendarDaysPerSimDay
                                : 1.0;
        epochSeconds = calendarDays / rate * kSecondsPerDay;
    }

    void SimClock::SetStandard(const CivilTime& time) noexcept
    {
        const double target = EpochSecondsFor(time);
        const double day = std::floor(target / kSecondsPerDay);
        const double secondsOfDay = target - day * kSecondsPerDay;
        const double rate = (std::isfinite(calendarDaysPerSimDay) && calendarDaysPerSimDay > 0.0)
                                ? calendarDaysPerSimDay
                                : 1.0;
        // The time of day is the half that is always honoured, so the clock is placed on a whole
        // simulated day plus `secondsOfDay`. Which simulated day is the question: the time of day
        // carries its OWN share of the calendar with it -- at a rate of 24, being at 08:20 is
        // already 8.3 calendar days into the simulated day -- so the naive `day / rate` is out by
        // most of a day most of the time.
        //
        // Landing on `day` means `floor(n * rate + carried) == day`, so `n` is in
        // `[(day - carried) / rate, (day + 1 - carried) / rate)`. The smallest integer at or above
        // the lower bound is the candidate; the one below it is the only other one worth trying,
        // and at a rate of 1.0 the first is always exact.
        const double carried = secondsOfDay / kSecondsPerDay * rate;
        const double lower = (day - carried) / rate;
        const double above = std::ceil(lower - 1e-9);
        const double reached = std::floor(above * rate + carried);
        const double under = std::floor((above - 1.0) * rate + carried);
        const double simDay = std::abs(under - day) < std::abs(reached - day) ? above - 1.0 : above;
        epochSeconds = simDay * kSecondsPerDay + secondsOfDay;
    }

} // namespace cnahouse::environment
