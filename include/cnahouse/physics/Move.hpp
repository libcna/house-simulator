// SPDX-License-Identifier: MIT
#pragma once

#include "Microsoft/Xna/Framework/Vector3.hpp"

#include "cnahouse/physics/Sweep.hpp"

namespace cnahouse::physics
{

    /// @brief §43.1's slope limit: a surface tilted more than this is a wall, not a floor.
    inline constexpr float kSlopeLimitDegrees = 46.0f;

    /// @brief The cosine of `kSlopeLimitDegrees`, which is what a normal's Y is compared against.
    ///
    /// A surface's outward normal makes the same angle with +Y as the surface makes with the
    /// horizontal, so `normal.Y >= cos(limit)` IS the slope test -- no trigonometry per contact.
    inline constexpr float kSlopeLimitCosine = 0.694658370459F;

    /// @brief §49.3 step 2: **three** slide iterations, and no more.
    ///
    /// Not "until it stops moving". Three is enough for a corner (two walls) with one left over,
    /// and a body wedged in a crack that needs a fourth is a body that should stop rather than one
    /// the loop should keep working on -- an unbounded slide loop is how a physics step becomes
    /// unbounded work, and this step has a 1/120 s budget it shares with everything else.
    inline constexpr int kSlideIterations = 3;

    /// @brief §49.3's `hit.t · 0.999`: stop a thousandth of the step short of what was hit.
    ///
    /// Landing exactly ON a surface leaves the next sweep starting inside it -- `time` 0,
    /// `startedInside`, and a body that cannot move along a wall it is touching. The gap is
    /// proportional to the step rather than absolute so it scales with how fast the body is going.
    inline constexpr float kContactBackoff = 0.999F;

    /// @brief Motion shorter than this is nothing: a tenth of a millimetre.
    inline constexpr float kMotionEpsilon = 1.0e-4F;

    /// @brief Can a body stand and walk on a surface with this outward normal (§43.1's 46°)?
    ///
    /// A ceiling is not walkable however flat it is, which falls out of the same test: its normal
    /// points down and `normal.Y` is negative.
    [[nodiscard]] bool IsWalkable(const Microsoft::Xna::Framework::Vector3& normal);

    /// @brief What one collide-and-slide step did.
    struct SlideResult
    {
        /// @brief Where the capsule's centre ended up.
        Microsoft::Xna::Framework::Vector3 position;
        /// @brief The part of the motion still unspent. Zero when all of it was used or slid away.
        Microsoft::Xna::Framework::Vector3 motion;
        /// @brief Sweeps performed, 1 to `kSlideIterations`.
        int iterations = 0;
        /// @brief How many of those hit something.
        int contacts = 0;
        /// @brief **The iterations ran out with motion left to spend.** Not the same as "hit a
        ///        wall": a body that walks straight into one has its whole motion slid away in a
        ///        single iteration and is not `blocked`. This is the wedge case, and `HOUSE-00562`'s
        ///        overlay is what it is reported for.
        bool blocked = false;
        /// @brief The last contact's outward normal, or zero if there was none.
        Microsoft::Xna::Framework::Vector3 lastNormal;
        /// @brief Whether that last contact was walkable.
        bool lastWalkable = false;
        /// @brief Contacts that were too steep to walk on, so the climb was taken out of the slide.
        int steepContacts = 0;
    };

    /// @brief §49.3 step 2: sweep, slide, repeat -- three times, with §43.1's slope limit.
    ///
    /// ```
    /// for iteration in 0..2:
    ///     hit = SweepCapsule(position, velocity·dt)
    ///     if none: position += velocity·dt; break
    ///     position += velocity·dt·hit.t·0.999
    ///     velocity  = velocity − normal·dot(velocity, normal)      // slide
    ///     remaining time reduced by hit.t
    /// ```
    ///
    /// **The slope limit is not in that pseudocode and has to be.** Sliding along a contact plane
    /// keeps whatever climb the plane has, so a body walking into a 70° bank would ride up it: the
    /// projection has no opinion about which surfaces are floors. A contact steeper than
    /// `kSlopeLimitDegrees` therefore has any upward component taken out of the slide afterwards,
    /// which leaves the body moving along the foot of the slope instead of up it.
    ///
    /// **A contact the body starts inside spends no distance.** `time` 0 with `startedInside` is
    /// not "hit immediately"; moving `0.999 · 0` gets nowhere, and the useful thing to do with it
    /// is slide the motion out of the surface and let `Depenetrate` (§49.3 step 5) fix the
    /// position.
    [[nodiscard]] SlideResult CollideAndSlide(const CollisionWorld& world,
                                              const CollisionCell& cell,
                                              class BroadPhase& broad,
                                              const Capsule& capsule,
                                              const Microsoft::Xna::Framework::Vector3& motion);

} // namespace cnahouse::physics
