// SPDX-License-Identifier: MIT
//
// `HOUSE-00474`. The reader of `docs/chunk-format.md`, asked the questions a corrupt or
// mis-written file would answer wrongly. The bytes are built here field by field: a test that
// asked the writer for its own output would pass with both sides wrong in the same way, and
// `ChunkRoundTripTests` is the separate check that they agree.
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "System/IO/MemoryStream.hpp"

#include "cnahouse/world/ChunkReader.hpp"

namespace
{
    using cnahouse::util::ErrorCode;
    using cnahouse::world::ChunkLayout;
    using cnahouse::world::ChunkLibrary;
    using cnahouse::world::ChunkReader;
    using cnahouse::world::ChunkVertexStride;

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

        Bytes& Box(float minX, float minY, float minZ, float maxX, float maxY, float maxZ)
        {
            return F32(minX).F32(minY).F32(minZ).F32(maxX).F32(maxY).F32(maxZ);
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

    /// The header of a well-formed file: magic, version 1, no flags, a world hash.
    Bytes Header()
    {
        Bytes bytes;
        bytes.U32(ChunkReader::kMagic).U32(ChunkReader::kVersion).U32(0u).Str("worldhash");
        return bytes;
    }

    /// Two cells and two materials, which every chunk below indexes into.
    void Tables(Bytes& bytes)
    {
        bytes.U32(2u).Str("L0_HALL").Str("L0_LOUNGE");
        bytes.U32(2u).Str("MAT_PLASTER").Str("MAT_TRIM");
    }

    /// One `dual` chunk: three vertices, one triangle, one sub-range covering it.
    void OneChunk(Bytes& bytes,
                  std::uint16_t cell = 0u,
                  std::uint16_t material = 0u,
                  ChunkLayout layout = ChunkLayout::Dual)
    {
        bytes.U16(cell).U16(material).U8(static_cast<std::uint8_t>(layout)).U8(0u);
        bytes.Box(0.0f, 0.0f, 0.0f, 4.0f, 2.5f, 0.0f);
        bytes.U32(3u);
        const std::uint32_t stride = ChunkVertexStride(layout);
        for (int v = 0; v < 3; ++v)
        {
            for (std::uint32_t f = 0; f < stride / 4u; ++f)
            {
                bytes.F32(static_cast<float>(v) + static_cast<float>(f) / 16.0f);
            }
        }
        bytes.U32(3u).U16(0u).U16(1u).U16(2u);
        bytes.U32(1u);
        bytes.Str("PROP_A").U32(0u).U32(3u).Box(0.0f, 0.0f, 0.0f, 4.0f, 2.5f, 0.0f);
    }

    cnahouse::util::Result<ChunkLibrary> ReadOf(const Bytes& bytes)
    {
        System::IO::MemoryStream stream(bytes.Data().data(), static_cast<int>(bytes.Data().size()), false);
        return ChunkReader::Read(stream, "test.bin");
    }

    Bytes WellFormed()
    {
        Bytes bytes = Header();
        Tables(bytes);
        bytes.U32(1u);
        OneChunk(bytes);
        return bytes;
    }

} // namespace

TEST(ChunkReaderTests, AWellFormedFileReadsBack)
{
    const auto library = ReadOf(WellFormed());
    ASSERT_TRUE(library) << library.Error().Message();
    EXPECT_EQ(library->worldHash, "worldhash");
    ASSERT_EQ(library->cells.size(), 2u);
    EXPECT_EQ(library->cells[0], "L0_HALL");
    ASSERT_EQ(library->chunks.size(), 1u);
    EXPECT_EQ(library->chunks[0].layout, ChunkLayout::Dual);
    EXPECT_EQ(library->chunks[0].vertexCount, 3u);
    EXPECT_EQ(library->chunks[0].indexCount, 3u);
    EXPECT_FALSE(library->chunks[0].wideIndices);
    EXPECT_EQ(library->chunks[0].vertices.size(), 3u * ChunkVertexStride(ChunkLayout::Dual));
    EXPECT_EQ(library->chunks[0].indices.size(), 6u);
    ASSERT_EQ(library->chunks[0].subRanges.size(), 1u);
    EXPECT_EQ(library->chunks[0].subRanges[0].source, "PROP_A");
}

TEST(ChunkReaderTests, TheVerticesAreKeptPackedAtTheLayoutsOwnStride)
{
    // 28 bytes for `dual`, 32 for `basic`, 20 for `alphatest`. Reading them into a C++ vertex
    // struct would need a struct per layout and two conversions to get back to the bytes the GPU
    // wants, so the reader keeps the file's own packing and `CellRuntime` unpacks once.
    EXPECT_EQ(ChunkVertexStride(ChunkLayout::Basic), 32u);
    EXPECT_EQ(ChunkVertexStride(ChunkLayout::Dual), 28u);
    EXPECT_EQ(ChunkVertexStride(ChunkLayout::AlphaTest), 20u);

    for (const ChunkLayout layout : {ChunkLayout::Basic, ChunkLayout::Dual, ChunkLayout::AlphaTest})
    {
        Bytes bytes = Header();
        Tables(bytes);
        bytes.U32(1u);
        OneChunk(bytes, 0u, 0u, layout);
        const auto library = ReadOf(bytes);
        ASSERT_TRUE(library) << library.Error().Message();
        EXPECT_EQ(library->chunks[0].vertices.size(), 3u * ChunkVertexStride(layout));
        // The first vertex's first float is 0.0 and its second 1/16, by construction above. A
        // reader that skipped a field or read at the wrong stride would land elsewhere.
        float first = 0.0f;
        std::memcpy(&first, library->chunks[0].vertices.data() + 4, sizeof(float));
        EXPECT_FLOAT_EQ(first, 1.0f / 16.0f);
    }
}

TEST(ChunkReaderTests, TheCellAndMaterialTablesAreWhatChunksNameThingsBy)
{
    const auto library = ReadOf(WellFormed());
    ASSERT_TRUE(library);
    EXPECT_EQ(library->IndexOfCell("L0_LOUNGE"), 1u);
    EXPECT_EQ(library->IndexOfCell("NOT_A_CELL"), library->cells.size());
    EXPECT_EQ(library->ChunksOf("L0_HALL").size(), 1u);
    EXPECT_TRUE(library->ChunksOf("L0_LOUNGE").empty());
    EXPECT_TRUE(library->ChunksOf("NOT_A_CELL").empty());
    EXPECT_EQ(library->GeometryBytes(), 3u * ChunkVertexStride(ChunkLayout::Dual) + 6u);
}

TEST(ChunkReaderTests, TheWrongFileEntirelyIsSaidPlainly)
{
    Bytes bytes = WellFormed();
    bytes.Poke(0, 'X');
    const auto library = ReadOf(bytes);
    ASSERT_FALSE(library);
    EXPECT_EQ(library.Error().Code(), ErrorCode::InvalidData);
    EXPECT_NE(library.Error().Message().find("CCHK"), std::string::npos);
    EXPECT_EQ(library.Error().Context(), "test.bin");
}

TEST(ChunkReaderTests, AVersionThisBuildDoesNotKnowIsRefused)
{
    Bytes bytes = Header();
    bytes.Poke(4, 2u);
    Tables(bytes);
    bytes.U32(0u);
    const auto library = ReadOf(bytes);
    ASSERT_FALSE(library);
    EXPECT_EQ(library.Error().Code(), ErrorCode::VersionMismatch);
    EXPECT_NE(library.Error().Message().find("version 2"), std::string::npos);
}

TEST(ChunkReaderTests, AReservedHeaderFlagIsRefusedRatherThanMasked)
{
    // Masking would drop whatever a newer writer meant by it and produce a world that is subtly
    // wrong forever, which is far worse than not loading.
    Bytes bytes = Header();
    bytes.Poke(8, 0x4u);
    Tables(bytes);
    bytes.U32(0u);
    const auto library = ReadOf(bytes);
    ASSERT_FALSE(library);
    EXPECT_EQ(library.Error().Code(), ErrorCode::VersionMismatch);
}

TEST(ChunkReaderTests, AChunkNamingACellThatIsNotThereIsRefused)
{
    Bytes bytes = Header();
    Tables(bytes);
    bytes.U32(1u);
    OneChunk(bytes, 7u);
    const auto library = ReadOf(bytes);
    ASSERT_FALSE(library);
    EXPECT_EQ(library.Error().Code(), ErrorCode::OutOfRange);
    EXPECT_NE(library.Error().Message().find("cell 7 of 2"), std::string::npos);
}

TEST(ChunkReaderTests, AVertexLayoutThisBuildDoesNotKnowStopsTheRead)
{
    // Guessing at the stride would misread every byte after this point, so it stops and says which
    // chunk rather than producing a library of noise.
    Bytes bytes = Header();
    Tables(bytes);
    bytes.U32(1u);
    OneChunk(bytes, 0u, 0u, static_cast<ChunkLayout>(9));
    const auto library = ReadOf(bytes);
    ASSERT_FALSE(library);
    EXPECT_EQ(library.Error().Code(), ErrorCode::VersionMismatch);
    EXPECT_NE(library.Error().Message().find("layout 9"), std::string::npos);
}

TEST(ChunkReaderTests, ATableThatRepeatsANameIsRefused)
{
    Bytes bytes = Header();
    bytes.U32(2u).Str("L0_HALL").Str("L0_HALL");
    bytes.U32(1u).Str("MAT_PLASTER");
    bytes.U32(0u);
    const auto library = ReadOf(bytes);
    ASSERT_FALSE(library);
    EXPECT_EQ(library.Error().Code(), ErrorCode::Duplicate);
}

TEST(ChunkReaderTests, AnInvertedBoundingBoxIsRefused)
{
    Bytes bytes = Header();
    Tables(bytes);
    bytes.U32(1u);
    bytes.U16(0u).U16(0u).U8(static_cast<std::uint8_t>(ChunkLayout::Dual)).U8(0u);
    bytes.Box(4.0f, 0.0f, 0.0f, 0.0f, 2.5f, 0.0f);
    const auto library = ReadOf(bytes);
    ASSERT_FALSE(library);
    EXPECT_EQ(library.Error().Code(), ErrorCode::InvalidData);
    EXPECT_NE(library.Error().Message().find("inverted"), std::string::npos);
}

TEST(ChunkReaderTests, SixteenBitIndicesThatCannotAddressTheVerticesAreRefused)
{
    // The writer's whole splitting rule exists to keep this true; a chunk that broke it would draw
    // with indices wrapping round to the start of its own buffer, which looks almost right.
    Bytes bytes = Header();
    Tables(bytes);
    bytes.U32(1u);
    bytes.U16(0u).U16(0u).U8(static_cast<std::uint8_t>(ChunkLayout::Dual)).U8(0u);
    bytes.Box(0.0f, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f);
    bytes.U32(ChunkReader::kMaxVertices16Bit + 2u);
    const auto library = ReadOf(bytes);
    ASSERT_FALSE(library);
    EXPECT_EQ(library.Error().Code(), ErrorCode::InvalidData);
    EXPECT_NE(library.Error().Message().find("16-bit"), std::string::npos);
}

TEST(ChunkReaderTests, AnIndexCountThatIsNotWholeTrianglesIsRefused)
{
    Bytes bytes = Header();
    Tables(bytes);
    bytes.U32(1u);
    bytes.U16(0u).U16(0u).U8(static_cast<std::uint8_t>(ChunkLayout::Dual)).U8(0u);
    bytes.Box(0.0f, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f);
    bytes.U32(3u);
    for (std::uint32_t f = 0; f < 3u * ChunkVertexStride(ChunkLayout::Dual) / 4u; ++f)
    {
        bytes.F32(0.0f);
    }
    bytes.U32(4u);
    const auto library = ReadOf(bytes);
    ASSERT_FALSE(library);
    EXPECT_EQ(library.Error().Code(), ErrorCode::InvalidData);
    EXPECT_NE(library.Error().Message().find("whole number of triangles"), std::string::npos);
}

TEST(ChunkReaderTests, SubRangesMustTileTheIndexBufferExactly)
{
    // §17.4's sub-range boxes "tile the chunk's index buffer -- no gaps, no overlaps". Checking it
    // here is what lets a culling pass draw one sub-range without asking about the others.
    Bytes bytes = Header();
    Tables(bytes);
    bytes.U32(1u);
    bytes.U16(0u).U16(0u).U8(static_cast<std::uint8_t>(ChunkLayout::Dual)).U8(0u);
    bytes.Box(0.0f, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f);
    bytes.U32(3u);
    for (std::uint32_t f = 0; f < 3u * ChunkVertexStride(ChunkLayout::Dual) / 4u; ++f)
    {
        bytes.F32(0.0f);
    }
    bytes.U32(6u).U16(0u).U16(1u).U16(2u).U16(0u).U16(1u).U16(2u);
    bytes.U32(2u);
    bytes.Str("PROP_A").U32(0u).U32(3u).Box(0.0f, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f);
    // Starts at 4, where the one before it ended at 3.
    bytes.Str("PROP_B").U32(4u).U32(3u).Box(0.0f, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f);
    const auto library = ReadOf(bytes);
    ASSERT_FALSE(library);
    EXPECT_EQ(library.Error().Code(), ErrorCode::InvalidData);
    EXPECT_NE(library.Error().Message().find("tile"), std::string::npos);
}

TEST(ChunkReaderTests, AChunkWithNoSubRangeIsRefused)
{
    Bytes bytes = Header();
    Tables(bytes);
    bytes.U32(1u);
    bytes.U16(0u).U16(0u).U8(static_cast<std::uint8_t>(ChunkLayout::Dual)).U8(0u);
    bytes.Box(0.0f, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f);
    bytes.U32(3u);
    for (std::uint32_t f = 0; f < 3u * ChunkVertexStride(ChunkLayout::Dual) / 4u; ++f)
    {
        bytes.F32(0.0f);
    }
    bytes.U32(3u).U16(0u).U16(1u).U16(2u);
    bytes.U32(0u);
    const auto library = ReadOf(bytes);
    ASSERT_FALSE(library);
    EXPECT_EQ(library.Error().Code(), ErrorCode::OutOfRange);
}

TEST(ChunkReaderTests, EveryTruncationIsRefused)
{
    // `BinaryReader` throws at the end of the stream, so truncation arrives as an exception rather
    // than a short read. Walking every prefix is the only way to be sure none of them is read as a
    // shorter but valid file.
    const Bytes whole = WellFormed();
    for (std::size_t cut = 1; cut < whole.Size(); ++cut)
    {
        Bytes bytes = whole;
        bytes.Truncate(cut);
        const auto library = ReadOf(bytes);
        ASSERT_FALSE(library) << "a file truncated to " << cut << " bytes was accepted";
    }
}

TEST(ChunkReaderTests, AFileThatIsNotThereIsNotFound)
{
    const auto library = ChunkReader::ReadFromTitle("content/world/definitely-not-here.bin");
    ASSERT_FALSE(library);
    EXPECT_EQ(library.Error().Code(), ErrorCode::NotFound);
}

TEST(ChunkReaderTests, AnEmptyWorldHashIsTheOneNameThatIsAllowed)
{
    // `deploy_world.py` stamps the hash; a file built before the world has been deployed carries
    // none, and that is a staleness check that cannot run rather than a corrupt file. Every other
    // string in the format names something, so an empty one there IS a length field gone wrong.
    Bytes bytes;
    bytes.U32(ChunkReader::kMagic).U32(ChunkReader::kVersion).U32(0u).Str("");
    Tables(bytes);
    bytes.U32(1u);
    OneChunk(bytes);
    const auto library = ReadOf(bytes);
    ASSERT_TRUE(library) << library.Error().Message();
    EXPECT_TRUE(library->worldHash.empty());

    Bytes named = Header();
    named.U32(2u).Str("L0_HALL").Str("");
    named.U32(1u).Str("MAT_PLASTER");
    named.U32(0u);
    const auto broken = ReadOf(named);
    ASSERT_FALSE(broken);
    EXPECT_NE(broken.Error().Message().find("empty"), std::string::npos);
}
