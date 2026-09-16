// SPDX-License-Identifier: MIT
#pragma once

#include <cstdint>
#include <string_view>
#include <vector>

#include "cnahouse/visibility/ExteriorBvh.hpp"
#include "cnahouse/visibility/PortalTraversal.hpp"

namespace cnahouse::world
{
    struct ChunkLibrary;
    class WorldData;
} // namespace cnahouse::world

namespace cnahouse::visibility
{

    /// @brief §25.6's instances, built from the chunks the content build produced
    ///        (`HOUSE-00700`).
    ///
    /// **The outdoors is not a room and the portal walk was never meant to reach it.** §25.6 is
    /// explicit -- *"`EXT_WORLD` is one enormous cell, so portal traversal cannot help inside
    /// it"* -- and its step 1 names *"terrain tiles, road segments, fences, trees, neighbourhood
    /// groups, garden props"* as the things the hierarchy holds. `HOUSE-00677` built that
    /// hierarchy and `HOUSE-00678` its traversal, and until this nothing put the ground in it:
    /// `HOUSE-00780` filed the exterior geometry as per-cell CHUNKS as an interim, so a lawn was
    /// drawn only when the walk happened to reach the yard it was filed under. Measured, that
    /// cost `ext-backyard` 31.23 % of its frame and `b1-gym` a view of `TERRAIN_grass` through a
    /// basement window well.
    ///
    /// A chunk's cell stays its RESIDENCY key (§27.2). What changes is which structure decides
    /// whether it is DRAWN.
    struct ExteriorScene
    {
        /// @brief One per exterior-space, house-outer-skin or weather-facing opening chunk,
        ///        in chunk order. `id` is
        ///        `chunk index + 1`, so it survives the reordering `ExteriorBvh::Build` does.
        std::vector<ExteriorInstance> instances;

        /// @brief The hierarchy over them, built once at load.
        ExteriorBvh bvh;

        /// @brief The chunk `ExteriorBvh::Instances()[index]` came from.
        [[nodiscard]] std::uint32_t ChunkOf(std::uint32_t index) const noexcept;

        [[nodiscard]] bool Empty() const noexcept
        {
            return instances.empty();
        }
    };

    /// @brief §25.6's category for a chunk drawn with @p material.
    ///
    /// Read off the material's own prefix, which the content build already groups by: a chunk IS
    /// one material (§17.4). Anything unrecognised is `Ground`, which is the conservative answer
    /// -- a category only ever removes something at distance, so the safe default is the one that
    /// removes nothing.
    [[nodiscard]] PropCategory CategoryForMaterial(std::string_view material) noexcept;

    /// @brief Whether a material identifies the house's outer skin.
    ///
    /// Outer-skin geometry deliberately keeps the adjacent room as its residency key, but a closed
    /// room is not reached by portal traversal from the yard. The stable authored siding and
    /// water-table ids are therefore the chunk-level bridge that places those surfaces in §25.6's
    /// exterior hierarchy without placing the room's interior walls there too.
    [[nodiscard]] bool IsExteriorSkinMaterial(std::string_view material) noexcept;

    /// @brief Stable weather-facing window roles split from room-owned indoor trim/glass.
    [[nodiscard]] bool IsExteriorWindowMaterial(std::string_view material) noexcept;

    /// @brief Stable weather-facing entry-leaf roles split from room-owned joinery.
    [[nodiscard]] bool IsExteriorDoorMaterial(std::string_view material) noexcept;

    /// @brief Exterior-cell chunks plus house outer skin and outside-facing opening detail,
    ///        with the hierarchy over them.
    ///
    /// @param library the chunks the content build produced.
    /// @param world what says which cells are exterior and validates every chunk's residency key.
    [[nodiscard]] ExteriorScene BuildExteriorScene(const world::ChunkLibrary& library,
                                                   const world::WorldData& world);

    /// @brief The cones §25.6's hierarchy is tested against this frame: the EXTERIOR cells' own.
    ///
    /// **This is the decision, and it is not "the camera frustum".** §25.6 says portal traversal
    /// cannot help INSIDE `EXT_WORLD`; it does not say the outdoors is drawn whenever it is in
    /// front of the eye. What §25.2's walk still answers is *how* the outdoors is being seen --
    /// through which windows, from which yard -- so the instances are tested against the cones of
    /// the exterior cells the walk reached, and against nothing else.
    ///
    /// An INTERIOR cell's cone is the space visible through the doorway that leads to it, and it
    /// does not stop at that room's far wall. Feeding one to the hierarchy accepts the lawn behind
    /// the house because the kitchen is visible through a doorway, which draws a garden no window
    /// looks at: invisible in the picture, because the depth buffer hides it, and paid for in full.
    /// A cell with no view out contributes no cone at all, and then the outdoors costs nothing.
    ///
    /// @param out cleared first, then filled. Reused across frames so the walk allocates nothing.
    void GatherExteriorCones(const world::WorldData& world,
                             std::span<const VisibleCell> visible,
                             std::vector<ClipFrustum>& out);

} // namespace cnahouse::visibility
