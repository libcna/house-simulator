// SPDX-License-Identifier: MIT
#include "cnahouse/physics/Fall.hpp"

#include <algorithm>

#include "cnahouse/physics/BroadPhase.hpp"
#include "cnahouse/physics/Terrain.hpp"

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

        const Xna::Vector3 down(0.0F, -distance, 0.0F);
        const CellSweepHit shapes = SweepCell(world, cell, broad, capsule, down);
        // §11.5's ground is the other thing a body can land on, and outdoors it is the ONLY one.
        // Left out of this sweep until `HOUSE-00614` dropped 2 000 bodies and found that every
        // landing on the lawn was silent: the fall passed through the height field and was ended
        // by `GroundProbe` instead, which sets `onGround` and reports NO landing -- so §47.2's
        // sound, §45's camera dip and §43.1's hard landing all went missing outdoors, and a body
        // arriving at 12 m/s settled a few centimetres INSIDE the ground, where the depenetration
        // (which reads shapes and not the field) could not push it out.
        const SweepHit ground = SweepCapsuleTerrain(world.terrain, capsule, down);

        // Whichever is nearer, the same rule `GroundProbe` uses: a body over a terrace has the
        // slab under it and the lawn under that, and the one it lands on is the slab.
        SweepHit hit;
        if (shapes.hit)
        {
            hit = static_cast<const SweepHit&>(shapes);
        }
        if (ground.hit && (!hit.hit || ground.time < hit.time))
        {
            hit = ground;
        }
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
