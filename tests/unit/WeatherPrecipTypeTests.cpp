// SPDX-License-Identifier: MIT
#include <gtest/gtest.h>

#include <cmath>
#include <limits>

#include "cnahouse/environment/Temperature.hpp"
#include "cnahouse/weather/WeatherState.hpp"

namespace
{
    using cnahouse::environment::BaseTemperatureC;
    using cnahouse::weather::kSleetRainThresholdC;
    using cnahouse::weather::kSnowSleetThresholdC;
    using cnahouse::weather::PrecipType;
    using cnahouse::weather::PrecipTypeAtTemperature;

    TEST(WeatherPrecipTypeTests, ExactTemperatureBandsAreSnowSleetAndRain)
    {
        EXPECT_EQ(PrecipTypeAtTemperature(PrecipType::Rain, -0.01F).Value(), PrecipType::Snow);
        EXPECT_EQ(PrecipTypeAtTemperature(PrecipType::Rain, kSnowSleetThresholdC).Value(), PrecipType::Sleet);
        EXPECT_EQ(PrecipTypeAtTemperature(PrecipType::Rain, 1.25F).Value(), PrecipType::Sleet);
        EXPECT_EQ(PrecipTypeAtTemperature(PrecipType::Rain, kSleetRainThresholdC).Value(), PrecipType::Sleet);
        EXPECT_EQ(
            PrecipTypeAtTemperature(PrecipType::Rain, std::nextafter(kSleetRainThresholdC, 3.0F)).Value(),
            PrecipType::Rain);
    }

    TEST(WeatherPrecipTypeTests, TemperatureOverridesEveryNominalWaterPhase)
    {
        for (const PrecipType nominal : {PrecipType::Rain, PrecipType::Snow, PrecipType::Sleet})
        {
            EXPECT_EQ(PrecipTypeAtTemperature(nominal, -4.0F).Value(), PrecipType::Snow);
            EXPECT_EQ(PrecipTypeAtTemperature(nominal, 1.0F).Value(), PrecipType::Sleet);
            EXPECT_EQ(PrecipTypeAtTemperature(nominal, 9.0F).Value(), PrecipType::Rain);
        }
    }

    TEST(WeatherPrecipTypeTests, NoneAndHailKeepTheirMeaning)
    {
        for (const float temperature : {-18.0F, 1.0F, 38.0F})
        {
            EXPECT_EQ(PrecipTypeAtTemperature(PrecipType::None, temperature).Value(), PrecipType::None);
            EXPECT_EQ(PrecipTypeAtTemperature(PrecipType::Hail, temperature).Value(), PrecipType::Hail);
        }
    }

    TEST(WeatherPrecipTypeTests, TheSameSnowArchetypeSnowsInJanuaryAndRainsInJuly)
    {
        constexpr float kSnowArchetypeOffsetC = -4.0F;
        const float january = static_cast<float>(BaseTemperatureC(18.0, 3.0)) + kSnowArchetypeOffsetC;
        const float july = static_cast<float>(BaseTemperatureC(201.0, 15.0)) + kSnowArchetypeOffsetC;

        EXPECT_EQ(PrecipTypeAtTemperature(PrecipType::Snow, january).Value(), PrecipType::Snow);
        EXPECT_EQ(PrecipTypeAtTemperature(PrecipType::Snow, july).Value(), PrecipType::Rain);
    }

    TEST(WeatherPrecipTypeTests, InvalidTemperatureAndNominalTypeAreRejected)
    {
        EXPECT_FALSE(PrecipTypeAtTemperature(PrecipType::Rain, std::numeric_limits<float>::quiet_NaN()));
        EXPECT_FALSE(PrecipTypeAtTemperature(PrecipType::Snow, -18.01F));
        EXPECT_FALSE(PrecipTypeAtTemperature(PrecipType::Rain, 38.01F));
        EXPECT_FALSE(PrecipTypeAtTemperature(static_cast<PrecipType>(255), 1.0F));
    }

} // namespace
