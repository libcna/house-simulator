// SPDX-License-Identifier: MIT
//
// `HOUSE-00856`. `neighbourhood.bin` parsed from bytes built here, field at a time, so every
// rejection can be provoked exactly: a reader tested only against files its own writer produced is
// a reader tested only on the bytes that happen to be correct.
//
// `NeighbourhoodRoundTripTests` is the other half -- it reads what `build_neighbourhood.py` really
// writes -- and `HOUSE-00225` is why there are two: a reader and a writer each tested against their
// own hand-written fixtures both passed while producing and expecting different bytes.
#include <cstdint>
#include <cstring>
#include <limits>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "System/IO/MemoryStream.hpp"

#include "cnahouse/util/Ids.hpp"
#include "cnahouse/world/NeighbourhoodReader.hpp"
#include "cnahouse/world/WorldTypes.hpp"

namespace
{
    using cnahouse::util::ErrorCode;
    using cnahouse::world::ChunkLayout;
    using cnahouse::world::NeighbourBuilding;
    using cnahouse::world::NeighbourhoodLibrary;
    using cnahouse::world::NeighbourhoodReader;

    /// A little-endian byte builder, so a test says what it writes in the order the format does.
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
            return Raw(&value, sizeof(value));
        }

        Bytes& U32(std::uint32_t value)
        {
            return Raw(&value, sizeof(value));
        }

        Bytes& F32(float value)
        {
            return Raw(&value, sizeof(value));
        }

        Bytes& Text(std::string_view value)
        {
            U16(static_cast<std::uint16_t>(value.size()));
            return Raw(value.data(), value.size());
        }

        /// Min *xyz* then max *xyz*, in the format's own order.
        Bytes& Box(float minX, float minY, float minZ, float maxX, float maxY, float maxZ)
        {
            return F32(minX).F32(minY).F32(minZ).F32(maxX).F32(maxY).F32(maxZ);
        }

        /// One `ChunkLayout::Basic` vertex: position, normal, UV.
        Bytes& Vertex(float x, float y, float z)
        {
            return F32(x).F32(y).F32(z).F32(0.0f).F32(1.0f).F32(0.0f).F32(0.25f).F32(0.75f);
        }

        [[nodiscard]] std::vector<std::uint8_t> Take() const
        {
            return data_;
        }

    private:
        Bytes& Raw(const void* source, std::size_t size)
        {
            const auto* bytes = static_cast<const std::uint8_t*>(source);
            data_.insert(data_.end(), bytes, bytes + size);
            return *this;
        }

        std::vector<std::uint8_t> data_;
    };

    /// A minimal but complete file: one material, one asset, one triangle.
    Bytes Good()
    {
        Bytes bytes;
        bytes.U32(NeighbourhoodReader::kMagic).U32(NeighbourhoodReader::kVersion).U32(0u);
        bytes.Text("sha256:cafe");
        bytes.U32(1u).Text("NB_WALL_CREAM");
        bytes.U32(1u);
        bytes.Text("MODEL_NB_HOUSE_A_CREAM").Box(-1.0f, 0.0f, -1.0f, 1.0f, 3.0f, 1.0f);
        bytes.U32(1u);
        bytes.U16(0u).U8(static_cast<std::uint8_t>(ChunkLayout::Basic)).U8(0u);
        bytes.Box(-1.0f, 0.0f, -1.0f, 1.0f, 3.0f, 1.0f);
        bytes.U32(3u).Vertex(-1.0f, 0.0f, -1.0f).Vertex(1.0f, 0.0f, -1.0f).Vertex(0.0f, 3.0f, 1.0f);
        bytes.U32(3u).U16(0u).U16(1u).U16(2u);
        return bytes;
    }

    cnahouse::util::Result<NeighbourhoodLibrary> ReadOf(const std::vector<std::uint8_t>& data)
    {
        System::IO::MemoryStream stream(data.data(), static_cast<int>(data.size()), false);
        return NeighbourhoodReader::Read(stream, "neighbourhood.bin");
    }

} // namespace

TEST(NeighbourhoodReaderTests, AWellFormedFileReadsBack)
{
    auto library = ReadOf(Good().Take());
    ASSERT_TRUE(library) << (library ? std::string() : library.Error().Message());
    EXPECT_EQ(library->worldHash, "sha256:cafe");
    ASSERT_EQ(library->materials.size(), 1u);
    ASSERT_EQ(library->assets.size(), 1u);
    EXPECT_EQ(library->assets[0].asset, "MODEL_NB_HOUSE_A_CREAM");
    ASSERT_EQ(library->assets[0].primitives.size(), 1u);
    EXPECT_EQ(library->assets[0].primitives[0].vertexCount, 3u);
    EXPECT_EQ(library->assets[0].primitives[0].vertices.size(), 3u * 32u);
    EXPECT_EQ(library->GeometryBytes(), 3u * 32u + 3u * 2u);
}

TEST(NeighbourhoodReaderTests, TheWrongFileEntirelyIsSaidPlainly)
{
    Bytes bytes;
    bytes.U32(0x4B484343u).U32(1u).U32(0u); // 'CCHK' -- chunks.bin, which is the likely mistake
    const auto library = ReadOf(bytes.Take());
    ASSERT_FALSE(library);
    EXPECT_EQ(library.Error().Code(), ErrorCode::InvalidData);
    EXPECT_NE(library.Error().Message().find("CNBH"), std::string::npos) << library.Error().Message();
}

TEST(NeighbourhoodReaderTests, AVersionOrAFlagThisBuildDoesNotKnowIsRefused)
{
    Bytes wrongVersion;
    wrongVersion.U32(NeighbourhoodReader::kMagic).U32(99u).U32(0u);
    auto library = ReadOf(wrongVersion.Take());
    ASSERT_FALSE(library);
    EXPECT_EQ(library.Error().Code(), ErrorCode::VersionMismatch);

    Bytes flagged;
    flagged.U32(NeighbourhoodReader::kMagic).U32(NeighbourhoodReader::kVersion).U32(0x8u);
    library = ReadOf(flagged.Take());
    ASSERT_FALSE(library);
    EXPECT_EQ(library.Error().Code(), ErrorCode::VersionMismatch)
        << "an unknown flag bit means a writer said something this build cannot honour";
}

TEST(NeighbourhoodReaderTests, AnEmptyLibraryIsAFailedBuildAndNotAnEmptyStreet)
{
    // §11.4 has a neighbourhood. A file holding none is a build that produced nothing, which is
    // exactly what went unnoticed for the fifteen tasks before `HOUSE-00856`.
    Bytes bytes;
    bytes.U32(NeighbourhoodReader::kMagic).U32(NeighbourhoodReader::kVersion).U32(0u);
    bytes.Text("sha256:cafe").U32(0u).U32(0u);
    const auto library = ReadOf(bytes.Take());
    ASSERT_FALSE(library);
    EXPECT_EQ(library.Error().Code(), ErrorCode::OutOfRange);
}

TEST(NeighbourhoodReaderTests, TheAssetTableMustBeInAscendingOrder)
{
    // `Find` is a binary search over it, and out of order it would miss an asset that is there.
    // The same check catches a duplicate, which would be a mesh nothing can reach.
    Bytes bytes;
    bytes.U32(NeighbourhoodReader::kMagic).U32(NeighbourhoodReader::kVersion).U32(0u);
    bytes.Text("").U32(1u).Text("NB_WALL_CREAM");
    bytes.U32(2u);
    for (const char* name : {"MODEL_NB_IMPOSTOR_GABLE", "MODEL_NB_HOUSE_A_CREAM"})
    {
        bytes.Text(name).Box(-1.0f, 0.0f, -1.0f, 1.0f, 3.0f, 1.0f);
        bytes.U32(1u).U16(0u).U8(0u).U8(0u).Box(-1.0f, 0.0f, -1.0f, 1.0f, 3.0f, 1.0f);
        bytes.U32(3u).Vertex(0.0f, 0.0f, 0.0f).Vertex(1.0f, 0.0f, 0.0f).Vertex(0.0f, 1.0f, 0.0f);
        bytes.U32(3u).U16(0u).U16(1u).U16(2u);
    }
    const auto library = ReadOf(bytes.Take());
    ASSERT_FALSE(library);
    EXPECT_EQ(library.Error().Code(), ErrorCode::InvalidData);
    EXPECT_NE(library.Error().Message().find("ascending"), std::string::npos) << library.Error().Message();
}

TEST(NeighbourhoodReaderTests, AnInvertedBoxIsRefusedAndSoIsANaN)
{
    for (const float bad : {-2.0f, std::numeric_limits<float>::quiet_NaN()})
    {
        // The rest of the file is COMPLETE and correct on purpose. A truncated one would fail for
        // the truncation instead, with the same error code, and would pass this test with the box
        // check gone -- which is exactly what an injection found.
        Bytes bytes;
        bytes.U32(NeighbourhoodReader::kMagic).U32(NeighbourhoodReader::kVersion).U32(0u);
        bytes.Text("").U32(1u).Text("NB_WALL_CREAM").U32(1u);
        // A NaN compares false against everything, so `min > max` would let it through and leave a
        // box that culls the whole street or none of it depending on which way the test runs.
        bytes.Text("MODEL_NB_HOUSE_A_CREAM").Box(-1.0f, 0.0f, -1.0f, bad, 3.0f, 1.0f);
        bytes.U32(1u).U16(0u).U8(0u).U8(0u).Box(-1.0f, 0.0f, -1.0f, 1.0f, 3.0f, 1.0f);
        bytes.U32(3u).Vertex(0.0f, 0.0f, 0.0f).Vertex(1.0f, 0.0f, 0.0f).Vertex(0.0f, 1.0f, 0.0f);
        bytes.U32(3u).U16(0u).U16(1u).U16(2u);
        const auto library = ReadOf(bytes.Take());
        ASSERT_FALSE(library) << "a box with " << bad << " in it was accepted";
        EXPECT_EQ(library.Error().Code(), ErrorCode::InvalidData);
        EXPECT_NE(library.Error().Message().find("bounding box"), std::string::npos)
            << library.Error().Message();
    }
}

TEST(NeighbourhoodReaderTests, APrimitiveNamingAMaterialThatIsNotThereIsRefused)
{
    Bytes bytes;
    bytes.U32(NeighbourhoodReader::kMagic).U32(NeighbourhoodReader::kVersion).U32(0u);
    bytes.Text("").U32(1u).Text("NB_WALL_CREAM").U32(1u);
    bytes.Text("MODEL_NB_HOUSE_A_CREAM").Box(-1.0f, 0.0f, -1.0f, 1.0f, 3.0f, 1.0f);
    bytes.U32(1u).U16(7u).U8(0u).U8(0u);
    const auto library = ReadOf(bytes.Take());
    ASSERT_FALSE(library);
    EXPECT_EQ(library.Error().Code(), ErrorCode::OutOfRange);
}

TEST(NeighbourhoodReaderTests, AVertexLayoutThisBuildDoesNotKnowStopsTheParse)
{
    // Guessing at the stride would misread every byte after this point, so it stops and says which
    // primitive rather than producing geometry made of the next asset's name.
    Bytes bytes;
    bytes.U32(NeighbourhoodReader::kMagic).U32(NeighbourhoodReader::kVersion).U32(0u);
    bytes.Text("").U32(1u).Text("NB_WALL_CREAM").U32(1u);
    bytes.Text("MODEL_NB_HOUSE_A_CREAM").Box(-1.0f, 0.0f, -1.0f, 1.0f, 3.0f, 1.0f);
    bytes.U32(1u).U16(0u).U8(9u).U8(0u);
    const auto library = ReadOf(bytes.Take());
    ASSERT_FALSE(library);
    EXPECT_EQ(library.Error().Code(), ErrorCode::VersionMismatch);
}

TEST(NeighbourhoodReaderTests, IndicesMustBeWholeTrianglesAndMustAddressTheirVertices)
{
    // Four indices is not a whole number of triangles; the parse must not draw one and a bit.
    Bytes bytes;
    bytes.U32(NeighbourhoodReader::kMagic).U32(NeighbourhoodReader::kVersion).U32(0u);
    bytes.Text("").U32(1u).Text("NB_WALL_CREAM").U32(1u);
    bytes.Text("MODEL_NB_HOUSE_A_CREAM").Box(-1.0f, 0.0f, -1.0f, 1.0f, 3.0f, 1.0f);
    bytes.U32(1u).U16(0u).U8(0u).U8(0u).Box(-1.0f, 0.0f, -1.0f, 1.0f, 3.0f, 1.0f);
    bytes.U32(3u).Vertex(0.0f, 0.0f, 0.0f).Vertex(1.0f, 0.0f, 0.0f).Vertex(0.0f, 1.0f, 0.0f);
    bytes.U32(4u).U16(0u).U16(1u).U16(2u).U16(0u);
    const auto library = ReadOf(bytes.Take());
    ASSERT_FALSE(library);
    EXPECT_EQ(library.Error().Code(), ErrorCode::InvalidData);
    EXPECT_NE(library.Error().Message().find("triangles"), std::string::npos) << library.Error().Message();
}

TEST(NeighbourhoodReaderTests, ATruncatedFileIsAnErrorAndNotAShortStreet)
{
    std::vector<std::uint8_t> data = Good().Take();
    data.resize(data.size() - 8);
    const auto library = ReadOf(data);
    ASSERT_FALSE(library);
    EXPECT_EQ(library.Error().Code(), ErrorCode::InvalidData);
}

TEST(NeighbourhoodReaderTests, EveryRowResolvesOrIsNamed)
{
    auto library = ReadOf(Good().Take());
    ASSERT_TRUE(library);

    std::vector<NeighbourBuilding> rows(2);
    rows[0].id = cnahouse::util::Intern("NB_HOUSE_N1");
    rows[0].asset = cnahouse::util::Intern("MODEL_NB_HOUSE_A_CREAM");
    rows[1].id = cnahouse::util::Intern("NB_CAR_01");
    rows[1].asset = cnahouse::util::Intern("MODEL_PARKED_CAR");

    const std::vector<std::string> missing = UnresolvedAssets(*library, rows);
    // The house resolves; the car does not, and is named rather than silently drawing nothing.
    // Nothing is excluded here on purpose: §11.4's vehicles are `HOUSE-00847`'s to deliver, and
    // teaching this function which prefixes belong to which task would put the tool's `is_ours`
    // rule in a second place.
    ASSERT_EQ(missing.size(), 1u);
    EXPECT_EQ(missing[0], "MODEL_PARKED_CAR");
}

TEST(NeighbourhoodReaderTests, AnUninternedRowIsReportedByItsHashRatherThanNotAtAll)
{
    auto library = ReadOf(Good().Take());
    ASSERT_TRUE(library);

    std::vector<NeighbourBuilding> rows(1);
    // `Id::Of` hashes without registering, so the name is genuinely not in this process.
    rows[0].asset = cnahouse::util::Id::Of("MODEL_NEVER_INTERNED_ANYWHERE");

    const std::vector<std::string> missing = UnresolvedAssets(*library, rows);
    ASSERT_EQ(missing.size(), 1u);
    EXPECT_TRUE(missing[0].starts_with("id:0x")) << missing[0];
}
