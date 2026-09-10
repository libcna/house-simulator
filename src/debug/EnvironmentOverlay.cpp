// SPDX-License-Identifier: MIT
#include "cnahouse/debug/EnvironmentOverlay.hpp"

#include <format>

#include "Microsoft/Xna/Framework/Color.hpp"
#include "Microsoft/Xna/Framework/Graphics/SpriteBatch.hpp"
#include "Microsoft/Xna/Framework/Vector2.hpp"

#include "cnahouse/environment/DayLength.hpp"
#include "cnahouse/environment/Season.hpp"
#include "cnahouse/ui/TextRenderer.hpp"

namespace cnahouse::debug
{
    namespace
    {
        constexpr const char* kWeekdays[7] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
    } // namespace

    std::string_view SeasonName(int season) noexcept
    {
        switch (season)
        {
            case static_cast<int>(environment::Season::Spring):
                return "spring";
            case static_cast<int>(environment::Season::Summer):
                return "summer";
            case static_cast<int>(environment::Season::Autumn):
                return "autumn";
            case static_cast<int>(environment::Season::Winter):
                return "winter";
            default:
                return "?";
        }
    }

    std::vector<std::string> EnvironmentOverlay::Lines(const environment::SimClock& clock) const
    {
        const environment::CivilTime wall = clock.Wall();
        const environment::CivilTime standard = clock.Standard();
        const environment::SeasonPhase season = clock.Season();
        const double dayLength = environment::DayLengthForTimeScale(clock.timeScale);

        std::vector<std::string> lines;
        lines.push_back("F8  environment");
        // The WALL clock first, because that is the one a person means by "what time is it", with
        // the DST flag beside it so a reading an hour off the standard line is explained rather
        // than looking like a bug.
        lines.push_back(std::format("time     {} {:04}-{:02}-{:02} {:02}:{:02}:{:02} {}",
                                    kWeekdays[wall.weekday >= 0 && wall.weekday < 7 ? wall.weekday : 0],
                                    wall.year,
                                    wall.month,
                                    wall.day,
                                    wall.hour,
                                    wall.minute,
                                    wall.second,
                                    clock.IsDaylightSaving() ? "DST" : "std"));
        // ...and the STANDARD reading under it, because `epochSeconds` is standard time and every
        // number below is derived from that rather than from the wall clock (§35.1).
        lines.push_back(std::format("standard {:02}:{:02}:{:02}  day {} of {}  epoch {:.1f} s",
                                    standard.hour,
                                    standard.minute,
                                    standard.second,
                                    standard.dayOfYear,
                                    standard.year,
                                    clock.epochSeconds));
        lines.push_back(std::format("rate     {:g}x  ({})",
                                    clock.timeScale,
                                    dayLength > 0.0 ? std::format("{:g} real min / sim day", dayLength)
                                                    : std::string("FROZEN")));
        // §35.2b's compression, with both day counts, because the whole point of the field is that
        // they differ: the calendar runs 24 times faster than the sun.
        lines.push_back(std::format("calendar {:.2f} days at {:g}/sim day  ({} sunrise(s))",
                                    clock.CalendarDays(),
                                    clock.calendarDaysPerSimDay,
                                    clock.SimDayIndex()));
        lines.push_back(std::format("season   {} {:.0f}% -> {} ({:.0f}%)  year {:.3f}",
                                    SeasonName(season.primary),
                                    static_cast<double>(1.0F - season.blend) * 100.0,
                                    SeasonName(season.secondary),
                                    static_cast<double>(season.blend) * 100.0,
                                    static_cast<double>(season.yearFraction)));
        // §36.2's base temperature is time-derived and belongs here rather than with the weather:
        // it is what §36 applies its archetype's delta TO, and it is what decides whether the
        // precipitation a person is debugging can be snow at all.
        lines.push_back(
            std::format("temp     {:.1f} C base (§36.2, before weather)", clock.OutdoorBaseTemperatureC()));
        lines.push_back("sun/moon, weather, RNG: §35.3 and §36, not built yet");
        return lines;
    }

    void EnvironmentOverlay::Draw(Microsoft::Xna::Framework::Graphics::SpriteBatch& batch,
                                  const ui::TextRenderer& text,
                                  const environment::SimClock& clock) const
    {
        if (!visible_ || !text.HasFont())
        {
            return;
        }
        // The same virtual units and left margin as §69's `F2` and §71's `F1`: the overlays are
        // read one after another and a reader should not have to find the text again.
        constexpr float kLineHeight = 18.0F;
        constexpr float kLeft = 12.0F;
        constexpr float kTop = 40.0F;

        const std::vector<std::string> lines = Lines(clock);
        for (std::size_t i = 0; i < lines.size(); ++i)
        {
            text.DrawShadowed(
                batch,
                lines[i],
                Microsoft::Xna::Framework::Vector2(kLeft, kTop + static_cast<float>(i) * kLineHeight),
                ui::Anchor::TopLeft,
                Microsoft::Xna::Framework::Color::White);
        }
    }

} // namespace cnahouse::debug
