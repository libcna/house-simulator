// SPDX-License-Identifier: MIT
//
// `HOUSE-01681`: §36.1's complete continuous weather vector and its JSON representation.
#include <array>
#include <limits>
#include <string>
#include <string_view>

#include <gtest/gtest.h>

#include "cnahouse/util/Result.hpp"
#include "cnahouse/weather/WeatherState.hpp"

namespace
{
    using cnahouse::util::ErrorCode;
    using cnahouse::weather::PrecipType;
    using cnahouse::weather::PrecipTypeName;
    using cnahouse::weather::WeatherState;

    WeatherState DistinctState()
    {
        WeatherState state;
        state.cloudCover = 0.81F;
        state.cloudCumuliform = 0.34F;
        state.precipType = PrecipType::Rain;
        state.precipIntensity = 0.42F;
        state.windSpeed = 6.1F;
        state.windDirectionDeg = 231.0F;
        state.gustFactor = 0.38F;
        state.fogDensity = 0.19F;
        state.thunderIntensity = 0.02F;
        state.temperatureC = 9.4F;
        state.humidity = 0.88F;
        state.surfaceWetness = 0.71F;
        state.snowDepth = 0.12F;
        state.rngState = {
            std::numeric_limits<std::uint64_t>::max(),
            0xFEDCBA9876543210ULL,
            0x0123456789ABCDEFULL,
            0x8000000000000000ULL,
        };
        return state;
    }

    std::string ValidJson(std::string_view precipType = "Rain")
    {
        return std::string{R"({
          "cloudCover": 0.81,
          "cloudCumuliform": 0.34,
          "precipType": ")"} +
               std::string(precipType) + R"(",
          "precipIntensity": 0.42,
          "windSpeed": 6.1,
          "windDirectionDeg": 231.0,
          "gustFactor": 0.38,
          "fogDensity": 0.19,
          "thunderIntensity": 0.02,
          "temperatureC": 9.4,
          "humidity": 0.88,
          "surfaceWetness": 0.71,
          "snowDepth": 0.12,
          "rngState": "0xFFFFFFFFFFFFFFFFFEDCBA98765432100123456789ABCDEF8000000000000000"
        })";
    }

    TEST(WeatherStateTests, EveryFieldRoundTripsThroughSystemTextJson)
    {
        const WeatherState written = DistinctState();
        const auto json = written.ToJson();
        ASSERT_TRUE(json) << json.Error().ToString();

        const auto read = WeatherState::FromJson(*json, "weather-save.json");
        ASSERT_TRUE(read) << read.Error().ToString();
        EXPECT_EQ(*read, written);
        EXPECT_NE(json->find("\"precipType\": \"Rain\""), std::string::npos);
        EXPECT_NE(json->find("\"rngState\": "
                             "\"0xfffffffffffffffffedcba98765432100123456789abcdef8000000000000000\""),
                  std::string::npos)
            << "all four unsigned RNG words were written as lossless hexadecimal text";
    }

    TEST(WeatherStateTests, EveryPrecipitationPhaseHasAStableJsonName)
    {
        constexpr std::array types{
            PrecipType::None,
            PrecipType::Rain,
            PrecipType::Snow,
            PrecipType::Sleet,
            PrecipType::Hail,
        };
        constexpr std::array<std::string_view, 5> names{"None", "Rain", "Snow", "Sleet", "Hail"};
        for (std::size_t i = 0; i < types.size(); ++i)
        {
            EXPECT_EQ(PrecipTypeName(types[i]), names[i]);
            const auto read = WeatherState::FromJson(ValidJson(names[i]), "weather.json");
            ASSERT_TRUE(read) << read.Error().ToString();
            EXPECT_EQ(read->precipType, types[i]);
        }
    }

    TEST(WeatherStateTests, EveryFieldIsRequiredRatherThanSilentlyDefaulted)
    {
        std::string json = ValidJson();
        const std::string field = R"(          "snowDepth": 0.12,
)";
        const auto position = json.find(field);
        ASSERT_NE(position, std::string::npos);
        json.erase(position, field.size());

        const auto read = WeatherState::FromJson(json, "weather.json");
        ASSERT_FALSE(read);
        EXPECT_EQ(read.Error().Code(), ErrorCode::SchemaMismatch);
        EXPECT_EQ(read.Error().Context(), "snowDepth");
    }

    TEST(WeatherStateTests, WrongTypesAndUnknownPhasesAreDataErrorsWithFieldPaths)
    {
        const auto wrongType = WeatherState::FromJson(R"({"cloudCover":"overcast"})", "weather.json");
        ASSERT_FALSE(wrongType);
        EXPECT_EQ(wrongType.Error().Context(), "cloudCover");

        const auto unknown = WeatherState::FromJson(ValidJson("FreezingRain"), "weather.json");
        ASSERT_FALSE(unknown);
        EXPECT_EQ(unknown.Error().Code(), ErrorCode::InvalidData);
        EXPECT_EQ(unknown.Error().Context(), "precipType");
    }

    TEST(WeatherStateTests, DocumentedRangesAreCheckedAtBothJsonDoors)
    {
        WeatherState invalid = DistinctState();
        invalid.snowDepth = 0.36F;
        auto written = invalid.ToJson();
        ASSERT_FALSE(written);
        EXPECT_EQ(written.Error().Code(), ErrorCode::OutOfRange);
        EXPECT_EQ(written.Error().Context(), "snowDepth");

        std::string json = ValidJson();
        const auto position = json.find("\"windSpeed\": 6.1");
        ASSERT_NE(position, std::string::npos);
        json.replace(position, std::string_view{"\"windSpeed\": 6.1"}.size(), "\"windSpeed\": 31");
        const auto read = WeatherState::FromJson(json, "weather.json");
        ASSERT_FALSE(read);
        EXPECT_EQ(read.Error().Code(), ErrorCode::OutOfRange);
        EXPECT_EQ(read.Error().Context(), "windSpeed");
    }

    TEST(WeatherStateTests, NonFiniteStateCannotReachAJsonWriter)
    {
        WeatherState state = DistinctState();
        state.cloudCover = std::numeric_limits<float>::quiet_NaN();
        const auto json = state.ToJson();
        ASSERT_FALSE(json);
        EXPECT_EQ(json.Error().Code(), ErrorCode::OutOfRange);
        EXPECT_EQ(json.Error().Context(), "cloudCover");
    }

    TEST(WeatherStateTests, AnInvalidEnumValueCannotBecomeNoPrecipitation)
    {
        WeatherState state = DistinctState();
        state.precipType = static_cast<PrecipType>(255);
        const auto json = state.ToJson();
        ASSERT_FALSE(json);
        EXPECT_EQ(json.Error().Code(), ErrorCode::InvalidData);
        EXPECT_EQ(json.Error().Context(), "precipType");
    }

    TEST(WeatherStateTests, TheForbiddenAllZeroGeneratorStateIsRejected)
    {
        WeatherState state = DistinctState();
        state.rngState = {};
        const auto json = state.ToJson();
        ASSERT_FALSE(json);
        EXPECT_EQ(json.Error().Code(), ErrorCode::InvalidData);
        EXPECT_EQ(json.Error().Context(), "rngState");
    }

    TEST(WeatherStateTests, MalformedRngTextIsRejectedWithoutLosingPrecision)
    {
        for (const std::string_view bad :
             {"FFFFFFFFFFFFFFFFFEDCBA98765432100123456789ABCDEF8000000000000000",
              "0x",
              "0xFFFFFFFFFFFFFFFF",
              "0xFFFFFFFFFFFFFFFFFEDCBA98765432100123456789ABCDEF800000000000000Z"})
        {
            std::string json = ValidJson();
            const std::string good = "0xFFFFFFFFFFFFFFFFFEDCBA98765432100123456789ABCDEF8000000000000000";
            const auto position = json.find(good);
            ASSERT_NE(position, std::string::npos);
            json.replace(position, good.size(), bad);

            const auto read = WeatherState::FromJson(json, "weather.json");
            ASSERT_FALSE(read) << bad;
            EXPECT_EQ(read.Error().Context(), "rngState") << bad;
        }
    }

} // namespace
