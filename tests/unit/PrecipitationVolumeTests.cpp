// SPDX-License-Identifier: MIT
//
// `HOUSE-01742`. The retained precipitation volume follows the camera and the already-smoothed
// weather wind. It has no RNG and no gust oscillator: wrapping is pure bounded arithmetic.
#include <array>
#include <cmath>
#include <limits>

#include <gtest/gtest.h>

#include "cnahouse/weather/PrecipitationVolume.hpp"

namespace
{
    namespace Xna = Microsoft::Xna::Framework;
    using cnahouse::weather::PrecipitationVolume;
    using cnahouse::weather::WeatherState;

    TEST(PrecipitationVolumeTests, MeteorologicalWindPointsWhereTheAirTravelsAndIgnoresGusts)
    {
        WeatherState weather;
        weather.windSpeed = 8.0F;
        weather.windDirectionDeg = 0.0F;
        weather.gustFactor = 0.0F;
        const Xna::Vector3 fromNorth = cnahouse::weather::PrecipitationWindVector(weather);
        EXPECT_NEAR(fromNorth.X, 0.0F, 1.0e-6F);
        EXPECT_FLOAT_EQ(fromNorth.Y, 0.0F);
        EXPECT_NEAR(fromNorth.Z, 8.0F, 1.0e-6F);

        weather.windDirectionDeg = 90.0F;
        weather.gustFactor = 1.0F;
        const Xna::Vector3 fromEast = cnahouse::weather::PrecipitationWindVector(weather);
        EXPECT_NEAR(fromEast.X, -8.0F, 1.0e-5F);
        EXPECT_FLOAT_EQ(fromEast.Y, 0.0F);
        EXPECT_NEAR(fromEast.Z, 0.0F, 1.0e-5F);
    }

    TEST(PrecipitationVolumeTests, CentreFollowsTheCameraThreeMetresDownwind)
    {
        PrecipitationVolume volume;
        WeatherState weather;
        weather.windSpeed = 12.0F;
        weather.windDirectionDeg = 270.0F; // from west, travelling east
        const Xna::Vector3 camera(10.0F, 3.0F, -20.0F);
        ASSERT_TRUE(volume.Follow(camera, weather, {}));
        EXPECT_NEAR(volume.Centre().X, 13.0F, 1.0e-5F);
        EXPECT_FLOAT_EQ(volume.Centre().Y, camera.Y);
        EXPECT_NEAR(volume.Centre().Z, camera.Z, 1.0e-5F);

        weather.windSpeed = 0.0F;
        ASSERT_TRUE(volume.Follow(camera, weather, {}));
        EXPECT_EQ(volume.Centre(), camera);
    }

    TEST(PrecipitationVolumeTests, CrossingAnyBoundaryWrapsThroughItsOppositeSide)
    {
        PrecipitationVolume volume;
        WeatherState calm;
        const Xna::Vector3 camera(10.0F, 5.0F, -20.0F);
        ASSERT_TRUE(volume.Follow(camera, calm, {}));

        const Xna::Vector3 below = volume.Wrap(Xna::Vector3(10.0F, -2.25F, -20.0F));
        const Xna::Vector3 above = volume.Wrap(Xna::Vector3(10.0F, 12.25F, -20.0F));
        const Xna::Vector3 east = volume.Wrap(Xna::Vector3(22.5F, 5.0F, -20.0F));
        EXPECT_NEAR(below.Y, 11.75F, 1.0e-5F);
        EXPECT_NEAR(above.Y, -1.75F, 1.0e-5F);
        EXPECT_NEAR(east.X, -1.5F, 1.0e-5F);
        EXPECT_LT(volume.BoundaryExcess(below), 0.0F);
        EXPECT_LT(volume.BoundaryExcess(above), 0.0F);
        EXPECT_LT(volume.BoundaryExcess(east), 0.0F);

        const float diagonal = 12.5F / std::sqrt(2.0F);
        const Xna::Vector3 corner =
            volume.Wrap(Xna::Vector3(camera.X + diagonal, camera.Y, camera.Z + diagonal));
        EXPECT_LT(volume.BoundaryExcess(corner), 0.0F);
    }

    TEST(PrecipitationVolumeTests, FollowingAWalkingCameraWrapsTheExistingPoolInPlace)
    {
        PrecipitationVolume volume;
        WeatherState weather;
        weather.windSpeed = 4.0F;
        weather.windDirectionDeg = 180.0F;
        std::array<Xna::Vector3, 4> particles{{Xna::Vector3(-11.0F, -8.0F, 0.0F),
                                               Xna::Vector3(11.0F, 8.0F, 0.0F),
                                               Xna::Vector3(0.0F, 0.0F, -11.0F),
                                               Xna::Vector3(0.0F, 0.0F, 11.0F)}};
        ASSERT_TRUE(volume.Follow(Xna::Vector3(20.0F, 2.0F, 30.0F), weather, particles));
        for (const Xna::Vector3& particle : particles)
        {
            EXPECT_LT(volume.BoundaryExcess(particle), 0.0F);
        }

        const Xna::Vector3 oldCentre = volume.Centre();
        const auto oldParticles = particles;
        WeatherState invalid = weather;
        invalid.windDirectionDeg = std::numeric_limits<float>::quiet_NaN();
        EXPECT_FALSE(volume.Follow({}, invalid, particles));
        EXPECT_EQ(volume.Centre(), oldCentre);
        EXPECT_EQ(particles, oldParticles);

        particles[1].Y = std::numeric_limits<float>::infinity();
        const auto nonFiniteParticles = particles;
        EXPECT_FALSE(volume.Follow({}, weather, particles));
        EXPECT_EQ(volume.Centre(), oldCentre);
        EXPECT_EQ(particles, nonFiniteParticles);
    }
} // namespace
