// SPDX-License-Identifier: MIT
#include "cnahouse/world/ChunkReader.hpp"

#include <format>
#include <memory>
#include <unordered_set>
#include <vector>

#include "Microsoft/Xna/Framework/TitleContainer.hpp"
#include "Microsoft/Xna/Framework/Vector3.hpp"
#include "System/IO/BinaryReader.hpp"
#include "System/IO/Stream.hpp"

namespace cnahouse::world
{
    namespace
    {
        using Microsoft::Xna::Framework::BoundingBox;
        using Microsoft::Xna::Framework::Vector3;
        using util::Err;
        using util::ErrorCode;

        /// Every rejection goes through here, so every one of them names the file.
        util::Error Bad(ErrorCode code, std::string message, std::string_view name)
        {
            return Err(code, std::move(message), std::string(name));
        }

        /// `u16` byte length then UTF-8. NOT `BinaryReader::ReadString`, whose 7-bit-encoded length
        /// prefix is a .NET-specific encoding and hostile to any other writer of this format --
        /// and the writer of this one is Python (`docs/chunk-format.md` §2).
        util::Result<std::string>
        ReadName(System::IO::BinaryReader& reader, std::string_view file, bool mayBeEmpty = false)
        {
            const std::uint32_t length = reader.ReadUInt16();
            if (length == 0u && !mayBeEmpty)
            {
                return Bad(ErrorCode::InvalidData, "a name is empty", file);
            }
            if (length > ChunkReader::kMaxNameBytes)
            {
                return Bad(ErrorCode::InvalidData,
                           std::format("a name claims {} bytes, above the {} limit",
                                       length,
                                       ChunkReader::kMaxNameBytes),
                           file);
            }
            const std::vector<std::uint8_t> bytes = reader.ReadBytes(static_cast<int>(length));
            if (bytes.size() != length)
            {
                return Bad(ErrorCode::InvalidData,
                           std::format("a name was cut short: {} of {} bytes", bytes.size(), length),
                           file);
            }
            if (bytes.empty())
            {
                return std::string();
            }
            return std::string(reinterpret_cast<const char*>(bytes.data()), bytes.size());
        }

        /// Min *xyz* then max *xyz*, six floats, in the order `build_chunks.py` writes them.
        util::Result<BoundingBox>
        ReadBounds(System::IO::BinaryReader& reader, std::string_view what, std::string_view file)
        {
            const float minX = reader.ReadSingle();
            const float minY = reader.ReadSingle();
            const float minZ = reader.ReadSingle();
            const float maxX = reader.ReadSingle();
            const float maxY = reader.ReadSingle();
            const float maxZ = reader.ReadSingle();
            // `!(min <= max)` rather than `min > max`, so a NaN is rejected too: a NaN compares
            // false against everything and would walk straight through the other spelling, leaving
            // a box that culls either everything or nothing depending on which way the test runs.
            if (!(minX <= maxX) || !(minY <= maxY) || !(minZ <= maxZ))
            {
                return Bad(ErrorCode::InvalidData,
                           std::format("{} has an inverted or non-finite bounding box", what),
                           file);
            }
            return BoundingBox(Vector3(minX, minY, minZ), Vector3(maxX, maxY, maxZ));
        }

        util::Result<std::vector<std::string>>
        ReadTable(System::IO::BinaryReader& reader, std::string_view what, std::string_view file)
        {
            const std::uint32_t count = reader.ReadUInt32();
            if (count > ChunkReader::kMaxTableEntries)
            {
                return Bad(ErrorCode::OutOfRange,
                           std::format("{}Count is {}, above the {} a u16 index can address",
                                       what,
                                       count,
                                       ChunkReader::kMaxTableEntries),
                           file);
            }
            std::vector<std::string> table;
            table.reserve(count);
            std::unordered_set<std::string> seen;
            for (std::uint32_t i = 0; i < count; ++i)
            {
                auto name = ReadName(reader, file);
                if (!name)
                {
                    return name.Error();
                }
                if (!seen.insert(*name).second)
                {
                    // The table IS the naming: a repeat makes two different indices mean the same
                    // thing, and a chunk that named the second would be indistinguishable from one
                    // that named the first.
                    return Bad(
                        ErrorCode::Duplicate, std::format("the {} table repeats '{}'", what, *name), file);
                }
                table.push_back(std::move(*name));
            }
            return table;
        }

        util::Result<Chunk> ReadChunk(System::IO::BinaryReader& reader,
                                      std::uint32_t index,
                                      std::size_t cellCount,
                                      std::size_t materialCount,
                                      std::string_view file)
        {
            Chunk chunk;
            chunk.cell = reader.ReadUInt16();
            chunk.material = reader.ReadUInt16();
            if (chunk.cell >= cellCount || chunk.material >= materialCount)
            {
                return Bad(ErrorCode::OutOfRange,
                           std::format("chunk {} names cell {} of {} and material {} of {}",
                                       index,
                                       chunk.cell,
                                       cellCount,
                                       chunk.material,
                                       materialCount),
                           file);
            }

            const std::uint8_t layout = reader.ReadByte();
            if (layout > static_cast<std::uint8_t>(ChunkLayout::AlphaTest))
            {
                // A newer writer added a layout. Guessing at the stride would misread every byte
                // after this point, so it stops here and says which chunk.
                return Bad(ErrorCode::VersionMismatch,
                           std::format("chunk {} uses vertex layout {}, which this build does not "
                                       "know; the known layouts are 0 (basic), 1 (dual) and 2 "
                                       "(alphatest)",
                                       index,
                                       layout),
                           file);
            }
            chunk.layout = static_cast<ChunkLayout>(layout);

            const std::uint8_t wide = reader.ReadByte();
            if (wide > 1u)
            {
                return Bad(ErrorCode::InvalidData,
                           std::format("chunk {} has wideIndices {}, which is neither 0 nor 1", index, wide),
                           file);
            }
            chunk.wideIndices = wide != 0u;

            auto bounds = ReadBounds(reader, std::format("chunk {}", index), file);
            if (!bounds)
            {
                return bounds.Error();
            }
            chunk.bounds = *bounds;

            chunk.vertexCount = reader.ReadUInt32();
            if (chunk.vertexCount == 0u || chunk.vertexCount > ChunkReader::kMaxVerticesPerChunk)
            {
                return Bad(ErrorCode::OutOfRange,
                           std::format("chunk {} has {} vertices, outside 1..{}",
                                       index,
                                       chunk.vertexCount,
                                       ChunkReader::kMaxVerticesPerChunk),
                           file);
            }
            if (!chunk.wideIndices && chunk.vertexCount > ChunkReader::kMaxVertices16Bit + 1u)
            {
                // The writer's whole splitting rule exists to keep this true. A chunk that broke it
                // would draw with indices wrapping round to the start of its own buffer, which is
                // geometry that looks almost right.
                return Bad(ErrorCode::InvalidData,
                           std::format("chunk {} has {} vertices and 16-bit indices, which cannot "
                                       "address them",
                                       index,
                                       chunk.vertexCount),
                           file);
            }

            const std::uint32_t stride = ChunkVertexStride(chunk.layout);
            const std::size_t vertexBytes = static_cast<std::size_t>(chunk.vertexCount) * stride;
            chunk.vertices = reader.ReadBytes(static_cast<int>(vertexBytes));
            if (chunk.vertices.size() != vertexBytes)
            {
                return Bad(ErrorCode::InvalidData,
                           std::format("chunk {}'s vertices were cut short: {} of {} bytes",
                                       index,
                                       chunk.vertices.size(),
                                       vertexBytes),
                           file);
            }

            chunk.indexCount = reader.ReadUInt32();
            if (chunk.indexCount == 0u || chunk.indexCount > ChunkReader::kMaxIndicesPerChunk)
            {
                return Bad(ErrorCode::OutOfRange,
                           std::format("chunk {} has {} indices, outside 1..{}",
                                       index,
                                       chunk.indexCount,
                                       ChunkReader::kMaxIndicesPerChunk),
                           file);
            }
            if (chunk.indexCount % 3u != 0u)
            {
                return Bad(ErrorCode::InvalidData,
                           std::format("chunk {} has {} indices, which is not a whole number of "
                                       "triangles",
                                       index,
                                       chunk.indexCount),
                           file);
            }
            const std::size_t indexBytes =
                static_cast<std::size_t>(chunk.indexCount) * (chunk.wideIndices ? 4u : 2u);
            chunk.indices = reader.ReadBytes(static_cast<int>(indexBytes));
            if (chunk.indices.size() != indexBytes)
            {
                return Bad(ErrorCode::InvalidData,
                           std::format("chunk {}'s indices were cut short: {} of {} bytes",
                                       index,
                                       chunk.indices.size(),
                                       indexBytes),
                           file);
            }

            const std::uint32_t subRangeCount = reader.ReadUInt32();
            if (subRangeCount == 0u || subRangeCount > chunk.indexCount / 3u)
            {
                // Zero would mean geometry belonging to nothing, and more sub-ranges than triangles
                // would mean at least one of them is empty. Both are the writer having gone wrong.
                return Bad(ErrorCode::OutOfRange,
                           std::format("chunk {} has {} sub-ranges over {} triangles",
                                       index,
                                       subRangeCount,
                                       chunk.indexCount / 3u),
                           file);
            }
            chunk.subRanges.reserve(subRangeCount);
            std::uint32_t covered = 0u;
            for (std::uint32_t i = 0; i < subRangeCount; ++i)
            {
                ChunkSubRange range;
                auto source = ReadName(reader, file);
                if (!source)
                {
                    return source.Error();
                }
                range.source = std::move(*source);
                range.indexStart = reader.ReadUInt32();
                range.indexCount = reader.ReadUInt32();
                // §17.4's sub-range boxes "tile the chunk's index buffer exactly -- no gaps, no
                // overlaps". Checking it here is what lets a culling pass draw one sub-range
                // without asking whether the ranges around it overlap the same triangles.
                if (range.indexStart != covered)
                {
                    return Bad(ErrorCode::InvalidData,
                               std::format("chunk {}'s sub-range '{}' starts at {} where the one "
                                           "before it ended at {}; the ranges must tile the index "
                                           "buffer",
                                           index,
                                           range.source,
                                           range.indexStart,
                                           covered),
                               file);
                }
                if (range.indexCount == 0u || range.indexCount % 3u != 0u ||
                    covered + range.indexCount > chunk.indexCount)
                {
                    return Bad(ErrorCode::InvalidData,
                               std::format("chunk {}'s sub-range '{}' covers {} indices from {}, "
                                           "which is empty, not whole triangles, or past the "
                                           "buffer's {}",
                                           index,
                                           range.source,
                                           range.indexCount,
                                           range.indexStart,
                                           chunk.indexCount),
                               file);
                }
                covered += range.indexCount;
                auto box =
                    ReadBounds(reader, std::format("chunk {}'s sub-range '{}'", index, range.source), file);
                if (!box)
                {
                    return box.Error();
                }
                range.bounds = *box;
                chunk.subRanges.push_back(std::move(range));
            }
            if (covered != chunk.indexCount)
            {
                return Bad(
                    ErrorCode::InvalidData,
                    std::format(
                        "chunk {}'s sub-ranges cover {} of its {} indices", index, covered, chunk.indexCount),
                    file);
            }
            return chunk;
        }

    } // namespace

    std::size_t ChunkLibrary::IndexOfCell(std::string_view cell) const
    {
        for (std::size_t i = 0; i < cells.size(); ++i)
        {
            if (cells[i] == cell)
            {
                return i;
            }
        }
        return cells.size();
    }

    std::vector<std::uint32_t> ChunkLibrary::ChunksOf(std::string_view cell) const
    {
        std::vector<std::uint32_t> out;
        const std::size_t index = IndexOfCell(cell);
        if (index == cells.size())
        {
            return out;
        }
        for (std::uint32_t i = 0; i < chunks.size(); ++i)
        {
            if (chunks[i].cell == index)
            {
                out.push_back(i);
            }
        }
        return out;
    }

    std::uint64_t ChunkLibrary::GeometryBytes() const
    {
        std::uint64_t total = 0;
        for (const Chunk& chunk : chunks)
        {
            total += chunk.vertices.size() + chunk.indices.size();
        }
        return total;
    }

    util::Result<ChunkLibrary> ChunkReader::Read(System::IO::Stream& stream, std::string_view name)
    {
        try
        {
            System::IO::BinaryReader reader(&stream, true);

            const std::uint32_t magic = reader.ReadUInt32();
            if (magic != kMagic)
            {
                // Said first and said plainly: the wrong file entirely. Reading on would interpret
                // arbitrary bytes as lengths and produce a far less useful error.
                return Bad(ErrorCode::InvalidData,
                           std::format("magic is {:#010x}, not 'CCHK' ({:#010x})", magic, kMagic),
                           name);
            }

            const std::uint32_t version = reader.ReadUInt32();
            if (version != kVersion)
            {
                return Bad(ErrorCode::VersionMismatch,
                           std::format(
                               "version {} is not supported; this build reads version {}", version, kVersion),
                           name);
            }

            const std::uint32_t flags = reader.ReadUInt32();
            if (flags != 0u)
            {
                return Bad(ErrorCode::VersionMismatch,
                           std::format("reserved header flags {:#010x} are set", flags),
                           name);
            }

            ChunkLibrary library;
            // The one name that may be empty. `worldHash` is `deploy_world.py`'s stamp, and a
            // world built before it has been deployed carries none: the staleness check simply
            // cannot run then, which is a different thing from the file being corrupt. Every other
            // string in the format names something and an empty one would be a length field gone
            // wrong.
            auto worldHash = ReadName(reader, name, true);
            if (!worldHash)
            {
                return worldHash.Error();
            }
            library.worldHash = std::move(*worldHash);

            auto cells = ReadTable(reader, "cell", name);
            if (!cells)
            {
                return cells.Error();
            }
            library.cells = std::move(*cells);

            auto materials = ReadTable(reader, "material", name);
            if (!materials)
            {
                return materials.Error();
            }
            library.materials = std::move(*materials);

            const std::uint32_t chunkCount = reader.ReadUInt32();
            if (chunkCount > kMaxChunks)
            {
                return Bad(ErrorCode::OutOfRange,
                           std::format("chunkCount is {}, above the {} limit", chunkCount, kMaxChunks),
                           name);
            }
            library.chunks.reserve(chunkCount);
            for (std::uint32_t i = 0; i < chunkCount; ++i)
            {
                auto chunk = ReadChunk(reader, i, library.cells.size(), library.materials.size(), name);
                if (!chunk)
                {
                    return chunk.Error();
                }
                library.chunks.push_back(std::move(*chunk));
            }

            return library;
        }
        catch (const std::exception& e)
        {
            // `BinaryReader` throws at the end of the stream, so TRUNCATION arrives here rather
            // than as a short read. It is the one failure the field-by-field checks cannot see, and
            // it must be an error rather than a world quietly missing its last few chunks.
            return Bad(ErrorCode::InvalidData,
                       std::format("the file ended early or could not be read: {}", e.what()),
                       name);
        }
    }

    util::Result<ChunkLibrary> ChunkReader::ReadFromTitle(std::string_view contentPath)
    {
        try
        {
            // `TitleContainer::OpenStream` is plain XNA 4.0 and is how every platform this project
            // targets opens a read-only asset -- including the Web build, where there is no
            // filesystem to open a `FileStream` on.
            std::unique_ptr<System::IO::Stream> stream =
                Microsoft::Xna::Framework::TitleContainer::OpenStream(std::string(contentPath));
            if (stream == nullptr)
            {
                return Bad(ErrorCode::NotFound, "the file could not be opened", contentPath);
            }
            return Read(*stream, contentPath);
        }
        catch (const std::exception& e)
        {
            return Bad(ErrorCode::NotFound, e.what(), contentPath);
        }
    }

} // namespace cnahouse::world
