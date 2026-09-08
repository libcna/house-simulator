// SPDX-License-Identifier: MIT
#pragma once

#include "Microsoft/Xna/Framework/Vector3.hpp"

#include "cnahouse/physics/CollisionData.hpp"

namespace cnahouse::physics
{

    /// @brief An upright capsule: `cna-house.md` §49.3's player and pet body.
    ///
    /// **The axis is +Y and there is nowhere to say otherwise.** A character capsule does not tip
    /// over, and the whole of `SweepCapsuleObb` rests on it: a `CollisionObb`'s only rotation is
    /// yaw about +Y (§49.2), and yaw preserves verticality, so in the box's own frame the capsule
    /// is still upright. That is what makes the Minkowski sum of the two a ROUNDED BOX rather than
    /// a general swept volume, and it is why this file is a page of geometry rather than a library.
    struct Capsule
    {
        /// @brief The middle of the capsule: half-way up the segment, not the feet.
        Microsoft::Xna::Framework::Vector3 centre;
        /// @brief Half the distance between the two cap centres. A sphere has 0.
        float halfHeight = 0.0f;
        float radius = 0.0f;

        /// @brief The lowest point, which is what a ground probe wants.
        [[nodiscard]] float Bottom() const
        {
            return centre.Y - halfHeight - radius;
        }

        [[nodiscard]] float Top() const
        {
            return centre.Y + halfHeight + radius;
        }
    };

    /// @brief What a sweep found.
    struct SweepHit
    {
        bool hit = false;
        /// @brief Fraction of the motion at which the capsule first touches, in [0, 1].
        ///
        /// **0 means it was already touching**, which is a different thing from "hit immediately
        /// after moving" and the caller has to treat it differently -- depenetrate, do not slide.
        float time = 1.0f;
        /// @brief Unit, pointing OUT of the box towards the capsule. What a slide reflects about.
        Microsoft::Xna::Framework::Vector3 normal;
        /// @brief Whether the capsule was already overlapping the box before it moved.
        bool startedInside = false;
    };

    /// @brief Sweeps @p capsule along @p motion against @p obb (`HOUSE-00543`).
    ///
    /// The capsule against a box is a point against their Minkowski sum, and for an upright capsule
    /// and a yawed box that sum is a box of half-extents `(ex, ey + halfHeight, ez)` rounded by
    /// `radius`. So this is a ray against a rounded box in the box's own frame, which is exact --
    /// no iteration, no margin, and corners that are round rather than square.
    ///
    /// **Square corners are the bug this avoids.** Expanding the box by the radius on all three
    /// axes and calling that the answer is one line shorter and wrong by up to `radius(√3 − 1)` at
    /// a corner: the body stops short of the geometry, on nothing, and the feel of it is catching
    /// on doorframes.
    ///
    /// A zero-length motion answers whether the two overlap NOW, with `time` 0 and
    /// `startedInside` set; nothing else about it is a special case.
    [[nodiscard]] SweepHit SweepCapsuleObb(const Capsule& capsule,
                                           const Microsoft::Xna::Framework::Vector3& motion,
                                           const CollisionObb& obb);

    /// @brief Sweeps @p capsule along @p motion against the triangle @p a @p b @p c
    ///        (`HOUSE-00544`).
    ///
    /// The stair ramps and the rafter envelope are triangle meshes (§49.2), and the terrain will be
    /// one. The reduction is the same as the box's and the shape is harder: an upright capsule
    /// against a triangle is a point against the triangle extruded along Y by `halfHeight` and then
    /// rounded by `radius` -- a rounded prism of six vertices, nine edges and five faces.
    ///
    /// Exact, and by enumeration rather than iteration: the ray is tested against each face's
    /// offset plane where the contact lands inside that face, each edge as a cylinder, and each
    /// vertex as a sphere, and the earliest valid contact wins. **A rounded convex body has no
    /// closed form that skips the edges**, and skipping them is what makes a body catch on the
    /// seam between two triangles of the same flat floor.
    ///
    /// A degenerate triangle -- two vertices in the same place, or three in a line -- has no
    /// surface to hit and returns a miss rather than a normal made of noise. The winding does not
    /// matter: a sweep is stopped by a surface from either side, and §14's winding is for drawing.
    [[nodiscard]] SweepHit SweepCapsuleTriangle(const Capsule& capsule,
                                                const Microsoft::Xna::Framework::Vector3& motion,
                                                const Microsoft::Xna::Framework::Vector3& a,
                                                const Microsoft::Xna::Framework::Vector3& b,
                                                const Microsoft::Xna::Framework::Vector3& c);

} // namespace cnahouse::physics
