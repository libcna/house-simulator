// SPDX-License-Identifier: MIT
#pragma once

#include "Microsoft/Xna/Framework/Vector3.hpp"

#include <cstdint>

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

    /// @brief How far, and which way, a capsule is inside something.
    ///
    /// `normal` points the way OUT and `depth` is how far along it the surface is. Both are what
    /// §49.3's step 5 pushes along; `depth` is not used to size the push -- that is a fixed 0.02 m
    /// -- but it is what decides which of several overlaps is the deepest and therefore which
    /// normal wins.
    struct Overlap
    {
        bool overlapped = false;
        float depth = 0.0f;
        Microsoft::Xna::Framework::Vector3 normal;
    };

    /// @brief Is @p capsule inside @p obb, and by how much (`HOUSE-00547`)?
    [[nodiscard]] Overlap OverlapCapsuleObb(const Capsule& capsule, const CollisionObb& obb);

    /// @brief Is @p capsule inside the triangle @p a @p b @p c, and by how much?
    [[nodiscard]] Overlap OverlapCapsuleTriangle(const Capsule& capsule,
                                                 const Microsoft::Xna::Framework::Vector3& a,
                                                 const Microsoft::Xna::Framework::Vector3& b,
                                                 const Microsoft::Xna::Framework::Vector3& c);

    /// @brief A sphere is a capsule with no segment, and §45's camera arm is the caller.
    ///
    /// Named because §45 names it -- *"sweep a sphere of radius 0.22 from the pivot to the desired
    /// camera position"* -- and not because it is a different calculation: `Capsule{centre, 0, radius}` IS
    /// a sphere, and `SweepCapsuleObb` and `SweepCapsuleTriangle` are already exact for one. A
    /// second implementation would be a second thing to keep right.
    [[nodiscard]] inline Capsule Sphere(const Microsoft::Xna::Framework::Vector3& centre, float radius)
    {
        return Capsule{centre, 0.0f, radius};
    }

    /// @brief What a sweep against a whole cell found, and which shape it was.
    struct CellSweepHit : SweepHit
    {
        /// @brief The GLOBAL shape index that was hit, or `kNothing`.
        std::uint32_t shape = kNothing;
        /// @brief Sentinel for "nothing was hit", which is not shape 0.
        static constexpr std::uint32_t kNothing = 0xFFFFFFFFu;
        /// @brief Shapes the narrow phase actually tested. What the broad phase saved.
        std::uint32_t tested = 0u;
    };

    /// @brief The earliest contact of @p capsule swept along @p motion against everything in
    ///        @p cell (`HOUSE-00545`).
    ///
    /// The broad phase narrows the cell to the shapes the swept box overlaps and the narrow phase
    /// takes the earliest of those; @p broad is passed in rather than made here so that a caller
    /// sweeping several times a frame reuses one.
    ///
    /// **Earliest, not first found.** A body walking into a corner meets two walls, and stopping at
    /// whichever the shape list happened to hold first would let it through the other.
    [[nodiscard]] CellSweepHit SweepCell(const CollisionWorld& world,
                                         const CollisionCell& cell,
                                         class BroadPhase& broad,
                                         const Capsule& capsule,
                                         const Microsoft::Xna::Framework::Vector3& motion);

    /// @brief The DEEPEST overlap of @p capsule with anything in @p cell, and which shape it is.
    struct CellOverlap : Overlap
    {
        std::uint32_t shape = CellSweepHit::kNothing;
        std::uint32_t tested = 0u;
    };

    [[nodiscard]] CellOverlap OverlapCell(const CollisionWorld& world,
                                          const CollisionCell& cell,
                                          class BroadPhase& broad,
                                          const Capsule& capsule);

    /// @brief §49.3 step 5: **4 iterations of 0.02 m push-out along the deepest overlap normal.**
    struct Depenetration
    {
        /// @brief Where the capsule ended up, relative to where it started.
        Microsoft::Xna::Framework::Vector3 offset;
        /// @brief Iterations actually run, 0 when it was clear to begin with.
        int iterations = 0;
        /// @brief Whether it ended clear of everything. **False is not a failure to report and
        ///        forget**: a body still overlapping after four pushes is somewhere it should
        ///        never have reached, and §49.5's guarantee suite is what notices.
        bool resolved = true;
        /// @brief The deepest overlap found on the FIRST iteration, for the overlay to show.
        float deepest = 0.0f;
    };

    /// @brief The fixed step's last act: nudge @p capsule out of whatever it is inside.
    ///
    /// **A fixed 0.02 m per iteration, not the measured depth.** Pushing out by the depth resolves
    /// in one step and teleports a body that has ended up deeply buried -- through a wall, into
    /// the room beyond -- which is worse than the overlap. Four small pushes move at most 0.08 m
    /// and a body that needs more than that has gone somewhere the rest of the step should have
    /// stopped it reaching.
    [[nodiscard]] Depenetration Depenetrate(const CollisionWorld& world,
                                            const CollisionCell& cell,
                                            class BroadPhase& broad,
                                            const Capsule& capsule);

    /// @brief §49.3's push-out per iteration, in metres.
    inline constexpr float kDepenetrationStep = 0.02f;
    /// @brief §49.3's iteration count.
    inline constexpr int kDepenetrationIterations = 4;

} // namespace cnahouse::physics
