// SPDX-License-Identifier: MIT
#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string_view>

namespace cnahouse::visibility
{

    /// @brief §25.6's categories, in the order its list gives them (`HOUSE-00674`).
    ///
    /// *"Distance culling per category: small props 45 m, garden furniture 70 m, fences 120 m,
    /// trees 180 m, neighbourhood LOD0 90 m, LOD1 160 m, LOD2 300 m, impostors 420 m."*
    ///
    /// **Why a category and not a per-instance number.** `EXT_WORLD` holds about 4 100 instances
    /// and portal traversal cannot help inside one cell (§25.6), so the distance test is what
    /// stands between the frame and all of them. A number per instance would be 4 100 numbers to
    /// author, get wrong and re-tune; nine categories are nine decisions, and every instance in
    /// the exterior data already declares what KIND of thing it is.
    enum class PropCategory : std::uint8_t
    {
        SmallProp,
        GardenFurniture,
        Fence,
        Tree,
        NeighbourhoodLod0,
        NeighbourhoodLod1,
        NeighbourhoodLod2,
        Impostor,
        /// @brief The ground itself: terrain tiles, road segments, sidewalks, the garden's paved
        ///        surfaces (`HOUSE-00700`).
        ///
        /// §25.6's step 1 names *"terrain tiles, road segments"* among the instances and its
        /// step 2 gives them no distance, which is not an omission: **the ground is not distance
        /// culled.** A lawn you cannot see because it is 200 m away is a lawn outside the frustum
        /// or behind the far plane, and both of those are already answered. So this category's
        /// distance is §10.3's far plane and the frustum does the work.
        Ground,
        Count,
    };

    /// @brief §25.6's distances, in metres, in the enum's order.
    inline constexpr std::array<float, static_cast<std::size_t>(PropCategory::Count)> kCullDistances{
        45.0F, 70.0F, 120.0F, 180.0F, 90.0F, 160.0F, 300.0F, 420.0F, 420.0F};

    [[nodiscard]] std::string_view CategoryName(PropCategory category) noexcept;

    /// @brief How far @p category is drawn, with §68's view-distance setting applied.
    ///
    /// §68 offers 0.6x to 1.4x and §71.3 gives each quality tier its own value; it multiplies the
    /// far plane and the residency radius, and these are the same decision seen per category. The
    /// scale is clamped to that band here so that a settings file cannot turn the exterior off
    /// altogether or push it past §10.3's 420 m far plane, where nothing is drawn anyway.
    [[nodiscard]] float CullDistanceFor(PropCategory category, float viewDistanceScale = 1.0F) noexcept;

    /// @brief §68's 0.6x-1.4x band, applied to @p viewDistanceScale.
    ///
    /// Exposed because `HOUSE-00678`'s hierarchy rejects a whole subtree against the LARGEST
    /// distance under it, and that multiplication has to be the same one `CullDistanceFor` does to
    /// each instance. A node rejected at an unclamped scale would cull instances the per-instance
    /// test would have kept, which is over-culling -- the one failure §25 has no tolerance for.
    [[nodiscard]] float ClampViewDistanceScale(float viewDistanceScale) noexcept;

    /// @brief Is something of @p category at @p distance metres worth drawing?
    [[nodiscard]] bool
    WithinCullDistance(PropCategory category, float distance, float viewDistanceScale = 1.0F) noexcept;

} // namespace cnahouse::visibility
