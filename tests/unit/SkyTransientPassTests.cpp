// SPDX-License-Identifier: MIT
//
// `HOUSE-01615`. The moving lights are a deterministic sample of SimClock, not frame-rate RNG.
#include <algorithm>
#include <cmath>
#include <limits>

#include <gtest/gtest.h>

#include "cnahouse/environment/SimClock.hpp"
#include "cnahouse/environment/SunModel.hpp"
#include "cnahouse/rendering/Camera.hpp"
#include "cnahouse/rendering/SkyTransientPass.hpp"

namespace
{
    namespace Xna = Microsoft::Xna::Framework;

    float Length(const Xna::Vector3& value)
    {
        return std::sqrt(value.X * value.X + value.Y * value.Y + value.Z * value.Z);
    }
} // namespace

TEST(SkyTransientPassTests, ClearNightCarriesTwoDistinctAboveHorizonSatellites)
{
    cnahouse::environment::SimClock clock;
    clock.epochSeconds = 100.0;
    cnahouse::environment::SunPosition sun;
    sun.altitudeDeg = -18.0;

    const auto frame = cnahouse::rendering::SkyTransientFrameFor(clock, sun, 0.0);
    EXPECT_FLOAT_EQ(frame.nightTransmission, 1.0F);
    for (const auto& satellite : frame.satellites)
    {
        EXPECT_GT(satellite.alpha, 0.0F);
        EXPECT_NEAR(Length(satellite.direction), 1.0F, 1e-6F);
        EXPECT_GT(satellite.direction.Y, 0.0F);
    }
    EXPECT_NE(frame.satellites[0].direction, frame.satellites[1].direction);

    const auto later = [&]
    {
        clock.epochSeconds += 30.0;
        return cnahouse::rendering::SkyTransientFrameFor(clock, sun, 0.0);
    }();
    EXPECT_NE(frame.satellites[0].direction, later.satellites[0].direction);
    EXPECT_NE(frame.satellites[1].direction, later.satellites[1].direction);
}

TEST(SkyTransientPassTests, MeteorCadenceIsOneDeterministicClearNightEventEveryFourSimulatedMinutes)
{
    cnahouse::environment::SimClock clock;
    cnahouse::environment::SunPosition sun;
    sun.altitudeDeg = -18.0;

    Xna::Vector3 priorDirection;
    for (std::int64_t event = 0; event < 6; ++event)
    {
        clock.epochSeconds = static_cast<double>(event) * cnahouse::rendering::kMeteorIntervalSimSeconds +
                             cnahouse::rendering::kMeteorDurationSimSeconds * 0.5;
        const auto frame = cnahouse::rendering::SkyTransientFrameFor(clock, sun, 0.0);
        ASSERT_TRUE(frame.meteor.active) << "event " << event;
        EXPECT_EQ(frame.meteor.eventIndex, event);
        EXPECT_NEAR(frame.meteor.alpha, 1.0F, 1e-6F);
        EXPECT_NEAR(Length(frame.meteor.headDirection), 1.0F, 1e-6F);
        EXPECT_NEAR(Length(frame.meteor.tailDirection), 1.0F, 1e-6F);
        EXPECT_NE(frame.meteor.headDirection, frame.meteor.tailDirection);
        if (event != 0)
        {
            EXPECT_NE(frame.meteor.headDirection, priorDirection)
                << "the event hash did not vary the meteor track";
        }
        priorDirection = frame.meteor.headDirection;
    }

    clock.epochSeconds = cnahouse::rendering::kMeteorDurationSimSeconds;
    EXPECT_FALSE(cnahouse::rendering::SkyTransientFrameFor(clock, sun, 0.0).meteor.active);
    clock.epochSeconds = cnahouse::rendering::kMeteorDurationSimSeconds * 0.5;
    EXPECT_FALSE(cnahouse::rendering::SkyTransientFrameFor(
                     clock, sun, cnahouse::rendering::kMeteorMaximumCloudCover + 0.01)
                     .meteor.active);
    sun.altitudeDeg = -13.9;
    EXPECT_FALSE(cnahouse::rendering::SkyTransientFrameFor(clock, sun, 0.0).meteor.active);
}

TEST(SkyTransientPassTests, PassBuildsTwoPointsAndOneFadingStreakWithoutAllocationGrowth)
{
    cnahouse::rendering::Camera camera;
    cnahouse::rendering::SkyTransientPass pass(camera);
    cnahouse::environment::SimClock clock;
    clock.epochSeconds = cnahouse::rendering::kMeteorDurationSimSeconds * 0.5;
    cnahouse::environment::SunPosition sun;
    sun.altitudeDeg = -18.0;

    ASSERT_TRUE(pass.SetCelestial(clock, sun, 0.0));
    ASSERT_TRUE(pass.IsActive());
    EXPECT_EQ(pass.PrimitiveCount(), 3u);
    ASSERT_EQ(pass.Vertices().size(), 12u);
    EXPECT_EQ(pass.Vertices()[8].Color.getAProperty(), 0);
    EXPECT_EQ(pass.Vertices()[9].Color.getAProperty(), 0);
    EXPECT_GT(pass.Vertices()[10].Color.getAProperty(), 0);
    EXPECT_GT(pass.Vertices()[11].Color.getAProperty(), 0);

    const auto* storage = pass.Vertices().data();
    clock.epochSeconds += 0.25;
    ASSERT_TRUE(pass.SetCelestial(clock, sun, 0.0));
    EXPECT_EQ(pass.Vertices().data(), storage);

    sun.altitudeDeg = 0.0;
    ASSERT_TRUE(pass.SetCelestial(clock, sun, 0.0));
    EXPECT_FALSE(pass.IsActive());
    EXPECT_TRUE(pass.Vertices().empty());

    clock.epochSeconds = std::numeric_limits<double>::quiet_NaN();
    EXPECT_FALSE(pass.SetCelestial(clock, sun, 0.0));
    EXPECT_FALSE(pass.IsActive()) << "invalid time changed the last valid daytime state";
}
