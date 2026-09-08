// SPDX-License-Identifier: MIT
//
// `HOUSE-00474`. The one check neither side can make alone: a `chunks.bin` written by
// `tools/world/build_chunks.py` is read by `src/world/ChunkReader.cpp`, and what comes back out is
// what went in.
//
// `ChunkReaderTests` builds its bytes by hand, field at a time, which makes it an excellent test of
// the READER and no test at all of the two agreeing. A writer that emitted a bounding box max
// before min, the sub-range count before the indices, or `TEXCOORD_1` before `TEXCOORD_0` would
// pass every test in this repository and draw the wrong thing in the game. This is the test that
// closes that.
//
// The fixture is generated at BUILD time into the build tree (`tests/CMakeLists.txt`) rather than
// committed, for the same reason no compiled content is committed: a checked-in binary produced by
// a tool in the same repository can go stale against the tool without anyone noticing.
#include <cstring>
#include <memory>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "System/IO/FileAccess.hpp"
#include "System/IO/FileMode.hpp"
#include "System/IO/FileStream.hpp"

#include "cnahouse/world/ChunkReader.hpp"

namespace
{
    using cnahouse::world::Chunk;
    using cnahouse::world::ChunkLayout;
    using cnahouse::world::ChunkLibrary;
    using cnahouse::world::ChunkReader;
    using cnahouse::world::ChunkVertexStride;

    /// The facts `build_chunks.py`'s `fixture_library` authors. Written out here rather than read
    /// from the tool's own output, because a test that asked the writer what it wrote and then
    /// checked the reader agreed would pass with both of them wrong in the same way.
    constexpr const char* kWorldHash = "0123456789abcdef0123456789abcdef";
    /// Sorted, because the writer sorts both tables and the indices mean nothing otherwise.
    constexpr const char* kCells[] = {"L0_HALL", "L0_LOUNGE"};
    constexpr const char* kMaterials[] = {"MAT_GLASS", "MAT_PLASTER", "MAT_TRIM"};

    ChunkLibrary Load()
    {
        System::IO::FileStream stream(std::string(CNAHOUSE_TEST_CHUNK_FIXTURE),
                                      System::IO::FileMode::Open,
                                      System::IO::FileAccess::Read);
        auto library = ChunkReader::Read(stream, CNAHOUSE_TEST_CHUNK_FIXTURE);
        EXPECT_TRUE(library) << (library ? std::string() : library.Error().Message());
        return library ? std::move(*library) : ChunkLibrary{};
    }

    float FloatAt(const Chunk& chunk, std::size_t vertex, std::size_t offset)
    {
        float value = 0.0f;
        std::memcpy(
            &value, chunk.vertices.data() + vertex * ChunkVertexStride(chunk.layout) + offset, sizeof(float));
        return value;
    }

    std::uint16_t IndexAt(const Chunk& chunk, std::size_t at)
    {
        std::uint16_t value = 0;
        std::memcpy(&value, chunk.indices.data() + at * 2, sizeof(value));
        return value;
    }

} // namespace

TEST(ChunkRoundTripTests, TheWriterAndTheReaderAgreeOnTheHeader)
{
    const ChunkLibrary library = Load();
    EXPECT_EQ(library.worldHash, kWorldHash);
    ASSERT_EQ(library.cells.size(), std::size(kCells));
    for (std::size_t i = 0; i < std::size(kCells); ++i)
    {
        EXPECT_EQ(library.cells[i], kCells[i]) << "cell " << i;
    }
    ASSERT_EQ(library.materials.size(), std::size(kMaterials));
    for (std::size_t i = 0; i < std::size(kMaterials); ++i)
    {
        EXPECT_EQ(library.materials[i], kMaterials[i]) << "material " << i;
    }
    ASSERT_EQ(library.chunks.size(), 3u);
}

TEST(ChunkRoundTripTests, EveryChunkNamesTheCellAndMaterialItWasWrittenFor)
{
    const ChunkLibrary library = Load();
    ASSERT_EQ(library.chunks.size(), 3u);
    // The writer emits chunks in the order it built them; the tables are sorted independently, so
    // a chunk's indices are a real lookup rather than a coincidence of ordering.
    EXPECT_EQ(library.cells[library.chunks[0].cell], "L0_HALL");
    EXPECT_EQ(library.materials[library.chunks[0].material], "MAT_PLASTER");
    EXPECT_EQ(library.cells[library.chunks[2].cell], "L0_LOUNGE");
    EXPECT_EQ(library.materials[library.chunks[2].material], "MAT_GLASS");
    EXPECT_EQ(library.ChunksOf("L0_HALL").size(), 2u);
    EXPECT_EQ(library.ChunksOf("L0_LOUNGE").size(), 1u);
}

TEST(ChunkRoundTripTests, TheThreeLayoutsArriveAtTheirOwnStrides)
{
    const ChunkLibrary library = Load();
    ASSERT_EQ(library.chunks.size(), 3u);
    EXPECT_EQ(library.chunks[0].layout, ChunkLayout::Dual);
    EXPECT_EQ(library.chunks[1].layout, ChunkLayout::Basic);
    EXPECT_EQ(library.chunks[2].layout, ChunkLayout::AlphaTest);
    EXPECT_EQ(library.chunks[0].vertices.size(), 4u * 28u);
    EXPECT_EQ(library.chunks[1].vertices.size(), 3u * 32u);
    EXPECT_EQ(library.chunks[2].vertices.size(), 3u * 20u);
    EXPECT_EQ(library.GeometryBytes(), 4u * 28u + 6u * 2u + 3u * 32u + 3u * 2u + 3u * 20u + 3u * 2u);
}

TEST(ChunkRoundTripTests, ADualVertexIsPositionThenBothUvSets)
{
    // The fixture's two UV sets differ on purpose. `(0.125, 0.25)` and `(0.5, 0.75)` on the first
    // vertex: a writer that swapped the channels, or a reader that packed them the other way
    // round, lands on the wrong number here rather than on a plausible one.
    const ChunkLibrary library = Load();
    const Chunk& dual = library.chunks[0];
    ASSERT_EQ(dual.layout, ChunkLayout::Dual);
    EXPECT_FLOAT_EQ(FloatAt(dual, 0, 0), 0.0f);
    EXPECT_FLOAT_EQ(FloatAt(dual, 0, 4), 0.0f);
    EXPECT_FLOAT_EQ(FloatAt(dual, 0, 8), 0.0f);
    EXPECT_FLOAT_EQ(FloatAt(dual, 0, 12), 0.125f);
    EXPECT_FLOAT_EQ(FloatAt(dual, 0, 16), 0.25f);
    EXPECT_FLOAT_EQ(FloatAt(dual, 0, 20), 0.5f);
    EXPECT_FLOAT_EQ(FloatAt(dual, 0, 24), 0.75f);
    // ...and the second vertex is 4 m along x, so the stride is right too.
    EXPECT_FLOAT_EQ(FloatAt(dual, 1, 0), 4.0f);
    EXPECT_FLOAT_EQ(FloatAt(dual, 1, 12), 0.375f);
    EXPECT_FLOAT_EQ(FloatAt(dual, 1, 20), 0.625f);
}

TEST(ChunkRoundTripTests, ABasicVertexCarriesTheNormalTheDualOneDoesNot)
{
    const ChunkLibrary library = Load();
    const Chunk& basic = library.chunks[1];
    ASSERT_EQ(basic.layout, ChunkLayout::Basic);
    // Three DIFFERENT normals, one per vertex: a reader that read the same one three times, or the
    // position again, could not be told apart by a fixture whose normals all pointed up.
    EXPECT_FLOAT_EQ(FloatAt(basic, 0, 12), 1.0f);
    EXPECT_FLOAT_EQ(FloatAt(basic, 1, 20), -1.0f);
    EXPECT_FLOAT_EQ(FloatAt(basic, 2, 16), -1.0f);
    EXPECT_FLOAT_EQ(FloatAt(basic, 2, 24), 1.0f);
    EXPECT_FLOAT_EQ(FloatAt(basic, 2, 28), 1.0f);
}

TEST(ChunkRoundTripTests, AnAlphaTestVertexIsPositionAndOneUv)
{
    const ChunkLibrary library = Load();
    const Chunk& alpha = library.chunks[2];
    ASSERT_EQ(alpha.layout, ChunkLayout::AlphaTest);
    EXPECT_FLOAT_EQ(FloatAt(alpha, 0, 0), -2.0f);
    EXPECT_FLOAT_EQ(FloatAt(alpha, 0, 4), 0.9f);
    EXPECT_FLOAT_EQ(FloatAt(alpha, 0, 8), 6.0f);
    EXPECT_FLOAT_EQ(FloatAt(alpha, 0, 12), 0.25f);
    EXPECT_FLOAT_EQ(FloatAt(alpha, 0, 16), 0.5f);
}

TEST(ChunkRoundTripTests, TheIndicesAndSubRangesSurvive)
{
    const ChunkLibrary library = Load();
    const Chunk& dual = library.chunks[0];
    ASSERT_EQ(dual.indexCount, 6u);
    EXPECT_FALSE(dual.wideIndices);
    const std::uint16_t expected[] = {0, 1, 2, 0, 2, 3};
    for (std::size_t i = 0; i < 6; ++i)
    {
        EXPECT_EQ(IndexAt(dual, i), expected[i]) << "index " << i;
    }
    ASSERT_EQ(dual.subRanges.size(), 2u);
    EXPECT_EQ(dual.subRanges[0].source, "L0_HALL:BLOCKOUT_wall");
    EXPECT_EQ(dual.subRanges[0].indexStart, 0u);
    EXPECT_EQ(dual.subRanges[0].indexCount, 3u);
    EXPECT_EQ(dual.subRanges[1].source, "L0_HALL:BLOCKOUT_floor");
    EXPECT_EQ(dual.subRanges[1].indexStart, 3u);
    EXPECT_EQ(dual.subRanges[1].indexCount, 3u);
}

TEST(ChunkRoundTripTests, ASubRangesBoxIsItsOwnAndNotTheChunks)
{
    // §17.4 wants a box per sub-range "so a big group can still be partially culled"; a sub-range
    // carrying the group's box culls nothing and costs 24 bytes to do it. The fixture's first
    // sub-range stops at z = 0 where the chunk reaches z = -3.
    const ChunkLibrary library = Load();
    const Chunk& dual = library.chunks[0];
    ASSERT_EQ(dual.subRanges.size(), 2u);
    EXPECT_FLOAT_EQ(dual.bounds.Min.Z, -3.0f);
    EXPECT_FLOAT_EQ(dual.subRanges[0].bounds.Min.Z, 0.0f);
    EXPECT_FLOAT_EQ(dual.subRanges[1].bounds.Min.Z, -3.0f);
    EXPECT_FLOAT_EQ(dual.bounds.Max.X, 4.0f);
    EXPECT_FLOAT_EQ(dual.bounds.Max.Y, 2.5f);
}

TEST(ChunkRoundTripTests, TheWholeHouseReadsBackWhenItHasBeenBuilt)
{
    // The fixture is three chunks. `build_chunks.py` over the real layout writes 488, and the one
    // thing three chunks cannot show is a reader that copes with a small file and not a real one --
    // a length that overflows, a table index that needs all 16 bits, a cell with no geometry.
    // Skipped rather than failed when the content build has not run: this is a unit test, and
    // requiring a 2.9 MB generated file would make it one only on a machine that had built it.
    const std::string path = "build/chunks.bin";
    System::IO::FileStream* probe = nullptr;
    try
    {
        probe = new System::IO::FileStream(path, System::IO::FileMode::Open, System::IO::FileAccess::Read);
    }
    catch (const std::exception&)
    {
        GTEST_SKIP() << "no " << path << "; run tools/world/build_chunks.py --out build/chunks.bin";
    }
    const std::unique_ptr<System::IO::FileStream> stream(probe);
    const auto library = ChunkReader::Read(*stream, path);
    ASSERT_TRUE(library) << library.Error().Message();
    EXPECT_GT(library->chunks.size(), 100u);
    EXPECT_GT(library->cells.size(), 50u);
    std::uint64_t triangles = 0;
    std::size_t dual = 0;
    for (const auto& chunk : library->chunks)
    {
        triangles += chunk.indexCount / 3u;
        dual += chunk.layout == ChunkLayout::Dual ? 1u : 0u;
        // Every chunk of the real house fits 16-bit indices; the writer reports it and this is the
        // reader agreeing.
        EXPECT_FALSE(chunk.wideIndices) << "a chunk of the real house needed 32-bit indices";
        EXPECT_LE(chunk.vertexCount, ChunkReader::kMaxVertices16Bit);
    }
    EXPECT_GT(triangles, 10000u);
    EXPECT_GT(dual, 0u) << "not one lightmap receiver in the whole house";
}
