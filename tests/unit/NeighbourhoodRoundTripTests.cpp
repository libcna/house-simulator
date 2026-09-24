// SPDX-License-Identifier: MIT
//
// `HOUSE-00856`. The one check neither side can make alone: a `neighbourhood.bin` written by
// `tools/world/build_neighbourhood.py` is read by `src/world/NeighbourhoodReader.cpp`, and what
// comes back out is what went in.
//
// `NeighbourhoodReaderTests` builds its bytes by hand, field at a time, which makes it an excellent
// test of the READER and no test at all of the two agreeing. A writer that emitted a box max before
// its min, the index count before the vertices, or a normal where the UV goes would pass every test
// in this repository and draw the wrong street.
//
// The fixture is generated at BUILD time into the build tree (`tests/CMakeLists.txt`) rather than
// committed, for the same reason no compiled content is committed: a checked-in binary produced by
// a tool in the same repository can go stale against the tool without anyone noticing.
#include <cmath>
#include <cstring>
#include <string>

#include <gtest/gtest.h>

#include "System/IO/FileAccess.hpp"
#include "System/IO/FileMode.hpp"
#include "System/IO/FileStream.hpp"

#include "cnahouse/world/NeighbourhoodReader.hpp"

namespace
{
    using cnahouse::world::ChunkLayout;
    using cnahouse::world::ChunkVertexStride;
    using cnahouse::world::NeighbourAsset;
    using cnahouse::world::NeighbourhoodLibrary;
    using cnahouse::world::NeighbourhoodReader;
    using cnahouse::world::NeighbourPrimitive;

    /// The facts `build_neighbourhood.py`'s `fixture_library` authors. Written out here rather than
    /// read from the tool's own output, because a test that asked the writer what it wrote and then
    /// checked the reader agreed would pass with both of them wrong in the same way.
    std::string ExpectedWorldHash()
    {
        std::string hash = "sha256:";
        for (int i = 0; i < 32; ++i)
        {
            hash += "5c";
        }
        return hash;
    }

    NeighbourhoodLibrary Load()
    {
        System::IO::FileStream stream(std::string(CNAHOUSE_TEST_NEIGHBOURHOOD_FIXTURE),
                                      System::IO::FileMode::Open,
                                      System::IO::FileAccess::Read);
        auto library = NeighbourhoodReader::Read(stream, CNAHOUSE_TEST_NEIGHBOURHOOD_FIXTURE);
        EXPECT_TRUE(library) << (library ? std::string() : library.Error().Message());
        return library ? std::move(*library) : NeighbourhoodLibrary{};
    }

    float FloatAt(const NeighbourPrimitive& primitive, std::size_t vertex, std::size_t offset)
    {
        float value = 0.0f;
        std::memcpy(&value,
                    primitive.vertices.data() + vertex * ChunkVertexStride(primitive.layout) + offset,
                    sizeof(float));
        return value;
    }

    std::uint16_t IndexAt(const NeighbourPrimitive& primitive, std::size_t at)
    {
        std::uint16_t value = 0;
        std::memcpy(&value, primitive.indices.data() + at * 2, sizeof(value));
        return value;
    }

} // namespace

TEST(NeighbourhoodRoundTripTests, TheWriterAndTheReaderAgreeOnTheHeader)
{
    const NeighbourhoodLibrary library = Load();
    // 'sha256:' then 64 hex characters -- the shape `deploy_world.py` writes, so a reader that
    // truncated the string is caught by its length as well as by its contents.
    EXPECT_EQ(library.worldHash.size(), 71u);
    EXPECT_TRUE(library.worldHash.starts_with("sha256:"));
    EXPECT_EQ(library.worldHash, ExpectedWorldHash());
    // Sorted, because the writer sorts the table and the u16 indices mean nothing otherwise.
    ASSERT_EQ(library.materials.size(), 3u);
    EXPECT_EQ(library.materials[0], "NB_IMPOSTOR");
    EXPECT_EQ(library.materials[1], "NB_ROOF_GREY");
    EXPECT_EQ(library.materials[2], "NB_WALL_CREAM");
}

TEST(NeighbourhoodRoundTripTests, TheAssetsArriveInAscendingIdOrder)
{
    const NeighbourhoodLibrary library = Load();
    ASSERT_EQ(library.assets.size(), 2u);
    EXPECT_EQ(library.assets[0].asset, "MODEL_NB_HOUSE_A_CREAM");
    EXPECT_EQ(library.assets[1].asset, "MODEL_NB_IMPOSTOR_GABLE");
    // `Find` is a binary search over that order, and a row resolves through it once per instance.
    ASSERT_NE(library.Find("MODEL_NB_IMPOSTOR_GABLE"), nullptr);
    EXPECT_EQ(library.Find("MODEL_NB_IMPOSTOR_GABLE")->primitives.size(), 1u);
    EXPECT_EQ(library.Find("MODEL_NB_HOUSE_A_CREAM"), &library.assets[0]);
    EXPECT_EQ(library.Find("MODEL_NB_HOUSE_A_SAGE"), nullptr)
        << "an asset the file does not hold must be a null, not the nearest one";
}

TEST(NeighbourhoodRoundTripTests, APrimitiveNamesTheMaterialItWasWrittenFor)
{
    const NeighbourhoodLibrary library = Load();
    ASSERT_EQ(library.assets.size(), 2u);
    const NeighbourAsset& house = library.assets[0];
    ASSERT_EQ(house.primitives.size(), 2u);
    // The writer emits the primitives sorted by material name and the table is sorted
    // independently, so this is a real lookup rather than a coincidence of ordering.
    EXPECT_EQ(library.materials[house.primitives[0].material], "NB_ROOF_GREY");
    EXPECT_EQ(library.materials[house.primitives[1].material], "NB_WALL_CREAM");
    EXPECT_EQ(library.materials[library.assets[1].primitives[0].material], "NB_IMPOSTOR");
}

TEST(NeighbourhoodRoundTripTests, AVertexIsPositionThenNormalThenUv)
{
    const NeighbourhoodLibrary library = Load();
    ASSERT_EQ(library.assets.size(), 2u);
    const NeighbourPrimitive& roof = library.assets[0].primitives[0];
    ASSERT_EQ(roof.layout, ChunkLayout::Basic);
    ASSERT_EQ(roof.vertices.size(), 3u * 32u);
    // The fixture's first vertex is deliberately asymmetric in all three fields: a writer that
    // emitted the normal where the UV goes lands on the wrong number here rather than a plausible
    // one, and the normal is not +Y so a reader that skipped it would be caught too.
    EXPECT_FLOAT_EQ(FloatAt(roof, 0, 0), -5.5f);
    EXPECT_FLOAT_EQ(FloatAt(roof, 0, 4), 6.0f);
    EXPECT_FLOAT_EQ(FloatAt(roof, 0, 8), -4.25f);
    EXPECT_FLOAT_EQ(FloatAt(roof, 0, 12), 0.0f);
    EXPECT_FLOAT_EQ(FloatAt(roof, 0, 16), 0.5f);
    EXPECT_FLOAT_EQ(FloatAt(roof, 0, 20), -0.5f);
    EXPECT_FLOAT_EQ(FloatAt(roof, 0, 24), 0.125f);
    EXPECT_FLOAT_EQ(FloatAt(roof, 0, 28), 0.25f);
}

TEST(NeighbourhoodRoundTripTests, TheIndicesAndTheBoxesSurvive)
{
    const NeighbourhoodLibrary library = Load();
    ASSERT_EQ(library.assets.size(), 2u);
    const NeighbourPrimitive& wall = library.assets[0].primitives[1];
    ASSERT_EQ(wall.indexCount, 6u);
    EXPECT_FALSE(wall.wideIndices);
    EXPECT_EQ(IndexAt(wall, 0), 0u);
    EXPECT_EQ(IndexAt(wall, 3), 0u);
    EXPECT_EQ(IndexAt(wall, 4), 2u);
    EXPECT_EQ(IndexAt(wall, 5), 3u);
    // The asset's box is the UNION of its primitives', and a primitive's is its own -- a
    // sub-box carrying the asset's would cull nothing and cost 24 bytes to do it.
    EXPECT_FLOAT_EQ(library.assets[0].bounds.Max.Y, 8.125f);
    EXPECT_FLOAT_EQ(wall.bounds.Max.Y, 6.0f);
    EXPECT_FLOAT_EQ(library.assets[0].primitives[0].bounds.Min.Y, 6.0f);
}

TEST(NeighbourhoodRoundTripTests, TheMeshIsInItsOwnSpaceAndNotTheWorlds)
{
    // The whole reason this format is not `chunks.bin`: the retained street places 118 instances of 34
    // assets, and a mesh with the placement baked in can be neither shared nor swapped for another LOD. A
    // fixture house sitting at x 90 would say the writer had baked a position.
    const NeighbourhoodLibrary library = Load();
    for (const NeighbourAsset& asset : library.assets)
    {
        EXPECT_LT(std::abs(asset.bounds.Min.X), 20.0f) << asset.asset;
        EXPECT_LT(std::abs(asset.bounds.Max.Z), 20.0f) << asset.asset;
        EXPECT_GE(asset.bounds.Min.Y, 0.0f) << asset.asset << " stands under its own ground";
    }
    EXPECT_EQ(library.GeometryBytes(), (3u + 4u + 3u) * 32u + (3u + 6u + 3u) * 2u);
}
