// SPDX-License-Identifier: MIT
#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "Microsoft/Xna/Framework/BoundingBox.hpp"

namespace cnahouse::world
{

    /// @brief Which vertex layout a chunk uses. `docs/chunk-format.md` §3.
    ///
    /// A chunk does not use CNA's 48-byte model vertex. It declares what its own effect reads and
    /// nothing more, which it can because the chunk is built by this project and uploaded into its
    /// own `VertexBuffer`. Chunks are grouped by effect class, so a chunk always has exactly one.
    enum class ChunkLayout : std::uint8_t
    {
        /// `Position` `Normal` `TexCoord0`, 32 bytes -- `BasicEffect`.
        Basic = 0,
        /// `Position` `TexCoord0` `TexCoord1`, 28 bytes -- `DualTextureEffect`, whose second
        /// channel is §18.3's lightmap and whose first is the albedo.
        Dual = 1,
        /// `Position` `TexCoord0`, 20 bytes -- `AlphaTestEffect`.
        AlphaTest = 2,
    };

    /// @brief How many bytes one vertex of @p layout occupies.
    [[nodiscard]] constexpr std::uint32_t ChunkVertexStride(ChunkLayout layout) noexcept
    {
        switch (layout)
        {
            case ChunkLayout::Basic:
                return 32u;
            case ChunkLayout::Dual:
                return 28u;
            case ChunkLayout::AlphaTest:
                return 20u;
        }
        return 0u;
    }

    /// @brief One source object's slice of a chunk's index buffer, with its own box.
    ///
    /// §17.4 wants a bounding box per sub-range "so a big group can still be partially culled", and
    /// the box is the SOURCE's, not the group's: a sub-range carrying the group's box culls nothing
    /// and costs 24 bytes to do it. `source` is the `layout.props.json` id for a prop and
    /// `<cell>:<material>` for a surface class of the generated shell (`HOUSE-00473`).
    struct ChunkSubRange
    {
        std::string source;
        std::uint32_t indexStart = 0u;
        std::uint32_t indexCount = 0u;
        Microsoft::Xna::Framework::BoundingBox bounds;
    };

    /// @brief A chunk drawn at every LOD level: bits 0-2 are §71.3's vegetation LOD levels.
    inline constexpr std::uint8_t kEveryLod = 0b111u;

    /// @brief The `lodMask` bit of LOD level @p level (0 High, 1 Web, 2 Android). Values outside
    ///        0-2 are clamped, so Ultra's negative bias draws LOD0.
    [[nodiscard]] constexpr std::uint8_t LodBit(int level) noexcept
    {
        return static_cast<std::uint8_t>(1u << (level < 0 ? 0 : (level > 2 ? 2 : level)));
    }

    /// @brief One draw call's worth of pre-batched static geometry, in world space.
    ///
    /// The vertices arrive packed in the layout's own byte order and are kept that way: they are
    /// uploaded verbatim, and unpacking them into a C++ struct only to pack them again would be two
    /// conversions and a vertex type per layout for no gain.
    struct Chunk
    {
        std::uint16_t cell = 0u;
        std::uint16_t material = 0u;
        ChunkLayout layout = ChunkLayout::Basic;
        /// @brief `false` for `u16` indices, `true` for `u32`. §17.4's "32-bit if needed".
        bool wideIndices = false;
        Microsoft::Xna::Framework::BoundingBox bounds;
        std::uint32_t vertexCount = 0u;
        std::vector<std::uint8_t> vertices;
        std::uint32_t indexCount = 0u;
        std::vector<std::uint8_t> indices;
        std::vector<ChunkSubRange> subRanges;
        /// @brief The LOD levels this chunk is drawn at (`HOUSE-02405`). An authored vegetation
        ///        LOD variant sets only its own levels; everything else is `kEveryLod`.
        std::uint8_t lodMask = kEveryLod;
    };

    /// @brief Everything `content/world/chunks.bin` holds.
    struct ChunkLibrary
    {
        std::string worldHash;
        std::vector<std::string> cells;
        std::vector<std::string> materials;
        std::vector<Chunk> chunks;

        /// @brief The chunk indices belonging to @p cell, in file order. Empty if it has none.
        [[nodiscard]] std::vector<std::uint32_t> ChunksOf(std::string_view cell) const;
        /// @brief The index of @p cell in `cells`, or `cells.size()` when it is not there.
        [[nodiscard]] std::size_t IndexOfCell(std::string_view cell) const;
        /// @brief Total bytes of vertex and index data, which is what a buffer upload costs.
        [[nodiscard]] std::uint64_t GeometryBytes() const;
    };

} // namespace cnahouse::world
