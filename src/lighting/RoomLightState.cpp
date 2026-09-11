// SPDX-License-Identifier: MIT
#include "cnahouse/lighting/RoomLightState.hpp"

#include <algorithm>

namespace cnahouse::lighting
{

    float RoomLightState::Level() const noexcept
    {
        // The strongest source sets the level and the others lift what is left towards 1. Adding
        // them would make a sunlit room with the lights on brighter than the renderer has range
        // for; taking the maximum would make turning the light on in that room do nothing, and §30
        // has a row about a player watching exactly that.
        const float strongest = std::max({artificial, daylight, borrowed, kAmbientFloor});
        const float rest = artificial + daylight + borrowed - strongest;
        const float lifted = strongest + (1.0F - strongest) * std::clamp(rest, 0.0F, 1.0F);
        return std::clamp(lifted, kAmbientFloor, 1.0F);
    }

} // namespace cnahouse::lighting
