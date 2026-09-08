// SPDX-License-Identifier: MIT
#include "cnahouse/physics/Nudgeable.hpp"

#include <algorithm>
#include <cmath>

#include "cnahouse/physics/BroadPhase.hpp"
#include "cnahouse/physics/Ground.hpp"
#include "cnahouse/physics/Move.hpp"

namespace cnahouse::physics
{
    namespace
    {
        namespace Xna = Microsoft::Xna::Framework;

        float Dot(const Xna::Vector3& a, const Xna::Vector3& b)
        {
            return a.X * b.X + a.Y * b.Y + a.Z * b.Z;
        }

        float Length(const Xna::Vector3& v)
        {
            return std::sqrt(Dot(v, v));
        }

        Xna::Vector3 Added(const Xna::Vector3& a, const Xna::Vector3& b)
        {
            return Xna::Vector3(a.X + b.X, a.Y + b.Y, a.Z + b.Z);
        }

        Xna::Vector3 Scaled(const Xna::Vector3& v, float by)
        {
            return Xna::Vector3(v.X * by, v.Y * by, v.Z * by);
        }
    } // namespace

    NudgeReport NudgeStep(const CollisionWorld& world,
                          const CollisionCell& cell,
                          BroadPhase& broad,
                          NudgeableProp& prop,
                          float dt)
    {
        NudgeReport report;
        if (dt <= 0.0F)
        {
            report.resting = prop.resting;
            return report;
        }

        if (prop.resting)
        {
            // Asleep on a support plane. Not integrated at all -- gravity into a resting body is
            // exactly the jitter §49.4's "never sleep-fail" is about -- and only `PushProp` clears
            // the flag.
            report.resting = true;
            report.supported = true;
            return report;
        }

        // What it is standing on, asked the way §49.3's step 3 asks it: a short downward sweep,
        // and NOT "did this step's move happen to hit the floor". A prop resting on a floor is
        // left a thousandth of its step clear of it, so it falls a hair and lands again every
        // step -- and a friction chosen from that alternates between the ground's and the air's.
        // Measured: an effective 0.58 per second against the 1.4 intended, which rolled a shoved
        // football 4.2 m across a 6 m room.
        const GroundProbeResult under = GroundProbe(world, cell, broad, prop.Body());
        report.supported = under.onGround;

        // Damping as the exact solution of `dv/dt = -k·v` over the step, and the displacement as
        // its integral -- not `v -= k·v·dt` with `x += v·dt`, whose answer depends on the step
        // size. §49.3's step is fixed at 1/120, but a fixed step is a promise about the ANSWER and
        // not an excuse to be wrong at any other rate: at 1/30 the cheap form stopped the ball
        // 0.32 m short of where 1/120 stopped it.
        const float rate = report.supported ? kPropGroundDamping : kPropAirDamping;
        const float decay = std::exp(-rate * dt);
        const float travel = (1.0F - decay) / rate;

        // Gravity, always: semi-implicit Euler, the same order and the same numbers as the
        // player's fall. NOT "the probe says it is supported, so stop falling" -- the probe
        // reaches 0.05 m and a prop that stopped when the floor came within reach would hover a
        // finger's width above it for ever. What ends a fall is the SWEEP below actually touching
        // the floor, which is where the vertical speed is taken away.
        prop.velocity.Y = std::max(prop.velocity.Y - kPropGravity * dt, -kPropTerminalSpeed);

        const Xna::Vector3 before = prop.position;
        Xna::Vector3 remaining(prop.velocity.X * travel, prop.velocity.Y * dt, prop.velocity.Z * travel);
        prop.velocity.X *= decay;
        prop.velocity.Z *= decay;
        Capsule body = prop.Body();

        // Three slides, the same as §49.3's step 2: a prop in a corner meets two walls.
        for (int iteration = 0; iteration < kSlideIterations; ++iteration)
        {
            if (Dot(remaining, remaining) < kMotionEpsilon * kMotionEpsilon)
            {
                remaining = Xna::Vector3();
                break;
            }
            const CellSweepHit hit = SweepCell(world, cell, broad, body, remaining);
            if (!hit.hit)
            {
                body.centre = Added(body.centre, remaining);
                remaining = Xna::Vector3();
                break;
            }
            if (hit.startedInside && !hit.touching)
            {
                report.blocked = true;
                break;
            }

            const float travelled = hit.time * kContactBackoff;
            body.centre = Added(body.centre, Scaled(remaining, travelled));
            remaining = Scaled(remaining, 1.0F - travelled);

            if (IsWalkable(hit.normal))
            {
                // A prop lands rather than bounces: §49.4 gives it damping and no restitution, and
                // a waste bin that bounced would be a different section.
                report.supported = true;
                prop.velocity.Y = std::max(prop.velocity.Y, 0.0F);
            }

            // Slide: drop the component going into the surface, keep the rest.
            const float into = Dot(remaining, hit.normal);
            remaining = Xna::Vector3(remaining.X - hit.normal.X * into,
                                     remaining.Y - hit.normal.Y * into,
                                     remaining.Z - hit.normal.Z * into);
            // ...and the same to the VELOCITY, or the prop keeps driving into the wall next step
            // and spends the rest of its life pressed against it.
            const float velocityInto = Dot(prop.velocity, hit.normal);
            if (velocityInto < 0.0F)
            {
                prop.velocity = Xna::Vector3(prop.velocity.X - hit.normal.X * velocityInto,
                                             prop.velocity.Y - hit.normal.Y * velocityInto,
                                             prop.velocity.Z - hit.normal.Z * velocityInto);
            }
            report.blocked = Dot(remaining, remaining) >= kMotionEpsilon * kMotionEpsilon;
        }

        prop.position = body.centre;

        // §49.3's step 5, for a prop: whatever it has ended up inside, it comes out of.
        const Depenetration out = Depenetrate(world, cell, broad, prop.Body());
        prop.position = Added(prop.position, out.offset);
        report.depenetrations = out.iterations;

        // The spin, damped the same exact way, and the facing it turns.
        const float spinDecay = std::exp(-kPropSpinDamping * dt);
        prop.yaw += prop.spin * (1.0F - spinDecay) / kPropSpinDamping;
        prop.spin *= spinDecay;

        const float horizontal =
            std::sqrt(prop.velocity.X * prop.velocity.X + prop.velocity.Z * prop.velocity.Z);
        if (report.supported && horizontal < kPropSleepSpeed && std::fabs(prop.spin) < kPropSleepSpin &&
            std::fabs(prop.velocity.Y) < kPropSleepSpeed)
        {
            prop.resting = true;
            prop.velocity = Xna::Vector3();
            prop.spin = 0.0F;
        }

        report.resting = prop.resting;
        report.travelled = Length(
            Xna::Vector3(prop.position.X - before.X, prop.position.Y - before.Y, prop.position.Z - before.Z));
        return report;
    }

    bool PushProp(NudgeableProp& prop, const Capsule& player, const Xna::Vector3& playerVelocity)
    {
        // The same overlap the world uses. A prop is a capsule and so is the player; the box-
        // shaped question `OverlapCapsuleObb` answers about an upright prism is the same one, with
        // the prop's own extents as the box, and it is the code the depenetration already trusts.
        CollisionObb asBox;
        asBox.centre = prop.position;
        asBox.halfExtents = Xna::Vector3(prop.radius, prop.halfHeight + prop.radius, prop.radius);
        asBox.kind = CollisionKind::Prop;
        const Overlap overlap = OverlapCapsuleObb(player, asBox);
        if (!overlap.overlapped)
        {
            return false;
        }

        // `overlap.normal` points out of the prop towards the body, so the prop goes the other
        // way. Horizontal only: the vertical part of a push is what would let a player press a
        // ball through the floor, and there is no jump in this game to launch one with (§43.1).
        const Xna::Vector3 away(-overlap.normal.X, 0.0F, -overlap.normal.Z);
        const float length = Length(away);
        if (length < kMotionEpsilon)
        {
            // Exactly above or below it -- a body standing ON a ball. There is no horizontal way
            // to push it, and shoving it down through the floor is not the answer.
            return true;
        }
        const Xna::Vector3 direction(away.X / length, 0.0F, away.Z / length);

        // How fast the body is going INTO the prop, not how fast it is going: a player walking
        // past a bin does not send it across the room.
        const float closing = std::max(0.0F, Dot(playerVelocity, direction));
        const float mass = std::max(0.05F, prop.mass);
        const float speed = std::min(kMaxPushSpeed, closing * kPushTransfer / mass);
        if (speed <= 0.0F)
        {
            return true;
        }

        prop.velocity = Xna::Vector3(direction.X * speed, prop.velocity.Y, direction.Z * speed);
        // An off-centre shove turns it. The lever is how far the contact is from the prop's middle
        // ACROSS the push, which for a capsule is the sideways part of the offset to the player.
        const Xna::Vector3 offset(player.centre.X - prop.position.X, 0.0F, player.centre.Z - prop.position.Z);
        const float lever = offset.X * direction.Z - offset.Z * direction.X;
        prop.spin += lever * speed * kPushSpin;
        prop.resting = false;
        return true;
    }

} // namespace cnahouse::physics
