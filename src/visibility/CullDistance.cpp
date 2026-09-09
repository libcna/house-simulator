// SPDX-License-Identifier: MIT
#include "cnahouse/visibility/CullDistance.hpp"

#include <algorithm>

namespace cnahouse::visibility
{
    namespace
    {
        /// §68's band. Outside it the setting is not a preference any more: below 0.6 the garden
        /// empties as the player walks down it, and above 1.4 the exterior costs more than the
        /// house without adding anything a player looks at.
        constexpr float kMinViewScale = 0.6F;
        constexpr float kMaxViewScale = 1.4F;
    } // namespace

    std::string_view CategoryName(PropCategory category) noexcept
    {
        switch (category)
        {
            case PropCategory::SmallProp:
                return "small prop";
            case PropCategory::GardenFurniture:
                return "garden furniture";
            case PropCategory::Fence:
                return "fence";
            case PropCategory::Tree:
                return "tree";
            case PropCategory::NeighbourhoodLod0:
                return "neighbourhood lod0";
            case PropCategory::NeighbourhoodLod1:
                return "neighbourhood lod1";
            case PropCategory::NeighbourhoodLod2:
                return "neighbourhood lod2";
            case PropCategory::Impostor:
                return "impostor";
            case PropCategory::Count:
                break;
        }
        return "?";
    }

    float ClampViewDistanceScale(float viewDistanceScale) noexcept
    {
        return std::clamp(viewDistanceScale, kMinViewScale, kMaxViewScale);
    }

    float CullDistanceFor(PropCategory category, float viewDistanceScale) noexcept
    {
        const auto index = static_cast<std::size_t>(category);
        if (index >= kCullDistances.size())
        {
            return 0.0F;
        }
        return kCullDistances[index] * ClampViewDistanceScale(viewDistanceScale);
    }

    bool WithinCullDistance(PropCategory category, float distance, float viewDistanceScale) noexcept
    {
        // `<=`, so the stated distance is the last one at which the thing is drawn rather than the
        // first at which it is not -- the same boundary rule §26.4's detail sets use, for the same
        // reason: a prop that pops as the player walks a millimetre is worse than one drawn a
        // millimetre too far.
        return distance <= CullDistanceFor(category, viewDistanceScale);
    }

} // namespace cnahouse::visibility
