// SPDX-License-Identifier: MIT
#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <limits>

#include "cnahouse/util/Rng.hpp"
#include "cnahouse/weather/WeatherRateLimiter.hpp"

namespace
{
    using cnahouse::util::Rng;
    using cnahouse::weather::PrecipType;
    using cnahouse::weather::WeatherRateLimiter;
    using cnahouse::weather::WeatherRates;
    using cnahouse::weather::WeatherState;

    [[nodiscard]] WeatherRates Rates()
    {
        return WeatherRates{0.045F, 0.08F, 0.07F, 0.9F, 4.5F, 0.15F, 0.03F, 0.055F, 0.25F, 0.08F};
    }

    [[nodiscard]] float AngularDistance(float a, float b)
    {
        float difference = std::fmod(std::abs(a - b), 360.0F);
        if (difference > 180.0F)
        {
            difference = 360.0F - difference;
        }
        return difference;
    }

    [[nodiscard]] WeatherState RandomState(Rng& rng)
    {
        WeatherState state;
        state.cloudCover = rng.NextFloat();
        state.cloudCumuliform = rng.NextFloat();
        state.precipIntensity = rng.NextFloat();
        state.windSpeed = 30.0F * rng.NextFloat();
        state.windDirectionDeg = 360.0F * rng.NextFloat();
        state.gustFactor = rng.NextFloat();
        state.fogDensity = rng.NextFloat();
        state.thunderIntensity = rng.NextFloat();
        state.temperatureC = -18.0F + 56.0F * rng.NextFloat();
        state.humidity = rng.NextFloat();
        return state;
    }

    TEST(WeatherRateTests, NoChannelEverExceedsItsRate)
    {
        const WeatherRates rates = Rates();
        const WeatherRateLimiter limiter(rates);
        std::array<float, 10> maxima{};

        for (std::uint64_t seed = 1; seed <= 200U; ++seed)
        {
            Rng rng(seed);
            WeatherState current = RandomState(rng);
            for (std::size_t minute = 0; minute < 10000U; ++minute)
            {
                const WeatherState desired = RandomState(rng);
                const auto advanced = limiter.Advance(current, desired, 1.0F);
                ASSERT_TRUE(advanced) << advanced.Error().ToString();
                const WeatherState& next = advanced.Value();
                const std::array deltas{
                    std::abs(next.cloudCover - current.cloudCover),
                    std::abs(next.cloudCumuliform - current.cloudCumuliform),
                    std::abs(next.precipIntensity - current.precipIntensity),
                    std::abs(next.windSpeed - current.windSpeed),
                    AngularDistance(next.windDirectionDeg, current.windDirectionDeg),
                    std::abs(next.gustFactor - current.gustFactor),
                    std::abs(next.fogDensity - current.fogDensity),
                    std::abs(next.thunderIntensity - current.thunderIntensity),
                    std::abs(next.temperatureC - current.temperatureC),
                    std::abs(next.humidity - current.humidity),
                };
                for (std::size_t channel = 0; channel < maxima.size(); ++channel)
                {
                    maxima[channel] = std::max(maxima[channel], deltas[channel]);
                }
                current = next;
            }
        }

        const std::array limits{rates.cloudCover,
                                rates.cloudCumuliform,
                                rates.precipIntensity,
                                rates.windSpeed,
                                rates.windDirectionDeg,
                                rates.gustFactor,
                                rates.fogDensity,
                                rates.thunderIntensity,
                                rates.temperatureC,
                                rates.humidity};
        for (std::size_t channel = 0; channel < maxima.size(); ++channel)
        {
            EXPECT_LE(maxima[channel], limits[channel] + 2e-6F) << "channel " << channel;
            EXPECT_GE(maxima[channel], limits[channel] - 1e-4F) << "channel " << channel;
        }
    }

    TEST(WeatherRateTests, DirectionUsesTheShortestWrappedPathAndNeverOvershoots)
    {
        const WeatherRateLimiter limiter(Rates());
        WeatherState current;
        WeatherState desired;
        current.windDirectionDeg = 359.0F;
        desired.windDirectionDeg = 1.0F;
        auto next = limiter.Advance(current, desired, 1.0F);
        ASSERT_TRUE(next);
        EXPECT_FLOAT_EQ(next.Value().windDirectionDeg, 1.0F);

        current.windDirectionDeg = 10.0F;
        desired.windDirectionDeg = 350.0F;
        next = limiter.Advance(current, desired, 1.0F);
        ASSERT_TRUE(next);
        EXPECT_LT(next.Value().windDirectionDeg, current.windDirectionDeg);
        EXPECT_NEAR(AngularDistance(next.Value().windDirectionDeg, current.windDirectionDeg), 4.5F, 1e-4F);
    }

    TEST(WeatherRateTests, ScalarTargetsAreReachedExactlyWithoutOvershoot)
    {
        const WeatherRateLimiter limiter(Rates());
        WeatherState current;
        current.precipIntensity = 1.0F;
        WeatherState desired = current;
        desired.precipIntensity = 0.05F;

        const auto next = limiter.Advance(current, desired, 100.0F);
        ASSERT_TRUE(next);
        EXPECT_FLOAT_EQ(next.Value().precipIntensity, desired.precipIntensity);
    }

    TEST(WeatherRateTests, NonTargetStateIsPreservedAndZeroMinutesChangesNothing)
    {
        const WeatherRateLimiter limiter(Rates());
        WeatherState current;
        current.precipType = PrecipType::Hail;
        current.surfaceWetness = 0.73F;
        current.snowDepth = 0.12F;
        current.rngState = {1U, 2U, 3U, 4U};
        WeatherState desired = current;
        desired.cloudCover = 1.0F;
        desired.precipType = PrecipType::Snow;
        desired.surfaceWetness = 0.0F;
        desired.snowDepth = 0.0F;
        desired.rngState = {5U, 6U, 7U, 8U};

        const auto unchanged = limiter.Advance(current, desired, 0.0F);
        ASSERT_TRUE(unchanged);
        EXPECT_EQ(unchanged.Value(), current);

        const auto advanced = limiter.Advance(current, desired, 1.0F);
        ASSERT_TRUE(advanced);
        EXPECT_EQ(advanced.Value().precipType, PrecipType::Hail);
        EXPECT_FLOAT_EQ(advanced.Value().surfaceWetness, 0.73F);
        EXPECT_FLOAT_EQ(advanced.Value().snowDepth, 0.12F);
        EXPECT_EQ(advanced.Value().rngState, current.rngState);
    }

    TEST(WeatherRateTests, InvalidInputsAreRejectedInsteadOfBecomingWeather)
    {
        WeatherState current;
        WeatherState desired;
        WeatherRateLimiter limiter(Rates());
        EXPECT_FALSE(limiter.Advance(current, desired, -1.0F));
        EXPECT_FALSE(limiter.Advance(current, desired, std::numeric_limits<float>::infinity()));

        desired.humidity = std::numeric_limits<float>::quiet_NaN();
        EXPECT_FALSE(limiter.Advance(current, desired, 1.0F));

        WeatherRates invalid = Rates();
        invalid.windSpeed = 0.0F;
        limiter = WeatherRateLimiter(invalid);
        EXPECT_FALSE(limiter.Advance(current, current, 1.0F));
    }

} // namespace
