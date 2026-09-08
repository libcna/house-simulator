// SPDX-License-Identifier: MIT
#pragma once

#include "Microsoft/Xna/Framework/Vector3.hpp"

#include "cnahouse/physics/Sweep.hpp"

namespace cnahouse::physics
{

    /// @brief §11.5: *"bilinear sample + a triangle test for slopes > 20°"*.
    inline constexpr float kTerrainTriangleSlopeDegrees = 20.0F;

    /// @brief The cosine of that, which is what a triangle's `normal.Y` is compared against.
    inline constexpr float kTerrainTriangleSlopeCosine = 0.9396926F;

    /// @brief The ground at one point.
    struct TerrainSample
    {
        /// @brief Whether the point is over the field at all. Outside it, `height` is the nearest
        ///        edge's -- clamped, not extrapolated, because a lot does not continue for ever
        ///        and §64's playable boundary is what stops a body going there.
        bool over = false;
        float height = 0.0F;
        /// @brief Unit and pointing up.
        Microsoft::Xna::Framework::Vector3 normal;
        /// @brief Index into `CollisionWorld::surfaces`: what a footstep on this sounds like.
        std::uint16_t surface = 0u;
        /// @brief The square this point is in is steeper than `kTerrainTriangleSlopeDegrees`, so
        ///        the height and the normal come from its TRIANGLES rather than from the bilinear
        ///        surface over it.
        bool steep = false;
    };

    /// @brief §11.5's ground query: the height, the normal and the material at (@p x, @p z).
    ///
    /// **One surface, and it is the triangles.** §11.5 asked for a bilinear sample, and a bilinear
    /// patch cannot be the collision surface: over the same four samples it and the two triangles
    /// differ by a quarter of the square's TWIST, and on this lot that reaches 74 mm on a square
    /// that is not even steep by §11.5's own 20°. A body told the ground is at the bilinear height
    /// and placed a millimetre over it stands 73 mm inside the ground it is drawn on, and gets
    /// shoved back out every tick. So this answers with the surface `SweepCapsuleTerrain` collides
    /// with, and `cna-house.md` §11.5 was corrected to say so.
    ///
    /// `steep` still reports §11.5's 20°, which is what it is genuinely useful for: telling a
    /// caller that the ground here is a slope rather than a lawn (§60).
    [[nodiscard]] TerrainSample TerrainAt(const CollisionTerrain& terrain, float x, float z);

    /// @brief Sweeps @p capsule along @p motion against the ground.
    ///
    /// Exact, and by the same code every other triangle in the world is swept against: the squares
    /// the motion crosses become triangles and `SweepCapsuleTriangle` does the rest. A height
    /// field could be swept analytically, and then there would be two capsule-vs-surface
    /// implementations to keep agreeing with each other.
    [[nodiscard]] SweepHit SweepCapsuleTerrain(const CollisionTerrain& terrain,
                                               const Capsule& capsule,
                                               const Microsoft::Xna::Framework::Vector3& motion);

    /// @brief Is @p capsule inside the ground, and by how much?
    [[nodiscard]] Overlap OverlapCapsuleTerrain(const CollisionTerrain& terrain, const Capsule& capsule);

} // namespace cnahouse::physics
