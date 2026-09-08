// SPDX-License-Identifier: MIT
#include "cnahouse/player/EyeProbe.hpp"

namespace cnahouse::player
{

    EyeClearance ProbeAhead(const physics::CollisionWorld& world,
                            const physics::CollisionCell& cell,
                            physics::BroadPhase& broad,
                            const CameraPose& pose)
    {
        // The CAMERA's forward and not the body's: the head bob's sway (`HOUSE-00627`) turns the
        // view a third of a degree off the body, the pitch turns it a great deal more, and a
        // player looking down at the floor they are standing on is the commonest way to meet a
        // surface at arm's length.
        const physics::RayHit ahead =
            physics::RayCastCell(world, cell, broad, pose.eye, pose.forward, kEyeProbeDistance);
        return ClearanceForProbe(ahead.hit, ahead.distance);
    }

} // namespace cnahouse::player
