// SPDX-License-Identifier: MIT
#include "cnahouse/environment/SimClock.hpp"

#include <cmath>

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

    CivilTime SimClock::Standard() const noexcept
    {
        return FromEpochSeconds(epochSeconds);
    }

    CivilTime SimClock::Wall() const noexcept
    {
        return FromEpochSeconds(epochSeconds + (IsDaylightSaving() ? 3600.0 : 0.0));
    }

    bool SimClock::IsDaylightSaving() const noexcept
    {
        if (!dstRulesUS)
        {
            return false;
        }
        const std::int64_t now = FloorSeconds(epochSeconds);
        const int year = FromEpochSeconds(epochSeconds).year;
        return now >= DstStart(year) && now < DstEnd(year);
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
        return FloorDiv(FloorSeconds(epochSeconds), 86400);
    }

    void SimClock::SetStandard(const CivilTime& time) noexcept
    {
        const std::int64_t day = DaysFromCivil(time.year, time.month, time.day) - kEpochDay;
        epochSeconds = static_cast<double>(day * 86400 + time.hour * 3600 + time.minute * 60 + time.second);
    }

} // namespace cnahouse::environment
