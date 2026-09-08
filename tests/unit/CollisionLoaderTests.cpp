// SPDX-License-Identifier: MIT
//
// `HOUSE-00541`. The reader of `docs/collision-format.md`, asked the questions a corrupt or
// mis-written file would answer wrongly. The bytes are built here field by field: a test that
// asked the writer for its own output would pass with both sides wrong in the same way, and
// `CollisionRoundTripTests` is the separate check that they agree.
//
// Most of these are not "does it parse" but "does it check what the format PROMISES". A bucket
// entry outside the cell's own shape list, a triangle outside its mesh's vertices, a shape index
// outside the world's shapes — each is a read past the end of an array in the sweep, and each is
// one comparison in the loader.
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "System/IO/MemoryStream.hpp"

#include "cnahouse/physics/CollisionLoader.hpp"

namespace
{
    using cnahouse::physics::CollisionKind;
    using cnahouse::physics::CollisionLoader;
    using cnahouse::physics::CollisionWorld;
    using cnahouse::util::ErrorCode;

    /// A little-endian builder, so every test below reads as the format table does.
    class Bytes
    {
    public:
        Bytes& U8(std::uint8_t value)
        {
            data_.push_back(value);
            return *this;
        }

        Bytes& U16(std::uint16_t value)
        {
            data_.push_back(static_cast<std::uint8_t>(value & 0xFFu));
            data_.push_back(static_cast<std::uint8_t>(value >> 8));
            return *this;
        }

        Bytes& U32(std::uint32_t value)
        {
            for (int shift = 0; shift < 32; shift += 8)
            {
                data_.push_back(static_cast<std::uint8_t>((value >> shift) & 0xFFu));
            }
            return *this;
        }

        Bytes& F32(float value)
        {
            std::uint32_t bits = 0;
            std::memcpy(&bits, &value, sizeof(bits));
            return U32(bits);
        }

        Bytes& Str(const std::string& text)
        {
            U16(static_cast<std::uint16_t>(text.size()));
            data_.insert(data_.end(), text.begin(), text.end());
            return *this;
        }

        Bytes& Vec(float x, float y, float z)
        {
            return F32(x).F32(y).F32(z);
        }

        Bytes& Box(float x0, float y0, float z0, float x1, float y1, float z1)
        {
            return Vec(x0, y0, z0).Vec(x1, y1, z1);
        }

        [[nodiscard]] const std::vector<std::uint8_t>& Data() const
        {
            return data_;
        }

        [[nodiscard]] std::size_t Size() const
        {
            return data_.size();
        }

        void Truncate(std::size_t size)
        {
            data_.resize(size);
        }

        void Poke(std::size_t offset, std::uint8_t value)
        {
            data_[offset] = value;
        }

    private:
        std::vector<std::uint8_t> data_;
    };

    /// Magic, version 1, no flags, a world hash, a 1 m grid, and one surface.
    Bytes Header()
    {
        Bytes bytes;
        bytes.U32(CollisionLoader::kMagic).U32(CollisionLoader::kVersion).U32(0u);
        bytes.Str("worldhash").F32(1.0f);
        bytes.U32(1u).Str("tile");
        return bytes;
    }

    /// One OBB: centre, half-extents, yaw, surface, kind.
    void OneObb(Bytes& bytes, std::uint16_t surface = 0u, std::uint8_t kind = 2u)
    {
        bytes.Vec(1.0f, 0.5f, 2.0f).Vec(0.25f, 0.5f, 1.5f).F32(0.5f).U16(surface).U8(kind);
    }

    /// One mesh: three vertices, one triangle.
    void OneMesh(Bytes& bytes, std::uint16_t surface = 0u, std::uint8_t kind = 3u)
    {
        bytes.U16(surface).U8(kind);
        bytes.Box(0.0f, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f);
        bytes.U32(3u).Vec(0.0f, 0.0f, 0.0f).Vec(1.0f, 0.0f, 0.0f).Vec(0.0f, 1.0f, 0.0f);
        bytes.U32(1u).U16(0u).U16(1u).U16(2u);
    }

    /// One cell over both shapes on a 1 x 2 grid: bucket 0 holds both, bucket 1 holds nothing.
    void OneCell(Bytes& bytes)
    {
        bytes.Str("L0_CELL");
        bytes.Box(0.0f, 0.0f, 0.0f, 2.0f, 2.0f, 2.0f);
        bytes.U32(2u).U32(0u).U32(1u);
        bytes.U32(1u).U32(2u);
        bytes.F32(0.0f).F32(0.0f);
        bytes.U16(2u).U16(0u).U16(1u);
        bytes.U16(0u);
    }

    Bytes WellFormed()
    {
        Bytes bytes = Header();
        bytes.U32(1u);
        OneObb(bytes);
        bytes.U32(1u);
        OneMesh(bytes);
        bytes.U32(1u);
        OneCell(bytes);
        return bytes;
    }

    cnahouse::util::Result<CollisionWorld> ReadOf(const Bytes& bytes)
    {
        System::IO::MemoryStream stream(bytes.Data().data(), static_cast<int>(bytes.Data().size()), false);
        return CollisionLoader::Read(stream, "test.bin");
    }

} // namespace

TEST(CollisionLoaderTests, AWellFormedFileReadsBack)
{
    const auto world = ReadOf(WellFormed());
    ASSERT_TRUE(world) << world.Error().Message();
    EXPECT_EQ(world->worldHash, "worldhash");
    EXPECT_FLOAT_EQ(world->gridCell, 1.0f);
    ASSERT_EQ(world->obbs.size(), 1u);
    ASSERT_EQ(world->meshes.size(), 1u);
    ASSERT_EQ(world->cells.size(), 1u);
    EXPECT_EQ(world->obbs[0].kind, CollisionKind::Wall);
    EXPECT_EQ(world->meshes[0].kind, CollisionKind::Stair);
    EXPECT_EQ(world->SurfaceName(0), "tile");
    EXPECT_EQ(world->ShapeCount(), 2u);
    EXPECT_EQ(world->TriangleCount(), 1u);
    EXPECT_EQ(world->cells[0].buckets.size(), 2u);
}

TEST(CollisionLoaderTests, TheWrongFileEntirelyIsSaidPlainly)
{
    Bytes bytes = WellFormed();
    bytes.Poke(0, 'X');
    const auto world = ReadOf(bytes);
    ASSERT_FALSE(world);
    EXPECT_EQ(world.Error().Code(), ErrorCode::InvalidData);
    EXPECT_NE(world.Error().Message().find("CCOL"), std::string::npos);
    EXPECT_EQ(world.Error().Context(), "test.bin");
}

TEST(CollisionLoaderTests, AVersionOrAFlagThisBuildDoesNotKnowIsRefused)
{
    Bytes version = WellFormed();
    version.Poke(4, 2u);
    auto world = ReadOf(version);
    ASSERT_FALSE(world);
    EXPECT_EQ(world.Error().Code(), ErrorCode::VersionMismatch);

    // Masking a reserved flag would drop whatever a newer writer meant by it and produce a world
    // that is subtly wrong forever, which is worse than not loading.
    Bytes flag = WellFormed();
    flag.Poke(8, 0x4u);
    world = ReadOf(flag);
    ASSERT_FALSE(world);
    EXPECT_EQ(world.Error().Code(), ErrorCode::VersionMismatch);
}

TEST(CollisionLoaderTests, AGridCellOfZeroIsRefused)
{
    // Every bucket index divides by this.
    Bytes bytes;
    bytes.U32(CollisionLoader::kMagic).U32(CollisionLoader::kVersion).U32(0u);
    bytes.Str("worldhash").F32(0.0f);
    bytes.U32(0u).U32(0u).U32(0u).U32(0u);
    const auto world = ReadOf(bytes);
    ASSERT_FALSE(world);
    EXPECT_EQ(world.Error().Code(), ErrorCode::InvalidData);
    EXPECT_NE(world.Error().Message().find("grid cell"), std::string::npos);
}

TEST(CollisionLoaderTests, AShapeNamingASurfaceThatIsNotThereIsRefused)
{
    Bytes obb = Header();
    obb.U32(1u);
    OneObb(obb, 7u);
    auto world = ReadOf(obb);
    ASSERT_FALSE(world);
    EXPECT_EQ(world.Error().Code(), ErrorCode::OutOfRange);
    EXPECT_NE(world.Error().Message().find("surface 7 of 1"), std::string::npos);

    Bytes mesh = Header();
    mesh.U32(0u).U32(1u);
    OneMesh(mesh, 7u);
    world = ReadOf(mesh);
    ASSERT_FALSE(world);
    EXPECT_EQ(world.Error().Code(), ErrorCode::OutOfRange);
}

TEST(CollisionLoaderTests, AKindThisBuildDoesNotKnowStopsTheRead)
{
    // Guessing would put a wall's behaviour on something that is not one, and the sweep would be
    // right about the geometry and wrong about what it hit.
    Bytes bytes = Header();
    bytes.U32(1u);
    OneObb(bytes, 0u, 9u);
    const auto world = ReadOf(bytes);
    ASSERT_FALSE(world);
    EXPECT_EQ(world.Error().Code(), ErrorCode::VersionMismatch);
    EXPECT_NE(world.Error().Message().find("kind 9"), std::string::npos);
}

TEST(CollisionLoaderTests, ANegativeHalfExtentIsRefused)
{
    // A box that contains nothing rejects every sweep, which reads in the game as a wall that is
    // not there rather than as a corrupt file.
    Bytes bytes = Header();
    bytes.U32(1u);
    bytes.Vec(1.0f, 0.5f, 2.0f).Vec(0.25f, -0.5f, 1.5f).F32(0.0f).U16(0u).U8(2u);
    const auto world = ReadOf(bytes);
    ASSERT_FALSE(world);
    EXPECT_EQ(world.Error().Code(), ErrorCode::InvalidData);
    EXPECT_NE(world.Error().Message().find("half-extent"), std::string::npos);
}

TEST(CollisionLoaderTests, AnInvertedBoundingBoxIsRefused)
{
    Bytes bytes = Header();
    bytes.U32(0u).U32(1u);
    bytes.U16(0u).U8(3u);
    bytes.Box(1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 1.0f);
    const auto world = ReadOf(bytes);
    ASSERT_FALSE(world);
    EXPECT_EQ(world.Error().Code(), ErrorCode::InvalidData);
    EXPECT_NE(world.Error().Message().find("inverted"), std::string::npos);
}

TEST(CollisionLoaderTests, ATriangleOutsideItsOwnMeshIsRefused)
{
    // Out of range here is a read past the end of the vertex array in the sweep -- a crash, or
    // worse, someone else's floats read as a triangle.
    Bytes bytes = Header();
    bytes.U32(0u).U32(1u);
    bytes.U16(0u).U8(3u);
    bytes.Box(0.0f, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f);
    bytes.U32(3u).Vec(0.0f, 0.0f, 0.0f).Vec(1.0f, 0.0f, 0.0f).Vec(0.0f, 1.0f, 0.0f);
    bytes.U32(1u).U16(0u).U16(1u).U16(9u);
    const auto world = ReadOf(bytes);
    ASSERT_FALSE(world);
    EXPECT_EQ(world.Error().Code(), ErrorCode::OutOfRange);
    EXPECT_NE(world.Error().Message().find("vertex 9 of 3"), std::string::npos);
}

TEST(CollisionLoaderTests, AMeshWithNoVerticesOrNoTrianglesIsRefused)
{
    Bytes empty = Header();
    empty.U32(0u).U32(1u).U16(0u).U8(3u);
    empty.Box(0.0f, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f).U32(0u);
    auto world = ReadOf(empty);
    ASSERT_FALSE(world);
    EXPECT_EQ(world.Error().Code(), ErrorCode::OutOfRange);

    Bytes noTriangles = Header();
    noTriangles.U32(0u).U32(1u).U16(0u).U8(3u);
    noTriangles.Box(0.0f, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f);
    noTriangles.U32(1u).Vec(0.0f, 0.0f, 0.0f).U32(0u);
    world = ReadOf(noTriangles);
    ASSERT_FALSE(world);
    EXPECT_EQ(world.Error().Code(), ErrorCode::OutOfRange);
}

TEST(CollisionLoaderTests, ACellReferencingAShapeThatIsNotThereIsRefused)
{
    Bytes bytes = Header();
    bytes.U32(1u);
    OneObb(bytes);
    bytes.U32(0u);
    bytes.U32(1u);
    bytes.Str("L0_CELL").Box(0.0f, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f);
    bytes.U32(1u).U32(5u);
    const auto world = ReadOf(bytes);
    ASSERT_FALSE(world);
    EXPECT_EQ(world.Error().Code(), ErrorCode::OutOfRange);
    EXPECT_NE(world.Error().Message().find("shape 5 of 1"), std::string::npos);
}

TEST(CollisionLoaderTests, ABucketEntryIsALocalIndexAndIsCheckedAsOne)
{
    // §3.4: bucket entries are LOCAL indices into the cell's own shape list. One that is not is a
    // sweep testing a shape belonging to another cell, or to nothing at all -- and a GLOBAL index
    // that happens to be in range would be silently wrong, which is exactly what this catches.
    Bytes bytes = Header();
    bytes.U32(1u);
    OneObb(bytes);
    bytes.U32(0u);
    bytes.U32(1u);
    bytes.Str("L0_CELL").Box(0.0f, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f);
    bytes.U32(1u).U32(0u);
    bytes.U32(1u).U32(1u);
    bytes.F32(0.0f).F32(0.0f);
    bytes.U16(1u).U16(3u);
    const auto world = ReadOf(bytes);
    ASSERT_FALSE(world);
    EXPECT_EQ(world.Error().Code(), ErrorCode::OutOfRange);
    EXPECT_NE(world.Error().Message().find("local shape 3 of 1"), std::string::npos);
}

TEST(CollisionLoaderTests, ABucketHoldingMoreEntriesThanTheCellHasShapesIsRefused)
{
    Bytes bytes = Header();
    bytes.U32(1u);
    OneObb(bytes);
    bytes.U32(0u).U32(1u);
    bytes.Str("L0_CELL").Box(0.0f, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f);
    bytes.U32(1u).U32(0u);
    bytes.U32(1u).U32(1u);
    bytes.F32(0.0f).F32(0.0f);
    bytes.U16(9u);
    const auto world = ReadOf(bytes);
    ASSERT_FALSE(world);
    EXPECT_EQ(world.Error().Code(), ErrorCode::OutOfRange);
}

TEST(CollisionLoaderTests, AnEmptyGridAndAnEscapedShapesGridAreBothRefused)
{
    Bytes empty = Header();
    empty.U32(1u);
    OneObb(empty);
    empty.U32(0u).U32(1u);
    empty.Str("L0_CELL").Box(0.0f, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f);
    empty.U32(1u).U32(0u).U32(0u).U32(2u);
    auto world = ReadOf(empty);
    ASSERT_FALSE(world);
    EXPECT_EQ(world.Error().Code(), ErrorCode::InvalidData);

    // A cell's grid is sized from the union of its own shapes, so ONE shape that escaped its cell
    // sizes it. The writer refuses to produce such a file; this refuses to allocate for one.
    Bytes huge = Header();
    huge.U32(1u);
    OneObb(huge);
    huge.U32(0u).U32(1u);
    huge.Str("L0_CELL").Box(0.0f, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f);
    huge.U32(1u).U32(0u).U32(4000u).U32(4000u);
    world = ReadOf(huge);
    ASSERT_FALSE(world);
    EXPECT_EQ(world.Error().Code(), ErrorCode::OutOfRange);
    EXPECT_NE(world.Error().Message().find("escaped its cell"), std::string::npos);
}

TEST(CollisionLoaderTests, ARepeatedCellIdIsRefused)
{
    // A cell id is what a caller looks a cell up by; two of them make the answer depend on which
    // the search happens to reach first.
    Bytes bytes = Header();
    bytes.U32(0u).U32(0u).U32(2u);
    for (int i = 0; i < 2; ++i)
    {
        bytes.Str("L0_CELL").Box(0.0f, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f);
        bytes.U32(0u).U32(1u).U32(1u).F32(0.0f).F32(0.0f).U16(0u);
    }
    const auto world = ReadOf(bytes);
    ASSERT_FALSE(world);
    EXPECT_EQ(world.Error().Code(), ErrorCode::Duplicate);
}

TEST(CollisionLoaderTests, AnEmptyWorldHashIsTheOneNameThatIsAllowed)
{
    // `deploy_world.py` stamps the hash; a file built before the world has been deployed carries
    // none, and that is a staleness check that cannot run rather than a corrupt file.
    Bytes bytes;
    bytes.U32(CollisionLoader::kMagic).U32(CollisionLoader::kVersion).U32(0u);
    bytes.Str("").F32(1.0f).U32(0u).U32(0u).U32(0u).U32(0u);
    auto world = ReadOf(bytes);
    ASSERT_TRUE(world) << world.Error().Message();
    EXPECT_TRUE(world->worldHash.empty());

    // A SURFACE name may not be empty: it names something.
    Bytes surface;
    surface.U32(CollisionLoader::kMagic).U32(CollisionLoader::kVersion).U32(0u);
    surface.Str("worldhash").F32(1.0f).U32(1u).Str("");
    world = ReadOf(surface);
    ASSERT_FALSE(world);
    EXPECT_NE(world.Error().Message().find("empty"), std::string::npos);
}

TEST(CollisionLoaderTests, EveryTruncationIsRefused)
{
    // `BinaryReader` throws at the end of the stream, so truncation arrives as an exception rather
    // than a short read. Walking every prefix is the only way to be sure none of them is read as a
    // shorter but valid file.
    const Bytes whole = WellFormed();
    for (std::size_t cut = 1; cut < whole.Size(); ++cut)
    {
        Bytes bytes = whole;
        bytes.Truncate(cut);
        const auto world = ReadOf(bytes);
        ASSERT_FALSE(world) << "a file truncated to " << cut << " bytes was accepted";
    }
}

TEST(CollisionLoaderTests, AFileThatIsNotThereIsNotFound)
{
    const auto world = CollisionLoader::ReadFromTitle("content/world/definitely-not-here.bin");
    ASSERT_FALSE(world);
    EXPECT_EQ(world.Error().Code(), ErrorCode::NotFound);
}
