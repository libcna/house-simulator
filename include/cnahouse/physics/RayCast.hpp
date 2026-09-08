// SPDX-License-Identifier: MIT
#pragma once

#include "Microsoft/Xna/Framework/Vector3.hpp"

#include "cnahouse/physics/Terrain.hpp"

namespace cnahouse::physics
{

    /// @brief What a ray met.
    struct RayHit
    {
        bool hit = false;
        /// @brief Metres along the ray. A unit @p direction is what makes this a distance.
        float distance = 0.0f;
        Microsoft::Xna::Framework::Vector3 point;
        /// @brief Unit, and always facing back towards where the ray came from.
        ///
        /// A triangle's winding is for drawing (§14) and a wall is opaque from both sides, so the
        /// normal is chosen by the ray rather than read off the geometry: `dot(normal, direction)`
        /// is never positive.
        Microsoft::Xna::Framework::Vector3 normal;
        /// @brief The global shape index that was hit, or `kTerrain`, or `kNothing`.
        std::uint32_t shape = kNothing;
        /// @brief Index into `CollisionWorld::surfaces`: what was hit, for a footstep or a decal.
        std::uint16_t surface = 0u;

        /// @brief Sentinel for "nothing was hit", which is not shape 0.
        static constexpr std::uint32_t kNothing = 0xFFFFFFFFu;
        /// @brief Sentinel for "the ground", which is not one of the world's shapes.
        static constexpr std::uint32_t kTerrain = 0xFFFFFFFEu;
    };

    /// @brief The ray against one yawed box.
    ///
    /// A slab test in the box's own frame, which is where the box is axis-aligned -- the same
    /// reduction `SweepCapsuleObb` rests on, minus the rounding, because a ray has no radius.
    ///
    /// **A ray that starts INSIDE the box hits it at distance 0**, with the normal of the face it
    /// would leave by reversed. §50.1's occlusion test asks "is there anything between the eye and
    /// the thing", and an eye inside a wall is occluded by that wall.
    [[nodiscard]] RayHit RayCastObb(const Microsoft::Xna::Framework::Vector3& origin,
                                    const Microsoft::Xna::Framework::Vector3& direction,
                                    float maxDistance,
                                    const CollisionObb& obb);

    /// @brief The ray against one triangle, both sides.
    [[nodiscard]] RayHit RayCastTriangle(const Microsoft::Xna::Framework::Vector3& origin,
                                         const Microsoft::Xna::Framework::Vector3& direction,
                                         float maxDistance,
                                         const Microsoft::Xna::Framework::Vector3& a,
                                         const Microsoft::Xna::Framework::Vector3& b,
                                         const Microsoft::Xna::Framework::Vector3& c);

    /// @brief The ray against §11.5's ground.
    ///
    /// Walks the height field square by square along the ray -- a 2-D grid traversal -- rather
    /// than testing every square of its bounding box. A ray across the lot covers 80 squares of
    /// its length and 5 120 of its box, and §46's sun-visibility test is allowed to be long.
    ///
    /// The first square with a hit gives the NEAREST hit, and needs no comparison against later
    /// ones: a square's triangles lie inside that square's own footprint, so a hit on them is at a
    /// distance inside the range over which the ray is in that square.
    [[nodiscard]] RayHit RayCastTerrain(const CollisionTerrain& terrain,
                                        const Microsoft::Xna::Framework::Vector3& origin,
                                        const Microsoft::Xna::Framework::Vector3& direction,
                                        float maxDistance);

    /// @brief The nearest thing the ray meets in @p cell, ground included (`HOUSE-00546`).
    ///
    /// §50.1's occlusion test -- *"if occluded by static geometry between eye and hit: continue"*
    /// -- is the caller this exists for, and it asks a short question: is anything in the way over
    /// 2.6 m. The broad phase narrows the cell to the shapes whose bucket the ray crosses.
    [[nodiscard]] RayHit RayCastCell(const CollisionWorld& world,
                                     const CollisionCell& cell,
                                     class BroadPhase& broad,
                                     const Microsoft::Xna::Framework::Vector3& origin,
                                     const Microsoft::Xna::Framework::Vector3& direction,
                                     float maxDistance);

} // namespace cnahouse::physics
