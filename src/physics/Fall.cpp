// SPDX-License-Identifier: MIT
#include "cnahouse/physics/Fall.hpp"

#include <algorithm>

#include "cnahouse/physics/BroadPhase.hpp"

namespace cnahouse::physics
{
    namespace
    {
        namespace Xna = Microsoft::Xna::Framework;
    }

    FallStep Fall(const CollisionWorld& world,
                  const CollisionCell& cell,
                  BroadPhase& broad,
                  const Capsule& capsule,
                  const FallState& state,
                  float dt)
    {
        FallStep result;
        result.position = capsule.centre;
        result.state = state;

        if (state.onGround)
        {
            // A body standing still stays exactly still. Integrating gravity into a body that is
            // already resting on something builds up a speed it can never spend and then hands it
            // to the first step that walks off an edge, which lands the body hard from a kerb.
            result.state.speed = 0.0F;
            return result;
        }

        // Semi-implicit Euler: speed first, then move at the new speed.
        result.state.speed = std::min(state.speed + kGravity * dt, kTerminalFallSpeed);
        const float distance = result.state.speed * dt;

        const CellSweepHit hit = SweepCell(world, cell, broad, capsule, Xna::Vector3(0.0F, -distance, 0.0F));
        if (!hit.hit)
        {
            result.position = Xna::Vector3(capsule.centre.X, capsule.centre.Y - distance, capsule.centre.Z);
            return result;
        }

        // §49.3's 0.999 again: land a thousandth of the step clear of the floor rather than on it,
        // so the next step's sweep does not start inside what was landed on.
        const float fell = hit.startedInside ? 0.0F : distance * hit.time * kContactBackoff;
        result.position = Xna::Vector3(capsule.centre.X, capsule.centre.Y - fell, capsule.centre.Z);

        if (!IsWalkable(hit.normal))
        {
            // The side of a bank, or the underside of something. The fall is not over -- the body
            // is simply not falling THROUGH this -- and the horizontal part of the step sheds it
            // sideways next tick.
            result.slid = true;
            return result;
        }

        result.state.onGround = true;
        result.state.speed = 0.0F;
        result.drop = std::max(0.0F, state.fellFrom - result.position.Y);
        result.landing = result.drop > kHardLandingDrop ? Landing::Hard : Landing::Soft;
        return result;
    }

} // namespace cnahouse::physics
