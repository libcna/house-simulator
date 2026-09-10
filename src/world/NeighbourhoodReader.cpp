// SPDX-License-Identifier: MIT
#include "cnahouse/world/NeighbourhoodReader.hpp"

#include <algorithm>
#include <format>
#include <memory>
#include <unordered_set>

#include "Microsoft/Xna/Framework/TitleContainer.hpp"
#include "Microsoft/Xna/Framework/Vector3.hpp"
#include "System/IO/BinaryReader.hpp"
#include "System/IO/Stream.hpp"
#include "cnahouse/util/Ids.hpp"

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

        /// `u16` byte length then UTF-8, the same encoding `chunks.bin` uses and for the same
        /// reason: `BinaryReader::ReadString`'s 7-bit-encoded length is .NET-specific and the
        /// writer of this format is Python (`docs/neighbourhood-format.md` §4).
        util::Result<std::string>
        ReadName(System::IO::BinaryReader& reader, std::string_view file, bool mayBeEmpty = false)
        {
            const std::uint32_t length = reader.ReadUInt16();
            if (length == 0u && !mayBeEmpty)
            {
                return Bad(ErrorCode::InvalidData, "a name is empty", file);
            }
            if (length > NeighbourhoodReader::kMaxNameBytes)
            {
                return Bad(ErrorCode::InvalidData,
                           std::format("a name claims {} bytes, above the {} limit",
                                       length,
                                       NeighbourhoodReader::kMaxNameBytes),
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

        /// Min *xyz* then max *xyz*, in the order `build_neighbourhood.py` writes them.
        util::Result<BoundingBox>
        ReadBounds(System::IO::BinaryReader& reader, std::string_view what, std::string_view file)
        {
            const float minX = reader.ReadSingle();
            const float minY = reader.ReadSingle();
            const float minZ = reader.ReadSingle();
            const float maxX = reader.ReadSingle();
            const float maxY = reader.ReadSingle();
            const float maxZ = reader.ReadSingle();
            // `!(min <= max)`, so a NaN is rejected too: a NaN compares false against everything
            // and would walk through the other spelling, leaving a box that culls either the whole
            // street or none of it depending on which way the test runs.
            if (!(minX <= maxX) || !(minY <= maxY) || !(minZ <= maxZ))
            {
                return Bad(ErrorCode::InvalidData,
                           std::format("{} has an inverted or non-finite bounding box", what),
                           file);
            }
            return BoundingBox(Vector3(minX, minY, minZ), Vector3(maxX, maxY, maxZ));
        }

        util::Result<NeighbourPrimitive> ReadPrimitive(System::IO::BinaryReader& reader,
                                                       std::string_view asset,
                                                       std::uint32_t index,
                                                       std::size_t materialCount,
                                                       std::string_view file)
        {
            NeighbourPrimitive primitive;
            primitive.material = reader.ReadUInt16();
            if (primitive.material >= materialCount)
            {
                return Bad(ErrorCode::OutOfRange,
                           std::format("'{}' primitive {} names material {} of {}",
                                       asset,
                                       index,
                                       primitive.material,
                                       materialCount),
                           file);
            }

            const std::uint8_t layout = reader.ReadByte();
            if (layout > static_cast<std::uint8_t>(ChunkLayout::AlphaTest))
            {
                // A newer writer added a layout. Guessing at the stride would misread every byte
                // after this point, so it stops here and says which primitive.
                return Bad(ErrorCode::VersionMismatch,
                           std::format("'{}' primitive {} uses vertex layout {}, which this build "
                                       "does not know",
                                       asset,
                                       index,
                                       layout),
                           file);
            }
            primitive.layout = static_cast<ChunkLayout>(layout);

            const std::uint8_t wide = reader.ReadByte();
            if (wide > 1u)
            {
                return Bad(ErrorCode::InvalidData,
                           std::format("'{}' primitive {} has wideIndices {}, which is neither 0 "
                                       "nor 1",
                                       asset,
                                       index,
                                       wide),
                           file);
            }
            primitive.wideIndices = wide != 0u;

            auto bounds = ReadBounds(reader, std::format("'{}' primitive {}", asset, index), file);
            if (!bounds)
            {
                return bounds.Error();
            }
            primitive.bounds = *bounds;

            primitive.vertexCount = reader.ReadUInt32();
            if (primitive.vertexCount == 0u ||
                primitive.vertexCount > NeighbourhoodReader::kMaxVerticesPerPrimitive)
            {
                return Bad(ErrorCode::OutOfRange,
                           std::format("'{}' primitive {} has {} vertices, outside 1..{}",
                                       asset,
                                       index,
                                       primitive.vertexCount,
                                       NeighbourhoodReader::kMaxVerticesPerPrimitive),
                           file);
            }
            if (!primitive.wideIndices && primitive.vertexCount > NeighbourhoodReader::kMaxVertices16Bit + 1u)
            {
                return Bad(ErrorCode::InvalidData,
                           std::format("'{}' primitive {} has {} vertices and 16-bit indices, "
                                       "which cannot address them",
                                       asset,
                                       index,
                                       primitive.vertexCount),
                           file);
            }

            const std::size_t vertexBytes =
                static_cast<std::size_t>(primitive.vertexCount) * ChunkVertexStride(primitive.layout);
            primitive.vertices = reader.ReadBytes(static_cast<int>(vertexBytes));
            if (primitive.vertices.size() != vertexBytes)
            {
                return Bad(ErrorCode::InvalidData,
                           std::format("'{}' primitive {}'s vertices were cut short: {} of {} bytes",
                                       asset,
                                       index,
                                       primitive.vertices.size(),
                                       vertexBytes),
                           file);
            }

            primitive.indexCount = reader.ReadUInt32();
            if (primitive.indexCount == 0u ||
                primitive.indexCount > NeighbourhoodReader::kMaxIndicesPerPrimitive)
            {
                return Bad(ErrorCode::OutOfRange,
                           std::format("'{}' primitive {} has {} indices, outside 1..{}",
                                       asset,
                                       index,
                                       primitive.indexCount,
                                       NeighbourhoodReader::kMaxIndicesPerPrimitive),
                           file);
            }
            if (primitive.indexCount % 3u != 0u)
            {
                return Bad(ErrorCode::InvalidData,
                           std::format("'{}' primitive {} has {} indices, which is not a whole "
                                       "number of triangles",
                                       asset,
                                       index,
                                       primitive.indexCount),
                           file);
            }
            const std::size_t indexBytes =
                static_cast<std::size_t>(primitive.indexCount) * (primitive.wideIndices ? 4u : 2u);
            primitive.indices = reader.ReadBytes(static_cast<int>(indexBytes));
            if (primitive.indices.size() != indexBytes)
            {
                return Bad(ErrorCode::InvalidData,
                           std::format("'{}' primitive {}'s indices were cut short: {} of {} bytes",
                                       asset,
                                       index,
                                       primitive.indices.size(),
                                       indexBytes),
                           file);
            }
            return primitive;
        }

    } // namespace

    std::uint64_t NeighbourAsset::GeometryBytes() const
    {
        std::uint64_t total = 0;
        for (const NeighbourPrimitive& primitive : primitives)
        {
            total += primitive.vertices.size() + primitive.indices.size();
        }
        return total;
    }

    const NeighbourAsset* NeighbourhoodLibrary::Find(std::string_view id) const
    {
        const auto found = std::lower_bound(assets.begin(),
                                            assets.end(),
                                            id,
                                            [](const NeighbourAsset& asset, std::string_view name)
                                            { return asset.asset < name; });
        if (found == assets.end() || found->asset != id)
        {
            return nullptr;
        }
        return &*found;
    }

    std::uint64_t NeighbourhoodLibrary::GeometryBytes() const
    {
        std::uint64_t total = 0;
        for (const NeighbourAsset& asset : assets)
        {
            total += asset.GeometryBytes();
        }
        return total;
    }

    std::vector<std::string> UnresolvedAssets(const NeighbourhoodLibrary& library,
                                              const std::vector<NeighbourBuilding>& rows)
    {
        // The rows carry hashed ids and the file carries names, so the comparison happens on the
        // hash: `Id::Of` rather than `Intern`, because interning a name here would register it and
        // this function is a question, not a change.
        std::unordered_set<std::uint32_t> held;
        held.reserve(library.assets.size());
        for (const NeighbourAsset& asset : library.assets)
        {
            held.insert(util::Id::Of(asset.asset).Value());
        }

        std::vector<std::string> out;
        for (const NeighbourBuilding& row : rows)
        {
            if (held.contains(row.asset.Value()))
            {
                continue;
            }
            const std::string_view name = util::IdRegistry::NameOf(row.asset);
            out.push_back(name.empty() ? std::format("id:{:#010x}", row.asset.Value()) : std::string(name));
        }
        std::sort(out.begin(), out.end());
        out.erase(std::unique(out.begin(), out.end()), out.end());
        return out;
    }

    util::Result<NeighbourhoodLibrary> NeighbourhoodReader::Read(System::IO::Stream& stream,
                                                                 std::string_view name)
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
                           std::format("magic is {:#010x}, not 'CNBH' ({:#010x})", magic, kMagic),
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

            NeighbourhoodLibrary library;
            // The one name that may be empty: a world built before `deploy_world.py` has run
            // carries no hash, which means the staleness check cannot run -- a different thing
            // from the file being corrupt.
            auto worldHash = ReadName(reader, name, true);
            if (!worldHash)
            {
                return worldHash.Error();
            }
            library.worldHash = std::move(*worldHash);

            const std::uint32_t materialCount = reader.ReadUInt32();
            if (materialCount > kMaxTableEntries)
            {
                return Bad(ErrorCode::OutOfRange,
                           std::format("materialCount is {}, above the {} a u16 index can address",
                                       materialCount,
                                       kMaxTableEntries),
                           name);
            }
            library.materials.reserve(materialCount);
            std::unordered_set<std::string> seenMaterials;
            for (std::uint32_t i = 0; i < materialCount; ++i)
            {
                auto material = ReadName(reader, name);
                if (!material)
                {
                    return material.Error();
                }
                if (!seenMaterials.insert(*material).second)
                {
                    // The table IS the naming: a repeat makes two indices mean the same material,
                    // and a primitive naming the second is indistinguishable from one naming the
                    // first.
                    return Bad(ErrorCode::Duplicate,
                               std::format("the material table repeats '{}'", *material),
                               name);
                }
                library.materials.push_back(std::move(*material));
            }

            const std::uint32_t assetCount = reader.ReadUInt32();
            if (assetCount == 0u || assetCount > kMaxAssets)
            {
                // Zero is not "an empty neighbourhood": §11.4 has one, and a file that holds none
                // is a build that produced nothing, which is exactly what went unnoticed for the
                // fifteen tasks before `HOUSE-00856`.
                return Bad(ErrorCode::OutOfRange,
                           std::format("assetCount is {}, outside 1..{}", assetCount, kMaxAssets),
                           name);
            }
            library.assets.reserve(assetCount);
            for (std::uint32_t i = 0; i < assetCount; ++i)
            {
                NeighbourAsset asset;
                auto id = ReadName(reader, name);
                if (!id)
                {
                    return id.Error();
                }
                asset.asset = std::move(*id);
                // Ascending order is the format's, and `Find` is a binary search over it. Checking
                // it here also catches a duplicate, which would otherwise be a mesh nothing can
                // reach.
                if (!library.assets.empty() && !(library.assets.back().asset < asset.asset))
                {
                    return Bad(ErrorCode::InvalidData,
                               std::format("asset '{}' follows '{}', so the table is not in "
                                           "ascending id order",
                                           asset.asset,
                                           library.assets.back().asset),
                               name);
                }

                auto bounds = ReadBounds(reader, std::format("asset '{}'", asset.asset), name);
                if (!bounds)
                {
                    return bounds.Error();
                }
                asset.bounds = *bounds;

                const std::uint32_t primitiveCount = reader.ReadUInt32();
                if (primitiveCount == 0u || primitiveCount > kMaxPrimitivesPerAsset)
                {
                    return Bad(ErrorCode::OutOfRange,
                               std::format("asset '{}' has {} primitives, outside 1..{}",
                                           asset.asset,
                                           primitiveCount,
                                           kMaxPrimitivesPerAsset),
                               name);
                }
                asset.primitives.reserve(primitiveCount);
                for (std::uint32_t p = 0; p < primitiveCount; ++p)
                {
                    auto primitive = ReadPrimitive(reader, asset.asset, p, library.materials.size(), name);
                    if (!primitive)
                    {
                        return primitive.Error();
                    }
                    asset.primitives.push_back(std::move(*primitive));
                }
                library.assets.push_back(std::move(asset));
            }

            return library;
        }
        catch (const std::exception& e)
        {
            // `BinaryReader` throws at the end of the stream, so TRUNCATION arrives here rather
            // than as a short read. It is the one failure the field-by-field checks cannot see.
            return Bad(ErrorCode::InvalidData,
                       std::format("the file ended early or could not be read: {}", e.what()),
                       name);
        }
    }

    util::Result<NeighbourhoodLibrary> NeighbourhoodReader::ReadFromTitle(std::string_view contentPath)
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
