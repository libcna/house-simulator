// SPDX-License-Identifier: MIT
#include <gtest/gtest.h>

#include <limits>

#include "cnahouse/weather/PrecipitationTransition.hpp"

namespace
{
    using cnahouse::weather::kPrecipTypeChangeIntensity;
    using cnahouse::weather::PrecipitationTransition;
    using cnahouse::weather::PrecipType;
    using cnahouse::weather::PrecipTypeAtTemperature;
    using cnahouse::weather::WeatherRates;
    using cnahouse::weather::WeatherState;

    [[nodiscard]] WeatherRates Rates()
    {
        return WeatherRates{0.045F, 0.08F, 0.07F, 0.9F, 4.5F, 0.15F, 0.03F, 0.055F, 0.25F, 0.08F};
    }

    TEST(PrecipitationTransitionTests, TypeChangesOnlyAtTheProtectedIntensity)
    {
        const PrecipitationTransition transition(Rates());
        WeatherState current;
        current.precipType = PrecipType::Rain;
        current.precipIntensity = 0.8F;
        WeatherState desired = current;
        desired.precipType = PrecipType::Snow;

        bool changed = false;
        for (int minute = 0; minute < 20 && !changed; ++minute)
        {
            const auto next = transition.Advance(current, desired, 1.0F);
            ASSERT_TRUE(next) << next.Error().ToString();
            if (next.Value().precipType != current.precipType)
            {
                EXPECT_LE(next.Value().precipIntensity, kPrecipTypeChangeIntensity);
                EXPECT_EQ(next.Value().precipType, PrecipType::Snow);
                changed = true;
            }
            else
            {
                EXPECT_EQ(next.Value().precipType, PrecipType::Rain);
            }
            current = next.Value();
        }
        EXPECT_TRUE(changed);
    }

    TEST(PrecipitationTransitionTests, IntensityRampsDownFlipsAndRampsBackUp)
    {
        const PrecipitationTransition transition(Rates());
        WeatherState current;
        current.precipType = PrecipType::Rain;
        current.precipIntensity = 0.4F;
        WeatherState desired = current;
        desired.precipType = PrecipType::Snow;
        desired.precipIntensity = 0.4F;

        float previous = current.precipIntensity;
        for (int minute = 0; minute < 20 && current.precipType == PrecipType::Rain; ++minute)
        {
            const auto next = transition.Advance(current, desired, 1.0F);
            ASSERT_TRUE(next);
            EXPECT_LE(next.Value().precipIntensity, previous);
            previous = next.Value().precipIntensity;
            current = next.Value();
        }
        ASSERT_EQ(current.precipType, PrecipType::Snow);
        EXPECT_FLOAT_EQ(current.precipIntensity, kPrecipTypeChangeIntensity);

        const auto rising = transition.Advance(current, desired, 1.0F);
        ASSERT_TRUE(rising);
        EXPECT_EQ(rising.Value().precipType, PrecipType::Snow);
        EXPECT_FLOAT_EQ(rising.Value().precipIntensity, kPrecipTypeChangeIntensity + Rates().precipIntensity);
    }

    TEST(PrecipitationTransitionTests, StartingPrecipitationCannotOutrunItsType)
    {
        const PrecipitationTransition transition(Rates());
        WeatherState current;
        WeatherState desired = current;
        desired.precipType = PrecipType::Rain;
        desired.precipIntensity = 0.8F;

        const auto next = transition.Advance(current, desired, 1.0F);
        ASSERT_TRUE(next) << next.Error().ToString();
        EXPECT_EQ(next->precipType, PrecipType::Rain);
        EXPECT_FLOAT_EQ(next->precipIntensity, kPrecipTypeChangeIntensity);
    }

    TEST(PrecipitationTransitionTests, AReachedDryTypeCannotRegainTheBlendsOldIntensity)
    {
        const PrecipitationTransition transition(Rates());
        WeatherState current;
        current.precipIntensity = kPrecipTypeChangeIntensity;
        WeatherState desired = current;
        desired.precipIntensity = 0.2F;

        const auto next = transition.Advance(current, desired, 1.0F);
        ASSERT_TRUE(next) << next.Error().ToString();
        EXPECT_EQ(next->precipType, PrecipType::None);
        EXPECT_FLOAT_EQ(next->precipIntensity, kPrecipTypeChangeIntensity);
    }

    TEST(PrecipitationTransitionTests, TemperatureDerivedCrossingUsesTheSameProtection)
    {
        const PrecipitationTransition transition(Rates());
        WeatherState current;
        current.precipType = PrecipType::Rain;
        current.precipIntensity = 0.7F;
        WeatherState desired = current;
        const auto coldType = PrecipTypeAtTemperature(PrecipType::Rain, -3.0F);
        ASSERT_TRUE(coldType);
        desired.precipType = coldType.Value();

        const auto next = transition.Advance(current, desired, 1.0F);
        ASSERT_TRUE(next);
        EXPECT_EQ(next.Value().precipType, PrecipType::Rain);
        EXPECT_LT(next.Value().precipIntensity, current.precipIntensity);
    }

    TEST(PrecipitationTransitionTests, WaterPhaseUsesTheNextRateLimitedLiveTemperature)
    {
        const PrecipitationTransition transition(Rates());
        WeatherState current;
        current.precipType = PrecipType::Rain;
        current.precipIntensity = 0.04F;
        current.temperatureC = 0.1F;
        WeatherState desired = current;
        desired.precipIntensity = 0.8F;
        desired.temperatureC = -10.0F;

        const auto next = transition.AdvanceDerived(current, desired, PrecipType::Rain, 1.0F);
        ASSERT_TRUE(next) << next.Error().ToString();
        EXPECT_FLOAT_EQ(next->temperatureC, -0.15F);
        EXPECT_EQ(next->precipType, PrecipType::Snow);
        EXPECT_FLOAT_EQ(next->precipIntensity, kPrecipTypeChangeIntensity);
    }

    TEST(PrecipitationTransitionTests, HailWaitsForThunderAndKeepsItUntilThePhaseChanges)
    {
        const PrecipitationTransition transition(Rates());
        WeatherState current;
        WeatherState desired = current;
        desired.precipType = PrecipType::Hail;
        desired.precipIntensity = 0.7F;
        desired.thunderIntensity = 0.8F;

        for (int minute = 0; minute < 6; ++minute)
        {
            const auto next = transition.Advance(current, desired, 1.0F);
            ASSERT_TRUE(next) << next.Error().ToString();
            current = *next;
            if (current.precipType == PrecipType::Hail)
            {
                EXPECT_GT(current.thunderIntensity, cnahouse::weather::kMinimumHailThunderIntensity);
            }
        }
        ASSERT_EQ(current.precipType, PrecipType::Hail);

        current.precipIntensity = 0.1F;
        current.thunderIntensity = 0.31F;
        desired.precipType = PrecipType::None;
        desired.precipIntensity = 0.0F;
        desired.thunderIntensity = 0.0F;
        const auto cleared = transition.Advance(current, desired, 1.0F);
        ASSERT_TRUE(cleared) << cleared.Error().ToString();
        EXPECT_EQ(cleared->precipType, PrecipType::None);
        EXPECT_GT(cleared->thunderIntensity, cnahouse::weather::kMinimumHailThunderIntensity);
    }

    TEST(PrecipitationTransitionTests, ALongStepStillExposesTheSwitchAtTheBoundary)
    {
        const PrecipitationTransition transition(Rates());
        WeatherState current;
        current.precipType = PrecipType::Rain;
        current.precipIntensity = 1.0F;
        WeatherState desired = current;
        desired.precipType = PrecipType::Snow;

        const auto next = transition.Advance(current, desired, 100.0F);
        ASSERT_TRUE(next);
        EXPECT_EQ(next.Value().precipType, PrecipType::Snow);
        EXPECT_FLOAT_EQ(next.Value().precipIntensity, kPrecipTypeChangeIntensity);
    }

    TEST(PrecipitationTransitionTests, ZeroTimeIsIdentityAndInvalidInputIsRejected)
    {
        const PrecipitationTransition transition(Rates());
        WeatherState current;
        current.precipType = PrecipType::Rain;
        current.precipIntensity = 0.02F;
        WeatherState desired = current;
        desired.precipType = PrecipType::Snow;
        desired.precipIntensity = 0.8F;

        const auto unchanged = transition.Advance(current, desired, 0.0F);
        ASSERT_TRUE(unchanged);
        EXPECT_EQ(unchanged.Value(), current);
        EXPECT_FALSE(transition.Advance(current, desired, -1.0F));
        desired.precipIntensity = std::numeric_limits<float>::quiet_NaN();
        EXPECT_FALSE(transition.Advance(current, desired, 1.0F));
    }

} // namespace
