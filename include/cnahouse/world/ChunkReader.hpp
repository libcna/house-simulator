// SPDX-License-Identifier: MIT
#pragma once

#include <cstdint>
#include <string_view>

#include "cnahouse/util/Result.hpp"
#include "cnahouse/world/ChunkData.hpp"

namespace System::IO
{
    class Stream;
}

namespace cnahouse::world
{

    /// @brief Reads `chunks.bin`. The format is `docs/chunk-format.md`.
    ///
    /// **Nothing here is graphics.** `TitleContainer::OpenStream` and `System::IO::BinaryReader`
    /// are plain XNA 4.0 / .NET API, so the file is parsed with no device at all and the parse can
    /// be tested without one (ADR-0001, `cna-house.md` §17.4). `CellRuntime` takes what comes back
    /// and uploads it.
    ///
    /// **Every failure is a `util::Result` naming the file and what was wrong with it.** There is
    /// no partial library and there must not be: a cell whose geometry half-loaded is a room with
    /// holes in it, and a hole is harder to notice than a missing room.
    class ChunkReader
    {
    public:
        /// @brief The 4-byte magic, `CCHK`.
        static constexpr std::uint32_t kMagic = 0x4B484343u; // 'C','C','H','K' little-endian
        /// @brief The only version this reader accepts.
        static constexpr std::uint32_t kVersion = 1u;
        /// @brief A name longer than this is a corrupt length field, not a name.
        static constexpr std::uint32_t kMaxNameBytes = 1024u;
        /// @brief `u16` cell and material indices cannot address more.
        static constexpr std::uint32_t kMaxTableEntries = 65536u;
        /// @brief One chunk per cell per material, with room to spare; a larger count is corrupt.
        static constexpr std::uint32_t kMaxChunks = 1048576u;
        /// @brief What `u16` indices address. A chunk over this must set `wideIndices`.
        static constexpr std::uint32_t kMaxVertices16Bit = 65535u;
        /// @brief A chunk larger than this is a corrupt count, not a chunk. 4 M vertices is 128 MB
        ///        at the widest layout, which is already far beyond §72's whole geometry budget.
        static constexpr std::uint32_t kMaxVerticesPerChunk = 4194304u;
        static constexpr std::uint32_t kMaxIndicesPerChunk = 16777216u;

        /// @brief Reads from @p stream. @p name is used only in error messages.
        [[nodiscard]] static util::Result<ChunkLibrary> Read(System::IO::Stream& stream,
                                                             std::string_view name);

        /// @brief Opens @p contentPath through `TitleContainer` and reads it.
        ///
        /// @param contentPath a path relative to the working directory, e.g.
        ///        `content/world/chunks.bin`.
        [[nodiscard]] static util::Result<ChunkLibrary> ReadFromTitle(std::string_view contentPath);
    };

} // namespace cnahouse::world
