// SPDX-License-Identifier: MIT
#include "cnahouse/physics/Sweep.hpp"

#include "cnahouse/physics/BroadPhase.hpp"

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

        // Already touching? Asked of the same code the overlap test uses -- and answered with its
        // normal, which is already the way OUT and already back in world axes -- so a sweep and a
        // depenetration cannot disagree about whether a body is inside a wall.
        const Overlap overlap = OverlapCapsuleObb(capsule, obb);
        if (overlap.overlapped)
        {
            result.hit = true;
            result.time = 0.0f;
            result.startedInside = true;
            result.normal = overlap.normal;
            return result;
        }

        if (LengthSquared(direction) >= kEpsilon * kEpsilon)
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

    namespace
    {
        Xna::Vector3 Subtract(const Xna::Vector3& a, const Xna::Vector3& b)
        {
            return Xna::Vector3(a.X - b.X, a.Y - b.Y, a.Z - b.Z);
        }

        Xna::Vector3 Cross(const Xna::Vector3& a, const Xna::Vector3& b)
        {
            return Xna::Vector3(a.Y * b.Z - a.Z * b.Y, a.Z * b.X - a.X * b.Z, a.X * b.Y - a.Y * b.X);
        }

        float Dot(const Xna::Vector3& a, const Xna::Vector3& b)
        {
            return a.X * b.X + a.Y * b.Y + a.Z * b.Z;
        }

        /// @p v scaled to unit length, or `false` when it has none to scale.
        bool Normalise(Xna::Vector3& v)
        {
            const float length = std::sqrt(LengthSquared(v));
            if (length < kEpsilon)
            {
                return false;
            }
            v = Xna::Vector3(v.X / length, v.Y / length, v.Z / length);
            return true;
        }

        /// One candidate contact, kept only while it is the earliest.
        struct Best
        {
            float time = 2.0f;
            Xna::Vector3 normal;
            bool found = false;

            void Offer(float t, const Xna::Vector3& n)
            {
                if (t >= -kEpsilon && t <= 1.0f && t < time)
                {
                    time = std::max(0.0f, t);
                    normal = n;
                    found = true;
                }
            }
        };

        /// Ray against the sphere of @p radius at @p centre.
        void SphereFeature(const Xna::Vector3& origin,
                           const Xna::Vector3& direction,
                           const Xna::Vector3& centre,
                           float radius,
                           Best& best)
        {
            const Xna::Vector3 delta = Subtract(origin, centre);
            const float t = SmallestRoot(LengthSquared(direction),
                                         2.0f * Dot(delta, direction),
                                         LengthSquared(delta) - radius * radius,
                                         1.0f);
            if (t < 0.0f)
            {
                return;
            }
            Xna::Vector3 normal = Subtract(Xna::Vector3(origin.X + direction.X * t,
                                                        origin.Y + direction.Y * t,
                                                        origin.Z + direction.Z * t),
                                           centre);
            if (Normalise(normal))
            {
                best.Offer(t, normal);
            }
        }

        /// Ray against the cylinder of @p radius about the segment @p p0 - @p p1, ends open: the
        /// spheres at the ends are the vertices' business, and testing them twice is only waste.
        void EdgeFeature(const Xna::Vector3& origin,
                         const Xna::Vector3& direction,
                         const Xna::Vector3& p0,
                         const Xna::Vector3& p1,
                         float radius,
                         Best& best)
        {
            const Xna::Vector3 axis = Subtract(p1, p0);
            const float axisLengthSquared = LengthSquared(axis);
            if (axisLengthSquared < kEpsilon * kEpsilon)
            {
                return;
            }
            // The ray and the axis, each with the component along the axis removed: what is left
            // is a 2-D circle problem in the plane perpendicular to the edge.
            const Xna::Vector3 delta = Subtract(origin, p0);
            const float dirAlong = Dot(direction, axis) / axisLengthSquared;
            const float deltaAlong = Dot(delta, axis) / axisLengthSquared;
            const Xna::Vector3 dirPerp(direction.X - axis.X * dirAlong,
                                       direction.Y - axis.Y * dirAlong,
                                       direction.Z - axis.Z * dirAlong);
            const Xna::Vector3 deltaPerp(
                delta.X - axis.X * deltaAlong, delta.Y - axis.Y * deltaAlong, delta.Z - axis.Z * deltaAlong);
            const float t = SmallestRoot(LengthSquared(dirPerp),
                                         2.0f * Dot(deltaPerp, dirPerp),
                                         LengthSquared(deltaPerp) - radius * radius,
                                         1.0f);
            if (t < 0.0f)
            {
                return;
            }
            const Xna::Vector3 point(
                origin.X + direction.X * t, origin.Y + direction.Y * t, origin.Z + direction.Z * t);
            const float along = Dot(Subtract(point, p0), axis) / axisLengthSquared;
            if (along < 0.0f || along > 1.0f)
            {
                return;
            }
            const Xna::Vector3 onAxis(p0.X + axis.X * along, p0.Y + axis.Y * along, p0.Z + axis.Z * along);
            Xna::Vector3 normal = Subtract(point, onAxis);
            if (Normalise(normal))
            {
                best.Offer(t, normal);
            }
        }

        /// Is @p point, which lies in the face's plane, inside the convex polygon?
        ///
        /// By the SIGN AGREEING across every edge rather than by a fixed direction: the corners of
        /// these faces come from an index table and their winding relative to the outward normal
        /// is whatever the table happens to give. A test that assumed one of the two orders
        /// rejected every contact on half the faces -- which reads as a body falling through a
        /// floor from above and standing on it from below.
        bool InsideFace(const Xna::Vector3& point,
                        const Xna::Vector3* points,
                        int count,
                        const Xna::Vector3& normal)
        {
            bool positive = false;
            bool negative = false;
            for (int i = 0; i < count; ++i)
            {
                const Xna::Vector3& from = points[i];
                const Xna::Vector3& to = points[(i + 1) % count];
                const float side = Dot(Subtract(point, from), Cross(normal, Subtract(to, from)));
                positive = positive || side > kEpsilon;
                negative = negative || side < -kEpsilon;
            }
            return !(positive && negative);
        }

        /// The closest point to @p point on the convex polygon, whether or not it projects inside.
        Xna::Vector3 ClosestOnFace(const Xna::Vector3& point,
                                   const Xna::Vector3* points,
                                   int count,
                                   const Xna::Vector3& normal)
        {
            const float above = Dot(Subtract(point, points[0]), normal);
            const Xna::Vector3 projected(
                point.X - normal.X * above, point.Y - normal.Y * above, point.Z - normal.Z * above);
            if (InsideFace(projected, points, count, normal))
            {
                return projected;
            }
            Xna::Vector3 best = points[0];
            float bestDistance = LengthSquared(Subtract(point, best));
            for (int i = 0; i < count; ++i)
            {
                const Xna::Vector3& from = points[i];
                const Xna::Vector3 edge = Subtract(points[(i + 1) % count], from);
                const float lengthSquared = LengthSquared(edge);
                float along = 0.0f;
                if (lengthSquared > kEpsilon * kEpsilon)
                {
                    along = std::clamp(Dot(Subtract(point, from), edge) / lengthSquared, 0.0f, 1.0f);
                }
                const Xna::Vector3 candidate(
                    from.X + edge.X * along, from.Y + edge.Y * along, from.Z + edge.Z * along);
                const float distance = LengthSquared(Subtract(point, candidate));
                if (distance < bestDistance)
                {
                    bestDistance = distance;
                    best = candidate;
                }
            }
            return best;
        }

        /// Ray against @p normal's plane through @p points[0], offset outward by @p radius, kept
        /// only where the contact projects inside the convex face.
        void FaceFeature(const Xna::Vector3& origin,
                         const Xna::Vector3& direction,
                         const Xna::Vector3* points,
                         int count,
                         const Xna::Vector3& normal,
                         float radius,
                         Best& best)
        {
            const float speed = Dot(direction, normal);
            if (speed >= -kEpsilon)
            {
                // Moving along the face or away from it: whatever it meets, it is not this side.
                return;
            }
            const float distance = Dot(Subtract(origin, points[0]), normal) - radius;
            const float t = distance / -speed;
            if (t < -kEpsilon || t > 1.0f)
            {
                return;
            }
            const Xna::Vector3 point(origin.X + direction.X * t - normal.X * radius,
                                     origin.Y + direction.Y * t - normal.Y * radius,
                                     origin.Z + direction.Z * t - normal.Z * radius);
            if (!InsideFace(point, points, count, normal))
            {
                // Outside this face: the contact is an edge's or a vertex's, and both are tested
                // separately.
                return;
            }
            best.Offer(std::max(0.0f, t), normal);
        }

        /// The triangle extruded along Y by the capsule's half-height: six vertices, nine edges and
        /// five faces. Rounding it by the radius is the Minkowski sum of the triangle and the
        /// capsule -- which is what the sweep and the overlap are BOTH asking about, so it is built
        /// once, here, rather than twice with a chance of the two drifting apart.
        struct Prism
        {
            Xna::Vector3 points[6];
            Xna::Vector3 corners[5][4];
            Xna::Vector3 normals[5];
            int counts[5] = {0, 0, 0, 0, 0};
            Xna::Vector3 centroid;
        };

        /// False when the triangle is degenerate -- two vertices in the same place, or three in a
        /// line. There is no surface there and no normal that is not noise.
        bool BuildPrism(float halfHeight,
                        const Xna::Vector3& a,
                        const Xna::Vector3& b,
                        const Xna::Vector3& c,
                        Prism& prism)
        {
            Xna::Vector3 face = Cross(Subtract(b, a), Subtract(c, a));
            if (!Normalise(face))
            {
                return false;
            }

            const float h = halfHeight;
            const Xna::Vector3 points[6] = {
                Xna::Vector3(a.X, a.Y + h, a.Z),
                Xna::Vector3(b.X, b.Y + h, b.Z),
                Xna::Vector3(c.X, c.Y + h, c.Z),
                Xna::Vector3(a.X, a.Y - h, a.Z),
                Xna::Vector3(b.X, b.Y - h, b.Z),
                Xna::Vector3(c.X, c.Y - h, c.Z),
            };
            for (int i = 0; i < 6; ++i)
            {
                prism.points[i] = points[i];
                prism.centroid = Xna::Vector3(prism.centroid.X + points[i].X / 6.0f,
                                              prism.centroid.Y + points[i].Y / 6.0f,
                                              prism.centroid.Z + points[i].Z / 6.0f);
            }

            // The five faces: the two triangle copies and the three quads the edges sweep, with
            // their OUTWARD normals. Outward is decided by the prism itself rather than by a
            // winding this function was not given -- a triangle mesh's winding is for drawing
            // (§14) and a body is stopped from either side.
            const int faceIndices[5][4] = {
                {0, 1, 2, -1},
                {3, 5, 4, -1},
                {0, 3, 4, 1},
                {1, 4, 5, 2},
                {2, 5, 3, 0},
            };
            for (int f = 0; f < 5; ++f)
            {
                int count = 0;
                for (const int index : faceIndices[f])
                {
                    if (index >= 0)
                    {
                        prism.corners[f][count++] = points[index];
                    }
                }
                Xna::Vector3 normal = Cross(Subtract(prism.corners[f][1], prism.corners[f][0]),
                                            Subtract(prism.corners[f][2], prism.corners[f][0]));
                if (!Normalise(normal))
                {
                    // A quad of zero area: the triangle's edge is parallel to Y, so the face is a
                    // line. The edge tests cover it.
                    continue;
                }
                if (Dot(Subtract(prism.centroid, prism.corners[f][0]), normal) > 0.0f)
                {
                    normal = Xna::Vector3(-normal.X, -normal.Y, -normal.Z);
                }
                prism.normals[f] = normal;
                prism.counts[f] = count;
            }
            return true;
        }

        /// The SQUARED distance from @p point to the prism's surface; @p nearest is the point on
        /// that surface and @p insideSolid says which side of it @p point is on. The prism is
        /// convex, so its five faces are the whole of its surface and the nearest of them is the
        /// nearest point.
        float ClosestOnPrism(const Prism& prism,
                             const Xna::Vector3& point,
                             Xna::Vector3& nearest,
                             bool& insideSolid)
        {
            nearest = prism.centroid;
            float best = -1.0f;
            insideSolid = true;
            for (int f = 0; f < 5; ++f)
            {
                if (prism.counts[f] == 0)
                {
                    continue;
                }
                if (Dot(Subtract(point, prism.corners[f][0]), prism.normals[f]) > 0.0f)
                {
                    insideSolid = false;
                }
                const Xna::Vector3 candidate =
                    ClosestOnFace(point, prism.corners[f], prism.counts[f], prism.normals[f]);
                const float distance = LengthSquared(Subtract(point, candidate));
                if (best < 0.0f || distance < best)
                {
                    best = distance;
                    nearest = candidate;
                }
            }
            if (best < 0.0f)
            {
                // Every face was degenerate, which `BuildPrism` has already refused to produce.
                insideSolid = false;
                return 0.0f;
            }
            return best;
        }

    } // namespace

    SweepHit SweepCapsuleTriangle(const Capsule& capsule,
                                  const Xna::Vector3& motion,
                                  const Xna::Vector3& a,
                                  const Xna::Vector3& b,
                                  const Xna::Vector3& c)
    {
        SweepHit result;

        Prism prism;
        if (!BuildPrism(capsule.halfHeight, a, b, c, prism))
        {
            // Two vertices in the same place, or three in a line. There is no surface to hit, and
            // a normal made of noise is worse than a miss: a body would be pushed in a direction
            // nothing chose.
            return result;
        }

        const Xna::Vector3 origin = capsule.centre;
        const float radius = capsule.radius;

        // Already touching? Asked of the same nearest-point code the overlap test uses, so a sweep
        // and a depenetration cannot disagree about whether a body is inside a wall.
        Xna::Vector3 nearest;
        bool insideSolid = false;
        const float nearestDistance = ClosestOnPrism(prism, origin, nearest, insideSolid);
        if (insideSolid || nearestDistance <= radius * radius)
        {
            result.hit = true;
            result.time = 0.0f;
            result.startedInside = true;
            Xna::Vector3 out = Subtract(origin, nearest);
            if (insideSolid)
            {
                out = Xna::Vector3(-out.X, -out.Y, -out.Z);
            }
            if (!Normalise(out))
            {
                out = prism.normals[0];
            }
            result.normal = out;
            return result;
        }

        Best best;
        for (int f = 0; f < 5; ++f)
        {
            if (prism.counts[f] == 0)
            {
                continue;
            }
            FaceFeature(origin, motion, prism.corners[f], prism.counts[f], prism.normals[f], radius, best);
        }
        const int edges[9][2] = {
            {0, 1},
            {1, 2},
            {2, 0},
            {3, 4},
            {4, 5},
            {5, 3},
            {0, 3},
            {1, 4},
            {2, 5},
        };
        for (const auto& edge : edges)
        {
            EdgeFeature(origin, motion, prism.points[edge[0]], prism.points[edge[1]], radius, best);
        }
        for (const Xna::Vector3& vertex : prism.points)
        {
            SphereFeature(origin, motion, vertex, radius, best);
        }

        if (!best.found)
        {
            return result;
        }
        result.hit = true;
        result.time = best.time;
        result.normal = best.normal;
        return result;
    }

    Overlap OverlapCapsuleObb(const Capsule& capsule, const CollisionObb& obb)
    {
        Overlap result;
        // The same frame and the same rounded box the sweep uses. Written once here and called by
        // `SweepCapsuleObb` for its own already-touching branch, so the two cannot disagree about
        // whether a body is inside a wall.
        const float cosYaw = std::cos(-obb.yaw);
        const float sinYaw = std::sin(-obb.yaw);
        const Xna::Vector3 offset(capsule.centre.X - obb.centre.X,
                                  capsule.centre.Y - obb.centre.Y,
                                  capsule.centre.Z - obb.centre.Z);
        const Xna::Vector3 origin(
            offset.X * cosYaw + offset.Z * sinYaw, offset.Y, -offset.X * sinYaw + offset.Z * cosYaw);
        const Xna::Vector3 inner(
            obb.halfExtents.X, obb.halfExtents.Y + capsule.halfHeight, obb.halfExtents.Z);
        const Xna::Vector3 nearest = Clamped(origin, inner);
        const Xna::Vector3 away(origin.X - nearest.X, origin.Y - nearest.Y, origin.Z - nearest.Z);
        const float awaySquared = LengthSquared(away);
        if (awaySquared > capsule.radius * capsule.radius)
        {
            return result;
        }
        result.overlapped = true;
        Xna::Vector3 normal;
        if (awaySquared > kEpsilon * kEpsilon)
        {
            const float length = std::sqrt(awaySquared);
            result.depth = capsule.radius - length;
            normal = Xna::Vector3(away.X / length, away.Y / length, away.Z / length);
        }
        else
        {
            // Inside the box itself: the way out is the nearest face, and the depth is how far
            // that face is plus the radius the surface stands off by.
            const float toX = inner.X - std::fabs(origin.X);
            const float toY = inner.Y - std::fabs(origin.Y);
            const float toZ = inner.Z - std::fabs(origin.Z);
            if (toX <= toY && toX <= toZ)
            {
                result.depth = toX + capsule.radius;
                normal = Xna::Vector3(origin.X < 0.0f ? -1.0f : 1.0f, 0.0f, 0.0f);
            }
            else if (toY <= toZ)
            {
                result.depth = toY + capsule.radius;
                normal = Xna::Vector3(0.0f, origin.Y < 0.0f ? -1.0f : 1.0f, 0.0f);
            }
            else
            {
                result.depth = toZ + capsule.radius;
                normal = Xna::Vector3(0.0f, 0.0f, origin.Z < 0.0f ? -1.0f : 1.0f);
            }
        }
        const float cosBack = std::cos(obb.yaw);
        const float sinBack = std::sin(obb.yaw);
        result.normal = Xna::Vector3(
            normal.X * cosBack + normal.Z * sinBack, normal.Y, -normal.X * sinBack + normal.Z * cosBack);
        return result;
    }

    Overlap OverlapCapsuleTriangle(const Capsule& capsule,
                                   const Xna::Vector3& a,
                                   const Xna::Vector3& b,
                                   const Xna::Vector3& c)
    {
        Overlap result;
        Prism prism;
        if (!BuildPrism(capsule.halfHeight, a, b, c, prism))
        {
            return result;
        }
        Xna::Vector3 nearest;
        bool insideSolid = false;
        const float distanceSquared = ClosestOnPrism(prism, capsule.centre, nearest, insideSolid);
        if (!insideSolid && distanceSquared > capsule.radius * capsule.radius)
        {
            return result;
        }
        result.overlapped = true;
        const float distance = std::sqrt(distanceSquared);
        result.depth = insideSolid ? capsule.radius + distance : capsule.radius - distance;
        Xna::Vector3 out = Subtract(capsule.centre, nearest);
        if (insideSolid)
        {
            out = Xna::Vector3(-out.X, -out.Y, -out.Z);
        }
        if (!Normalise(out))
        {
            out = prism.normals[0];
        }
        result.normal = out;
        return result;
    }

    CellOverlap OverlapCell(const CollisionWorld& world,
                            const CollisionCell& cell,
                            BroadPhase& broad,
                            const Capsule& capsule)
    {
        CellOverlap result;
        const float half = capsule.halfHeight + capsule.radius;
        const Xna::BoundingBox box(Xna::Vector3(capsule.centre.X - capsule.radius,
                                                capsule.centre.Y - half,
                                                capsule.centre.Z - capsule.radius),
                                   Xna::Vector3(capsule.centre.X + capsule.radius,
                                                capsule.centre.Y + half,
                                                capsule.centre.Z + capsule.radius));
        const std::size_t obbCount = world.obbs.size();
        for (const std::uint32_t index : broad.Query(cell, box))
        {
            ++result.tested;
            Overlap one;
            if (index < obbCount)
            {
                one = OverlapCapsuleObb(capsule, world.obbs[index]);
            }
            else
            {
                const CollisionMesh& mesh = world.meshes[index - obbCount];
                for (std::size_t t = 0; t + 2 < mesh.indices.size(); t += 3)
                {
                    const Overlap each = OverlapCapsuleTriangle(capsule,
                                                                mesh.vertices[mesh.indices[t]],
                                                                mesh.vertices[mesh.indices[t + 1]],
                                                                mesh.vertices[mesh.indices[t + 2]]);
                    if (each.overlapped && (!one.overlapped || each.depth > one.depth))
                    {
                        one = each;
                    }
                }
            }
            // The DEEPEST, which is what §49.3 says to push along. Pushing out of the shallowest
            // first would leave the body inside the other and spend an iteration doing it.
            if (one.overlapped && (!result.overlapped || one.depth > result.depth))
            {
                const std::uint32_t tested = result.tested;
                static_cast<Overlap&>(result) = one;
                result.shape = index;
                result.tested = tested;
            }
        }
        return result;
    }

    Depenetration Depenetrate(const CollisionWorld& world,
                              const CollisionCell& cell,
                              BroadPhase& broad,
                              const Capsule& capsule)
    {
        Depenetration result;
        Capsule moving = capsule;
        for (int i = 0; i < kDepenetrationIterations; ++i)
        {
            const CellOverlap overlap = OverlapCell(world, cell, broad, moving);
            if (i == 0)
            {
                result.deepest = overlap.overlapped ? overlap.depth : 0.0f;
            }
            if (!overlap.overlapped)
            {
                result.resolved = true;
                return result;
            }
            result.resolved = false;
            ++result.iterations;
            // A fixed step, not the measured depth: pushing out by the depth resolves in one go
            // and teleports a body that has ended up deeply buried into whatever is beyond.
            result.offset = Xna::Vector3(result.offset.X + overlap.normal.X * kDepenetrationStep,
                                         result.offset.Y + overlap.normal.Y * kDepenetrationStep,
                                         result.offset.Z + overlap.normal.Z * kDepenetrationStep);
            moving.centre = Xna::Vector3(moving.centre.X + overlap.normal.X * kDepenetrationStep,
                                         moving.centre.Y + overlap.normal.Y * kDepenetrationStep,
                                         moving.centre.Z + overlap.normal.Z * kDepenetrationStep);
        }
        // Four pushes used and still inside: `resolved` stays false and the caller decides. §49.5's
        // guarantee suite is what notices a body that gets here regularly.
        result.resolved = !OverlapCell(world, cell, broad, moving).overlapped;
        return result;
    }

    CellSweepHit SweepCell(const CollisionWorld& world,
                           const CollisionCell& cell,
                           BroadPhase& broad,
                           const Capsule& capsule,
                           const Xna::Vector3& motion)
    {
        CellSweepHit result;

        // The box the capsule occupies over the WHOLE motion: its shape at the start united with
        // its shape at the end. Narrowing to the start alone is how a fast body tunnels.
        const float r = capsule.radius;
        const float half = capsule.halfHeight + r;
        const Xna::Vector3 from = capsule.centre;
        const Xna::Vector3 to(from.X + motion.X, from.Y + motion.Y, from.Z + motion.Z);
        const Xna::BoundingBox swept(
            Xna::Vector3(
                std::min(from.X, to.X) - r, std::min(from.Y, to.Y) - half, std::min(from.Z, to.Z) - r),
            Xna::Vector3(
                std::max(from.X, to.X) + r, std::max(from.Y, to.Y) + half, std::max(from.Z, to.Z) + r));

        const std::size_t obbCount = world.obbs.size();
        for (const std::uint32_t index : broad.Query(cell, swept))
        {
            ++result.tested;
            SweepHit hit;
            if (index < obbCount)
            {
                hit = SweepCapsuleObb(capsule, motion, world.obbs[index]);
            }
            else
            {
                const CollisionMesh& mesh = world.meshes[index - obbCount];
                for (std::size_t t = 0; t + 2 < mesh.indices.size(); t += 3)
                {
                    const SweepHit one = SweepCapsuleTriangle(capsule,
                                                              motion,
                                                              mesh.vertices[mesh.indices[t]],
                                                              mesh.vertices[mesh.indices[t + 1]],
                                                              mesh.vertices[mesh.indices[t + 2]]);
                    if (one.hit && (!hit.hit || one.time < hit.time))
                    {
                        hit = one;
                    }
                }
            }
            // Earliest, not first found: a body walking into a corner meets two walls, and
            // stopping at whichever the shape list happened to hold first would let it through
            // the other.
            if (hit.hit && (!result.hit || hit.time < result.time))
            {
                const std::uint32_t tested = result.tested;
                static_cast<SweepHit&>(result) = hit;
                result.shape = index;
                result.tested = tested;
            }
        }
        return result;
    }

} // namespace cnahouse::physics
