// SPDX-License-Identifier: MIT
#include "cnahouse/physics/Move.hpp"

#include <cmath>

#include "cnahouse/physics/BroadPhase.hpp"

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

            if (!result.lastWalkable)
            {
                ++result.steepContacts;
                if (remaining.Y > 0.0F)
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

} // namespace cnahouse::physics
