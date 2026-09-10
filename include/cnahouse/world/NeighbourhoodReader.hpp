// SPDX-License-Identifier: MIT
#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include "cnahouse/util/Result.hpp"
#include "cnahouse/world/NeighbourhoodData.hpp"
#include "cnahouse/world/WorldTypes.hpp"

namespace System::IO
{
    class Stream;
}

namespace cnahouse::world
{

    /// @brief Reads `neighbourhood.bin`. The format is `docs/neighbourhood-format.md`.
    ///
    /// **Nothing here is graphics.** `TitleContainer::OpenStream` and `System::IO::BinaryReader`
    /// are plain XNA 4.0 / .NET API, so the file is parsed with no device at all and the parse can
    /// be tested without one (ADR-0001). Whatever draws §11.4 takes what comes back and uploads it.
    ///
    /// **Every failure is a `util::Result` naming the file and what was wrong with it.** There is
    /// no partial library: a neighbourhood half-loaded is a street with gaps in it, and a gap in
    /// the background is exactly the kind of thing nobody notices until a screenshot.
    class NeighbourhoodReader
    {
    public:
        /// @brief The 4-byte magic, `CNBH`.
        static constexpr std::uint32_t kMagic = 0x48424E43u; // 'C','N','B','H' little-endian
        /// @brief The only version this reader accepts.
        static constexpr std::uint32_t kVersion = 1u;
        /// @brief A name longer than this is a corrupt length field, not a name.
        static constexpr std::uint32_t kMaxNameBytes = 1024u;
        /// @brief `u16` material indices cannot address more.
        static constexpr std::uint32_t kMaxTableEntries = 65536u;
        /// @brief §11.4 has 34 assets; a file claiming more than this is corrupt, not ambitious.
        static constexpr std::uint32_t kMaxAssets = 65536u;
        /// @brief One primitive per material, so this is the table's own limit again.
        static constexpr std::uint32_t kMaxPrimitivesPerAsset = 4096u;
        /// @brief What `u16` indices address. A primitive over this must set `wideIndices`.
        static constexpr std::uint32_t kMaxVertices16Bit = 65535u;
        /// @brief A neighbour larger than this is a corrupt count, not a house.
        static constexpr std::uint32_t kMaxVerticesPerPrimitive = 4194304u;
        static constexpr std::uint32_t kMaxIndicesPerPrimitive = 16777216u;

        /// @brief Reads from @p stream. @p name is used only in error messages.
        [[nodiscard]] static util::Result<NeighbourhoodLibrary> Read(System::IO::Stream& stream,
                                                                     std::string_view name);

        /// @brief Opens @p contentPath through `TitleContainer` and reads it.
        ///
        /// @param contentPath a path relative to the working directory, e.g.
        ///        `content/world/neighbourhood.bin`.
        [[nodiscard]] static util::Result<NeighbourhoodLibrary> ReadFromTitle(std::string_view contentPath);
    };

    /// @brief The assets §11.4's rows name that @p library does not hold, sorted and deduplicated.
    ///
    /// This is `docs/neighbourhood-format.md` §3's resolution done for every row at once. A row
    /// whose `asset` is in here draws nothing, and nothing drawing is the failure mode this whole
    /// format exists to make visible: before `HOUSE-00856` the meshes were written into
    /// `build/neighbourhood` and no code anywhere opened that directory.
    ///
    /// **Nothing is excluded, deliberately.** §11.4's vehicles share the array and are
    /// `HOUSE-00847`'s to deliver, so today they come back in this list -- and the test asserts
    /// they are the ONLY two, which is a statement about who owes what. Teaching this function
    /// which prefixes belong to which task would put `neighbourhood_gen.is_ours` in a second
    /// place, and the second copy is the one that goes stale.
    ///
    /// A row whose asset was never interned comes back as `id:0x…`, which is the honest answer:
    /// the name is not in this process.
    [[nodiscard]] std::vector<std::string> UnresolvedAssets(const NeighbourhoodLibrary& library,
                                                            const std::vector<NeighbourBuilding>& rows);

} // namespace cnahouse::world
