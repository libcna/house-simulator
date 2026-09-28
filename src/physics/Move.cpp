// SPDX-License-Identifier: MIT
#include "cnahouse/physics/Move.hpp"

#include <cmath>

#include "cnahouse/physics/BroadPhase.hpp"
#include "cnahouse/physics/Terrain.hpp"

namespace cnahouse::physics
{
    namespace
    {
        namespace Xna = Microsoft::Xna::Framework;

        float Dot(const Xna::Vector3& a, const Xna::Vector3& b)
        {
            return a.X * b.X + a.Y * b.Y + a.Z * b.Z;
        }

        float LengthSquared(const Xna::Vector3& v)
        {
            return Dot(v, v);
        }

        Xna::Vector3 Scaled(const Xna::Vector3& v, float s)
        {
            return Xna::Vector3(v.X * s, v.Y * s, v.Z * s);
        }

        Xna::Vector3 Added(const Xna::Vector3& a, const Xna::Vector3& b)
        {
            return Xna::Vector3(a.X + b.X, a.Y + b.Y, a.Z + b.Z);
        }

        SweepHit SupportSweep(const CollisionWorld& world,
                              const CollisionCell& cell,
                              BroadPhase& broad,
                              const Capsule& body,
                              float span)
        {
            const Xna::Vector3 down(0.0F, -span, 0.0F);
            SweepHit hit = SweepCell(world, cell, broad, body, down);
            if (cell.outdoors && hit.hit && IsWalkable(hit.normal))
            {
                const auto terrain = SweepCapsuleTerrain(world.terrain, body, down);
                if (terrain.hit && IsWalkable(terrain.normal) && terrain.time < hit.time)
                {
                    hit = terrain;
                }
            }
            return hit;
        }

    } // namespace

    bool IsWalkable(const Xna::Vector3& normal)
    {
        return normal.Y >= kSlopeLimitCosine;
    }

    SlideResult CollideAndSlide(const CollisionWorld& world,
                                const CollisionCell& cell,
                                BroadPhase& broad,
                                const Capsule& capsule,
                                const Xna::Vector3& motion)
    {
        SlideResult result;
        result.position = capsule.centre;

        Capsule moving = capsule;
        Xna::Vector3 remaining = motion;
        Xna::Vector3 previousNormal;
        bool previousWalkable = false;
        bool previousContact = false;

        for (int i = 0; i < kSlideIterations; ++i)
        {
            if (LengthSquared(remaining) < kMotionEpsilon * kMotionEpsilon)
            {
                // Nothing left worth moving. Sliding along a wall you walked straight into ends
                // here on the second iteration, and it is not a blocked body.
                remaining = Xna::Vector3();
                break;
            }
            ++result.iterations;

            const CellSweepHit hit = SweepCell(world, cell, broad, moving, remaining);
            if (!hit.hit)
            {
                moving.centre = Added(moving.centre, remaining);
                remaining = Xna::Vector3();
                break;
            }

            ++result.contacts;
            result.lastNormal = hit.normal;
            result.lastWalkable = IsWalkable(hit.normal);

            // Travel to a thousandth of the step short of the contact. A body that STARTED inside
            // needs no branch of its own: `startedInside` comes with `time` 0 -- "already
            // touching" is not "hit immediately after moving" -- and 0.999 of no distance is no
            // distance, so it spends nothing here.
            //
            // A body that is merely TOUCHING is a different case and does have one, below: it has
            // an ordinary contact normal, it is going nowhere on its own, and it has to be able to
            // walk along what it is resting on.
            //
            // **It is not let through, either.** The obvious kindness -- ignore a start overlap the
            // motion is moving out of, so a body does not freeze against something it is already
            // in -- was tried and walked a body 0.97 m into the attic stair ramp: a triangle's
            // prism is 1.2 m thick for §43.1's body, "inside" it is a volume rather than a
            // surface, and the way OUT of that volume is not a surface normal to compare a motion
            // against. A body that starts inside spends this step standing still and is put back
            // by §49.3's step 5, which is the step that exists for it.
            const float travelled = hit.time * kContactBackoff;
            moving.centre = Added(moving.centre, Scaled(remaining, travelled));
            remaining = Scaled(remaining, 1.0F - travelled);

            // Slide: drop the component going INTO the surface and keep the rest. This is the
            // whole of the pseudocode's `velocity − normal·dot(velocity, normal)`; doing it to the
            // remaining DISTANCE rather than to the velocity is the same thing with the step's
            // duration already folded in, and it keeps `dt` out of a function that has no clock.
            remaining = Xna::Vector3(remaining.X - hit.normal.X * Dot(remaining, hit.normal),
                                     remaining.Y - hit.normal.Y * Dot(remaining, hit.normal),
                                     remaining.Z - hit.normal.Z * Dot(remaining, hit.normal));

            // At a ramp/rail contact, projecting onto one plane and then the other repeatedly
            // spends all three iterations without taking a step. Slide along their shared
            // crease instead. One plane must be walkable: this does not permit climbing walls.
            bool supportedCrease = false;
            const bool rampBesideWall =
                (previousWalkable && previousNormal.Y < 0.9999F && std::fabs(hit.normal.Y) < 1.0e-4F) ||
                (result.lastWalkable && hit.normal.Y < 0.9999F && std::fabs(previousNormal.Y) < 1.0e-4F);
            if (previousContact && rampBesideWall)
            {
                const Xna::Vector3 crease(previousNormal.Y * hit.normal.Z - previousNormal.Z * hit.normal.Y,
                                          previousNormal.Z * hit.normal.X - previousNormal.X * hit.normal.Z,
                                          previousNormal.X * hit.normal.Y - previousNormal.Y * hit.normal.X);
                const float squared = LengthSquared(crease);
                if (squared > 1.0e-6F)
                {
                    remaining = Scaled(crease, Dot(remaining, crease) / squared);
                    supportedCrease = true;
                }
            }
            previousNormal = hit.normal;
            previousWalkable = result.lastWalkable;
            previousContact = true;

            if (hit.startedInside && !hit.touching)
            {
                // Inside something. The way out of a VOLUME is not a surface normal, so there is
                // nothing to slide along and the projection above means nothing: an earlier
                // version that trusted it walked a body 0.97 m into the attic stair ramp, whose
                // prism is 1.2 m thick for §43.1's body. The step is spent standing still and
                // §49.3's step 5 puts the body back, which is the step that exists for it. What
                // is LEFT of the step is kept rather than zeroed, so the result still says the
                // body was blocked -- it asked to move and did not.
                break;
            }

            if (!result.lastWalkable)
            {
                ++result.steepContacts;
                if (remaining.Y > 0.0F && !supportedCrease)
                {
                    // §43.1's 46° limit. The projection above is happy to send a body up the face
                    // of a 70° bank, because a plane is a plane to it; a slope that steep is a
                    // wall, and what is left of the step belongs along its foot.
                    remaining.Y = 0.0F;
                }
            }
        }

        result.position = moving.centre;
        result.motion = remaining;
        result.blocked = LengthSquared(remaining) >= kMotionEpsilon * kMotionEpsilon;
        return result;
    }

    namespace
    {
        /// How far a move got in the x/z plane. The step assist is a question about horizontal
        /// progress only: a body that slid 0.4 m sideways along a wall got somewhere, and one that
        /// rose 0.4 m up a ramp got somewhere else, and only the first is what a step is for.
        float HorizontalProgress(const Xna::Vector3& from, const Xna::Vector3& to)
        {
            const float dx = to.X - from.X;
            const float dz = to.Z - from.Z;
            return std::sqrt(dx * dx + dz * dz);
        }

    } // namespace

    StepAssist MoveWithStepAssist(const CollisionWorld& world,
                                  const CollisionCell& cell,
                                  BroadPhase& broad,
                                  const Capsule& capsule,
                                  const Xna::Vector3& motion)
    {
        StepAssist result;
        const SlideResult plain = CollideAndSlide(world, cell, broad, capsule, motion);
        result.slide = plain;
        result.position = plain.position;

        const float asked = HorizontalProgress(Xna::Vector3(), motion);
        const float got = HorizontalProgress(capsule.centre, plain.position);

        // "Blocked horizontally" (§49.3 step 4). This is an early-out and not a rule: a step that
        // went its whole length cannot be improved on by a lift, because both attempts are capped
        // by the distance asked for. Skipping it saves two sweeps on every tick a body spends in
        // open floor, which is most of them.
        if (asked - got > kMotionEpsilon)
        {
            Capsule lifted = capsule;
            lifted.centre =
                Xna::Vector3(capsule.centre.X, capsule.centre.Y + kStepUpHeight, capsule.centre.Z);

            const SlideResult raised = CollideAndSlide(world, cell, broad, lifted, motion);

            // §49.3 says *"if ... a 0.22 m raised sweep is CLEAR"*, and it means clear: the
            // raised body must meet nothing at all over the whole step. "Got further than the
            // unraised one did" is the tempting weaker test and it accepts a body that has
            // merely nosed 0.2 m closer to a 0.24 m ledge and is now balanced against its top
            // corner, 15 mm short of standing on it. Clear also puts the threshold exactly
            // where §43.1 does: the raised feet are at `feet + 0.22`, so what they clear is a
            // step of 0.22 and not a millimetre more.
            //
            // It is also the whole of the head-room question. A body that cannot be lifted --
            // a kerb under a 1.85 m ceiling, when the body is 1.80 m tall -- is lifted INTO
            // the ceiling, and a body inside a ceiling is not clear of anything.
            if (raised.contacts == 0 &&
                HorizontalProgress(lifted.centre, raised.position) > got + kMotionEpsilon)
            {
                // Give the lift back. A body left 0.22 m up is standing on nothing, and the
                // surface it settles onto has to be one §43.1 says can be stood on.
                Capsule ahead = lifted;
                ahead.centre = raised.position;
                // Far enough to find the ground under the raised body WHEREVER it is. The lift is
                // 0.22 m, but the body it was applied to was already standing a little clear of
                // the floor, so a settle bounded by the lift alone lands a hair short and the
                // whole step is thrown away -- which left a body walking into a 0.15 m kerb pushed
                // back by the depenetration and walking at it again, six ticks a cycle, for ever
                // (`HOUSE-00555` found it).
                const float span = kStepUpHeight + kStepDownHeight;
                const CellSweepHit down =
                    SweepCell(world, cell, broad, ahead, Xna::Vector3(0.0F, -span, 0.0F));
                const float settled = raised.position.Y - span * down.time * kContactBackoff;
                // Three things, and NOT "is what it landed on walkable".
                //
                // A step UP does not end below where it started: sweeping far enough to find the
                // floor also means being able to fall off the far side of whatever was stepped
                // over, and that is a step DOWN -- a different mechanism, with its own limit, a
                // few lines below. And it does not end inside anything, which is what stops a body
                // being accepted into the ledge it was nosing at.
                //
                // Walkability is deliberately left to §49.3's step 3. A body climbing a 0.15 m
                // kerb comes down on the kerb's rounded top EDGE, whose contact normal is 58° from
                // vertical -- an artefact of the capsule's own shape, not a fact about the kerb,
                // whose top is flat. Refusing that refused every kerb in the house: the body was
                // stopped by the edge, pushed back 0.02 m by the depenetration, and walked at it
                // again, six ticks a cycle, for ever (`HOUSE-00555` found it). `GroundProbe` runs
                // next and is the thing that decides whether the body is standing.
                Capsule landed = ahead;
                landed.centre = Xna::Vector3(raised.position.X, settled, raised.position.Z);
                if (down.hit && settled >= capsule.centre.Y - kMotionEpsilon &&
                    !OverlapCell(world, cell, broad, landed).overlapped)
                {
                    result.slide = raised;
                    result.steppedUp = true;
                    result.rise = settled - capsule.centre.Y;
                    result.position = landed.centre;
                }
            }
        }

        // §43.1's step-down, from wherever the body ended up. Walking off the edge of a tread is
        // not a fall, and treating it as one makes a staircase a sequence of stumbles.
        Capsule settled = capsule;
        settled.centre = result.position;
        // Match GroundProbe/Fall: never settle through the owned terrain onto a
        // farther neighbour slab. Constructed decks exclude that terrain via the cell.
        // Only replace a walkable lower slab. Preserve rounded kerb/wall edge
        // contacts: replacing them with flat terrain undoes the edge approach.
        const SweepHit ground = SupportSweep(world, cell, broad, settled, kStepDownHeight);
        if (!ground.hit)
        {
            result.airborne = true;
            return result;
        }
        // A body already resting on it needs no case of its own: `startedInside` comes with
        // `time` 0, so the drop below is zero and nothing happens.
        if (!IsWalkable(ground.normal))
        {
            if (ground.startedInside)
            {
                // Not a fall: a body standing on a floor with its shoulder against a WALL has that
                // wall as the nearest thing a downward sweep meets, at zero distance, with a
                // horizontal normal, and it says nothing about what is underfoot. Reported as
                // airborne it started a fall on every tick spent leaning on a wall, and the
                // depenetration then pushed the body back out -- a limit cycle (`HOUSE-00555`).
                // §49.3's step 3 runs next and is the authority on standing.
                return result;
            }
            // Something is down there, but §43.1 says it is not a floor -- the side of a bank, the
            // face of a wall below an overhang. There is nothing to settle ONTO, so this is a fall
            // like any other and the body is left where the step put it.
            result.airborne = true;
            return result;
        }
        const float drop = kStepDownHeight * ground.time * kContactBackoff;
        if (drop > kMotionEpsilon)
        {
            result.steppedDown = true;
            result.drop = drop;
            result.position = Xna::Vector3(result.position.X, result.position.Y - drop, result.position.Z);
        }
        return result;
    }

} // namespace cnahouse::physics
