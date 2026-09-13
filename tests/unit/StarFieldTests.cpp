// SPDX-License-Identifier: MIT
//
// `HOUSE-01610`. The success case crosses the Python CSTR writer/C++ runtime reader boundary;
// mutations and pure geometry checks keep damaged catalogues and fake billboards off the GPU.
#include <cmath>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <iterator>
#include <limits>
#include <string>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include "System/IO/MemoryStream.hpp"

#include "cnahouse/rendering/StarField.hpp"

namespace
{
    namespace Xna = Microsoft::Xna::Framework;
    using cnahouse::rendering::StarCatalogue;
    using cnahouse::rendering::StarCatalogueReader;

    std::vector<std::uint8_t> FixtureBytes()
    {
        std::ifstream input(CNAHOUSE_TEST_STAR_CATALOGUE_FIXTURE, std::ios::binary);
        return std::vector<std::uint8_t>(std::istreambuf_iterator<char>(input),
                                         std::istreambuf_iterator<char>());
    }

    cnahouse::util::Result<StarCatalogue> ReadOf(const std::vector<std::uint8_t>& bytes)
    {
        System::IO::MemoryStream stream(bytes.data(), static_cast<int>(bytes.size()), false);
        return StarCatalogueReader::Read(stream, "stars.bin");
    }

    template<typename T>
    void Replace(std::vector<std::uint8_t>& bytes, std::size_t offset, const T& value)
    {
        ASSERT_LE(offset + sizeof(value), bytes.size());
        std::memcpy(bytes.data() + offset, &value, sizeof(value));
    }

    float Dot(const Xna::Vector3& a, const Xna::Vector3& b)
    {
        return a.X * b.X + a.Y * b.Y + a.Z * b.Z;
    }

    Xna::Vector3 Subtract(const Xna::Vector3& a, const Xna::Vector3& b)
    {
        return Xna::Vector3(a.X - b.X, a.Y - b.Y, a.Z - b.Z);
    }
} // namespace

TEST(StarFieldTests, GeneratedCatalogueCrossesTheWriterReaderBoundary)
{
    const auto catalogue = ReadOf(FixtureBytes());
    ASSERT_TRUE(catalogue) << (catalogue ? std::string() : catalogue.Error().ToString());
    ASSERT_EQ(catalogue->size(), StarCatalogueReader::kStarCount);
    EXPECT_FLOAT_EQ(catalogue->front().visualMagnitude, -1.46F);
    EXPECT_NEAR(catalogue->front().rightAscensionDeg, 101.287083F, 1e-4F);
    EXPECT_NEAR(catalogue->front().declinationDeg, -16.716111F, 1e-4F);
    EXPECT_FLOAT_EQ(catalogue->front().bvColourIndex, 0.0F);
    EXPECT_FLOAT_EQ(catalogue->back().visualMagnitude, 4.94F);
}

TEST(StarFieldTests, SizeHeaderFieldsAndBrightestFirstOrderingAreStrict)
{
    const std::vector<std::uint8_t> good = FixtureBytes();
    ASSERT_EQ(good.size(), StarCatalogueReader::kEncodedBytes);
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
             {12u, 1499u},
         })
    {
        std::vector<std::uint8_t> changed = good;
        Replace(changed, mutation.first, mutation.second);
        EXPECT_FALSE(ReadOf(changed)) << "accepted damaged header field at byte " << mutation.first;
    }

    std::vector<std::uint8_t> outOfOrder = good;
    const float illegallyBrighterSecond = -1.75F;
    Replace(outOfOrder, 16u + 16u + 8u, illegallyBrighterSecond);
    const auto result = ReadOf(outOfOrder);
    ASSERT_FALSE(result);
    EXPECT_NE(result.Error().Message().find("star 1"), std::string::npos);
}

TEST(StarFieldTests, NonFiniteAndOutOfRangeCatalogueFieldsAreRejected)
{
    const std::vector<std::uint8_t> good = FixtureBytes();
    std::vector<std::uint8_t> nonFinite = good;
    const float nan = std::numeric_limits<float>::quiet_NaN();
    Replace(nonFinite, 16u, nan);
    auto result = ReadOf(nonFinite);
    ASSERT_FALSE(result);
    EXPECT_EQ(result.Error().Code(), cnahouse::util::ErrorCode::InvalidData);

    std::vector<std::uint8_t> badDeclination = good;
    const float outside = 90.01F;
    Replace(badDeclination, 20u, outside);
    result = ReadOf(badDeclination);
    ASSERT_FALSE(result);
    EXPECT_EQ(result.Error().Code(), cnahouse::util::ErrorCode::OutOfRange);
}

TEST(StarFieldTests, EquatorialAxesAreUnitLengthAndPreserveRightAscensionConvention)
{
    const Xna::Vector3 equinox = cnahouse::rendering::EquatorialDirection(0.0F, 0.0F);
    const Xna::Vector3 sixHours = cnahouse::rendering::EquatorialDirection(90.0F, 0.0F);
    const Xna::Vector3 pole = cnahouse::rendering::EquatorialDirection(0.0F, 90.0F);
    EXPECT_NEAR(equinox.X, 1.0F, 1e-6F);
    EXPECT_NEAR(equinox.Y, 0.0F, 1e-6F);
    EXPECT_NEAR(equinox.Z, 0.0F, 1e-6F);
    EXPECT_NEAR(sixHours.X, 0.0F, 1e-6F);
    EXPECT_NEAR(sixHours.Z, -1.0F, 1e-6F);
    EXPECT_NEAR(pole.Y, 1.0F, 1e-6F);
    EXPECT_NEAR(Dot(cnahouse::rendering::EquatorialDirection(213.2F, -48.7F),
                    cnahouse::rendering::EquatorialDirection(213.2F, -48.7F)),
                1.0F,
                1e-6F);
}

TEST(StarFieldTests, MagnitudeDrivesSizeAndAlphaWhileBvSamplesAClampedColourLut)
{
    const auto brightBlue = cnahouse::rendering::AppearanceForStar(-1.46F, -0.4F);
    const auto faintRed = cnahouse::rendering::AppearanceForStar(4.94F, 2.0F);
    EXPECT_GT(brightBlue.halfSize, faintRed.halfSize);
    EXPECT_GT(brightBlue.alpha, faintRed.alpha);
    EXPECT_GT(brightBlue.colour.Z, brightBlue.colour.X);
    EXPECT_GT(faintRed.colour.X, faintRed.colour.Z);

    const auto clampedBlue = cnahouse::rendering::AppearanceForStar(-20.0F, -20.0F);
    const auto clampedRed = cnahouse::rendering::AppearanceForStar(20.0F, 20.0F);
    EXPECT_FLOAT_EQ(clampedBlue.alpha, 1.0F);
    EXPECT_FLOAT_EQ(clampedBlue.colour.X, brightBlue.colour.X);
    EXPECT_FLOAT_EQ(clampedRed.colour.Z, faintRed.colour.Z);
}

TEST(StarFieldTests, EveryQuadFacesTheObserverAtTheAuthoredCelestialRadius)
{
    const StarCatalogue catalogue{
        {0.0F, 0.0F, -1.0F, 0.0F}, {90.0F, 30.0F, 4.0F, 1.5F}, {0.0F, 90.0F, 2.0F, 0.6F}};
    const auto vertices = cnahouse::rendering::BuildStarVertices(catalogue);
    ASSERT_EQ(vertices.size(), catalogue.size() * 4u);
    for (std::size_t star = 0; star < catalogue.size(); ++star)
    {
        const Xna::Vector3 direction = cnahouse::rendering::EquatorialDirection(
            catalogue[star].rightAscensionDeg, catalogue[star].declinationDeg);
        const Xna::Vector3 centre(direction.X * 880.0F, direction.Y * 880.0F, direction.Z * 880.0F);
        for (std::size_t corner = 0; corner < 4u; ++corner)
        {
            const Xna::Vector3 offset = Subtract(vertices[star * 4u + corner].Position, centre);
            EXPECT_NEAR(Dot(offset, direction), 0.0F, 1e-3F)
                << "star " << star << " corner " << corner << " is not in its billboard plane";
        }
        EXPECT_EQ(vertices[star * 4u].Color, vertices[star * 4u + 3u].Color);
    }
}
