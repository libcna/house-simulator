// SPDX-License-Identifier: MIT
#include "cnahouse/environment/Season.hpp"

#include <cmath>

namespace cnahouse::environment
{
    namespace
    {
        constexpr int kSeasons = static_cast<int>(Season::Count);
        constexpr double kSeasonLength = 1.0 / static_cast<double>(kSeasons);

        [[nodiscard]] double Wrap(double value) noexcept
        {
            if (!std::isfinite(value))
            {
                return 0.0;
            }
            return value - std::floor(value);
        }
    } // namespace

    SeasonPhase SeasonAt(double yearFraction) noexcept
    {
        const double wrapped = Wrap(yearFraction);
        SeasonPhase phase;
        phase.yearFraction = static_cast<float>(wrapped);

        // Which quarter, and where in it. The index is clamped rather than trusted: `wrapped` is
        // in [0, 1) by construction, but one ULP below 1.0 times 4 is 4.0 on a machine that
        // rounds, and a `primary` of 4 would index past the end of every per-season table there
        // will ever be.
        int index = static_cast<int>(wrapped / kSeasonLength);
        index = index < 0 ? 0 : (index >= kSeasons ? kSeasons - 1 : index);
        const double within = (wrapped - static_cast<double>(index) * kSeasonLength) / kSeasonLength;

        phase.primary = index;
        const double ramp = static_cast<double>(kSeasonBlendFraction);
        if (within < ramp)
        {
            // The opening fifth: still partly the season before, fading out.
            phase.secondary = (index + kSeasons - 1) % kSeasons;
            phase.blend = static_cast<float>(0.5 * (1.0 - within / ramp));
        }
        else if (within > 1.0 - ramp)
        {
            // The closing fifth: already partly the season after. §36.3's own example.
            phase.secondary = (index + 1) % kSeasons;
            phase.blend = static_cast<float>(0.5 * (within - (1.0 - ramp)) / ramp);
        }
        else
        {
            // The middle three fifths are the season and nothing else. `secondary` still names the
            // season ahead rather than being left as `primary`: a caller that mixes unconditionally
            // gets the right answer with a zero weight, and one that reads `secondary` to decide
            // what is coming gets an answer that does not depend on the blend having started.
            phase.secondary = (index + 1) % kSeasons;
            phase.blend = 0.0F;
        }
        return phase;
    }

    float MixBySeason(const SeasonPhase& phase, const float (&perSeason)[4]) noexcept
    {
        const int primary = phase.primary < 0 || phase.primary >= kSeasons ? 0 : phase.primary;
        const int secondary = phase.secondary < 0 || phase.secondary >= kSeasons ? primary : phase.secondary;
        const float blend = phase.blend < 0.0F ? 0.0F : (phase.blend > 1.0F ? 1.0F : phase.blend);
        return perSeason[primary] * (1.0F - blend) + perSeason[secondary] * blend;
    }

} // namespace cnahouse::environment
