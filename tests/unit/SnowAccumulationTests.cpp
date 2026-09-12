// SPDX-License-Identifier: MIT
#include <gtest/gtest.h>

#include <array>
#include <limits>

#include "cnahouse/weather/SnowAccumulation.hpp"

namespace
{
    using cnahouse::weather::IntegrateSnowDepth;
    using cnahouse::weather::PrecipType;
    using cnahouse::weather::SnowMeltRate;
    using cnahouse::weather::WeatherState;

    TEST(SnowAccumulationTests, SnowAccumulatesAtTheDocumentedRate)
    {
        WeatherState state;
        state.precipType = PrecipType::Snow;
        state.precipIntensity = 0.75F;
        state.temperatureC = -5.0F;
        state.snowDepth = 0.01F;

        const auto next = IntegrateSnowDepth(state, 1.0F, 20.0F);
        ASSERT_TRUE(next) << next.Error().ToString();
        EXPECT_NEAR(next.Value().snowDepth, state.snowDepth + 20.0F * 0.0009F * state.precipIntensity, 1e-7F);
    }

    TEST(SnowAccumulationTests, OtherPrecipitationPhasesDoNotCreateSnowCover)
    {
        for (const PrecipType type :
             std::array{PrecipType::None, PrecipType::Rain, PrecipType::Sleet, PrecipType::Hail})
        {
            WeatherState state;
            state.precipType = type;
            state.precipIntensity = 1.0F;
            state.temperatureC = -5.0F;
            state.snowDepth = 0.1F;
            const auto next = IntegrateSnowDepth(state, 0.0F, 60.0F);
            ASSERT_TRUE(next);
            EXPECT_FLOAT_EQ(next.Value().snowDepth, state.snowDepth) << static_cast<int>(type);
        }
    }

    TEST(SnowAccumulationTests, TemperatureMeltsAndDirectSunlightDoublesTheRate)
    {
        EXPECT_FLOAT_EQ(SnowMeltRate(-0.1F, 1.0F), 0.0F);
        EXPECT_FLOAT_EQ(SnowMeltRate(0.0F, 1.0F), 0.0F);
        EXPECT_FLOAT_EQ(SnowMeltRate(10.0F, 0.0F), 0.0006F);
        EXPECT_FLOAT_EQ(SnowMeltRate(10.0F, 1.0F), 0.0012F);

        WeatherState state;
        state.snowDepth = 0.2F;
        state.temperatureC = 10.0F;
        const auto next = IntegrateSnowDepth(state, 1.0F, 100.0F);
        ASSERT_TRUE(next);
        EXPECT_FLOAT_EQ(next.Value().snowDepth, 0.08F);
    }

    TEST(SnowAccumulationTests, SnowDepthClampsAtBothPhysicalBounds)
    {
        WeatherState snowing;
        snowing.precipType = PrecipType::Snow;
        snowing.precipIntensity = 1.0F;
        snowing.temperatureC = -10.0F;
        snowing.snowDepth = 0.34F;
        const auto full = IntegrateSnowDepth(snowing, 0.0F, 100.0F);
        ASSERT_TRUE(full);
        EXPECT_FLOAT_EQ(full.Value().snowDepth, 0.35F);

        WeatherState melting = snowing;
        melting.precipType = PrecipType::None;
        melting.precipIntensity = 0.0F;
        melting.temperatureC = 38.0F;
        melting.snowDepth = 0.01F;
        const auto empty = IntegrateSnowDepth(melting, 1.0F, 100.0F);
        ASSERT_TRUE(empty);
        EXPECT_FLOAT_EQ(empty.Value().snowDepth, 0.0F);
    }

    TEST(SnowAccumulationTests, OnlyDepthChangesAndInvalidInputsAreRejected)
    {
        WeatherState state;
        state.precipType = PrecipType::Snow;
        state.precipIntensity = 0.8F;
        state.temperatureC = -3.0F;
        state.snowDepth = 0.1F;

        const auto unchanged = IntegrateSnowDepth(state, 0.5F, 0.0F);
        ASSERT_TRUE(unchanged);
        EXPECT_EQ(unchanged.Value(), state);

        const auto next = IntegrateSnowDepth(state, 0.5F, 1.0F);
        ASSERT_TRUE(next);
        WeatherState expected = next.Value();
        expected.snowDepth = state.snowDepth;
        EXPECT_EQ(expected, state);

        EXPECT_FALSE(IntegrateSnowDepth(state, -0.1F, 1.0F));
        EXPECT_FALSE(IntegrateSnowDepth(state, 1.1F, 1.0F));
        EXPECT_FALSE(IntegrateSnowDepth(state, 0.5F, std::numeric_limits<float>::infinity()));
        state.snowDepth = std::numeric_limits<float>::quiet_NaN();
        EXPECT_FALSE(IntegrateSnowDepth(state, 0.5F, 1.0F));
    }

} // namespace
