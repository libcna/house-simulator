// SPDX-License-Identifier: MIT
#include "cnahouse/audio/AmbienceDirector.hpp"

#include <algorithm>
#include <cmath>

namespace cnahouse::audio
{

    float AmbienceDirector::DayMix(double sunAltitudeDeg) noexcept
    {
        if (!std::isfinite(sunAltitudeDeg))
        {
            return 0.0F;
        }
        const double span = kDayAltitudeDeg - kNightAltitudeDeg;
        return static_cast<float>(std::clamp((sunAltitudeDeg - kNightAltitudeDeg) / span, 0.0, 1.0));
    }

    AmbienceMix AmbienceDirector::Advance(world::CellKind listenerKind,
                                          double sunAltitudeDeg,
                                          float deltaSeconds) noexcept
    {
        const float target = listenerKind == world::CellKind::Exterior ? 1.0F : 0.0F;
        if (!initialized_)
        {
            exterior_ = target;
            initialized_ = true;
        }
        else if (std::isfinite(deltaSeconds) && deltaSeconds > 0.0F)
        {
            const float step = deltaSeconds / kCellCrossfadeSeconds;
            exterior_ += std::clamp(target - exterior_, -step, step);
        }

        const float day = DayMix(sunAltitudeDeg);
        return AmbienceMix{
            .interior = 1.0F - exterior_,
            .exteriorDay = exterior_ * day,
            .exteriorNight = exterior_ * (1.0F - day),
        };
    }

    void AmbienceDirector::Reset() noexcept
    {
        initialized_ = false;
        exterior_ = 0.0F;
    }

} // namespace cnahouse::audio
