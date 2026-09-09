// SPDX-License-Identifier: MIT
#include "cnahouse/physics/CollisionLoader.hpp"

#include <algorithm>
#include <cmath>
#include <format>
#include <memory>
#include <unordered_set>

#include "Microsoft/Xna/Framework/TitleContainer.hpp"
#include "System/IO/BinaryReader.hpp"
#include "System/IO/Stream.hpp"

namespace cnahouse::physics
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

        /// `u16` byte length then UTF-8, and NOT `BinaryReader::ReadString`, whose 7-bit-encoded
        /// prefix is a .NET encoding hostile to any other writer -- and this one is Python.
        util::Result<std::string>
        ReadName(System::IO::BinaryReader& reader, std::string_view file, bool mayBeEmpty = false)
        {
            const std::uint32_t length = reader.ReadUInt16();
            if (length == 0u)
            {
                if (mayBeEmpty)
                {
                    return std::string();
                }
                return Bad(ErrorCode::InvalidData, "a name is empty", file);
            }
            if (length > CollisionLoader::kMaxNameBytes)
            {
                return Bad(ErrorCode::InvalidData,
                           std::format("a name claims {} bytes, above the {} limit",
                                       length,
                                       CollisionLoader::kMaxNameBytes),
                           file);
            }
            const std::vector<std::uint8_t> bytes = reader.ReadBytes(static_cast<int>(length));
            if (bytes.size() != length)
            {
                return Bad(ErrorCode::InvalidData,
                           std::format("a name was cut short: {} of {} bytes", bytes.size(), length),
                           file);
            }
            return std::string(reinterpret_cast<const char*>(bytes.data()), bytes.size());
        }

        Vector3 ReadVector3(System::IO::BinaryReader& reader)
        {
            const float x = reader.ReadSingle();
            const float y = reader.ReadSingle();
            const float z = reader.ReadSingle();
            return Vector3(x, y, z);
        }

        util::Result<BoundingBox>
        ReadBounds(System::IO::BinaryReader& reader, std::string_view what, std::string_view file)
        {
            const Vector3 low = ReadVector3(reader);
            const Vector3 high = ReadVector3(reader);
            // `!(min <= max)` rather than `min > max`, so a NaN is rejected too: a NaN compares
            // false against everything and would walk straight through the other spelling, leaving
            // a box that rejects every sweep or accepts every one depending which way the test runs.
            if (!(low.X <= high.X) || !(low.Y <= high.Y) || !(low.Z <= high.Z))
            {
                return Bad(ErrorCode::InvalidData,
                           std::format("{} has an inverted or non-finite bounding box", what),
                           file);
            }
            return BoundingBox(low, high);
        }

        util::Result<CollisionKind>
        ReadKind(System::IO::BinaryReader& reader, std::string_view what, std::string_view file)
        {
            const std::uint8_t raw = reader.ReadByte();
            if (raw > static_cast<std::uint8_t>(CollisionKind::Exterior))
            {
                // A newer writer added a kind. Guessing would put a wall's behaviour on something
                // that is not one, and the sweep would be right about geometry and wrong about
                // what it hit.
                return Bad(ErrorCode::VersionMismatch,
                           std::format("{} has kind {}, which this build does not know; the known "
                                       "kinds are 0 floor, 1 ceiling, 2 wall, 3 stair, 4 prop, "
                                       "5 exterior",
                                       what,
                                       raw),
                           file);
            }
            return static_cast<CollisionKind>(raw);
        }

    } // namespace

    const std::vector<std::uint16_t>* CollisionCell::BucketAt(float x, float z) const
    {
        if (nx == 0u || nz == 0u || buckets.empty())
        {
            return nullptr;
        }
        // Truncation towards zero would fold the whole strip left of the origin onto column 0, so
        // a point outside the grid is refused rather than clamped: the caller asked about a place
        // this cell's grid says nothing about.
        const float column = x - originX;
        const float row = z - originZ;
        if (column < 0.0f || row < 0.0f)
        {
            return nullptr;
        }
        const auto i = static_cast<std::uint32_t>(column);
        const auto j = static_cast<std::uint32_t>(row);
        if (i >= nx || j >= nz)
        {
            return nullptr;
        }
        return &buckets[static_cast<std::size_t>(j) * nx + i];
    }

    const CollisionCell* CollisionWorld::Cell(std::string_view id) const
    {
        for (const CollisionCell& cell : cells)
        {
            if (cell.id == id)
            {
                return &cell;
            }
        }
        return nullptr;
    }

    std::string_view CollisionWorld::SurfaceName(std::uint16_t index) const
    {
        return index < surfaces.size() ? std::string_view(surfaces[index]) : std::string_view();
    }

    float CollisionTerrain::Height(std::uint32_t ix, std::uint32_t iz) const
    {
        if (heights.empty())
        {
            return 0.0f;
        }
        // Clamped, not wrapped and not refused. A body that walks off the far end of the lot
        // stands on the edge of it; §64's playable-volume boundary is what stops it going there,
        // and this is not the place to argue with it.
        const std::uint32_t x = std::min(ix, samplesX - 1u);
        const std::uint32_t z = std::min(iz, samplesZ - 1u);
        return heights[static_cast<std::size_t>(z) * samplesX + x];
    }

    std::uint16_t CollisionTerrain::Material(std::uint32_t ix, std::uint32_t iz) const
    {
        if (materialIndex.empty() || materials.empty())
        {
            return 0u;
        }
        const std::uint32_t x = std::min(ix, samplesX - 1u);
        const std::uint32_t z = std::min(iz, samplesZ - 1u);
        return materials[materialIndex[static_cast<std::size_t>(z) * samplesX + x]];
    }

    std::size_t CollisionWorld::TriangleCount() const
    {
        std::size_t total = 0;
        for (const CollisionMesh& mesh : meshes)
        {
            total += mesh.TriangleCount();
        }
        return total;
    }

    util::Result<CollisionWorld> CollisionLoader::Read(System::IO::Stream& stream, std::string_view name)
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
                           std::format("magic is {:#010x}, not 'CCOL' ({:#010x})", magic, kMagic),
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

            CollisionWorld world;
            // The one name that may be empty: `deploy_world.py` stamps the hash and a world built
            // before it has been deployed carries none, which is a staleness check that cannot run
            // rather than a corrupt file.
            auto worldHash = ReadName(reader, name, true);
            if (!worldHash)
            {
                return worldHash.Error();
            }
            world.worldHash = std::move(*worldHash);

            world.gridCell = reader.ReadSingle();
            if (!(world.gridCell > 0.0f))
            {
                // `!(x > 0)` so a NaN is caught too. Every bucket index divides by this.
                return Bad(ErrorCode::InvalidData,
                           std::format("the grid cell is {}, which cannot index anything",
                                       static_cast<double>(world.gridCell)),
                           name);
            }

            const std::uint32_t surfaceCount = reader.ReadUInt32();
            if (surfaceCount > kMaxSurfaces)
            {
                return Bad(ErrorCode::OutOfRange,
                           std::format("surfaceCount is {}, above the {} a u16 index addresses",
                                       surfaceCount,
                                       kMaxSurfaces),
                           name);
            }
            world.surfaces.reserve(surfaceCount);
            for (std::uint32_t i = 0; i < surfaceCount; ++i)
            {
                auto surface = ReadName(reader, name);
                if (!surface)
                {
                    return surface.Error();
                }
                world.surfaces.push_back(std::move(*surface));
            }

            const std::uint32_t obbCount = reader.ReadUInt32();
            if (obbCount > kMaxObbs)
            {
                return Bad(ErrorCode::OutOfRange,
                           std::format("obbCount is {}, above the {} limit", obbCount, kMaxObbs),
                           name);
            }
            world.obbs.reserve(obbCount);
            for (std::uint32_t i = 0; i < obbCount; ++i)
            {
                CollisionObb obb;
                obb.centre = ReadVector3(reader);
                obb.halfExtents = ReadVector3(reader);
                obb.yaw = reader.ReadSingle();
                obb.surface = reader.ReadUInt16();
                auto kind = ReadKind(reader, std::format("OBB {}", i), name);
                if (!kind)
                {
                    return kind.Error();
                }
                obb.kind = *kind;
                if (obb.surface >= world.surfaces.size())
                {
                    return Bad(
                        ErrorCode::OutOfRange,
                        std::format("OBB {} names surface {} of {}", i, obb.surface, world.surfaces.size()),
                        name);
                }
                // A negative or non-finite half-extent is a box that contains nothing and rejects
                // every sweep, which reads in the game as a wall that is not there.
                if (!(obb.halfExtents.X >= 0.0f) || !(obb.halfExtents.Y >= 0.0f) ||
                    !(obb.halfExtents.Z >= 0.0f))
                {
                    return Bad(ErrorCode::InvalidData,
                               std::format("OBB {} has a negative or non-finite half-extent", i),
                               name);
                }
                world.obbs.push_back(obb);
            }

            const std::uint32_t meshCount = reader.ReadUInt32();
            if (meshCount > kMaxMeshes)
            {
                return Bad(ErrorCode::OutOfRange,
                           std::format("meshCount is {}, above the {} limit", meshCount, kMaxMeshes),
                           name);
            }
            world.meshes.reserve(meshCount);
            for (std::uint32_t i = 0; i < meshCount; ++i)
            {
                CollisionMesh mesh;
                mesh.surface = reader.ReadUInt16();
                auto kind = ReadKind(reader, std::format("mesh {}", i), name);
                if (!kind)
                {
                    return kind.Error();
                }
                mesh.kind = *kind;
                if (mesh.surface >= world.surfaces.size())
                {
                    return Bad(
                        ErrorCode::OutOfRange,
                        std::format("mesh {} names surface {} of {}", i, mesh.surface, world.surfaces.size()),
                        name);
                }
                auto bounds = ReadBounds(reader, std::format("mesh {}", i), name);
                if (!bounds)
                {
                    return bounds.Error();
                }
                mesh.bounds = *bounds;

                const std::uint32_t vertexCount = reader.ReadUInt32();
                if (vertexCount == 0u || vertexCount > kMaxMeshVertices)
                {
                    return Bad(
                        ErrorCode::OutOfRange,
                        std::format(
                            "mesh {} has {} vertices, outside 1..{}", i, vertexCount, kMaxMeshVertices),
                        name);
                }
                mesh.vertices.reserve(vertexCount);
                for (std::uint32_t v = 0; v < vertexCount; ++v)
                {
                    mesh.vertices.push_back(ReadVector3(reader));
                }

                const std::uint32_t triangleCount = reader.ReadUInt32();
                if (triangleCount == 0u || triangleCount > kMaxTrianglesPerMesh)
                {
                    return Bad(ErrorCode::OutOfRange,
                               std::format("mesh {} has {} triangles, outside 1..{}",
                                           i,
                                           triangleCount,
                                           kMaxTrianglesPerMesh),
                               name);
                }
                mesh.indices.reserve(static_cast<std::size_t>(triangleCount) * 3);
                for (std::uint32_t t = 0; t < triangleCount * 3u; ++t)
                {
                    const std::uint16_t index = reader.ReadUInt16();
                    if (index >= vertexCount)
                    {
                        // Out of range here is a read past the end of the vertex array in the
                        // sweep -- a crash, or worse, someone else's floats read as a triangle.
                        return Bad(ErrorCode::OutOfRange,
                                   std::format("mesh {} references vertex {} of {}", i, index, vertexCount),
                                   name);
                    }
                    mesh.indices.push_back(index);
                }
                world.meshes.push_back(std::move(mesh));
            }

            const std::uint32_t cellCount = reader.ReadUInt32();
            if (cellCount > kMaxCells)
            {
                return Bad(ErrorCode::OutOfRange,
                           std::format("cellCount is {}, above the {} limit", cellCount, kMaxCells),
                           name);
            }
            const std::size_t shapeTotal = world.ShapeCount();
            world.cells.reserve(cellCount);
            std::unordered_set<std::string> seen;
            for (std::uint32_t c = 0; c < cellCount; ++c)
            {
                CollisionCell cell;
                auto id = ReadName(reader, name);
                if (!id)
                {
                    return id.Error();
                }
                cell.id = std::move(*id);
                if (!seen.insert(cell.id).second)
                {
                    // A cell id is what a caller looks a cell up by; two of them make the answer
                    // depend on which the search happens to reach first.
                    return Bad(
                        ErrorCode::Duplicate, std::format("cell '{}' appears more than once", cell.id), name);
                }
                // §49.2's ground is a property of the CELL, not of the world: the height field
                // runs under the house as well as over the lawn, and a body on the basement stair
                // is a tenth of a metre from it (`HOUSE-00774`).
                const std::uint8_t outdoors = reader.ReadByte();
                if (outdoors > 1u)
                {
                    return Bad(
                        ErrorCode::InvalidData,
                        std::format("cell '{}' says outdoors is {}, which is not 0 or 1", cell.id, outdoors),
                        name);
                }
                cell.outdoors = outdoors != 0u;
                auto bounds = ReadBounds(reader, std::format("cell '{}'", cell.id), name);
                if (!bounds)
                {
                    return bounds.Error();
                }
                cell.bounds = *bounds;

                const std::uint32_t shapeCount = reader.ReadUInt32();
                if (shapeCount > kMaxShapesPerCell)
                {
                    return Bad(ErrorCode::OutOfRange,
                               std::format("cell '{}' lists {} shapes; the format's u16 bucket "
                                           "indices address {}",
                                           cell.id,
                                           shapeCount,
                                           kMaxShapesPerCell),
                               name);
                }
                cell.shapes.reserve(shapeCount);
                for (std::uint32_t s = 0; s < shapeCount; ++s)
                {
                    const std::uint32_t index = reader.ReadUInt32();
                    if (index >= shapeTotal)
                    {
                        return Bad(
                            ErrorCode::OutOfRange,
                            std::format("cell '{}' references shape {} of {}", cell.id, index, shapeTotal),
                            name);
                    }
                    cell.shapes.push_back(index);
                }

                cell.nx = reader.ReadUInt32();
                cell.nz = reader.ReadUInt32();
                if (cell.nx == 0u || cell.nz == 0u)
                {
                    return Bad(ErrorCode::InvalidData,
                               std::format("cell '{}' has a {} x {} grid", cell.id, cell.nx, cell.nz),
                               name);
                }
                const std::uint64_t bucketCount =
                    static_cast<std::uint64_t>(cell.nx) * static_cast<std::uint64_t>(cell.nz);
                if (bucketCount > kMaxBucketsPerCell)
                {
                    // A cell's grid is sized from the union of its own shapes, so one shape that
                    // escaped its cell sizes it. `build_collision.py` refuses to WRITE such a file
                    // for the same reason; this refuses to allocate for one.
                    return Bad(ErrorCode::OutOfRange,
                               std::format("cell '{}' has a {} x {} = {} bucket grid, above the {} "
                                           "limit; a shape has escaped its cell",
                                           cell.id,
                                           cell.nx,
                                           cell.nz,
                                           bucketCount,
                                           kMaxBucketsPerCell),
                               name);
                }
                cell.originX = reader.ReadSingle();
                cell.originZ = reader.ReadSingle();

                cell.buckets.resize(static_cast<std::size_t>(bucketCount));
                for (std::size_t b = 0; b < cell.buckets.size(); ++b)
                {
                    const std::uint32_t count = reader.ReadUInt16();
                    if (count > cell.shapes.size())
                    {
                        return Bad(ErrorCode::OutOfRange,
                                   std::format("cell '{}' bucket {} lists {} entries over {} shapes",
                                               cell.id,
                                               b,
                                               count,
                                               cell.shapes.size()),
                                   name);
                    }
                    cell.buckets[b].reserve(count);
                    for (std::uint32_t e = 0; e < count; ++e)
                    {
                        const std::uint16_t local = reader.ReadUInt16();
                        if (local >= cell.shapes.size())
                        {
                            // Bucket entries are LOCAL indices (§3.4). One that is not is a sweep
                            // testing a shape belonging to another cell, or to nothing.
                            return Bad(ErrorCode::OutOfRange,
                                       std::format("cell '{}' bucket {} references local shape {} "
                                                   "of {}",
                                                   cell.id,
                                                   b,
                                                   local,
                                                   cell.shapes.size()),
                                       name);
                        }
                        cell.buckets[b].push_back(local);
                    }
                }
                world.cells.push_back(std::move(cell));
            }

            // §3.5, the ground (`HOUSE-00553`). A `0` here is a world with no exterior -- the
            // round-trip fixture is one -- and must read as "no ground" rather than as an error.
            const std::uint8_t hasTerrain = reader.ReadByte();
            if (hasTerrain > 1u)
            {
                return Bad(ErrorCode::InvalidData,
                           std::format("hasTerrain is {}, which is neither 0 nor 1", hasTerrain),
                           name);
            }
            if (hasTerrain == 1u)
            {
                CollisionTerrain& terrain = world.terrain;
                terrain.present = true;
                terrain.samplesX = reader.ReadUInt32();
                terrain.samplesZ = reader.ReadUInt32();
                if (terrain.samplesX < 2u || terrain.samplesZ < 2u)
                {
                    // One sample in an axis is not a surface: bilinear needs a square to
                    // interpolate over, and a field a body can stand on needs at least one.
                    return Bad(ErrorCode::InvalidData,
                               std::format("the terrain is {} x {} samples; two are needed on each "
                                           "axis to make one square",
                                           terrain.samplesX,
                                           terrain.samplesZ),
                               name);
                }
                const std::uint64_t samples = static_cast<std::uint64_t>(terrain.samplesX) * terrain.samplesZ;
                if (samples > kMaxTerrainSamples)
                {
                    return Bad(ErrorCode::InvalidData,
                               std::format("the terrain holds {} samples, above the {} limit",
                                           samples,
                                           kMaxTerrainSamples),
                               name);
                }
                terrain.originX = reader.ReadSingle();
                terrain.originZ = reader.ReadSingle();
                terrain.step = reader.ReadSingle();
                if (!(terrain.step > 0.0f) || !std::isfinite(terrain.step) ||
                    !std::isfinite(terrain.originX) || !std::isfinite(terrain.originZ))
                {
                    // A step of zero divides by nothing on every sample lookup, and a NaN origin
                    // puts the whole field nowhere.
                    return Bad(ErrorCode::InvalidData,
                               std::format("the terrain's step is {} and its origin ({}, {})",
                                           terrain.step,
                                           terrain.originX,
                                           terrain.originZ),
                               name);
                }

                terrain.heights.resize(static_cast<std::size_t>(samples));
                for (float& height : terrain.heights)
                {
                    height = reader.ReadSingle();
                    if (!std::isfinite(height))
                    {
                        return Bad(ErrorCode::InvalidData, "a terrain height is not finite", name);
                    }
                }

                const std::uint32_t materialCount = reader.ReadUInt32();
                if (materialCount == 0u || materialCount > kMaxSurfaces)
                {
                    return Bad(ErrorCode::InvalidData,
                               std::format("the terrain names {} materials, outside 1..{}",
                                           materialCount,
                                           kMaxSurfaces),
                               name);
                }
                terrain.materials.reserve(materialCount);
                for (std::uint32_t m = 0; m < materialCount; ++m)
                {
                    const std::uint16_t surface = reader.ReadUInt16();
                    if (surface >= world.surfaces.size())
                    {
                        // A footstep would read a name that is not there. §3.5 resolves through
                        // §3.2's table precisely so that there is only one table to be wrong.
                        return Bad(ErrorCode::InvalidData,
                                   std::format("terrain material {} names surface {}, and there "
                                               "are {}",
                                               m,
                                               surface,
                                               world.surfaces.size()),
                                   name);
                    }
                    terrain.materials.push_back(surface);
                }

                terrain.materialIndex.resize(static_cast<std::size_t>(samples));
                for (std::uint8_t& index : terrain.materialIndex)
                {
                    index = reader.ReadByte();
                    if (index >= terrain.materials.size())
                    {
                        return Bad(ErrorCode::InvalidData,
                                   std::format("a terrain sample names material {}, and there are "
                                               "{}",
                                               index,
                                               terrain.materials.size()),
                                   name);
                    }
                }
            }

            return world;
        }
        catch (const std::exception& e)
        {
            // `BinaryReader` throws at the end of the stream, so TRUNCATION arrives here rather
            // than as a short read. It is the one failure the field-by-field checks cannot see,
            // and it must be an error rather than a world quietly missing its last few rooms.
            return Bad(ErrorCode::InvalidData,
                       std::format("the file ended early or could not be read: {}", e.what()),
                       name);
        }
    }

    util::Result<CollisionWorld> CollisionLoader::ReadFromTitle(std::string_view contentPath)
    {
        try
        {
            // `TitleContainer::OpenStream` is plain XNA 4.0 and is how every platform this project
            // targets opens a read-only asset -- including the Web build, which has no filesystem
            // to open a `FileStream` on.
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

} // namespace cnahouse::physics
