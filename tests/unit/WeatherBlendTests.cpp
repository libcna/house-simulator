// SPDX-License-Identifier: MIT
#include <gtest/gtest.h>

#include <limits>

#include "cnahouse/weather/WeatherBlend.hpp"
#include "cnahouse/weather/WeatherRateLimiter.hpp"

namespace
{
    using cnahouse::weather::PrecipType;
    using cnahouse::weather::WeatherBlend;
    using cnahouse::weather::WeatherRateLimiter;
    using cnahouse::weather::WeatherRates;
    using cnahouse::weather::WeatherState;

    [[nodiscard]] WeatherState Start()
    {
        WeatherState state;
        state.precipType = PrecipType::Rain;
        state.surfaceWetness = 0.7F;
        state.snowDepth = 0.1F;
        state.rngState = {1U, 2U, 3U, 4U};
        return state;
    }

    [[nodiscard]] WeatherState Target()
    {
        WeatherState state;
        state.cloudCover = 1.0F;
        state.cloudCumuliform = 1.0F;
        state.precipIntensity = 1.0F;
        state.windSpeed = 20.0F;
        state.windDirectionDeg = 90.0F;
        state.gustFactor = 1.0F;
        state.fogDensity = 1.0F;
        state.thunderIntensity = 1.0F;
        state.temperatureC = 31.0F;
        state.humidity = 1.0F;
        state.precipType = PrecipType::Snow;
        state.surfaceWetness = 0.0F;
        state.snowDepth = 0.0F;
        state.rngState = {5U, 6U, 7U, 8U};
        return state;
    }

    TEST(WeatherBlendTests, QuarterProgressIsSmoothstepRatherThanLinear)
    {
        const WeatherBlend blend(Start(), Target(), 20.0F);
        const auto desired = blend.DesiredAt(5.0F);
        ASSERT_TRUE(desired) << desired.Error().ToString();

        constexpr float kSmoothQuarter = 0.15625F;
        EXPECT_FLOAT_EQ(desired.Value().cloudCover, kSmoothQuarter);
        EXPECT_FLOAT_EQ(desired.Value().windSpeed, 20.0F * kSmoothQuarter);
        EXPECT_FLOAT_EQ(desired.Value().temperatureC, 11.0F + (31.0F - 11.0F) * kSmoothQuarter);
    }

    TEST(WeatherBlendTests, EndpointsClampExactlyAndDoNotRewriteOwnedState)
    {
        const WeatherState start = Start();
        const WeatherState target = Target();
        const WeatherBlend blend(start, target, 20.0F);

        const auto atStart = blend.DesiredAt(0.0F);
        const auto atEnd = blend.DesiredAt(20.0F);
        const auto afterEnd = blend.DesiredAt(200.0F);
        ASSERT_TRUE(atStart);
        ASSERT_TRUE(atEnd);
        ASSERT_TRUE(afterEnd);
        EXPECT_EQ(atStart.Value(), start);
        EXPECT_FLOAT_EQ(atEnd.Value().cloudCover, target.cloudCover);
        EXPECT_FLOAT_EQ(atEnd.Value().cloudCumuliform, target.cloudCumuliform);
        EXPECT_FLOAT_EQ(atEnd.Value().precipIntensity, target.precipIntensity);
        EXPECT_FLOAT_EQ(atEnd.Value().windSpeed, target.windSpeed);
        EXPECT_FLOAT_EQ(atEnd.Value().windDirectionDeg, target.windDirectionDeg);
        EXPECT_FLOAT_EQ(atEnd.Value().gustFactor, target.gustFactor);
        EXPECT_FLOAT_EQ(atEnd.Value().fogDensity, target.fogDensity);
        EXPECT_FLOAT_EQ(atEnd.Value().thunderIntensity, target.thunderIntensity);
        EXPECT_FLOAT_EQ(atEnd.Value().temperatureC, target.temperatureC);
        EXPECT_FLOAT_EQ(atEnd.Value().humidity, target.humidity);
        EXPECT_FLOAT_EQ(afterEnd.Value().cloudCover, target.cloudCover);
        EXPECT_EQ(atEnd.Value().precipType, start.precipType);
        EXPECT_FLOAT_EQ(atEnd.Value().surfaceWetness, start.surfaceWetness);
        EXPECT_FLOAT_EQ(atEnd.Value().snowDepth, start.snowDepth);
        EXPECT_EQ(atEnd.Value().rngState, start.rngState);
    }

    TEST(WeatherBlendTests, SnapshotIsCapturedAndDoesNotChaseTheLiveState)
    {
        WeatherState start = Start();
        const WeatherBlend blend(start, Target(), 20.0F);
        start.cloudCover = 0.9F;

        const auto desired = blend.DesiredAt(10.0F);
        ASSERT_TRUE(desired);
        EXPECT_FLOAT_EQ(desired.Value().cloudCover, 0.5F);
        EXPECT_FLOAT_EQ(blend.Snapshot().cloudCover, 0.0F);
    }

    TEST(WeatherBlendTests, WindDirectionTakesTheShortestWrappedArc)
    {
        WeatherState start = Start();
        WeatherState target = Target();
        start.windDirectionDeg = 350.0F;
        target.windDirectionDeg = 10.0F;
        const WeatherBlend blend(start, target, 20.0F);

        const auto halfway = blend.DesiredAt(10.0F);
        const auto finished = blend.DesiredAt(20.0F);
        ASSERT_TRUE(halfway);
        ASSERT_TRUE(finished);
        EXPECT_NEAR(halfway.Value().windDirectionDeg, 0.0F, 1e-5F);
        EXPECT_FLOAT_EQ(finished.Value().windDirectionDeg, 10.0F);
    }

    TEST(WeatherBlendTests, RateLimiterStillBoundsAShortSmoothTransition)
    {
        const WeatherState start = Start();
        const WeatherBlend blend(start, Target(), 5.0F);
        const WeatherRates rates{0.045F, 0.08F, 0.07F, 0.9F, 4.5F, 0.15F, 0.03F, 0.055F, 0.25F, 0.08F};
        const WeatherRateLimiter limiter(rates);
        const auto desired = blend.DesiredAt(2.5F);
        ASSERT_TRUE(desired);
        const auto live = limiter.Advance(start, desired.Value(), 1.0F);
        ASSERT_TRUE(live);
        EXPECT_FLOAT_EQ(live.Value().cloudCover, rates.cloudCover);
        EXPECT_FLOAT_EQ(live.Value().windSpeed, rates.windSpeed);
        EXPECT_FLOAT_EQ(live.Value().temperatureC, start.temperatureC + rates.temperatureC);
    }

    TEST(WeatherBlendTests, InvalidTimeAndStateCannotEnterTheCurve)
    {
        const WeatherBlend zeroDuration(Start(), Target(), 0.0F);
        EXPECT_FALSE(zeroDuration.DesiredAt(0.0F));

        const WeatherBlend valid(Start(), Target(), 20.0F);
        EXPECT_FALSE(valid.DesiredAt(-1.0F));
        EXPECT_FALSE(valid.DesiredAt(std::numeric_limits<float>::infinity()));

        WeatherState invalid = Target();
        invalid.cloudCover = std::numeric_limits<float>::quiet_NaN();
        const WeatherBlend invalidTarget(Start(), invalid, 20.0F);
        const auto rejected = invalidTarget.DesiredAt(1.0F);
        ASSERT_FALSE(rejected);
        EXPECT_NE(rejected.Error().Context().find("target"), std::string::npos);
    }

} // namespace
