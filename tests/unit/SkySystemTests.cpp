// SPDX-License-Identifier: MIT
//
// `HOUSE-01643` through `HOUSE-01649`. The success case crosses the Python-writer/C++-reader
// boundary.
// Mutations then prove that the runtime does not allocate or draw plausible-looking sky data from
// damaged content.
#include <algorithm>
#include <array>
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

#include "cnahouse/environment/MoonModel.hpp"
#include "cnahouse/environment/SunModel.hpp"
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
    ASSERT_EQ(model->sunIntensity.size(), 7u);
    EXPECT_DOUBLE_EQ(model->sunIntensity.front().elevationDeg, -6.0);
    EXPECT_FLOAT_EQ(model->sunIntensity.front().intensity, 0.0F);
    EXPECT_DOUBLE_EQ(model->sunIntensity.back().elevationDeg, 90.0);
    EXPECT_FLOAT_EQ(model->sunIntensity.back().intensity, 1.0F);
    EXPECT_EQ(model->cloudLayers[0].id, "CL_HIGH");
    EXPECT_EQ(model->cloudLayers[1].id, "CL_MID");
    EXPECT_EQ(model->cloudLayers[2].id, "CL_LOW");
    EXPECT_EQ(model->cloudLayers[0].texture, "Textures/Sky/cloud_cirrus");
    EXPECT_FLOAT_EQ(model->cloudLayers[0].radius, 880.0F);
    EXPECT_FLOAT_EQ(model->cloudLayers[1].radius, 860.0F);
    EXPECT_FLOAT_EQ(model->cloudLayers[2].radius, 830.0F);
    EXPECT_FLOAT_EQ(model->cloudLayers[0].scrollScale, 0.15F);
    EXPECT_FLOAT_EQ(model->cloudLayers[1].scrollScale, 0.60F);
    EXPECT_FLOAT_EQ(model->cloudLayers[2].scrollScale, 1.00F);
    ASSERT_EQ(model->cloudAlphaBands.size(), 5u);
    EXPECT_FLOAT_EQ(model->cloudAlphaBands.front().minimumCover, 0.0F);
    EXPECT_FLOAT_EQ(model->cloudAlphaBands.back().maximumCover, 1.0F);
    EXPECT_EQ(model->stormCloudAlpha, (std::array<float, 3>{0.0F, 0.7F, 1.0F}));

    for (const std::pair<std::string, std::string>& mutation : {
             std::pair<std::string, std::string>{"cna-house/sky/1", "cna-house/sky/2"},
             {"\"cloudCoverSamples\": 8", "\"cloudCoverSamples\": 7"},
             {"\"azimuthOffsetSamples\": 16", "\"azimuthOffsetSamples\": 15"},
             {"\"id\": \"CL_HIGH\"", "\"id\": \"CL_LOW\""},
             {"\"altitude\": 860.0", "\"altitude\": 890.0"},
             {"\"scrollScale\": 0.15", "\"scrollScale\": -0.15"},
             {"\"cloudCover\": [\n        0.0", "\"cloudCover\": [\n        0.01"},
             {"\"stormCloudAlpha\": {\n    \"high\": 0.0", "\"stormCloudAlpha\": {\n    \"high\": 1.2"},
             {"\"intensity\": 0.0", "\"intensity\": 1.2"},
         })
    {
        std::string changed = json;
        const std::size_t at = changed.find(mutation.first);
        ASSERT_NE(at, std::string::npos);
        changed.replace(at, mutation.first.size(), mutation.second);
        EXPECT_FALSE(cnahouse::rendering::SkyColourModelReader::Read(changed, "layout.sky.json"));
    }
}

TEST(SkySystemTests, SunGlowIsDirectionalMatchesTheAuthoredCurveAndVanishesUnderCloud)
{
    auto mesh = ReadOf(FixtureBytes());
    auto model = cnahouse::rendering::SkyColourModelReader::Read(SkyJson(), "layout.sky.json");
    ASSERT_TRUE(mesh);
    ASSERT_TRUE(model);
    cnahouse::rendering::Camera camera;
    cnahouse::rendering::SkySystem sky(camera, std::move(*mesh), std::move(*model));

    cnahouse::environment::SunPosition sun;
    sun.altitudeDeg = 0.0;
    sun.azimuthDeg = 90.0; // east, +X
    cnahouse::environment::MoonPosition moon;
    moon.altitudeDeg = -20.0;
    const cnahouse::environment::MoonPhase phase{};
    ASSERT_TRUE(sky.SetSky(sun, moon, phase, 0.0));

    const auto east = std::max_element(sky.ColouredVertices().begin(),
                                       sky.ColouredVertices().end(),
                                       [](const auto& left, const auto& right)
                                       { return left.Position.X < right.Position.X; });
    const auto west = std::min_element(sky.ColouredVertices().begin(),
                                       sky.ColouredVertices().end(),
                                       [](const auto& left, const auto& right)
                                       { return left.Position.X < right.Position.X; });
    ASSERT_NE(east, sky.ColouredVertices().end());
    ASSERT_NE(west, sky.ColouredVertices().end());
    const float eastRed = east->Color.ToVector3().X;
    const float westRed = west->Color.ToVector3().X;
    EXPECT_GT(eastRed - westRed, 0.07F)
        << "the generated 0-degree sun curve's 0.45 intensity did not drive its 0.18 glow";

    sun.azimuthDeg = 91.0;
    EXPECT_FALSE(sky.SetSky(sun, moon, phase, 0.0));
    sun.azimuthDeg = 91.001;
    EXPECT_TRUE(sky.SetSky(sun, moon, phase, 0.0));

    ASSERT_TRUE(sky.SetSky(sun, moon, phase, 1.0));
    EXPECT_EQ(east->Color, west->Color) << "(1-cloudCover)^2 did not extinguish directional glow";
}

TEST(SkySystemTests, AstronomicalNightRespondsToMoonAltitudeAndPhase)
{
    auto mesh = ReadOf(FixtureBytes());
    auto model = cnahouse::rendering::SkyColourModelReader::Read(SkyJson(), "layout.sky.json");
    ASSERT_TRUE(mesh);
    ASSERT_TRUE(model);
    cnahouse::rendering::Camera camera;
    cnahouse::rendering::SkySystem sky(camera, std::move(*mesh), std::move(*model));

    cnahouse::environment::SunPosition sun;
    sun.altitudeDeg = -18.0;
    cnahouse::environment::MoonPosition moon;
    moon.altitudeDeg = 60.0;
    cnahouse::environment::MoonPhase phase;
    phase.illuminatedFraction = 1.0;
    ASSERT_TRUE(sky.SetSky(sun, moon, phase, 0.0));
    moon.altitudeDeg = 61.0;
    EXPECT_FALSE(sky.SetSky(sun, moon, phase, 0.0));
    moon.altitudeDeg = 61.001;
    EXPECT_TRUE(sky.SetSky(sun, moon, phase, 0.0));
    constexpr std::size_t kFirstHorizonVertex = 1u + 17u * 32u;
    const auto fullMoon = sky.ColouredVertices()[kFirstHorizonVertex].Color.ToVector3();

    phase.illuminatedFraction = 0.0;
    ASSERT_TRUE(sky.SetSky(sun, moon, phase, 0.0));
    const auto newMoon = sky.ColouredVertices()[kFirstHorizonVertex].Color.ToVector3();
    EXPECT_GT(fullMoon.Z, newMoon.Z) << "an elevated full moon did not lift the night sky";
    phase.illuminatedFraction = 1.0 / 128.0;
    EXPECT_FALSE(sky.SetSky(sun, moon, phase, 0.0));
    phase.illuminatedFraction += 1.0e-6;
    EXPECT_TRUE(sky.SetSky(sun, moon, phase, 0.0));

    moon.altitudeDeg = -1.0;
    phase.illuminatedFraction = 1.0;
    ASSERT_TRUE(sky.SetSky(sun, moon, phase, 0.0));
    EXPECT_EQ(sky.ColouredVertices()[kFirstHorizonVertex].Color.ToVector3(), newMoon)
        << "a moon below the geometric horizon brightened the night";
}

TEST(SkySystemTests, ThreeCloudRingsHaveTheAuthoredRadiiAndNoDegenerateTriangles)
{
    auto model = cnahouse::rendering::SkyColourModelReader::Read(SkyJson(), "layout.sky.json");
    ASSERT_TRUE(model);
    const auto rings = cnahouse::rendering::SkySystem::BuildCloudRings(model->cloudLayers);
    constexpr std::array<float, 3> kRadii{880.0F, 860.0F, 830.0F};

    for (std::size_t layer = 0; layer < rings.size(); ++layer)
    {
        const auto& ring = rings[layer];
        ASSERT_EQ(ring.vertices.size(), cnahouse::rendering::SkySystem::kCloudVerticesPerLayer);
        ASSERT_EQ(ring.indices.size(), cnahouse::rendering::SkySystem::kCloudIndicesPerLayer);
        float smallestAreaSquared = std::numeric_limits<float>::max();
        for (std::size_t index = 0; index < ring.indices.size(); index += 3u)
        {
            ASSERT_LT(ring.indices[index], ring.vertices.size());
            ASSERT_LT(ring.indices[index + 1u], ring.vertices.size());
            ASSERT_LT(ring.indices[index + 2u], ring.vertices.size());
            const auto& a = ring.vertices[ring.indices[index]].Position;
            const auto& b = ring.vertices[ring.indices[index + 1u]].Position;
            const auto& c = ring.vertices[ring.indices[index + 2u]].Position;
            const float abX = b.X - a.X;
            const float abY = b.Y - a.Y;
            const float abZ = b.Z - a.Z;
            const float acX = c.X - a.X;
            const float acY = c.Y - a.Y;
            const float acZ = c.Z - a.Z;
            const float crossX = abY * acZ - abZ * acY;
            const float crossY = abZ * acX - abX * acZ;
            const float crossZ = abX * acY - abY * acX;
            smallestAreaSquared =
                std::min(smallestAreaSquared, crossX * crossX + crossY * crossY + crossZ * crossZ);
        }
        EXPECT_GT(smallestAreaSquared, 1.0F) << "layer " << layer;
        EXPECT_FLOAT_EQ(ring.vertices.front().Position.Y, kRadii[layer]);
        EXPECT_NEAR(ring.vertices.back().Position.Y, 0.0F, 0.001F);
        for (const auto& vertex : ring.vertices)
        {
            const float radius =
                std::sqrt(vertex.Position.X * vertex.Position.X + vertex.Position.Y * vertex.Position.Y +
                          vertex.Position.Z * vertex.Position.Z);
            EXPECT_NEAR(radius, kRadii[layer], 0.001F);
            EXPECT_FLOAT_EQ(vertex.TextureCoordinate.X,
                            vertex.Position.X / cnahouse::rendering::SkySystem::kCloudTextureRepeatMetres);
            EXPECT_FLOAT_EQ(vertex.TextureCoordinate.Y,
                            vertex.Position.Z / cnahouse::rendering::SkySystem::kCloudTextureRepeatMetres);
        }
    }
}

TEST(SkySystemTests, CloudUvMotionFollowsMeteorologicalWindAndEachAuthoredRate)
{
    auto mesh = ReadOf(FixtureBytes());
    auto model = cnahouse::rendering::SkyColourModelReader::Read(SkyJson(), "layout.sky.json");
    ASSERT_TRUE(mesh);
    ASSERT_TRUE(model);
    cnahouse::rendering::Camera camera;
    cnahouse::rendering::SkySystem sky(camera, std::move(*mesh), std::move(*model));

    ASSERT_TRUE(sky.SetWind(12.0, 0.0)); // from north: the visible pattern travels south (+Z)
    ASSERT_TRUE(sky.AdvanceClouds(10.0));
    EXPECT_NEAR(sky.CloudOffsets()[0].X, 0.0F, 1.0e-6F);
    EXPECT_NEAR(sky.CloudOffsets()[0].Y, -0.075F, 1.0e-6F);
    EXPECT_NEAR(sky.CloudOffsets()[1].Y, -0.300F, 1.0e-6F);
    EXPECT_NEAR(sky.CloudOffsets()[2].Y, -0.500F, 1.0e-6F);

    const auto beforeInvalid = sky.CloudOffsets();
    EXPECT_FALSE(sky.SetWind(std::numeric_limits<double>::quiet_NaN(), 90.0));
    EXPECT_FALSE(sky.AdvanceClouds(-1.0));
    EXPECT_EQ(sky.CloudOffsets(), beforeInvalid);

    ASSERT_TRUE(sky.SetWind(10.0, 90.0)); // from east: the visible pattern travels west (-X)
    ASSERT_TRUE(sky.AdvanceClouds(1.0));
    EXPECT_GT(sky.CloudOffsets()[0].X, 0.0F)
        << "sampling must move east for the texture feature itself to move west";
    for (const auto& offset : sky.CloudOffsets())
    {
        EXPECT_LE(std::abs(offset.X), 0.5F);
        EXPECT_LE(std::abs(offset.Y), 0.5F);
    }
}

TEST(SkySystemTests, CloudLayerAlphasInterpolateContinuouslyThroughCoverAndThunder)
{
    auto model = cnahouse::rendering::SkyColourModelReader::Read(SkyJson(), "layout.sky.json");
    ASSERT_TRUE(model);

    const auto clear = cnahouse::rendering::SkySystem::CloudAlphasFor(*model, 0.0, 0.0);
    EXPECT_EQ(clear, (std::array<float, 3>{0.15F, 0.0F, 0.0F}));
    const auto mostly = cnahouse::rendering::SkySystem::CloudAlphasFor(*model, 0.20, 0.0);
    EXPECT_EQ(mostly, (std::array<float, 3>{0.35F, 0.20F, 0.0F}));

    // 0.325 is halfway from the mostly-clear band's 0.20 centre to partly-cloudy's 0.45.
    const auto between = cnahouse::rendering::SkySystem::CloudAlphasFor(*model, 0.325, 0.0);
    EXPECT_NEAR(between[0], 0.325F, 1.0e-6F);
    EXPECT_NEAR(between[1], 0.400F, 1.0e-6F);
    EXPECT_NEAR(between[2], 0.025F, 1.0e-6F);
    const auto justBefore = cnahouse::rendering::SkySystem::CloudAlphasFor(*model, 0.29999, 0.0);
    const auto justAfter = cnahouse::rendering::SkySystem::CloudAlphasFor(*model, 0.30001, 0.0);
    for (std::size_t i = 0; i < justBefore.size(); ++i)
    {
        EXPECT_NEAR(justBefore[i], justAfter[i], 0.0001F)
            << "a band boundary became a visible state swap in layer " << i;
    }

    const auto storm = cnahouse::rendering::SkySystem::CloudAlphasFor(*model, 0.2, 1.0);
    EXPECT_EQ(storm, (std::array<float, 3>{0.0F, 0.7F, 1.0F}));
    const auto halfStorm = cnahouse::rendering::SkySystem::CloudAlphasFor(*model, 0.325, 0.5);
    EXPECT_NEAR(halfStorm[0], 0.1625F, 1.0e-6F);
    EXPECT_NEAR(halfStorm[1], 0.5500F, 1.0e-6F);
    EXPECT_NEAR(halfStorm[2], 0.5125F, 1.0e-6F);

    auto mesh = ReadOf(FixtureBytes());
    ASSERT_TRUE(mesh);
    cnahouse::rendering::Camera camera;
    cnahouse::rendering::SkySystem sky(camera, std::move(*mesh), std::move(*model));
    EXPECT_TRUE(sky.SetCloudState(0.325, 0.5));
    EXPECT_EQ(sky.CloudAlphas(), halfStorm);
    EXPECT_FALSE(sky.SetCloudState(0.325, 0.5));
    EXPECT_FALSE(sky.SetCloudState(std::numeric_limits<double>::quiet_NaN(), 0.5));
    EXPECT_EQ(sky.CloudAlphas(), halfStorm);
}

TEST(SkySystemTests, EveryCloudLayerTakesTheLiveHorizonTint)
{
    auto mesh = ReadOf(FixtureBytes());
    auto model = cnahouse::rendering::SkyColourModelReader::Read(SkyJson(), "layout.sky.json");
    ASSERT_TRUE(mesh);
    ASSERT_TRUE(model);
    cnahouse::rendering::Camera camera;
    cnahouse::rendering::SkySystem sky(camera, std::move(*mesh), std::move(*model));

    ASSERT_TRUE(sky.SetSky(-0.58, 0.0));
    const auto sunset = sky.CloudRings()[0].vertices.front().Color.ToVector3();
    EXPECT_NEAR(sunset.X, 0.7634F, 1.0F / 255.0F);
    EXPECT_NEAR(sunset.Y, 0.3576F, 1.0F / 255.0F);
    EXPECT_NEAR(sunset.Z, 0.1519F, 1.0F / 255.0F);
    EXPECT_GT(sunset.X, sunset.Y);
    EXPECT_GT(sunset.Y, sunset.Z) << "sunset did not warm the cloud texture";
    for (const auto& ring : sky.CloudRings())
    {
        for (const auto& vertex : ring.vertices)
        {
            EXPECT_EQ(vertex.Color, ring.vertices.front().Color);
        }
    }

    ASSERT_TRUE(sky.SetSky(60.0, 0.0));
    const auto daytimeTint = sky.CloudRings()[1].vertices.front().Color.ToVector3();
    EXPECT_LT(daytimeTint.X, daytimeTint.Y);
    EXPECT_LT(daytimeTint.Y, daytimeTint.Z) << "daylight did not restore the blue sky tint";

    ASSERT_TRUE(sky.SetSky(60.0, 1.0));
    const auto overcast = sky.CloudRings()[2].vertices.back().Color.ToVector3();
    EXPECT_NEAR(overcast.X, 0.370F, 1.0F / 255.0F);
    EXPECT_NEAR(overcast.Y, 0.400F, 1.0F / 255.0F);
    EXPECT_NEAR(overcast.Z, 0.440F, 1.0F / 255.0F);
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
