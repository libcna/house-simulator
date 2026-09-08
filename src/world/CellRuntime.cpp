// SPDX-License-Identifier: MIT
#include "cnahouse/world/CellRuntime.hpp"

#include <cstring>
#include <format>

#include "Microsoft/Xna/Framework/Graphics/BufferUsage.hpp"
#include "Microsoft/Xna/Framework/Graphics/GraphicsDevice.hpp"
#include "Microsoft/Xna/Framework/Graphics/IndexBuffer.hpp"
#include "Microsoft/Xna/Framework/Graphics/IndexElementSize.hpp"
#include "Microsoft/Xna/Framework/Graphics/VertexBuffer.hpp"
#include "Microsoft/Xna/Framework/Graphics/VertexDeclaration.hpp"
#include "Microsoft/Xna/Framework/Graphics/VertexElement.hpp"
#include "Microsoft/Xna/Framework/Graphics/VertexElementFormat.hpp"
#include "Microsoft/Xna/Framework/Graphics/VertexElementUsage.hpp"
#include "Microsoft/Xna/Framework/Graphics/VertexPositionNormalTexture.hpp"
#include "Microsoft/Xna/Framework/Graphics/VertexPositionTexture.hpp"
#include "Microsoft/Xna/Framework/Vector2.hpp"
#include "Microsoft/Xna/Framework/Vector3.hpp"

namespace cnahouse::world
{
    namespace
    {
        using util::Err;
        using util::ErrorCode;
        using namespace Microsoft::Xna::Framework::Graphics;
        using Microsoft::Xna::Framework::Vector2;
        using Microsoft::Xna::Framework::Vector3;

        /// Reads one IEEE-754 binary32 out of the file's little-endian bytes.
        ///
        /// `std::memcpy` rather than a cast: the byte array has no alignment guarantee and reading
        /// a float through a misaligned `float*` is undefined behaviour, which UBSAN says out loud.
        float ReadFloat(const std::uint8_t* bytes)
        {
            float value = 0.0f;
            std::memcpy(&value, bytes, sizeof(float));
            return value;
        }

        /// @brief `basic`: `Position` `Normal` `TexCoord0`, 32 bytes -- `VertexPositionNormalTexture`
        ///        exactly, so this declaration says what the built-in type already means.
        const VertexDeclaration& BasicDeclaration()
        {
            static const VertexDeclaration declaration(
                32,
                {VertexElement(0, VertexElementFormat::Vector3, VertexElementUsage::Position, 0),
                 VertexElement(12, VertexElementFormat::Vector3, VertexElementUsage::Normal, 0),
                 VertexElement(24, VertexElementFormat::Vector2, VertexElementUsage::TextureCoordinate, 0)});
            return declaration;
        }

        /// @brief `dual`: `Position` `TexCoord0` `TexCoord1` over the SAME 32 bytes.
        ///
        /// `DualTextureEffect` reads two texture coordinates and no normal. The bytes arrive
        /// through `VertexPositionNormalTexture`, whose stream is `x y z nx ny nz u v`; this
        /// declaration reads `nx ny` as TEXCOORD0 and `nz u` as TEXCOORD1, and `v` goes unread.
        /// See `CellRuntime`'s class comment for why the carrier is a built-in type.
        const VertexDeclaration& DualDeclaration()
        {
            static const VertexDeclaration declaration(
                32,
                {VertexElement(0, VertexElementFormat::Vector3, VertexElementUsage::Position, 0),
                 VertexElement(12, VertexElementFormat::Vector2, VertexElementUsage::TextureCoordinate, 0),
                 VertexElement(20, VertexElementFormat::Vector2, VertexElementUsage::TextureCoordinate, 1)});
            return declaration;
        }

        /// @brief `alphatest`: `Position` `TexCoord0`, 20 bytes -- `VertexPositionTexture` exactly.
        const VertexDeclaration& AlphaTestDeclaration()
        {
            static const VertexDeclaration declaration(
                20,
                {VertexElement(0, VertexElementFormat::Vector3, VertexElementUsage::Position, 0),
                 VertexElement(12, VertexElementFormat::Vector2, VertexElementUsage::TextureCoordinate, 0)});
            return declaration;
        }

    } // namespace

    const VertexDeclaration& CellRuntime::DeclarationFor(ChunkLayout layout)
    {
        switch (layout)
        {
            case ChunkLayout::Dual:
                return DualDeclaration();
            case ChunkLayout::AlphaTest:
                return AlphaTestDeclaration();
            case ChunkLayout::Basic:
                break;
        }
        return BasicDeclaration();
    }

    std::uint32_t CellRuntime::UploadStride(ChunkLayout layout)
    {
        return static_cast<std::uint32_t>(DeclarationFor(layout).getVertexStrideProperty());
    }

    CellRuntime::CellRuntime(GraphicsDevice& device, const ChunkLibrary& library)
        : device_(device)
        , library_(library)
    {
    }

    CellRuntime::~CellRuntime() = default;

    util::Result<CellRuntime::ResidentChunk> CellRuntime::Upload(const Chunk& chunk, std::uint32_t index)
    {
        ResidentChunk resident;
        resident.chunk = index;
        resident.primitiveCount = chunk.indexCount / 3u;

        const std::uint32_t fileStride = ChunkVertexStride(chunk.layout);
        const std::uint32_t uploadStride = UploadStride(chunk.layout);
        const std::size_t count = chunk.vertexCount;

        try
        {
            resident.vertices = std::make_unique<VertexBuffer>(
                device_, DeclarationFor(chunk.layout), static_cast<int>(count), BufferUsage::WriteOnly);
            if (chunk.layout == ChunkLayout::AlphaTest)
            {
                std::vector<VertexPositionTexture> carrier(count);
                for (std::size_t i = 0; i < count; ++i)
                {
                    const std::uint8_t* v = chunk.vertices.data() + i * fileStride;
                    carrier[i] =
                        VertexPositionTexture(Vector3(ReadFloat(v), ReadFloat(v + 4), ReadFloat(v + 8)),
                                              Vector2(ReadFloat(v + 12), ReadFloat(v + 16)));
                }
                resident.vertices->SetData(carrier.data(), static_cast<int>(count));
            }
            else
            {
                // Both 32-byte layouts go through the same carrier. For `basic` the fields mean
                // what they are called; for `dual` they are six floats the declaration reads as
                // two texture coordinates, which is the class comment's whole subject.
                std::vector<VertexPositionNormalTexture> carrier(count);
                for (std::size_t i = 0; i < count; ++i)
                {
                    const std::uint8_t* v = chunk.vertices.data() + i * fileStride;
                    const Vector3 position(ReadFloat(v), ReadFloat(v + 4), ReadFloat(v + 8));
                    if (chunk.layout == ChunkLayout::Basic)
                    {
                        carrier[i] = VertexPositionNormalTexture(
                            position,
                            Vector3(ReadFloat(v + 12), ReadFloat(v + 16), ReadFloat(v + 20)),
                            Vector2(ReadFloat(v + 24), ReadFloat(v + 28)));
                    }
                    else
                    {
                        carrier[i] = VertexPositionNormalTexture(
                            position,
                            Vector3(ReadFloat(v + 12), ReadFloat(v + 16), ReadFloat(v + 20)),
                            Vector2(ReadFloat(v + 24), 0.0f));
                    }
                }
                resident.vertices->SetData(carrier.data(), static_cast<int>(count));
            }

            resident.indices = std::make_unique<IndexBuffer>(
                device_,
                chunk.wideIndices ? IndexElementSize::ThirtyTwoBits : IndexElementSize::SixteenBits,
                static_cast<int>(chunk.indexCount),
                BufferUsage::WriteOnly);
            if (chunk.wideIndices)
            {
                std::vector<std::uint32_t> values(chunk.indexCount);
                std::memcpy(values.data(), chunk.indices.data(), chunk.indices.size());
                resident.indices->SetData(values.data(), static_cast<int>(chunk.indexCount));
            }
            else
            {
                std::vector<std::uint16_t> values(chunk.indexCount);
                std::memcpy(values.data(), chunk.indices.data(), chunk.indices.size());
                resident.indices->SetData(values.data(), static_cast<int>(chunk.indexCount));
            }
        }
        catch (const std::exception& e)
        {
            // A device that refuses a buffer is not something a caller can retry around, but it
            // must say WHICH chunk: 488 of them and one message is a bug report nobody can act on.
            return Err(ErrorCode::Unknown,
                       std::format("chunk {} ({} vertices, {} indices) could not be uploaded: {}",
                                   index,
                                   chunk.vertexCount,
                                   chunk.indexCount,
                                   e.what()),
                       library_.cells.empty() ? std::string("chunks.bin") : library_.cells[chunk.cell]);
        }

        resident.bytes = static_cast<std::uint64_t>(count) * uploadStride +
                         static_cast<std::uint64_t>(chunk.indexCount) * (chunk.wideIndices ? 4u : 2u);
        return resident;
    }

    util::Result<void> CellRuntime::Load(std::string_view cell)
    {
        if (IsResident(cell))
        {
            return {};
        }
        if (library_.IndexOfCell(cell) == library_.cells.size())
        {
            return Err(ErrorCode::NotFound,
                       std::format("cell '{}' is not in this chunk file", cell),
                       std::string("chunks.bin"));
        }

        std::vector<ResidentChunk> uploaded;
        for (const std::uint32_t index : library_.ChunksOf(cell))
        {
            auto chunk = Upload(library_.chunks[index], index);
            if (!chunk)
            {
                // All or nothing: a half-loaded cell is a room with holes in it, and a hole is
                // harder to notice than a room that did not arrive.
                return chunk.Error();
            }
            uploaded.push_back(std::move(*chunk));
        }
        resident_.emplace(std::string(cell), std::move(uploaded));
        return {};
    }

    void CellRuntime::Unload(std::string_view cell)
    {
        const auto it = resident_.find(cell);
        if (it != resident_.end())
        {
            resident_.erase(it);
        }
    }

    void CellRuntime::UnloadAll()
    {
        resident_.clear();
    }

    bool CellRuntime::IsResident(std::string_view cell) const
    {
        return resident_.find(cell) != resident_.end();
    }

    const std::vector<CellRuntime::ResidentChunk>* CellRuntime::Chunks(std::string_view cell) const
    {
        const auto it = resident_.find(cell);
        return it == resident_.end() ? nullptr : &it->second;
    }

    std::size_t CellRuntime::ResidentChunks() const
    {
        std::size_t total = 0;
        for (const auto& [name, chunks] : resident_)
        {
            total += chunks.size();
        }
        return total;
    }

    std::uint64_t CellRuntime::ResidentBytes() const
    {
        std::uint64_t total = 0;
        for (const auto& [name, chunks] : resident_)
        {
            for (const ResidentChunk& chunk : chunks)
            {
                total += chunk.bytes;
            }
        }
        return total;
    }

} // namespace cnahouse::world
