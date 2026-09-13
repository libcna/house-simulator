// SPDX-License-Identifier: MIT
//
// `HOUSE-01610`. The success case crosses the Python CSTR writer/C++ runtime reader boundary;
// mutations and pure geometry checks keep damaged catalogues and fake billboards off the GPU.
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <iterator>
#include <limits>
#include <numbers>
#include <string>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include "System/IO/MemoryStream.hpp"

#include "cnahouse/environment/MoonModel.hpp"
#include "cnahouse/environment/SimClock.hpp"
#include "cnahouse/environment/SunModel.hpp"
#include "cnahouse/rendering/Camera.hpp"
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

    cnahouse::environment::SimClock At(int year, int month, int day, int hour)
    {
        cnahouse::environment::SimClock clock;
        clock.calendarDaysPerSimDay = 1.0;
        cnahouse::environment::CivilTime time;
        time.year = year;
        time.month = month;
        time.day = day;
        time.hour = hour;
        clock.SetStandard(time);
        return clock;
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

TEST(StarFieldTests, VisibilityCombinesTwilightCloudAndVisibleMoonExactlyOnce)
{
    cnahouse::environment::SunPosition sun;
    cnahouse::environment::MoonPosition moon;
    cnahouse::environment::MoonPhase phase;
    moon.altitudeDeg = -1.0;
    phase.illuminatedFraction = 1.0;

    sun.altitudeDeg = -4.0;
    auto visibility = cnahouse::rendering::StarVisibilityFor(sun, moon, phase, 0.0, -1.46F, 4.94F);
    EXPECT_FLOAT_EQ(visibility.twilight, 0.0F);
    EXPECT_FLOAT_EQ(visibility.overallAlpha, 0.0F);
    EXPECT_FLOAT_EQ(visibility.magnitudeCutoff, -1.46F);

    sun.altitudeDeg = -14.0;
    visibility = cnahouse::rendering::StarVisibilityFor(sun, moon, phase, 0.0, -1.46F, 4.94F);
    EXPECT_FLOAT_EQ(visibility.twilight, 1.0F);
    EXPECT_FLOAT_EQ(visibility.moonBrightness, 0.0F) << "a moon below the horizon is not sky glow";
    EXPECT_FLOAT_EQ(visibility.overallAlpha, 1.0F);
    EXPECT_FLOAT_EQ(visibility.magnitudeCutoff, 4.94F);

    sun.altitudeDeg = -9.0;
    moon.altitudeDeg = 90.0;
    visibility = cnahouse::rendering::StarVisibilityFor(sun, moon, phase, 0.25, -1.46F, 4.94F);
    const double cloud = std::pow(0.75, cnahouse::rendering::kStarCloudExponent);
    EXPECT_FLOAT_EQ(visibility.twilight, 0.5F);
    EXPECT_NEAR(visibility.cloudTransmission, cloud, 1e-7);
    EXPECT_FLOAT_EQ(visibility.moonBrightness, 1.0F);
    EXPECT_NEAR(
        visibility.overallAlpha, 0.5 * cloud * (1.0 - cnahouse::rendering::kStarMoonSuppression), 1e-7);
    EXPECT_NEAR(visibility.magnitudeCutoff, 1.74F, 1e-6F);

    sun.altitudeDeg = -14.0;
    visibility = cnahouse::rendering::StarVisibilityFor(sun, moon, phase, 1.0, -1.46F, 4.94F);
    EXPECT_FLOAT_EQ(visibility.cloudTransmission, 0.0F);
    EXPECT_FLOAT_EQ(visibility.overallAlpha, 0.0F);
}

TEST(StarFieldTests, TwilightRevealsOnlyTheBrightestPrefixAndReusesVertexStorage)
{
    cnahouse::rendering::Camera camera;
    const StarCatalogue catalogue{{0.0F, 0.0F, -1.0F, 0.0F},
                                  {30.0F, 10.0F, 0.0F, 0.3F},
                                  {60.0F, 20.0F, 2.0F, 0.6F},
                                  {90.0F, 30.0F, 4.0F, 1.0F}};
    cnahouse::rendering::StarField field(camera, catalogue);
    const auto* storage = field.Vertices().data();
    cnahouse::environment::SimClock clock;
    cnahouse::environment::SunPosition sun;
    cnahouse::environment::MoonPosition moon;
    cnahouse::environment::MoonPhase phase;
    sun.altitudeDeg = -9.0;
    moon.altitudeDeg = -10.0;

    ASSERT_TRUE(field.SetCelestial(clock, sun, moon, phase, 0.0));
    EXPECT_EQ(field.Vertices().data(), storage);
    ASSERT_EQ(field.VisibleStarCount(), 2u);
    EXPECT_GT(field.Vertices()[0].Color.getAProperty(), 0);
    EXPECT_GT(field.Vertices()[4].Color.getAProperty(), 0);
    EXPECT_EQ(field.Vertices()[8].Color.getAProperty(), 0);
    EXPECT_EQ(field.Vertices()[12].Color.getAProperty(), 0);

    sun.altitudeDeg = -14.0;
    ASSERT_TRUE(field.SetCelestial(clock, sun, moon, phase, 0.0));
    EXPECT_EQ(field.VisibleStarCount(), catalogue.size());
    EXPECT_EQ(field.Vertices().data(), storage);

    sun.altitudeDeg = -4.0;
    ASSERT_TRUE(field.SetCelestial(clock, sun, moon, phase, 0.0));
    EXPECT_EQ(field.VisibleStarCount(), 0u);
    EXPECT_EQ(field.Vertices()[0].Color.getAProperty(), 0);
}

TEST(StarFieldTests, TwinkleAmplitudeRisesTowardTheHorizonAndStaysBounded)
{
    EXPECT_FLOAT_EQ(cnahouse::rendering::StarTwinkleAmplitude(1.0F),
                    cnahouse::rendering::kStarZenithTwinkleAmplitude);
    EXPECT_FLOAT_EQ(cnahouse::rendering::StarTwinkleAmplitude(0.5F), 0.08F);
    EXPECT_FLOAT_EQ(cnahouse::rendering::StarTwinkleAmplitude(0.1F),
                    cnahouse::rendering::kStarMaximumTwinkleAmplitude);
    EXPECT_FLOAT_EQ(cnahouse::rendering::StarTwinkleAmplitude(0.0F), 0.0F);
    EXPECT_FLOAT_EQ(cnahouse::rendering::StarTwinkleAmplitude(-0.5F), 0.0F);
    EXPECT_FLOAT_EQ(cnahouse::rendering::StarTwinkleAmplitude(std::numeric_limits<float>::quiet_NaN()), 0.0F);

    for (std::size_t star = 0; star < 1500u; ++star)
    {
        for (std::uint64_t tick = 0; tick < 40u; ++tick)
        {
            const float factor = cnahouse::rendering::StarTwinkleFactor(star, 0.1F, tick);
            EXPECT_GE(factor, 1.0F - cnahouse::rendering::kStarMaximumTwinkleAmplitude);
            EXPECT_LE(factor, 1.0F + cnahouse::rendering::kStarMaximumTwinkleAmplitude);
            EXPECT_FLOAT_EQ(factor, cnahouse::rendering::StarTwinkleFactor(star, 0.1F, tick));
        }
    }
}

TEST(StarFieldTests, TwinkleSamplesExactlyAtTwentyHertzAndKeepsTheRemainder)
{
    cnahouse::rendering::Camera camera;
    const StarCatalogue catalogue{{0.0F, 90.0F, -1.0F, 0.0F}};
    cnahouse::rendering::StarField field(camera, catalogue);
    const auto* storage = field.Vertices().data();
    const auto initialColour = field.Vertices().front().Color;
    const std::uint64_t initialGeometryUpdates = field.GeometryUpdateCount();

    EXPECT_FALSE(field.AdvanceTwinkle(0.049));
    EXPECT_EQ(field.TwinkleSampleTick(), 0u);
    EXPECT_EQ(field.GeometryUpdateCount(), initialGeometryUpdates);
    EXPECT_TRUE(field.AdvanceTwinkle(0.001));
    EXPECT_EQ(field.TwinkleSampleTick(), 1u);
    EXPECT_EQ(field.GeometryUpdateCount(), initialGeometryUpdates + 1u);
    EXPECT_EQ(field.Vertices().data(), storage);
    EXPECT_NE(field.Vertices().front().Color, initialColour);

    for (int frame = 0; frame < 60; ++frame)
    {
        static_cast<void>(field.AdvanceTwinkle(1.0 / 60.0));
    }
    EXPECT_EQ(field.TwinkleSampleTick(), 21u);
    EXPECT_FALSE(field.AdvanceTwinkle(0.0));
    EXPECT_FALSE(field.AdvanceTwinkle(-1.0));
    EXPECT_FALSE(field.AdvanceTwinkle(std::numeric_limits<double>::quiet_NaN()));
    EXPECT_EQ(field.TwinkleSampleTick(), 21u);
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

TEST(StarFieldTests, SiderealTimeUsesTheSharedJ2000EarthRotationAndObserverLongitude)
{
    constexpr double kGreenwichAtJ2000Deg = 18.697374558 * 15.0;
    EXPECT_NEAR(cnahouse::environment::LocalSiderealTimeDeg(0.0, 0.0), kGreenwichAtJ2000Deg, 1e-10);
    EXPECT_NEAR(
        cnahouse::environment::LocalSiderealTimeDeg(0.0, -75.30), kGreenwichAtJ2000Deg - 75.30, 1e-10);
    const double nextDay = cnahouse::environment::LocalSiderealTimeDeg(1.0, 0.0);
    EXPECT_NEAR(nextDay - kGreenwichAtJ2000Deg, 0.9856473662862, 1e-9)
        << "one solar day turns the stars almost, but not exactly, one revolution";
}

TEST(StarFieldTests, NorthCelestialPoleAlwaysHasAltitudeEqualToConfiguredLatitude)
{
    const cnahouse::rendering::StarCatalogueEntry pole{0.0F, 90.0F, 2.0F, 0.6F};
    for (const double latitude : {-64.13, 0.0, 40.05, 64.13})
    {
        const cnahouse::rendering::StarOrientation orientation{137.0, latitude};
        const Xna::Vector3 direction = cnahouse::rendering::HorizonDirection(pole, orientation);
        const double altitudeDeg = std::asin(static_cast<double>(direction.Y)) * 180.0 / std::numbers::pi;
        EXPECT_NEAR(altitudeDeg, latitude, 1e-5);
        EXPECT_NEAR(direction.X, 0.0F, 1e-6F);
        EXPECT_LT(direction.Z, 0.0F) << "the north celestial pole is due north";
    }
}

TEST(StarFieldTests, PolarisFromTheGeneratedCatalogueSitsAtPhiladelphiaLatitude)
{
    const auto catalogue = ReadOf(FixtureBytes());
    ASSERT_TRUE(catalogue);
    const auto polaris = std::find_if(catalogue->begin(),
                                      catalogue->end(),
                                      [](const cnahouse::rendering::StarCatalogueEntry& star)
                                      {
                                          return std::abs(star.rightAscensionDeg - 37.952917F) < 1e-3F &&
                                                 std::abs(star.declinationDeg - 89.264194F) < 1e-3F;
                                      });
    ASSERT_NE(polaris, catalogue->end());
    const cnahouse::environment::SimClock clock;
    const Xna::Vector3 direction =
        cnahouse::rendering::HorizonDirection(*polaris, cnahouse::rendering::StarOrientationFor(clock));
    const double altitudeDeg = std::asin(static_cast<double>(direction.Y)) * 180.0 / std::numbers::pi;
    const double azimuthDeg =
        std::atan2(static_cast<double>(direction.X), -static_cast<double>(direction.Z)) * 180.0 /
        std::numbers::pi;
    EXPECT_NEAR(altitudeDeg, clock.latitudeDeg, 0.75);
    EXPECT_NEAR(azimuthDeg, 0.0, 1.25);
}

TEST(StarFieldTests, HourAngleRotatesTheSkyAndSixMonthsChangeTheMidnightConstellations)
{
    const cnahouse::rendering::StarCatalogueEntry equator{30.0F, 0.0F, 1.0F, 0.0F};
    const cnahouse::rendering::StarOrientation transit{30.0, 40.05};
    const cnahouse::rendering::StarOrientation sixSiderealHoursLater{120.0, 40.05};
    const Xna::Vector3 onMeridian = cnahouse::rendering::HorizonDirection(equator, transit);
    const Xna::Vector3 onWesternHorizon =
        cnahouse::rendering::HorizonDirection(equator, sixSiderealHoursLater);
    EXPECT_NEAR(Dot(onMeridian, onWesternHorizon), 0.0F, 1e-6F);
    EXPECT_NEAR(onWesternHorizon.X, -1.0F, 1e-6F);

    const auto january = cnahouse::rendering::StarOrientationFor(At(2031, 1, 15, 0));
    const auto july = cnahouse::rendering::StarOrientationFor(At(2031, 7, 16, 0));
    const Xna::Vector3 januaryDirection = cnahouse::rendering::HorizonDirection(equator, january);
    const Xna::Vector3 julyDirection = cnahouse::rendering::HorizonDirection(equator, july);
    EXPECT_LT(Dot(januaryDirection, julyDirection), -0.95F)
        << "the same local clock time six months apart must face the opposite constellations";
}

TEST(StarFieldTests, LiveClockRebuildsInPlaceAndInvalidObserverDataIsIgnored)
{
    cnahouse::rendering::Camera camera;
    const StarCatalogue catalogue{{0.0F, 0.0F, -1.0F, 0.0F}, {90.0F, 30.0F, 4.0F, 1.5F}};
    cnahouse::rendering::StarField field(camera, catalogue);
    ASSERT_EQ(field.GeometryUpdateCount(), 1u);
    const auto* storage = field.Vertices().data();
    const Xna::Vector3 initialCorner = field.Vertices().front().Position;

    cnahouse::environment::SimClock clock;
    EXPECT_FALSE(field.SetObserver(clock)) << "construction already used the default clock";
    clock.Advance(60.0);
    EXPECT_TRUE(field.SetObserver(clock));
    EXPECT_EQ(field.GeometryUpdateCount(), 2u);
    EXPECT_EQ(field.Vertices().data(), storage) << "the per-frame rotation must not allocate";
    EXPECT_NE(field.Vertices().front().Position, initialCorner);

    clock.latitudeDeg = std::numeric_limits<double>::quiet_NaN();
    EXPECT_FALSE(field.SetObserver(clock));
    EXPECT_EQ(field.GeometryUpdateCount(), 2u);
}
