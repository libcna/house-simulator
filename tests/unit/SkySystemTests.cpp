// SPDX-License-Identifier: MIT
//
// `HOUSE-01643` and `HOUSE-01644`. The success case crosses the Python-writer/C++-reader boundary.
// Mutations then prove that the runtime does not allocate or draw plausible-looking sky data from
// damaged content.
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <iterator>
#include <limits>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "System/IO/MemoryStream.hpp"

#include "cnahouse/rendering/Camera.hpp"
#include "cnahouse/rendering/SkySystem.hpp"
#include "cnahouse/util/Result.hpp"

namespace
{
    using cnahouse::rendering::SkyDomeMesh;
    using cnahouse::rendering::SkyDomeReader;

    std::vector<std::uint8_t> FixtureBytes()
    {
        std::ifstream input(CNAHOUSE_TEST_SKY_DOME_FIXTURE, std::ios::binary);
        return std::vector<std::uint8_t>(std::istreambuf_iterator<char>(input),
                                         std::istreambuf_iterator<char>());
    }

    std::string SkyJson()
    {
        std::ifstream input("content/world/layout.sky.json", std::ios::binary);
        return std::string(std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>());
    }

    cnahouse::util::Result<SkyDomeMesh> ReadOf(const std::vector<std::uint8_t>& bytes)
    {
        System::IO::MemoryStream stream(bytes.data(), static_cast<int>(bytes.size()), false);
        return SkyDomeReader::Read(stream, "sky_dome.bin");
    }

    template<typename T>
    void Replace(std::vector<std::uint8_t>& bytes, std::size_t offset, const T& value)
    {
        ASSERT_LE(offset + sizeof(value), bytes.size());
        std::memcpy(bytes.data() + offset, &value, sizeof(value));
    }
} // namespace

TEST(SkySystemTests, TheGeneratedDomeCrossesTheWriterReaderBoundary)
{
    const auto mesh = ReadOf(FixtureBytes());
    ASSERT_TRUE(mesh) << (mesh ? std::string() : mesh.Error().ToString());
    EXPECT_EQ(mesh->longitudeSegments, 32u);
    EXPECT_EQ(mesh->latitudeSegments, 18u);
    EXPECT_EQ(mesh->domeVertexCount, 577u);
    EXPECT_EQ(mesh->positions.size(), 610u);
    EXPECT_EQ(mesh->indices.size(), 3648u);
    EXPECT_FLOAT_EQ(mesh->radius, 900.0F);
    EXPECT_FLOAT_EQ(mesh->skirtDepth, 90.0F);
    EXPECT_EQ(mesh->indices.size() / 3u, 1216u);
}

TEST(SkySystemTests, SizeMagicVersionFlagsAndGeometryHeaderAreAllStrict)
{
    const std::vector<std::uint8_t> good = FixtureBytes();
    ASSERT_EQ(good.size(), SkyDomeReader::kEncodedBytes);

    for (const std::size_t size : {good.size() - 1u, good.size() + 1u})
    {
        std::vector<std::uint8_t> changed = good;
        changed.resize(size);
        EXPECT_FALSE(ReadOf(changed)) << "accepted a " << size << " byte file";
    }

    for (const std::pair<std::size_t, std::uint32_t>& mutation : {
             std::pair<std::size_t, std::uint32_t>{0u, 0x45504F4Eu},
             {4u, 2u},
             {8u, 1u},
             {12u, 31u},
             {24u, 17u},
             {28u, 576u},
             {32u, 609u},
             {36u, 3645u},
         })
    {
        std::vector<std::uint8_t> changed = good;
        Replace(changed, mutation.first, mutation.second);
        EXPECT_FALSE(ReadOf(changed)) << "accepted damaged field at byte " << mutation.first;
    }
}

TEST(SkySystemTests, NonFinitePositionsAndOutOfRangeIndicesAreRejected)
{
    const std::vector<std::uint8_t> good = FixtureBytes();
    ASSERT_EQ(good.size(), SkyDomeReader::kEncodedBytes);

    std::vector<std::uint8_t> nonFinite = good;
    const float nan = std::numeric_limits<float>::quiet_NaN();
    Replace(nonFinite, 40u, nan);
    auto result = ReadOf(nonFinite);
    ASSERT_FALSE(result);
    EXPECT_EQ(result.Error().Code(), cnahouse::util::ErrorCode::InvalidData);
    EXPECT_NE(result.Error().Message().find("vertex 0"), std::string::npos);

    std::vector<std::uint8_t> badIndex = good;
    constexpr std::size_t kFirstIndex = 40u + SkyDomeReader::kVertexCount * 12u;
    const std::uint16_t outside = static_cast<std::uint16_t>(SkyDomeReader::kVertexCount);
    Replace(badIndex, kFirstIndex, outside);
    result = ReadOf(badIndex);
    ASSERT_FALSE(result);
    EXPECT_EQ(result.Error().Code(), cnahouse::util::ErrorCode::OutOfRange);
    EXPECT_NE(result.Error().Message().find("index 0"), std::string::npos);
}

TEST(SkySystemTests, DomeWorldTracksEveryCameraAxisWithoutScalingTheMesh)
{
    cnahouse::rendering::Camera camera;
    camera.eye = Microsoft::Xna::Framework::Vector3(12.5F, -3.0F, 84.25F);
    const Microsoft::Xna::Framework::Matrix world = cnahouse::rendering::SkySystem::DomeWorld(camera);

    EXPECT_FLOAT_EQ(world.M11, 1.0F);
    EXPECT_FLOAT_EQ(world.M22, 1.0F);
    EXPECT_FLOAT_EQ(world.M33, 1.0F);
    EXPECT_FLOAT_EQ(world.M41, camera.eye.X);
    EXPECT_FLOAT_EQ(world.M42, camera.eye.Y);
    EXPECT_FLOAT_EQ(world.M43, camera.eye.Z);
    EXPECT_FLOAT_EQ(world.M44, 1.0F);
}

TEST(SkySystemTests, TheGeneratedColourModelLoadsAndItsIdentityAndSampleAxesAreStrict)
{
    const std::string json = SkyJson();
    auto model = cnahouse::rendering::SkyColourModelReader::Read(json, "layout.sky.json");
    ASSERT_TRUE(model) << (model ? std::string() : model.Error().ToString());
    ASSERT_EQ(model->gradient.size(), 32u);
    EXPECT_DOUBLE_EQ(model->gradient.front().sunElevationDeg, -18.0);
    EXPECT_DOUBLE_EQ(model->gradient.back().sunElevationDeg, 90.0);
    EXPECT_EQ(model->cloudCoverSamples, 8u);
    EXPECT_EQ(model->azimuthOffsetSamples, 16u);

    for (const std::pair<std::string, std::string>& mutation : {
             std::pair<std::string, std::string>{"cna-house/sky/1", "cna-house/sky/2"},
             {"\"cloudCoverSamples\": 8", "\"cloudCoverSamples\": 7"},
             {"\"azimuthOffsetSamples\": 16", "\"azimuthOffsetSamples\": 15"},
         })
    {
        std::string changed = json;
        const std::size_t at = changed.find(mutation.first);
        ASSERT_NE(at, std::string::npos);
        changed.replace(at, mutation.first.size(), mutation.second);
        EXPECT_FALSE(cnahouse::rendering::SkyColourModelReader::Read(changed, "layout.sky.json"));
    }
}

TEST(SkySystemTests, ColoursFollowAltitudeAndOvercastButUpdatesAreMaterialNotPerFrame)
{
    auto mesh = ReadOf(FixtureBytes());
    auto model = cnahouse::rendering::SkyColourModelReader::Read(SkyJson(), "layout.sky.json");
    ASSERT_TRUE(mesh);
    ASSERT_TRUE(model);
    cnahouse::rendering::Camera camera;
    cnahouse::rendering::SkySystem sky(camera, std::move(*mesh), std::move(*model));

    ASSERT_EQ(sky.ColouredVertices().size(), 610u);
    const Microsoft::Xna::Framework::Vector3 pole = sky.ColouredVertices()[0].Color.ToVector3();
    constexpr std::size_t kFirstHorizonVertex = 1u + 17u * 32u;
    const Microsoft::Xna::Framework::Vector3 horizon =
        sky.ColouredVertices()[kFirstHorizonVertex].Color.ToVector3();
    EXPECT_NEAR(pole.X, 0.0100F, 1.0F / 255.0F);
    EXPECT_NEAR(pole.Y, 0.0120F, 1.0F / 255.0F);
    EXPECT_NEAR(pole.Z, 0.0300F, 1.0F / 255.0F);
    EXPECT_NEAR(horizon.X, 0.0180F, 1.0F / 255.0F);
    EXPECT_NEAR(horizon.Y, 0.0200F, 1.0F / 255.0F);
    EXPECT_NEAR(horizon.Z, 0.0480F, 1.0F / 255.0F);

    const std::uint64_t initialUpdates = sky.ColourUpdateCount();
    for (int frame = 0; frame < 600; ++frame)
    {
        const double elevation = -18.0 + 0.5 * static_cast<double>(frame) / 599.0;
        sky.SetSky(elevation, 0.0);
    }
    EXPECT_GE(sky.ColourUpdateCount(), initialUpdates + 1u);
    EXPECT_LE(sky.ColourUpdateCount(), initialUpdates + 2u)
        << "a half-degree move over 600 frames was recomputed like a per-frame effect";

    ASSERT_TRUE(sky.SetSky(30.0, 1.0));
    const auto grey = sky.ColouredVertices().front().Color;
    for (const auto& vertex : sky.ColouredVertices())
    {
        EXPECT_EQ(vertex.Color, grey) << "full overcast did not collapse altitude variation";
    }
    EXPECT_FALSE(sky.SetSky(30.25, 0.991));
    EXPECT_TRUE(sky.SetSky(30.251, 0.991));

    double totalMilliseconds = 0.0;
    double maximumMilliseconds = 0.0;
    constexpr int kMeasuredUpdates = 64;
    for (int update = 0; update < kMeasuredUpdates; ++update)
    {
        ASSERT_TRUE(sky.SetSky((update % 2 == 0) ? -18.0 : 90.0, (update % 3 == 0) ? 0.0 : 0.5));
        totalMilliseconds += sky.LastColourMilliseconds();
        maximumMilliseconds = std::max(maximumMilliseconds, sky.LastColourMilliseconds());
    }
    const double meanMilliseconds = totalMilliseconds / static_cast<double>(kMeasuredUpdates);
    RecordProperty("mean_colour_update_ms", std::to_string(meanMilliseconds));
    RecordProperty("max_colour_update_ms", std::to_string(maximumMilliseconds));
    EXPECT_LT(maximumMilliseconds, 5.0)
        << "recolouring 610 CPU vertices is no longer negligible (mean " << meanMilliseconds << " ms)";
}
