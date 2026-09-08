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
    struct CollisionWorld
    {
        std::string worldHash;
        /// @brief The loose grid's cell size in metres. 1.0 today; read, not assumed.
        float gridCell = 1.0f;
        std::vector<std::string> surfaces;
        std::vector<CollisionObb> obbs;
        std::vector<CollisionMesh> meshes;
        std::vector<CollisionCell> cells;

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
