// SPDX-License-Identifier: MIT
#include "cnahouse/physics/Terrain.hpp"

#include <algorithm>
#include <cmath>

namespace cnahouse::physics
{
    namespace
    {
        namespace Xna = Microsoft::Xna::Framework;

        /// The four corners of the square (@p ix, @p iz), and where in it the point falls.
        struct Square
        {
            std::uint32_t ix = 0u;
            std::uint32_t iz = 0u;
            float u = 0.0F; ///< 0..1 across the square in +x
            float v = 0.0F; ///< 0..1 across it in +z
            float h00 = 0.0F;
            float h10 = 0.0F;
            float h01 = 0.0F;
            float h11 = 0.0F;
            bool over = false; ///< the point was inside the field before it was clamped
        };

        Square Locate(const CollisionTerrain& terrain, float x, float z)
        {
            Square square;
            const float fx = (x - terrain.originX) / terrain.step;
            const float fz = (z - terrain.originZ) / terrain.step;
            const auto lastX = static_cast<float>(terrain.samplesX - 1u);
            const auto lastZ = static_cast<float>(terrain.samplesZ - 1u);
            square.over = fx >= 0.0F && fz >= 0.0F && fx <= lastX && fz <= lastZ;

            const float cx = std::clamp(fx, 0.0F, lastX);
            const float cz = std::clamp(fz, 0.0F, lastZ);
            square.ix = std::min(static_cast<std::uint32_t>(cx), terrain.samplesX - 2u);
            square.iz = std::min(static_cast<std::uint32_t>(cz), terrain.samplesZ - 2u);
            square.u = cx - static_cast<float>(square.ix);
            square.v = cz - static_cast<float>(square.iz);
            square.h00 = terrain.Height(square.ix, square.iz);
            square.h10 = terrain.Height(square.ix + 1u, square.iz);
            square.h01 = terrain.Height(square.ix, square.iz + 1u);
            square.h11 = terrain.Height(square.ix + 1u, square.iz + 1u);
            return square;
        }

        Xna::Vector3 Normalised(float x, float y, float z)
        {
            const float length = std::sqrt(x * x + y * y + z * z);
            if (length < 1.0e-12F)
            {
                return Xna::Vector3(0.0F, 1.0F, 0.0F);
            }
            return Xna::Vector3(x / length, y / length, z / length);
        }

        /// The upward normal of the plane through three heights on a @p step grid. The cross
        /// product is written out because the two edges are axis-aligned in x/z, which turns nine
        /// multiplies into three.
        Xna::Vector3 PlaneNormal(float dhdx, float dhdz)
        {
            return Normalised(-dhdx, 1.0F, -dhdz);
        }

        /// §11.5's cut: is this square steep enough that the bilinear surface and the triangles it
        /// is drawn as disagree by enough to matter?
        bool Steep(const Square& square, float step)
        {
            // The steepest of the two triangles' planes. Taking the whole square's corner-to-corner
            // drop instead would call a saddle flat.
            const float a = PlaneNormal((square.h10 - square.h00) / step, (square.h11 - square.h10) / step).Y;
            const float b = PlaneNormal((square.h11 - square.h01) / step, (square.h01 - square.h00) / step).Y;
            return std::min(a, b) < kTerrainTriangleSlopeCosine;
        }

    } // namespace

    TerrainSample TerrainAt(const CollisionTerrain& terrain, float x, float z)
    {
        TerrainSample sample;
        if (!terrain.present || terrain.heights.empty())
        {
            return sample;
        }
        const Square square = Locate(terrain, x, z);
        sample.over = square.over;
        // The NEAREST sample's material, not an interpolated one: a material is a name, and half
        // of grass and half of gravel is neither.
        sample.surface = terrain.Material(square.ix + (square.u >= 0.5F ? 1u : 0u),
                                          square.iz + (square.v >= 0.5F ? 1u : 0u));
        sample.steep = Steep(square, terrain.step);

        // The square as it is DRAWN and as it is COLLIDED WITH: two triangles, split along the
        // (0,0)-(1,1) diagonal, and which side of that diagonal the point falls on picks one.
        //
        // Not the bilinear patch. §11.5 asks for one and it cannot be the collision surface: a
        // bilinear patch and the two triangles over the same four samples differ by a quarter of
        // the square's twist, which on THIS lot reaches 74 mm on a square that is not even steep
        // (`HOUSE-00553` measured it, and §11.5 now says so). A body told the ground is at the
        // bilinear height, and placed a millimetre above that, stands 73 mm inside the ground it
        // is drawn on -- and the depenetration then shoves it out every single tick.
        if (square.v <= square.u)
        {
            const float dhdx = (square.h10 - square.h00) / terrain.step;
            const float dhdz = (square.h11 - square.h10) / terrain.step;
            sample.height = square.h00 + dhdx * square.u * terrain.step + dhdz * square.v * terrain.step;
            sample.normal = PlaneNormal(dhdx, dhdz);
        }
        else
        {
            const float dhdx = (square.h11 - square.h01) / terrain.step;
            const float dhdz = (square.h01 - square.h00) / terrain.step;
            sample.height = square.h00 + dhdx * square.u * terrain.step + dhdz * square.v * terrain.step;
            sample.normal = PlaneNormal(dhdx, dhdz);
        }
        return sample;
    }

    namespace
    {
        /// The corner of square (@p ix, @p iz) offset by (@p dx, @p dz) as a world point.
        Xna::Vector3 Corner(const CollisionTerrain& terrain,
                            std::uint32_t ix,
                            std::uint32_t iz,
                            std::uint32_t dx,
                            std::uint32_t dz)
        {
            return Xna::Vector3(terrain.originX + static_cast<float>(ix + dx) * terrain.step,
                                terrain.Height(ix + dx, iz + dz),
                                terrain.originZ + static_cast<float>(iz + dz) * terrain.step);
        }

        /// The inclusive range of squares an x/z box covers, clamped to the field.
        struct Range
        {
            std::uint32_t x0 = 0u;
            std::uint32_t x1 = 0u;
            std::uint32_t z0 = 0u;
            std::uint32_t z1 = 0u;
            bool empty = true;
        };

        Range Cover(const CollisionTerrain& terrain, float minX, float maxX, float minZ, float maxZ)
        {
            Range range;
            if (!terrain.present || terrain.samplesX < 2u || terrain.samplesZ < 2u)
            {
                return range;
            }
            const auto lastX = static_cast<float>(terrain.samplesX - 2u);
            const auto lastZ = static_cast<float>(terrain.samplesZ - 2u);
            const float lowX = std::floor((minX - terrain.originX) / terrain.step);
            const float highX = std::floor((maxX - terrain.originX) / terrain.step);
            const float lowZ = std::floor((minZ - terrain.originZ) / terrain.step);
            const float highZ = std::floor((maxZ - terrain.originZ) / terrain.step);
            if (highX < 0.0F || highZ < 0.0F || lowX > lastX || lowZ > lastZ)
            {
                return range; // entirely off the lot
            }
            range.x0 = static_cast<std::uint32_t>(std::clamp(lowX, 0.0F, lastX));
            range.x1 = static_cast<std::uint32_t>(std::clamp(highX, 0.0F, lastX));
            range.z0 = static_cast<std::uint32_t>(std::clamp(lowZ, 0.0F, lastZ));
            range.z1 = static_cast<std::uint32_t>(std::clamp(highZ, 0.0F, lastZ));
            range.empty = false;
            return range;
        }

    } // namespace

    SweepHit
    SweepCapsuleTerrain(const CollisionTerrain& terrain, const Capsule& capsule, const Xna::Vector3& motion)
    {
        SweepHit result;
        const float r = capsule.radius;
        const float minX = std::min(capsule.centre.X, capsule.centre.X + motion.X) - r;
        const float maxX = std::max(capsule.centre.X, capsule.centre.X + motion.X) + r;
        const float minZ = std::min(capsule.centre.Z, capsule.centre.Z + motion.Z) - r;
        const float maxZ = std::max(capsule.centre.Z, capsule.centre.Z + motion.Z) + r;
        const Range range = Cover(terrain, minX, maxX, minZ, maxZ);
        if (range.empty)
        {
            return result;
        }

        for (std::uint32_t iz = range.z0; iz <= range.z1; ++iz)
        {
            for (std::uint32_t ix = range.x0; ix <= range.x1; ++ix)
            {
                const Xna::Vector3 c00 = Corner(terrain, ix, iz, 0u, 0u);
                const Xna::Vector3 c10 = Corner(terrain, ix, iz, 1u, 0u);
                const Xna::Vector3 c01 = Corner(terrain, ix, iz, 0u, 1u);
                const Xna::Vector3 c11 = Corner(terrain, ix, iz, 1u, 1u);
                const SweepHit a = SweepCapsuleTriangle(capsule, motion, c00, c10, c11);
                if (a.hit && (!result.hit || a.time < result.time))
                {
                    result = a;
                }
                const SweepHit b = SweepCapsuleTriangle(capsule, motion, c00, c11, c01);
                if (b.hit && (!result.hit || b.time < result.time))
                {
                    result = b;
                }
            }
        }
        return result;
    }

    Overlap OverlapCapsuleTerrain(const CollisionTerrain& terrain, const Capsule& capsule)
    {
        Overlap result;
        const float r = capsule.radius;
        const Range range = Cover(
            terrain, capsule.centre.X - r, capsule.centre.X + r, capsule.centre.Z - r, capsule.centre.Z + r);
        if (range.empty)
        {
            return result;
        }

        for (std::uint32_t iz = range.z0; iz <= range.z1; ++iz)
        {
            for (std::uint32_t ix = range.x0; ix <= range.x1; ++ix)
            {
                const Xna::Vector3 c00 = Corner(terrain, ix, iz, 0u, 0u);
                const Xna::Vector3 c10 = Corner(terrain, ix, iz, 1u, 0u);
                const Xna::Vector3 c01 = Corner(terrain, ix, iz, 0u, 1u);
                const Xna::Vector3 c11 = Corner(terrain, ix, iz, 1u, 1u);
                for (const Overlap& one : {OverlapCapsuleTriangle(capsule, c00, c10, c11),
                                           OverlapCapsuleTriangle(capsule, c00, c11, c01)})
                {
                    if (one.overlapped && (!result.overlapped || one.depth > result.depth))
                    {
                        result = one;
                    }
                }
            }
        }
        return result;
    }

} // namespace cnahouse::physics
