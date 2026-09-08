// SPDX-License-Identifier: MIT
#include "cnahouse/physics/Sweep.hpp"

#include <algorithm>
#include <cmath>

namespace cnahouse::physics
{
    namespace
    {
        namespace Xna = Microsoft::Xna::Framework;

        /// Below this a motion is not a motion and a normal is not a direction.
        constexpr float kEpsilon = 1e-6f;

        Xna::Vector3 Clamped(const Xna::Vector3& point, const Xna::Vector3& extents)
        {
            return Xna::Vector3(std::clamp(point.X, -extents.X, extents.X),
                                std::clamp(point.Y, -extents.Y, extents.Y),
                                std::clamp(point.Z, -extents.Z, extents.Z));
        }

        float LengthSquared(const Xna::Vector3& v)
        {
            return v.X * v.X + v.Y * v.Y + v.Z * v.Z;
        }

        /// The smaller root of `a t² + b t + c = 0` in [0, limit], or -1 when there is none.
        ///
        /// Written with the discriminant guarded rather than `std::sqrt` of a negative: a ray that
        /// grazes past a rounded edge produces exactly this case, and a NaN time propagates into
        /// the caller's `min` and silently wins.
        float SmallestRoot(float a, float b, float c, float limit)
        {
            if (std::fabs(a) < kEpsilon)
            {
                return -1.0f;
            }
            const float discriminant = b * b - 4.0f * a * c;
            if (discriminant < 0.0f)
            {
                return -1.0f;
            }
            const float root = std::sqrt(discriminant);
            const float first = (-b - root) / (2.0f * a);
            const float second = (-b + root) / (2.0f * a);
            const float low = std::min(first, second);
            const float high = std::max(first, second);
            if (low >= -kEpsilon && low <= limit)
            {
                return std::max(0.0f, low);
            }
            if (high >= -kEpsilon && high <= limit)
            {
                return std::max(0.0f, high);
            }
            return -1.0f;
        }

    } // namespace

    SweepHit SweepCapsuleObb(const Capsule& capsule, const Xna::Vector3& motion, const CollisionObb& obb)
    {
        SweepHit result;

        // Into the box's own frame. Yaw is about +Y, so the inverse is a rotation by -yaw in the
        // x/z plane -- and the capsule stays upright through it, which is the whole premise.
        const float cosYaw = std::cos(-obb.yaw);
        const float sinYaw = std::sin(-obb.yaw);
        const Xna::Vector3 offset(capsule.centre.X - obb.centre.X,
                                  capsule.centre.Y - obb.centre.Y,
                                  capsule.centre.Z - obb.centre.Z);
        const Xna::Vector3 origin(
            offset.X * cosYaw + offset.Z * sinYaw, offset.Y, -offset.X * sinYaw + offset.Z * cosYaw);
        const Xna::Vector3 direction(
            motion.X * cosYaw + motion.Z * sinYaw, motion.Y, -motion.X * sinYaw + motion.Z * cosYaw);

        // The Minkowski sum: the box grown by the capsule's half-height in Y, then rounded by the
        // radius. `inner` is the box the rounding is applied to; the rounding is `radius`.
        const Xna::Vector3 inner(
            obb.halfExtents.X, obb.halfExtents.Y + capsule.halfHeight, obb.halfExtents.Z);
        const float radius = capsule.radius;

        // Already touching? The nearest point of `inner` to the origin decides it, exactly.
        const Xna::Vector3 nearest = Clamped(origin, inner);
        const Xna::Vector3 away(origin.X - nearest.X, origin.Y - nearest.Y, origin.Z - nearest.Z);
        const float awaySquared = LengthSquared(away);
        if (awaySquared <= radius * radius)
        {
            result.hit = true;
            result.time = 0.0f;
            result.startedInside = true;
            // The way OUT, which is what a caller depenetrates along. Deep inside the box every
            // direction is as good as another and the deepest axis is the shortest way out; on the
            // surface the offset itself is the normal.
            if (awaySquared > kEpsilon * kEpsilon)
            {
                const float length = std::sqrt(awaySquared);
                result.normal = Xna::Vector3(away.X / length, away.Y / length, away.Z / length);
            }
            else
            {
                const float toX = inner.X - std::fabs(origin.X);
                const float toY = inner.Y - std::fabs(origin.Y);
                const float toZ = inner.Z - std::fabs(origin.Z);
                if (toX <= toY && toX <= toZ)
                {
                    result.normal = Xna::Vector3(origin.X < 0.0f ? -1.0f : 1.0f, 0.0f, 0.0f);
                }
                else if (toY <= toZ)
                {
                    result.normal = Xna::Vector3(0.0f, origin.Y < 0.0f ? -1.0f : 1.0f, 0.0f);
                }
                else
                {
                    result.normal = Xna::Vector3(0.0f, 0.0f, origin.Z < 0.0f ? -1.0f : 1.0f);
                }
            }
        }
        else if (LengthSquared(direction) >= kEpsilon * kEpsilon)
        {
            // The slab test against the box grown by the radius on every axis. That box CONTAINS
            // the rounded one, so a miss here is a miss -- and a hit is a candidate whose feature
            // the entry point names.
            const Xna::Vector3 outer(inner.X + radius, inner.Y + radius, inner.Z + radius);
            float enter = 0.0f;
            float exit = 1.0f;
            int entryAxis = -1;
            float entrySign = 0.0f;
            bool missed = false;
            const float o[3] = {origin.X, origin.Y, origin.Z};
            const float d[3] = {direction.X, direction.Y, direction.Z};
            const float e[3] = {outer.X, outer.Y, outer.Z};
            for (int axis = 0; axis < 3 && !missed; ++axis)
            {
                if (std::fabs(d[axis]) < kEpsilon)
                {
                    // Parallel to this pair of slabs: outside them is outside for the whole sweep.
                    missed = std::fabs(o[axis]) > e[axis];
                    continue;
                }
                float t0 = (-e[axis] - o[axis]) / d[axis];
                float t1 = (e[axis] - o[axis]) / d[axis];
                float sign = -1.0f;
                if (t0 > t1)
                {
                    std::swap(t0, t1);
                    sign = 1.0f;
                }
                if (t0 > enter)
                {
                    enter = t0;
                    entryAxis = axis;
                    entrySign = sign;
                }
                exit = std::min(exit, t1);
                missed = enter > exit;
            }

            if (!missed && entryAxis >= 0)
            {
                // Which axes the entry point lies OUTSIDE the inner box on says which feature of
                // the rounded box it really met: one is a face and exact already, two an edge, and
                // three a corner.
                const Xna::Vector3 at(origin.X + direction.X * enter,
                                      origin.Y + direction.Y * enter,
                                      origin.Z + direction.Z * enter);
                const float p[3] = {at.X, at.Y, at.Z};
                const float in[3] = {inner.X, inner.Y, inner.Z};
                int outside[3] = {0, 0, 0};
                int outsideCount = 0;
                for (int axis = 0; axis < 3; ++axis)
                {
                    if (std::fabs(p[axis]) > in[axis])
                    {
                        outside[axis] = 1;
                        ++outsideCount;
                    }
                }

                float time = -1.0f;
                Xna::Vector3 normal;
                if (outsideCount <= 1)
                {
                    // A face. The slab entry IS the answer and the normal is the slab's own axis.
                    time = enter;
                    normal = Xna::Vector3(entryAxis == 0 ? entrySign : 0.0f,
                                          entryAxis == 1 ? entrySign : 0.0f,
                                          entryAxis == 2 ? entrySign : 0.0f);
                }
                else
                {
                    // An edge or a corner: solve against the rounded feature itself. The centre is
                    // the inner box's nearest point on the axes that are outside; on an edge the
                    // remaining axis is free, which makes it a cylinder, and on a corner none is,
                    // which makes it a sphere. Both are one quadratic.
                    Xna::Vector3 centre = Clamped(at, inner);
                    const float c[3] = {centre.X, centre.Y, centre.Z};
                    float a = 0.0f;
                    float b = 0.0f;
                    float k = -radius * radius;
                    for (int axis = 0; axis < 3; ++axis)
                    {
                        if (outsideCount == 3 || outside[axis] == 1)
                        {
                            const float delta = o[axis] - c[axis];
                            a += d[axis] * d[axis];
                            b += 2.0f * delta * d[axis];
                            k += delta * delta;
                        }
                    }
                    const float root = SmallestRoot(a, b, k, exit);
                    if (root >= 0.0f)
                    {
                        const Xna::Vector3 point(origin.X + direction.X * root,
                                                 origin.Y + direction.Y * root,
                                                 origin.Z + direction.Z * root);
                        // The centre is recomputed at the REAL contact, not at the slab estimate:
                        // on an edge the free axis moves between the two and using the estimate
                        // tilts the normal by a degree or two, which a slide then follows.
                        centre = Clamped(point, inner);
                        Xna::Vector3 out(point.X - centre.X, point.Y - centre.Y, point.Z - centre.Z);
                        const float length = std::sqrt(LengthSquared(out));
                        if (length > kEpsilon)
                        {
                            time = root;
                            normal = Xna::Vector3(out.X / length, out.Y / length, out.Z / length);
                        }
                    }
                }

                if (time >= 0.0f)
                {
                    result.hit = true;
                    result.time = std::clamp(time, 0.0f, 1.0f);
                    result.normal = normal;
                }
            }
        }

        if (result.hit)
        {
            // Back out of the box's frame. Only the normal needs it; the time is a fraction.
            const float cosBack = std::cos(obb.yaw);
            const float sinBack = std::sin(obb.yaw);
            result.normal = Xna::Vector3(result.normal.X * cosBack + result.normal.Z * sinBack,
                                         result.normal.Y,
                                         -result.normal.X * sinBack + result.normal.Z * cosBack);
        }
        return result;
    }

} // namespace cnahouse::physics
