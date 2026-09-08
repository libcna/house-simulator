// SPDX-License-Identifier: MIT
#pragma once

#include <string_view>

#include "Microsoft/Xna/Framework/Vector3.hpp"

#include "cnahouse/physics/Move.hpp"
#include "cnahouse/physics/Terrain.hpp"

namespace cnahouse::physics
{

    /// @brief How far below the body's feet §49.3's *"short downward sweep"* looks, in metres.
    ///
    /// The number has to sit between two others. Below it: the largest gap the rest of the fixed
    /// step can legitimately leave under a body that IS standing -- a slide backs off by a
    /// thousandth of its step and a depenetration pushes out by 0.02 m, so 0.02 m is the floor.
    /// Above it: §43.1's 0.45 m step-down, which is the mechanism for a gap bigger than "resting"
    /// and must not be pre-empted by the probe calling that gap solid ground. 0.05 m is clear of
    /// both, and is under a third of the 0.22 m step-up, so standing beside a kerb is not standing
    /// on it.
    inline constexpr float kGroundProbeReach = 0.05F;

    /// @brief §49.3 step 3: *"a short downward sweep; sets onGround, groundNormal, surfaceKind,
    ///        cellId"*.
    struct GroundProbeResult
    {
        /// @brief Something within `kGroundProbeReach` that §43.1 says can be stood on.
        bool onGround = false;
        /// @brief Something is down there, but it is steeper than §43.1's 46°. `onGround` is
        ///        false and the body is on a slope it will slide off, which is not the same as
        ///        being over nothing.
        bool steep = false;
        /// @brief How far the feet are above it, in metres. 0 when they are touching.
        float distance = 0.0F;
        /// @brief The world height of the surface under the body.
        float height = 0.0F;
        /// @brief Unit, out of the surface. `SlideResult` and `Fall` compare against this.
        Microsoft::Xna::Framework::Vector3 normal;
        /// @brief Index into `CollisionWorld::surfaces` -- what a footstep on this sounds like.
        std::uint16_t surface = 0u;
        /// @brief What kind of thing it is: §60 wants `Stair` and the rest want `Floor`.
        CollisionKind kind = CollisionKind::Floor;
        /// @brief The cell whose geometry answered, for `HOUSE-00559`'s cell tracking.
        std::string_view cellId;
        /// @brief True when the ground is §11.5's height field rather than one of the shapes.
        bool terrain = false;
    };

    /// @brief Finds what @p capsule is standing on in @p cell.
    ///
    /// **A downward SWEEP and not a height lookup.** A capsule resting on a slope touches it
    /// UPHILL of its centre, so its lowest point is not where the ground under its centre is: on a
    /// 17° square the difference is 13 mm and on a 60° one it is 0.36 m (`HOUSE-00553`). Sweeping
    /// the body's own shape gets that right without anybody having to remember the formula.
    ///
    /// The ground is §11.5's height field as well as the cell's shapes, because §49.2's exterior
    /// collision is *"the terrain height field plus OBBs"* and a body on the lawn is standing on
    /// the former.
    [[nodiscard]] GroundProbeResult GroundProbe(const CollisionWorld& world,
                                                const CollisionCell& cell,
                                                class BroadPhase& broad,
                                                const Capsule& capsule,
                                                float reach = kGroundProbeReach);

} // namespace cnahouse::physics
