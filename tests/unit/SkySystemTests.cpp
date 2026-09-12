// SPDX-License-Identifier: MIT
//
// `HOUSE-01643`. The success case crosses the Python-writer/C++-reader boundary. Mutations then
// prove that the runtime does not allocate or draw a plausible-looking dome from damaged content.
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
