// SPDX-License-Identifier: MIT
//
// `HOUSE-01743`. The compact rain state owns fixed positions and turns only the retained liquid
// phases into velocity-aligned quads in the shared renderer.
#include <algorithm>
#include <cmath>
#include <vector>

#include <gtest/gtest.h>

#include "cnahouse/rendering/ParticleRenderer.hpp"
#include "cnahouse/weather/RainParticles.hpp"

namespace
{
    namespace Xna = Microsoft::Xna::Framework;

    cnahouse::weather::WeatherState Rain(float intensity = 1.0F)
    {
        cnahouse::weather::WeatherState state;
        state.precipType = cnahouse::weather::PrecipType::Rain;
        state.precipIntensity = intensity;
        return state;
    }

    TEST(RainParticlesTests, CountFollowsIntensityQualityAndRetainedLiquidPhases)
    {
        using cnahouse::rendering::ParticleQuality;
        using cnahouse::weather::PrecipType;
        using cnahouse::weather::RainParticleCount;
        cnahouse::weather::WeatherState state = Rain();
        EXPECT_EQ(RainParticleCount(state, ParticleQuality::Low), 495u);
        EXPECT_EQ(RainParticleCount(state, ParticleQuality::Medium), 720u);
        EXPECT_EQ(RainParticleCount(state, ParticleQuality::High), 900u);

        state.precipIntensity = 0.25F;
        EXPECT_EQ(RainParticleCount(state, ParticleQuality::High),
                  static_cast<std::size_t>(std::lround(900.0F * std::pow(0.25F, 0.8F))));
        for (const PrecipType phase : {PrecipType::Rain, PrecipType::Sleet, PrecipType::Hail})
        {
            state.precipType = phase;
            EXPECT_GT(RainParticleCount(state, ParticleQuality::High), 0u);
        }
        for (const PrecipType phase : {PrecipType::None, PrecipType::Snow})
        {
            state.precipType = phase;
            EXPECT_EQ(RainParticleCount(state, ParticleQuality::High), 0u);
        }
    }

    TEST(RainParticlesTests, MotionUsesGravityWindWrapAndVelocityElongation)
    {
        using cnahouse::rendering::ParticleQuality;
        cnahouse::rendering::Camera camera;
        cnahouse::rendering::ParticleRenderer renderer(camera);
        cnahouse::weather::RainParticles rain;
        cnahouse::weather::WeatherState state = Rain();
        state.windSpeed = 10.0F;
        state.windDirectionDeg = 0.0F;

        ASSERT_TRUE(rain.Update(0.0F, {}, state, ParticleQuality::High, renderer));
        const std::vector<Xna::Vector3> before(rain.Positions().begin(), rain.Positions().end());
        const Xna::Vector3 velocity = cnahouse::weather::RainParticleVelocity(state);
        EXPECT_FLOAT_EQ(velocity.X, 0.0F);
        EXPECT_FLOAT_EQ(velocity.Y, -9.0F);
        EXPECT_FLOAT_EQ(velocity.Z, 5.5F);
        EXPECT_NEAR(cnahouse::weather::RainStreakLength(velocity),
                    std::sqrt(9.0F * 9.0F + 5.5F * 5.5F) * 0.045F,
                    1.0e-6F);

        ASSERT_TRUE(rain.Update(0.01F, {}, state, ParticleQuality::High, renderer));
        ASSERT_EQ(rain.ActiveCount(), 900u);
        ASSERT_EQ(renderer.Particles().size(), 900u);
        const auto moved = rain.Positions();
        const auto stable = std::ranges::find_if(
            before, [](const Xna::Vector3& position) { return position.Y > -6.0F && position.Z < 10.0F; });
        ASSERT_NE(stable, before.end());
        const std::size_t index = static_cast<std::size_t>(std::distance(before.begin(), stable));
        EXPECT_NEAR(moved[index].Y - stable->Y, velocity.Y * 0.01F, 1.0e-5F);
        EXPECT_NEAR(moved[index].Z - stable->Z, velocity.Z * 0.01F, 1.0e-5F);

        const auto& quad = renderer.Particles().front();
        EXPECT_EQ(quad.elongationAxis, velocity);
        EXPECT_NEAR(quad.halfSize.Y, 0.5F * cnahouse::weather::RainStreakLength(velocity), 1.0e-6F);
        for (const Xna::Vector3& position : moved)
        {
            EXPECT_LE(std::abs(position.Y), 0.5F * cnahouse::weather::kPrecipitationHeightMetres);
            EXPECT_LE(std::sqrt(position.X * position.X +
                                (position.Z - cnahouse::weather::kPrecipitationWindOffsetMetres) *
                                    (position.Z - cnahouse::weather::kPrecipitationWindOffsetMetres)),
                      cnahouse::weather::kPrecipitationRadiusMetres + 1.0e-5F);
        }
    }

    TEST(RainParticlesTests, SnowClearsThePreviousRainFrameWithoutMovingStorage)
    {
        cnahouse::rendering::Camera camera;
        cnahouse::rendering::ParticleRenderer renderer(camera);
        cnahouse::weather::RainParticles rain;
        cnahouse::weather::WeatherState state = Rain(0.7F);
        ASSERT_TRUE(
            rain.Update(1.0F / 60.0F, {}, state, cnahouse::rendering::ParticleQuality::Medium, renderer));
        const Xna::Vector3* storage = rain.Positions().data();
        ASSERT_GT(renderer.Particles().size(), 0u);

        state.precipType = cnahouse::weather::PrecipType::Snow;
        ASSERT_TRUE(
            rain.Update(1.0F / 60.0F, {}, state, cnahouse::rendering::ParticleQuality::Medium, renderer));
        EXPECT_EQ(rain.Positions().data(), storage);
        EXPECT_EQ(rain.ActiveCount(), 0u);
        EXPECT_TRUE(renderer.Particles().empty());
    }
} // namespace
