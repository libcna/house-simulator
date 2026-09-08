// SPDX-License-Identifier: MIT
#include "cnahouse/physics/RayCast.hpp"

#include <algorithm>
#include <cmath>
#include <limits>

#include "cnahouse/physics/BroadPhase.hpp"

namespace cnahouse::physics
{
    namespace
    {
        namespace Xna = Microsoft::Xna::Framework;

        constexpr float kEpsilon = 1.0e-6f;

        float Dot(const Xna::Vector3& a, const Xna::Vector3& b)
        {
            return a.X * b.X + a.Y * b.Y + a.Z * b.Z;
        }

        Xna::Vector3 At(const Xna::Vector3& origin, const Xna::Vector3& direction, float t)
        {
            return Xna::Vector3(
                origin.X + direction.X * t, origin.Y + direction.Y * t, origin.Z + direction.Z * t);
        }

        /// Flips @p normal to face back along the ray. A wall is opaque from both sides and a
        /// triangle's winding is for drawing (§14), so which way a surface "faces" is the ray's
        /// question to answer, not the geometry's.
        Xna::Vector3 Facing(const Xna::Vector3& normal, const Xna::Vector3& direction)
        {
            if (Dot(normal, direction) > 0.0f)
            {
                return Xna::Vector3(-normal.X, -normal.Y, -normal.Z);
            }
            return normal;
        }

    } // namespace

    RayHit RayCastObb(const Xna::Vector3& origin,
                      const Xna::Vector3& direction,
                      float maxDistance,
                      const CollisionObb& obb)
    {
        RayHit result;
        if (maxDistance <= 0.0f)
        {
            return result;
        }

        // Into the box's own frame, where it is axis-aligned. Yaw is about +Y, so the inverse is a
        // rotation by -yaw in x/z, and lengths are unchanged -- which is what lets `enter` come
        // straight back out as a distance in metres.
        const float cosYaw = std::cos(-obb.yaw);
        const float sinYaw = std::sin(-obb.yaw);
        const Xna::Vector3 offset(origin.X - obb.centre.X, origin.Y - obb.centre.Y, origin.Z - obb.centre.Z);
        const float o[3] = {
            offset.X * cosYaw + offset.Z * sinYaw, offset.Y, -offset.X * sinYaw + offset.Z * cosYaw};
        const float d[3] = {direction.X * cosYaw + direction.Z * sinYaw,
                            direction.Y,
                            -direction.X * sinYaw + direction.Z * cosYaw};
        const float e[3] = {obb.halfExtents.X, obb.halfExtents.Y, obb.halfExtents.Z};

        float enter = 0.0f;
        float exit = maxDistance;
        int axis = -1;
        float sign = 0.0f;
        for (int i = 0; i < 3; ++i)
        {
            if (std::fabs(d[i]) < kEpsilon)
            {
                if (std::fabs(o[i]) > e[i])
                {
                    return result; // parallel to this pair of slabs and outside them
                }
                continue;
            }
            float t0 = (-e[i] - o[i]) / d[i];
            float t1 = (e[i] - o[i]) / d[i];
            float face = -1.0f;
            if (t0 > t1)
            {
                std::swap(t0, t1);
                face = 1.0f;
            }
            if (t0 > enter)
            {
                enter = t0;
                axis = i;
                sign = face;
            }
            exit = std::min(exit, t1);
            if (enter > exit)
            {
                return result;
            }
        }

        result.hit = true;
        result.distance = enter;
        result.point = At(origin, direction, enter);
        result.surface = obb.surface;
        if (axis < 0)
        {
            // No slab was crossed on the way in: the origin is already inside the box. §50.1's
            // occlusion test wants that to count -- an eye inside a wall is occluded by it -- and
            // the useful normal is the nearest face's, turned to face the ray.
            const float to[3] = {e[0] - std::fabs(o[0]), e[1] - std::fabs(o[1]), e[2] - std::fabs(o[2])};
            const int nearest = (to[0] <= to[1] && to[0] <= to[2]) ? 0 : (to[1] <= to[2] ? 1 : 2);
            float local[3] = {0.0f, 0.0f, 0.0f};
            local[nearest] = o[nearest] < 0.0f ? -1.0f : 1.0f;
            const float cosBack = std::cos(obb.yaw);
            const float sinBack = std::sin(obb.yaw);
            result.normal = Facing(Xna::Vector3(local[0] * cosBack + local[2] * sinBack,
                                                local[1],
                                                -local[0] * sinBack + local[2] * cosBack),
                                   direction);
            return result;
        }

        float local[3] = {0.0f, 0.0f, 0.0f};
        local[axis] = sign;
        const float cosBack = std::cos(obb.yaw);
        const float sinBack = std::sin(obb.yaw);
        result.normal = Facing(Xna::Vector3(local[0] * cosBack + local[2] * sinBack,
                                            local[1],
                                            -local[0] * sinBack + local[2] * cosBack),
                               direction);
        return result;
    }

    RayHit RayCastTriangle(const Xna::Vector3& origin,
                           const Xna::Vector3& direction,
                           float maxDistance,
                           const Xna::Vector3& a,
                           const Xna::Vector3& b,
                           const Xna::Vector3& c)
    {
        // Möller-Trumbore, two-sided. The determinant's SIGN says which face the ray came at, and
        // nothing here cares: only its magnitude is tested, so a triangle wound away from the ray
        // stops it just as one wound towards it does.
        RayHit result;
        const Xna::Vector3 ab(b.X - a.X, b.Y - a.Y, b.Z - a.Z);
        const Xna::Vector3 ac(c.X - a.X, c.Y - a.Y, c.Z - a.Z);
        const Xna::Vector3 pv(direction.Y * ac.Z - direction.Z * ac.Y,
                              direction.Z * ac.X - direction.X * ac.Z,
                              direction.X * ac.Y - direction.Y * ac.X);
        const float det = Dot(ab, pv);
        if (std::fabs(det) < kEpsilon)
        {
            // Parallel to the triangle's plane, or the triangle has no area. A ray that grazes a
            // wall edge-on is not stopped by it, which is the answer §50.1 wants.
            return result;
        }
        const float inverse = 1.0f / det;
        const Xna::Vector3 tv(origin.X - a.X, origin.Y - a.Y, origin.Z - a.Z);
        const float u = Dot(tv, pv) * inverse;
        if (u < 0.0f || u > 1.0f)
        {
            return result;
        }
        const Xna::Vector3 qv(
            tv.Y * ab.Z - tv.Z * ab.Y, tv.Z * ab.X - tv.X * ab.Z, tv.X * ab.Y - tv.Y * ab.X);
        const float v = Dot(direction, qv) * inverse;
        if (v < 0.0f || u + v > 1.0f)
        {
            return result;
        }
        const float t = Dot(ac, qv) * inverse;
        if (t < 0.0f || t > maxDistance)
        {
            return result;
        }

        result.hit = true;
        result.distance = t;
        result.point = At(origin, direction, t);
        Xna::Vector3 normal(ab.Y * ac.Z - ab.Z * ac.Y, ab.Z * ac.X - ab.X * ac.Z, ab.X * ac.Y - ab.Y * ac.X);
        const float length = std::sqrt(Dot(normal, normal));
        if (length > kEpsilon)
        {
            normal = Xna::Vector3(normal.X / length, normal.Y / length, normal.Z / length);
        }
        result.normal = Facing(normal, direction);
        return result;
    }

    RayHit RayCastTerrain(const CollisionTerrain& terrain,
                          const Xna::Vector3& origin,
                          const Xna::Vector3& direction,
                          float maxDistance)
    {
        RayHit result;
        if (!terrain.present || terrain.samplesX < 2u || terrain.samplesZ < 2u || maxDistance <= 0.0f)
        {
            return result;
        }

        const auto squaresX = static_cast<int>(terrain.samplesX) - 1;
        const auto squaresZ = static_cast<int>(terrain.samplesZ) - 1;

        // Clip the ray to the field's x/z footprint first, so a ray that starts off the lot walks
        // no squares before it reaches one.
        //
        // The `from > to` exit below is an EARLY-OUT and not a rule: every hit reported comes from
        // an exact triangle test, so a walk that should never have started can only waste time,
        // never invent a hit. It is here because §46's sun-visibility ray is 60 m long and most of
        // the rays cast in this house never touch the ground at all.
        float from = 0.0f;
        float to = maxDistance;
        const float lowEdge[2] = {terrain.originX, terrain.originZ};
        const float highEdge[2] = {terrain.MaxX(), terrain.MaxZ()};
        const float o[2] = {origin.X, origin.Z};
        const float d[2] = {direction.X, direction.Z};
        for (int i = 0; i < 2; ++i)
        {
            if (std::fabs(d[i]) < kEpsilon)
            {
                if (o[i] < lowEdge[i] || o[i] > highEdge[i])
                {
                    return result;
                }
                continue;
            }
            float t0 = (lowEdge[i] - o[i]) / d[i];
            float t1 = (highEdge[i] - o[i]) / d[i];
            if (t0 > t1)
            {
                std::swap(t0, t1);
            }
            from = std::max(from, t0);
            to = std::min(to, t1);
        }
        if (from > to)
        {
            return result;
        }

        // Amanatides & Woo over the square grid: which square the ray is in, which way it steps,
        // and how far along the ray the next boundary of each axis is.
        const float startX = (origin.X + direction.X * from - terrain.originX) / terrain.step;
        const float startZ = (origin.Z + direction.Z * from - terrain.originZ) / terrain.step;
        int ix = std::clamp(static_cast<int>(std::floor(startX)), 0, squaresX - 1);
        int iz = std::clamp(static_cast<int>(std::floor(startZ)), 0, squaresZ - 1);

        const int stepX = direction.X > 0.0f ? 1 : (direction.X < 0.0f ? -1 : 0);
        const int stepZ = direction.Z > 0.0f ? 1 : (direction.Z < 0.0f ? -1 : 0);
        const float hugeT = std::numeric_limits<float>::max();
        float nextX = hugeT;
        float nextZ = hugeT;
        float deltaX = hugeT;
        float deltaZ = hugeT;
        if (stepX != 0)
        {
            const float boundary =
                terrain.originX + static_cast<float>(ix + (stepX > 0 ? 1 : 0)) * terrain.step;
            nextX = (boundary - origin.X) / direction.X;
            deltaX = terrain.step / std::fabs(direction.X);
        }
        if (stepZ != 0)
        {
            const float boundary =
                terrain.originZ + static_cast<float>(iz + (stepZ > 0 ? 1 : 0)) * terrain.step;
            nextZ = (boundary - origin.Z) / direction.Z;
            deltaZ = terrain.step / std::fabs(direction.Z);
        }

        // A bound on the walk that cannot be wrong: a ray crosses at most one boundary per axis
        // per square, so it visits no more squares than the grid has on both axes together.
        const int limit = squaresX + squaresZ + 2;
        for (int visited = 0; visited < limit; ++visited)
        {
            const Xna::Vector3 c00(
                terrain.originX + static_cast<float>(ix) * terrain.step,
                terrain.Height(static_cast<std::uint32_t>(ix), static_cast<std::uint32_t>(iz)),
                terrain.originZ + static_cast<float>(iz) * terrain.step);
            const Xna::Vector3 c10(
                c00.X + terrain.step,
                terrain.Height(static_cast<std::uint32_t>(ix) + 1u, static_cast<std::uint32_t>(iz)),
                c00.Z);
            const Xna::Vector3 c01(
                c00.X,
                terrain.Height(static_cast<std::uint32_t>(ix), static_cast<std::uint32_t>(iz) + 1u),
                c00.Z + terrain.step);
            const Xna::Vector3 c11(
                c00.X + terrain.step,
                terrain.Height(static_cast<std::uint32_t>(ix) + 1u, static_cast<std::uint32_t>(iz) + 1u),
                c00.Z + terrain.step);
            for (const RayHit& one : {RayCastTriangle(origin, direction, maxDistance, c00, c10, c11),
                                      RayCastTriangle(origin, direction, maxDistance, c00, c11, c01)})
            {
                if (one.hit && (!result.hit || one.distance < result.distance))
                {
                    result = one;
                    result.shape = RayHit::kTerrain;
                    result.surface =
                        terrain.Material(static_cast<std::uint32_t>(ix), static_cast<std::uint32_t>(iz));
                }
            }
            if (result.hit)
            {
                // The first square that answers gives the NEAREST answer: its triangles lie inside
                // its own footprint, so a hit on them is at a distance the ray spends in it.
                return result;
            }

            if (nextX <= nextZ)
            {
                if (nextX > to)
                {
                    return result;
                }
                ix += stepX;
                nextX += deltaX;
                if (ix < 0 || ix >= squaresX)
                {
                    return result;
                }
            }
            else
            {
                if (nextZ > to)
                {
                    return result;
                }
                iz += stepZ;
                nextZ += deltaZ;
                if (iz < 0 || iz >= squaresZ)
                {
                    return result;
                }
            }
        }
        return result;
    }

    RayHit RayCastCell(const CollisionWorld& world,
                       const CollisionCell& cell,
                       BroadPhase& broad,
                       const Xna::Vector3& origin,
                       const Xna::Vector3& direction,
                       float maxDistance)
    {
        RayHit result;
        if (maxDistance <= 0.0f)
        {
            return result;
        }

        const Xna::Vector3 end = At(origin, direction, maxDistance);
        const Xna::BoundingBox box(
            Xna::Vector3(std::min(origin.X, end.X), std::min(origin.Y, end.Y), std::min(origin.Z, end.Z)),
            Xna::Vector3(std::max(origin.X, end.X), std::max(origin.Y, end.Y), std::max(origin.Z, end.Z)));

        const std::size_t obbCount = world.obbs.size();
        for (const std::uint32_t index : broad.Query(cell, box))
        {
            RayHit one;
            if (index < obbCount)
            {
                one = RayCastObb(origin, direction, maxDistance, world.obbs[index]);
            }
            else
            {
                const CollisionMesh& mesh = world.meshes[index - obbCount];
                for (std::size_t t = 0; t + 2 < mesh.indices.size(); t += 3)
                {
                    const RayHit each = RayCastTriangle(origin,
                                                        direction,
                                                        maxDistance,
                                                        mesh.vertices[mesh.indices[t]],
                                                        mesh.vertices[mesh.indices[t + 1]],
                                                        mesh.vertices[mesh.indices[t + 2]]);
                    if (each.hit && (!one.hit || each.distance < one.distance))
                    {
                        one = each;
                        one.surface = mesh.surface;
                    }
                }
            }
            // Nearest, not first found: §50.1 asks what is BETWEEN the eye and a thing, and the
            // first shape the bucket happened to list is not an answer to that.
            if (one.hit && (!result.hit || one.distance < result.distance))
            {
                result = one;
                result.shape = index;
            }
        }

        const RayHit ground = RayCastTerrain(world.terrain, origin, direction, maxDistance);
        if (ground.hit && (!result.hit || ground.distance < result.distance))
        {
            result = ground;
        }
        return result;
    }

} // namespace cnahouse::physics
