// SPDX-License-Identifier: MIT
#pragma once

#include <cstdint>
#include <map>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "cnahouse/util/Result.hpp"
#include "cnahouse/world/ChunkData.hpp"

namespace Microsoft::Xna::Framework::Graphics
{
    class GraphicsDevice;
    class IndexBuffer;
    class VertexBuffer;
    class VertexDeclaration;
} // namespace Microsoft::Xna::Framework::Graphics

namespace cnahouse::world
{

    /// @brief A cell's static geometry on the GPU: `cna-house.md` §17.1's per-cell residency.
    ///
    /// One `VertexBuffer` and one `IndexBuffer` per chunk, uploaded once and drawn with
    /// `Matrix::Identity` — §17.4 bakes every placement into the vertices precisely so that a chunk
    /// needs no transform. Loading and unloading are per **cell**, because that is the unit
    /// `VisibilitySystem` decides about and `ResidencySystem` streams.
    ///
    /// ## The vertex declaration is the meaning; the built-in type is only a carrier
    ///
    /// XNA 4.0's `VertexBuffer.SetData<T>` is generic over any struct, so real XNA declares a
    /// `VertexPositionDualTexture : IVertexType` and uploads it. CNA offers four concrete overloads
    /// instead — `VertexPositionColor`, `VertexPositionColorTexture`, `VertexPositionNormalTexture`,
    /// `VertexPositionTexture` — plus a `CNAEXT SetDataRaw` this project may not call (ADR-0001).
    /// None of the four carries two texture coordinates, which is exactly what `DualTextureEffect`
    /// reads.
    ///
    /// CNA supports this deliberately rather than by accident: `VertexBuffer::ValidateSetDataRange`
    /// says *"a built-in type's own declaration already describes exactly that stream, but this
    /// buffer may carry any declaration the caller chose, so every declared element still has to
    /// fit in the bytes actually uploaded"*. So a `dual` chunk is uploaded through
    /// `VertexPositionNormalTexture`, whose stream is 32 floats-worth of bytes in a fixed order,
    /// under a declaration that reads those bytes as `Position` `TexCoord0` `TexCoord1`. The struct
    /// field names are not what reaches the GPU; the declaration is. The cost is four bytes a
    /// vertex — 32 uploaded where the file stores 28 — and `ResidentBytes()` counts what was
    /// actually uploaded, not what the file held, so the difference is visible rather than assumed.
    class CellRuntime
    {
    public:
        /// @brief One chunk's buffers, ready to draw.
        struct ResidentChunk
        {
            /// @brief Index into the library's `chunks`, so the material, bounds and sub-ranges
            ///        are read from there rather than copied here.
            std::uint32_t chunk = 0u;
            std::unique_ptr<Microsoft::Xna::Framework::Graphics::VertexBuffer> vertices;
            std::unique_ptr<Microsoft::Xna::Framework::Graphics::IndexBuffer> indices;
            /// @brief Triangles, which is what `DrawIndexedPrimitives` asks for.
            std::uint32_t primitiveCount = 0u;
            /// @brief Bytes uploaded into the two buffers, at the stride actually used.
            std::uint64_t bytes = 0u;
        };

        /// @brief Borrows @p device and @p library; both must outlive this object.
        CellRuntime(Microsoft::Xna::Framework::Graphics::GraphicsDevice& device, const ChunkLibrary& library);
        ~CellRuntime();

        CellRuntime(const CellRuntime&) = delete;
        CellRuntime& operator=(const CellRuntime&) = delete;

        /// @brief Uploads every chunk of @p cell. Loading a resident cell is a no-op success.
        ///
        /// A cell the library has no chunks for is **also** a success, and resident with none: an
        /// empty yard is a real answer, and failing on it would make every caller special-case the
        /// outdoors. A cell the world does not contain at all is `NotFound`.
        [[nodiscard]] util::Result<void> Load(std::string_view cell);

        /// @brief Releases @p cell's buffers. Unloading a cell that is not resident does nothing.
        void Unload(std::string_view cell);

        /// @brief Releases every cell.
        void UnloadAll();

        [[nodiscard]] bool IsResident(std::string_view cell) const;

        /// @brief @p cell's chunks, or `nullptr` when it is not resident.
        [[nodiscard]] const std::vector<ResidentChunk>* Chunks(std::string_view cell) const;

        /// @brief Every resident chunk's index into the library, ascending (`HOUSE-00676`).
        ///
        /// What a draw list is built from. Residency is per CELL and a draw list is per chunk, so
        /// somebody has to flatten one into the other; doing it here means it happens when a cell
        /// loads rather than once a frame, and that the answer is in a stable order whatever order
        /// the cells arrived in.
        [[nodiscard]] std::span<const std::uint32_t> ResidentChunkIndices() const noexcept
        {
            return residentIndices_;
        }

        /// @brief The buffers for library chunk @p chunk, or `nullptr` when its cell is not
        ///        resident (`HOUSE-00676`).
        ///
        /// The reverse of `Chunks`, and the direction a sorted draw list needs: a `RenderItem`
        /// names a chunk, and the pass that draws it has to get from that number to two GPU buffers
        /// without knowing which cell it came from -- the sort has just thrown that grouping away
        /// on purpose.
        [[nodiscard]] const ResidentChunk* Find(std::uint32_t chunk) const noexcept;

        [[nodiscard]] std::size_t ResidentCells() const
        {
            return resident_.size();
        }

        /// @brief Chunks resident across every cell.
        [[nodiscard]] std::size_t ResidentChunks() const;
        /// @brief Bytes uploaded across every resident cell, vertices and indices together.
        [[nodiscard]] std::uint64_t ResidentBytes() const;

        /// @brief The declaration one layout's bytes are read through. Shared, not per chunk.
        [[nodiscard]] static const Microsoft::Xna::Framework::Graphics::VertexDeclaration&
        DeclarationFor(ChunkLayout layout);

        /// @brief The stride the GPU actually sees, which is not always the file's stride.
        ///
        /// `dual` is 28 bytes in `chunks.bin` and 32 on the GPU, because the only public upload
        /// path wide enough for it carries 32 (see the class comment).
        [[nodiscard]] static std::uint32_t UploadStride(ChunkLayout layout);

    private:
        Microsoft::Xna::Framework::Graphics::GraphicsDevice& device_;
        const ChunkLibrary& library_;
        /// Ordered, so that the resident set is reported in a stable order whatever the load order.
        std::map<std::string, std::vector<ResidentChunk>, std::less<>> resident_;
        /// @brief Library chunk index -> its buffers, or null. One entry per chunk in the file.
        ///
        /// Rebuilt whole after every load and unload rather than patched. Loading is rare and a
        /// house is a few hundred chunks, so the cost is nothing; patching would mean a stale
        /// pointer into a vector that has just been erased, which is the one failure mode this
        /// index could have.
        std::vector<const ResidentChunk*> byChunk_;
        std::vector<std::uint32_t> residentIndices_;

        [[nodiscard]] util::Result<ResidentChunk> Upload(const Chunk& chunk, std::uint32_t index);

        /// @brief Rebuilds `byChunk_` and `residentIndices_` from `resident_`.
        void RebuildChunkIndex();
    };

} // namespace cnahouse::world
