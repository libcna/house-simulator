// SPDX-License-Identifier: MIT
#include "cnahouse/player/BoundaryGuard.hpp"

#include <algorithm>

namespace cnahouse::player
{

    bool BoundaryGuard::Contain(Microsoft::Xna::Framework::Vector3& position) noexcept
    {
        const bool inside = volume_.Contains(position);
        if (!inside)
        {
            position = Microsoft::Xna::Framework::Vector3(std::clamp(position.X, volume_.minX, volume_.maxX),
                                                          std::clamp(position.Y, volume_.minY, volume_.maxY),
                                                          std::clamp(position.Z, volume_.minZ, volume_.maxZ));
            if (!outside_)
            {
                // An EDGE. A body pressed against the boundary for a second would otherwise count
                // 120 escapes, and the assertion `HOUSE-00618` makes is that the number is zero --
                // which only means anything if one escape counts as one.
                ++escapes_;
            }
        }
        outside_ = !inside;
        return !inside;
    }

} // namespace cnahouse::player
