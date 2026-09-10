// SPDX-License-Identifier: MIT
#include "cnahouse/visibility/LodSelection.hpp"

#include <algorithm>
#include <cmath>

namespace cnahouse::visibility
{
    namespace
    {
        constexpr std::size_t kLevels = static_cast<std::size_t>(LodLevel::Count);

        [[nodiscard]] constexpr std::size_t IndexOf(LodLevel level) noexcept
        {
            return std::min(static_cast<std::size_t>(level), kLevels - 1u);
        }
    } // namespace

    std::string_view LodLevelName(LodLevel level) noexcept
    {
        switch (level)
        {
            case LodLevel::Lod0:
                return "lod0";
            case LodLevel::Lod1:
                return "lod1";
            case LodLevel::Lod2:
                return "lod2";
            case LodLevel::Impostor:
                return "impostor";
            case LodLevel::Culled:
            case LodLevel::Count:
                break;
        }
        return "culled";
    }

    float LodThreshold(LodLevel level) noexcept
    {
        const std::size_t index = IndexOf(level);
        // `Culled` is what is left below the last threshold, so it has none. Returning 0 rather
        // than the last one is what makes `SelectLod`'s deadband test below say "a culled object
        // never falls further".
        return index < kLodThresholds.size() ? kLodThresholds[index] : 0.0F;
    }

    float ProjectedHeight(float radius, float distance, float viewportHeight, float fovYRadians) noexcept
    {
        // A thing with no size projects to nothing, which is `Culled`. Said here rather than left
        // to the arithmetic: a zero radius with a zero distance is 0/0, and `Culled` is the honest
        // answer to "how big is nothing" while a NaN is an answer that poisons the comparison.
        if (!(radius > 0.0F) || !(viewportHeight > 0.0F) || !(fovYRadians > 0.0F))
        {
            return 0.0F;
        }
        // §26.1's own clamp: `d > r`. Inside the sphere the projection has no height to give --
        // the object surrounds the eye -- and the nearest meaningful distance is its own surface.
        const float safe = std::max(distance, radius * (1.0F + 1e-3F));
        const float halfHeight = std::tan(fovYRadians * 0.5F);
        if (!(halfHeight > 0.0F))
        {
            return 0.0F;
        }
        return (2.0F * radius * viewportHeight) / (2.0F * safe * halfHeight);
    }

    LodLevel LevelFor(float height) noexcept
    {
        for (std::size_t index = 0; index < kLodThresholds.size(); ++index)
        {
            if (height >= kLodThresholds[index])
            {
                return static_cast<LodLevel>(index);
            }
        }
        return LodLevel::Culled;
    }

    LodLevel SelectLod(float height, LodLevel previous) noexcept
    {
        const LodLevel metric = LevelFor(height);
        const std::size_t was = IndexOf(previous);
        const std::size_t now = IndexOf(metric);
        if (now < was)
        {
            // Finer. §26.1: "switch up AT the threshold" -- `LevelFor` has already applied it, so
            // there is no second test and no second number to get wrong.
            return metric;
        }
        if (now > was)
        {
            // Coarser, and this is where the deadband lives: it takes 0.85 x the threshold of the
            // level we are AT, not of the one we would fall to. An object at LOD1's 90 px holds
            // LOD1 down to 76.5 and takes LOD1 again only back at 90.
            return height < LodThreshold(previous) * kLodHysteresis ? metric : previous;
        }
        return previous;
    }

    LodLevel Coarsen(LodLevel level, int steps) noexcept
    {
        const int shifted = static_cast<int>(IndexOf(level)) + steps;
        const int last = static_cast<int>(LodLevel::Culled);
        return static_cast<LodLevel>(std::clamp(shifted, 0, last));
    }

} // namespace cnahouse::visibility
