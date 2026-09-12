// SPDX-License-Identifier: MIT
#include <gtest/gtest.h>

#include <limits>

#include "cnahouse/weather/SurfaceWetness.hpp"

namespace
{
    using cnahouse::weather::IntegrateSurfaceWetness;
    using cnahouse::weather::SurfaceDryingRate;
    using cnahouse::weather::WeatherState;

    TEST(SurfaceWetnessTests, RainAccumulatesAtTheDocumentedRate)
    {
        WeatherState state;
        state.surfaceWetness = 0.2F;
        state.precipIntensity = 0.8F;
        state.temperatureC = -18.0F;

        const auto next = IntegrateSurfaceWetness(state, 5.0F, false);
        ASSERT_TRUE(next) << next.Error().ToString();
        EXPECT_NEAR(
            next.Value().surfaceWetness, state.surfaceWetness + 5.0F * 0.045F * state.precipIntensity, 1e-6F);
    }

    TEST(SurfaceWetnessTests, DryAirHeatAndWindIncreaseDryingWhileShelterReducesIt)
    {
        WeatherState mild;
        mild.temperatureC = 10.0F;
        mild.humidity = 0.5F;
        mild.windSpeed = 0.0F;

        WeatherState hot = mild;
        hot.temperatureC = 38.0F;
        WeatherState humid = mild;
        humid.humidity = 1.0F;
        WeatherState windy = mild;
        windy.windSpeed = 30.0F;

        EXPECT_FLOAT_EQ(SurfaceDryingRate(mild, false), 0.25F);
        EXPECT_GT(SurfaceDryingRate(hot, false), SurfaceDryingRate(mild, false));
        EXPECT_FLOAT_EQ(SurfaceDryingRate(humid, false), 0.0F);
        EXPECT_FLOAT_EQ(SurfaceDryingRate(windy, false), 2.0F * SurfaceDryingRate(mild, false));
        EXPECT_FLOAT_EQ(SurfaceDryingRate(mild, true), 0.25F * SurfaceDryingRate(mild, false));
    }

    TEST(SurfaceWetnessTests, DryWeatherReducesWetnessAtTheDocumentedRate)
    {
        WeatherState state;
        state.surfaceWetness = 0.8F;
        state.precipIntensity = 0.0F;
        state.temperatureC = 38.0F;
        state.humidity = 0.0F;
        state.windSpeed = 30.0F;

        const auto next = IntegrateSurfaceWetness(state, 10.0F, false);
        ASSERT_TRUE(next);
        EXPECT_FLOAT_EQ(next.Value().surfaceWetness, state.surfaceWetness - 20.0F * 0.006F);
    }

    TEST(SurfaceWetnessTests, WetnessClampsAtBothPhysicalBounds)
    {
        WeatherState wet;
        wet.surfaceWetness = 0.99F;
        wet.precipIntensity = 1.0F;
        wet.temperatureC = -18.0F;
        const auto saturated = IntegrateSurfaceWetness(wet, 10.0F, false);
        ASSERT_TRUE(saturated);
        EXPECT_FLOAT_EQ(saturated.Value().surfaceWetness, 1.0F);

        WeatherState dry = wet;
        dry.surfaceWetness = 0.01F;
        dry.precipIntensity = 0.0F;
        dry.temperatureC = 38.0F;
        dry.humidity = 0.0F;
        dry.windSpeed = 30.0F;
        const auto empty = IntegrateSurfaceWetness(dry, 10.0F, false);
        ASSERT_TRUE(empty);
        EXPECT_FLOAT_EQ(empty.Value().surfaceWetness, 0.0F);
    }

    TEST(SurfaceWetnessTests, OnlyWetnessChangesAndInvalidInputsAreRejected)
    {
        WeatherState state;
        state.surfaceWetness = 0.4F;
        state.precipIntensity = 0.7F;

        const auto unchanged = IntegrateSurfaceWetness(state, 0.0F, false);
        ASSERT_TRUE(unchanged);
        EXPECT_EQ(unchanged.Value(), state);

        const auto next = IntegrateSurfaceWetness(state, 1.0F, false);
        ASSERT_TRUE(next);
        WeatherState expected = next.Value();
        expected.surfaceWetness = state.surfaceWetness;
        EXPECT_EQ(expected, state);

        EXPECT_FALSE(IntegrateSurfaceWetness(state, -1.0F, false));
        EXPECT_FALSE(IntegrateSurfaceWetness(state, std::numeric_limits<float>::infinity(), false));
        state.humidity = std::numeric_limits<float>::quiet_NaN();
        EXPECT_FALSE(IntegrateSurfaceWetness(state, 1.0F, false));
    }

} // namespace
