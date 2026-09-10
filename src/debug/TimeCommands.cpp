// SPDX-License-Identifier: MIT
#include "cnahouse/debug/TimeCommands.hpp"

#include <charconv>
#include <cmath>
#include <format>

#include "cnahouse/environment/DayLength.hpp"

namespace cnahouse::debug
{
    namespace
    {
        using environment::SimClock;

        [[nodiscard]] bool ParseDouble(std::string_view text, double& out) noexcept
        {
            // `from_chars` and not `stod`: no locale, no exception, no allocation, and it refuses
            // trailing rubbish rather than parsing a prefix. "12abc" is a typo, not 12.
            const char* first = text.data();
            const char* last = text.data() + text.size();
            const auto result = std::from_chars(first, last, out);
            return result.ec == std::errc{} && result.ptr == last && std::isfinite(out);
        }

        [[nodiscard]] bool ParseHourMinute(std::string_view text, double& secondsOfDay)
        {
            const std::size_t colon = text.find(':');
            if (colon == std::string_view::npos)
            {
                return false;
            }
            double hours = 0.0;
            double minutes = 0.0;
            if (!ParseDouble(text.substr(0, colon), hours) || !ParseDouble(text.substr(colon + 1), minutes))
            {
                return false;
            }
            // A clock face and not an offset: 25:00 is a typo, and so is 12:60. Refusing is better
            // than wrapping, because a person who typed 12:60 meant 13:00 and should be told they
            // did not say so.
            if (hours < 0.0 || hours > 23.0 || minutes < 0.0 || minutes > 59.0)
            {
                return false;
            }
            if (hours != std::floor(hours) || minutes != std::floor(minutes))
            {
                return false;
            }
            secondsOfDay = hours * 3600.0 + minutes * 60.0;
            return true;
        }

        [[nodiscard]] std::string Reading(const SimClock& clock)
        {
            const environment::CivilTime wall = clock.Wall();
            const double dayLength = environment::DayLengthForTimeScale(clock.timeScale);
            return std::format("{:04}-{:02}-{:02} {:02}:{:02}:{:02} {}, scale {:g}x ({}), "
                               "year {:.3f}, {:.1f} C",
                               wall.year,
                               wall.month,
                               wall.day,
                               wall.hour,
                               wall.minute,
                               wall.second,
                               clock.IsDaylightSaving() ? "DST" : "std",
                               clock.timeScale,
                               dayLength > 0.0 ? std::format("{:g} real min/day", dayLength)
                                               : std::string("frozen"),
                               clock.YearFraction(),
                               clock.OutdoorBaseTemperatureC());
        }
    } // namespace

    void RegisterTimeCommands(Console& console, TimeCommandContext context)
    {
        console.Register(
            "time",
            "time [set <hh:mm> | scale <x> | advance <days>]  -- §35's clock; no verb reports it",
            [context](std::span<const std::string_view> arguments) -> CommandResult
            {
                if (context.clock == nullptr)
                {
                    return CommandResult{false, "time: this session has no clock"};
                }
                SimClock& clock = *context.clock;
                if (arguments.empty())
                {
                    return CommandResult{true, Reading(clock)};
                }
                const std::string_view verb = arguments[0];
                if (arguments.size() < 2)
                {
                    return CommandResult{false, std::format("time {}: needs a value", verb)};
                }

                if (verb == "set")
                {
                    double secondsOfDay = 0.0;
                    if (!ParseHourMinute(arguments[1], secondsOfDay))
                    {
                        return CommandResult{
                            false, std::format("time set: '{}' is not a 24-hour hh:mm", arguments[1])};
                    }
                    // The time of day is the half §35.2b always honours, so this moves the clock
                    // WITHIN its current simulated day. Under the compressed calendar that also
                    // moves the date -- up to `calendarDaysPerSimDay` of it -- which is why the
                    // reply is the whole reading rather than an "ok".
                    const double day = std::floor(clock.epochSeconds / environment::kSecondsPerDay);
                    clock.epochSeconds = day * environment::kSecondsPerDay + secondsOfDay;
                    return CommandResult{true, std::format("time set: {}", Reading(clock))};
                }
                if (verb == "scale")
                {
                    double scale = 0.0;
                    if (!ParseDouble(arguments[1], scale) || scale < 0.0)
                    {
                        return CommandResult{
                            false,
                            std::format("time scale: '{}' is not a number at or above zero", arguments[1])};
                    }
                    // Zero is §35.2's frozen row and is accepted on purpose: it is the state the
                    // design calls "essential for pixel-regression tests".
                    clock.timeScale = scale;
                    return CommandResult{true, std::format("time scale: {}", Reading(clock))};
                }
                if (verb == "advance")
                {
                    double days = 0.0;
                    if (!ParseDouble(arguments[1], days))
                    {
                        return CommandResult{
                            false, std::format("time advance: '{}' is not a number of days", arguments[1])};
                    }
                    // CALENDAR days, which is what a person means by "advance 30": a month later,
                    // not thirty sunrises. `SetCalendar` is exact, so `advance 1` really is one
                    // day later and not one day plus the compression's rounding.
                    clock.SetCalendar(clock.CalendarDays() + days);
                    return CommandResult{true, std::format("time advance {:g}: {}", days, Reading(clock))};
                }
                return CommandResult{false, std::format("time: '{}' is not set, scale or advance", verb)};
            });
    }

} // namespace cnahouse::debug
