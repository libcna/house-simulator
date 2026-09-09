// SPDX-License-Identifier: MIT
#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include "Microsoft/Xna/Framework/BoundingBox.hpp"
#include "Microsoft/Xna/Framework/Vector3.hpp"

namespace cnahouse::physics
{

    /// @brief What a collision shape is part of. `docs/collision-format.md` §3.3.
    ///
    /// Not decoration: §64 asks a footstep which surface it landed on, §49.3's ground probe cares
    /// whether it hit a floor or a wall, and a debug overlay colours by this.
    enum class CollisionKind : std::uint8_t
    {
        Floor = 0,
        Ceiling = 1,
        Wall = 2,
        Stair = 3,
        Prop = 4,
        Exterior = 5,
    };

    /// @brief An oriented box: most of the world, and the shape a capsule sweep is cheapest against.
    ///
    /// `yaw` is radians about +Y and is the ONLY rotation a static shape may have -- §49.2's walls,
    /// floors and ceilings are axis-aligned and a prop's proxy is placed by the yaw its row states.
    /// A shape needing more than that is a triangle mesh instead, which is why both exist.
    struct CollisionObb
    {
        Microsoft::Xna::Framework::Vector3 centre;
        Microsoft::Xna::Framework::Vector3 halfExtents;
        float yaw = 0.0f;
        std::uint16_t surface = 0u;
        CollisionKind kind = CollisionKind::Wall;
    };

    /// @brief A triangle mesh: the stair ramps, the rafter envelope, and any proxy that is not a box.
    struct CollisionMesh
    {
        std::uint16_t surface = 0u;
        CollisionKind kind = CollisionKind::Wall;
        /// @brief Written by the builder rather than recomputed here: the file states it, and two
        ///        computations of the same box are two chances to disagree about a degenerate one.
        Microsoft::Xna::Framework::BoundingBox bounds;
        std::vector<Microsoft::Xna::Framework::Vector3> vertices;
        /// @brief Three `u16` per triangle, flattened. `u16` because `vertexCount` cannot exceed
        ///        65 535 -- the writer refuses a proxy that does rather than truncating it.
        std::vector<std::uint16_t> indices;

        [[nodiscard]] std::size_t TriangleCount() const
        {
            return indices.size() / 3;
        }
    };

    /// @brief One cell's shapes and the loose grid over them. §3.4.
    struct CollisionCell
    {
        std::string id;
        /// @brief Is this cell the open outdoors, so that §11.5's ground is part of its collision?
        ///
        /// §49.2: *"Exterior collision uses the terrain height field plus OBBs"*. The height field
        /// is ONE surface over the whole lot and the house stands on it, so the ground passes
        /// through the basement and a tenth of a metre under `L0`'s floor. A body on the basement
        /// stair must not be pushed by it and a body on the lawn must; the difference is which
        /// cell it is in, and this is the file saying so (`HOUSE-00774`).
        bool outdoors = false;
        Microsoft::Xna::Framework::BoundingBox bounds;
        /// @brief GLOBAL shape indices: `0 … obbs.size()-1` are OBBs, the rest are meshes at
        ///        `index - obbs.size()`.
        std::vector<std::uint32_t> shapes;
        std::uint32_t nx = 0u;
        std::uint32_t nz = 0u;
        /// @brief The grid's minimum corner, x and z.
        float originX = 0.0f;
        float originZ = 0.0f;
        /// @brief `nx * nz` buckets of LOCAL indices into `shapes`. A cell holds far fewer than
        ///        65 535 shapes while the world holds more, so local indices halve the largest
        ///        part of the file.
        std::vector<std::vector<std::uint16_t>> buckets;

        /// @brief The bucket a point falls in, or `nullptr` when it is outside the grid.
        [[nodiscard]] const std::vector<std::uint16_t>* BucketAt(float x, float z) const;
    };

    /// @brief Everything `content/world/collision.bin` holds.
    /// @brief §11.5's ground: heights on a regular x/z grid, with a material per sample.
    ///
    /// **A height field, not a mesh.** 81 × 65 samples a metre apart describe the whole lot in
    /// 26 KB, where the same ground as triangles would be 10 240 of them in every cell list it
    /// touched. What a collider does with it is `physics/Terrain.hpp`.
    struct CollisionTerrain
    {
        /// @brief False when the world has no exterior. Everything below is then empty.
        bool present = false;
        std::uint32_t samplesX = 0;
        std::uint32_t samplesZ = 0;
        /// @brief The sample grid's minimum corner.
        float originX = 0.0f;
        float originZ = 0.0f;
        /// @brief Metres between samples.
        float step = 1.0f;
        /// @brief `samplesX × samplesZ` heights in METRES, row-major: z outer, x inner.
        std::vector<float> heights;
        /// @brief Indices into `CollisionWorld::surfaces`.
        std::vector<std::uint16_t> materials;
        /// @brief One index into `materials` per sample.
        std::vector<std::uint8_t> materialIndex;

        /// @brief The height at sample (@p ix, @p iz). Out of range clamps to the edge, which is
        ///        what a body walking off the far end of the lot should stand on.
        [[nodiscard]] float Height(std::uint32_t ix, std::uint32_t iz) const;

        /// @brief The surface-table index at sample (@p ix, @p iz), clamped the same way.
        [[nodiscard]] std::uint16_t Material(std::uint32_t ix, std::uint32_t iz) const;

        /// @brief The east/north edge of the field, in world metres.
        [[nodiscard]] float MaxX() const
        {
            return originX + static_cast<float>(samplesX - 1) * step;
        }

        [[nodiscard]] float MaxZ() const
        {
            return originZ + static_cast<float>(samplesZ - 1) * step;
        }
    };

    struct CollisionWorld
    {
        std::string worldHash;
        /// @brief The loose grid's cell size in metres. 1.0 today; read, not assumed.
        float gridCell = 1.0f;
        std::vector<std::string> surfaces;
        std::vector<CollisionObb> obbs;
        std::vector<CollisionMesh> meshes;
        std::vector<CollisionCell> cells;
        CollisionTerrain terrain;

        [[nodiscard]] const CollisionCell* Cell(std::string_view id) const;
        /// @brief The surface name a shape's index refers to, or empty when it has none.
        [[nodiscard]] std::string_view SurfaceName(std::uint16_t index) const;

        /// @brief OBBs plus meshes: what a global shape index addresses.
        [[nodiscard]] std::size_t ShapeCount() const
        {
            return obbs.size() + meshes.size();
        }

        /// @brief Triangles across every mesh.
        [[nodiscard]] std::size_t TriangleCount() const;
    };

} // namespace cnahouse::physics
