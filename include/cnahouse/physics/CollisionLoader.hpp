// SPDX-License-Identifier: MIT
#pragma once

#include <cstdint>
#include <string_view>

#include "cnahouse/physics/CollisionData.hpp"
#include "cnahouse/util/Result.hpp"

namespace System::IO
{
    class Stream;
}

namespace cnahouse::physics
{

    /// @brief Reads `collision.bin`. The format is `docs/collision-format.md`.
    ///
    /// **Nothing here is graphics.** `TitleContainer::OpenStream` and `System::IO::BinaryReader`
    /// are plain XNA 4.0 / .NET API, so the static collision world is parsed with no device at all
    /// (ADR-0001, `cna-house.md` §49.2).
    ///
    /// **Every failure is a `util::Result` naming the file and what was wrong with it**, and there
    /// is no partial world. A half-loaded collision world is worse than none: the rooms that did
    /// load look right, and the player walks through the wall of the one that did not.
    ///
    /// The reader checks what the format PROMISES rather than only what it can parse — a bucket
    /// index inside the cell's own shape list, a triangle inside its mesh's vertices, a shape
    /// index inside the world's shapes. Every one of those is a crash or a silent
    /// wrong-shape-tested in the sweep, and every one is one comparison here.
    class CollisionLoader
    {
    public:
        /// @brief The 4-byte magic, `CCOL`.
        static constexpr std::uint32_t kMagic = 0x4C4F4343u; // 'C','C','O','L' little-endian
        /// @brief The only version this reader accepts.
        ///
        /// 3 since `HOUSE-00774`: a cell says whether it is the open outdoors, because §11.5's
        /// height field is one surface over the whole lot and the basement is under part of it.
        static constexpr std::uint32_t kVersion = 3u;
        /// @brief A name longer than this is a corrupt length field, not a name.
        static constexpr std::uint32_t kMaxNameBytes = 1024u;
        /// @brief `u16` surface indices cannot address more.
        static constexpr std::uint32_t kMaxSurfaces = 65536u;
        /// @brief `u16` mesh indices address no more vertices, which is why the writer refuses a
        ///        proxy that exceeds it rather than truncating.
        static constexpr std::uint32_t kMaxMeshVertices = 65535u;
        /// @brief §72 budgets ~4 300 OBBs and 18 meshes; these are corrupt-length guards, not
        ///        budgets, and are far above anything the house can legitimately contain.
        static constexpr std::uint32_t kMaxObbs = 1048576u;
        static constexpr std::uint32_t kMaxMeshes = 65536u;
        static constexpr std::uint32_t kMaxTrianglesPerMesh = 1048576u;
        static constexpr std::uint32_t kMaxCells = 65536u;
        /// @brief `docs/collision-format.md` §3.4: a cell's `shapeCount` is `u16`-addressable
        ///        because bucket entries are `u16` local indices.
        static constexpr std::uint32_t kMaxShapesPerCell = 65535u;
        /// @brief `EXT_WORLD` is 400 x 400 = 160 000 buckets, the largest this house has.
        static constexpr std::uint32_t kMaxBucketsPerCell = 1048576u;
        /// @brief §11.5's field is 81 × 65. Four million samples is 16 MB of heights, which is a
        ///        file that is wrong rather than a lot bigger than this one.
        static constexpr std::uint32_t kMaxTerrainSamples = 4194304u;

        /// @brief Reads from @p stream. @p name is used only in error messages.
        [[nodiscard]] static util::Result<CollisionWorld> Read(System::IO::Stream& stream,
                                                               std::string_view name);

        /// @brief Opens @p contentPath through `TitleContainer` and reads it.
        [[nodiscard]] static util::Result<CollisionWorld> ReadFromTitle(std::string_view contentPath);
    };

} // namespace cnahouse::physics
