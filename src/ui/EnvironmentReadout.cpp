// SPDX-License-Identifier: MIT
#include "cnahouse/ui/EnvironmentReadout.hpp"

#include <algorithm>
#include <format>
#include <string_view>

#include "Microsoft/Xna/Framework/Color.hpp"
#include "Microsoft/Xna/Framework/Graphics/SpriteBatch.hpp"
#include "Microsoft/Xna/Framework/Vector2.hpp"

#include "cnahouse/environment/Season.hpp"
#include "cnahouse/ui/TextRenderer.hpp"

namespace cnahouse::ui
{
    namespace
    {
        [[nodiscard]] std::string_view SeasonName(int season) noexcept
        {
            switch (season)
            {
                case static_cast<int>(environment::Season::Spring):
                    return "Spring";
                case static_cast<int>(environment::Season::Summer):
                    return "Summer";
                case static_cast<int>(environment::Season::Autumn):
                    return "Autumn";
                case static_cast<int>(environment::Season::Winter):
                    return "Winter";
                default:
                    return "Season";
            }
        }

        [[nodiscard]] std::string SeasonLabel(const environment::SeasonPhase& phase)
        {
            if (phase.blend <= 0.0F)
            {
                return std::string(SeasonName(phase.primary));
            }

            // The outer fifth of a season is genuinely a blend, not an instant enum switch. Keep
            // the two names in calendar order on both sides of the boundary: Winter/Spring does
            // not become Spring/Winter merely because `primary` changed at the equinox.
            constexpr int kSeasonCount = static_cast<int>(environment::Season::Count);
            const bool secondaryComesNext = phase.secondary == (phase.primary + 1) % kSeasonCount;
            const int earlier = secondaryComesNext ? phase.primary : phase.secondary;
            const int later = secondaryComesNext ? phase.secondary : phase.primary;
            return std::format("{}/{}", SeasonName(earlier), SeasonName(later));
        }
    } // namespace

    std::string EnvironmentReadout::Line(const environment::SimClock& clock) const
    {
        const environment::CivilTime wall = clock.Wall();
        const environment::SeasonPhase phase = clock.Season();
        // Floor rather than round: `[0, 1)` must display 0..99 %. Showing 100 % before the cycle
        // has wrapped would make the readout claim the year is finished while it is still moving.
        const int yearPercent = std::clamp(static_cast<int>(phase.yearFraction * 100.0F), 0, 99);
        return std::format("{:02}:{:02}  ·  {}  ·  Year {}%  ·  {:.1f} °C",
                           wall.hour,
                           wall.minute,
                           SeasonLabel(phase),
                           yearPercent,
                           clock.OutdoorTemperatureC());
    }

    void EnvironmentReadout::Draw(Microsoft::Xna::Framework::Graphics::SpriteBatch& batch,
                                  const TextRenderer& text,
                                  const environment::SimClock& clock) const
    {
        if (!text.HasFont())
        {
            return;
        }
        text.DrawShadowed(batch,
                          Line(clock),
                          Microsoft::Xna::Framework::Vector2(0.0F, 62.0F),
                          Anchor::TopCentre,
                          Microsoft::Xna::Framework::Color::White);
    }

} // namespace cnahouse::ui
