// SPDX-License-Identifier: MIT
#pragma once

#include "cnahouse/physics/BroadPhase.hpp"
#include "cnahouse/physics/RayCast.hpp"
#include "cnahouse/player/FirstPersonCamera.hpp"

namespace cnahouse::player
{

    /// @brief §44's *"0.10 m forward probe"*, and the clearance its answer calls for
    ///        (`HOUSE-00628`).
    ///
    /// **A ray and not a sweep, because the thing being avoided is a plane and not a collision.**
    /// The body is already kept out of the wall by §49's capsule; what is left is that the near
    /// plane sits 0.10 m in front of the eye and the eye can legitimately be 0.30 m from a wall
    /// with its nose against it. So the question is only how far the geometry the player is
    /// LOOKING at is, and `RayCastCell` answers exactly that -- including §11.5's ground, which is
    /// what a player lying on a slope is looking at.
    ///
    /// It lives here rather than on the camera because the camera does not query the world: it is
    /// handed an eye height, a pitch, a bob and now a clearance, and every one of those is
    /// somebody else's measurement.
    [[nodiscard]] EyeClearance ProbeAhead(const physics::CollisionWorld& world,
                                          const physics::CollisionCell& cell,
                                          physics::BroadPhase& broad,
                                          const CameraPose& pose);

} // namespace cnahouse::player
