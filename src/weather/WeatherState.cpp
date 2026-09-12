// SPDX-License-Identifier: MIT
#include "cnahouse/weather/WeatherState.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <format>
#include <tuple>
#include <utility>

#include "System/Text/Json/JsonWriterOptions.hpp"
#include "System/Text/Json/Utf8JsonWriter.hpp"
#include "cnahouse/util/Json.hpp"

namespace cnahouse::weather
{
    namespace
    {
        using cnahouse::util::ErrorCode;
        using cnahouse::util::JsonValue;
        using cnahouse::util::Result;

        Result<void> InRange(float value, float minimum, float maximum, std::string_view field)
        {
            if (!(value >= minimum && value <= maximum))
            {
                return util::Err(ErrorCode::OutOfRange,
                                 std::format("value {} is outside {}..{}",
                                             static_cast<double>(value),
                                             static_cast<double>(minimum),
                                             static_cast<double>(maximum)),
                                 std::string(field));
            }
            return util::Ok();
        }

        Result<util::Rng::State> RngStateFromHex(std::string_view text)
        {
            if (!text.starts_with("0x") || text.size() != 66)
            {
                return util::Err(ErrorCode::InvalidData,
                                 "expected 0x followed by the RNG's 64 hexadecimal state digits",
                                 "rngState");
            }
            util::Rng rng(0);
            if (!rng.FromHex(text.substr(2)))
            {
                return util::Err(ErrorCode::InvalidData,
                                 "expected 0x followed by the RNG's 64 hexadecimal state digits",
                                 "rngState");
            }
            return rng.GetState();
        }

        template<typename T>
        bool Assign(Result<T>& value, T& destination, util::Error& error)
        {
            if (!value)
            {
                error = value.Error();
                return false;
            }
            destination = std::move(*value);
            return true;
        }

    } // namespace

    util::Result<PrecipType> ParsePrecipType(std::string_view name, std::string_view context)
    {
        constexpr std::array<std::pair<std::string_view, PrecipType>, 5> kNames{
            std::pair{"None", PrecipType::None},
            std::pair{"Rain", PrecipType::Rain},
            std::pair{"Snow", PrecipType::Snow},
            std::pair{"Sleet", PrecipType::Sleet},
            std::pair{"Hail", PrecipType::Hail},
        };
        for (const auto& [candidate, value] : kNames)
        {
            if (name == candidate)
            {
                return value;
            }
        }
        return util::Err(ErrorCode::InvalidData,
                         std::format("'{}' is not None, Rain, Snow, Sleet or Hail", name),
                         std::string(context));
    }

    util::Result<PrecipType> PrecipTypeAtTemperature(PrecipType nominalType, float temperatureC)
    {
        if (!std::isfinite(temperatureC) || temperatureC < -18.0F || temperatureC > 38.0F)
        {
            return util::Err(ErrorCode::OutOfRange,
                             "precipitation temperature must be finite and in [-18, 38] C",
                             "weather/temperatureC");
        }
        switch (nominalType)
        {
            case PrecipType::None:
            case PrecipType::Hail:
                return nominalType;
            case PrecipType::Rain:
            case PrecipType::Snow:
            case PrecipType::Sleet:
                if (temperatureC > kSleetRainThresholdC)
                {
                    return PrecipType::Rain;
                }
                if (temperatureC < kSnowSleetThresholdC)
                {
                    return PrecipType::Snow;
                }
                return PrecipType::Sleet;
        }
        return util::Err(
            ErrorCode::InvalidData, "the nominal precipitation phase is not defined", "weather/precipType");
    }

    util::Result<void> WeatherState::Validate() const
    {
        switch (precipType)
        {
            case PrecipType::None:
            case PrecipType::Rain:
            case PrecipType::Snow:
            case PrecipType::Sleet:
            case PrecipType::Hail:
                break;
            default:
                return util::Err(ErrorCode::InvalidData,
                                 "the precipitation phase is not a defined PrecipType",
                                 "precipType");
        }
        if (std::ranges::all_of(rngState, [](std::uint64_t word) { return word == 0; }))
        {
            return util::Err(
                ErrorCode::InvalidData, "xoshiro256++ cannot use the all-zero state", "rngState");
        }

        const std::array ranges{
            std::tuple{cloudCover, 0.0F, 1.0F, std::string_view{"cloudCover"}},
            std::tuple{cloudCumuliform, 0.0F, 1.0F, std::string_view{"cloudCumuliform"}},
            std::tuple{precipIntensity, 0.0F, 1.0F, std::string_view{"precipIntensity"}},
            std::tuple{windSpeed, 0.0F, 30.0F, std::string_view{"windSpeed"}},
            std::tuple{windDirectionDeg, 0.0F, 360.0F, std::string_view{"windDirectionDeg"}},
            std::tuple{gustFactor, 0.0F, 1.0F, std::string_view{"gustFactor"}},
            std::tuple{fogDensity, 0.0F, 1.0F, std::string_view{"fogDensity"}},
            std::tuple{thunderIntensity, 0.0F, 1.0F, std::string_view{"thunderIntensity"}},
            std::tuple{temperatureC, -18.0F, 38.0F, std::string_view{"temperatureC"}},
            std::tuple{humidity, 0.0F, 1.0F, std::string_view{"humidity"}},
            std::tuple{surfaceWetness, 0.0F, 1.0F, std::string_view{"surfaceWetness"}},
            std::tuple{snowDepth, 0.0F, 0.35F, std::string_view{"snowDepth"}},
        };
        for (const auto& [value, minimum, maximum, field] : ranges)
        {
            const auto valid = InRange(value, minimum, maximum, field);
            if (!valid)
            {
                return valid.Error();
            }
        }
        return util::Ok();
    }

    util::Result<WeatherState> WeatherState::FromJson(std::string_view text, std::string_view name)
    {
        auto document = util::JsonDocument::Parse(text, std::string(name));
        if (!document)
        {
            return document.Error();
        }
        const JsonValue& root = document->Root();
        WeatherState state;
        util::Error error;

        auto cloudCoverValue = root.RequireFloat("cloudCover");
        if (!Assign(cloudCoverValue, state.cloudCover, error))
        {
            return error;
        }
        auto cloudCumuliformValue = root.RequireFloat("cloudCumuliform");
        if (!Assign(cloudCumuliformValue, state.cloudCumuliform, error))
        {
            return error;
        }
        auto precipTypeText = root.RequireString("precipType");
        if (!precipTypeText)
        {
            return precipTypeText.Error();
        }
        auto precipTypeValue = ParsePrecipType(*precipTypeText);
        if (!precipTypeValue)
        {
            return precipTypeValue.Error();
        }
        state.precipType = *precipTypeValue;

        auto precipIntensityValue = root.RequireFloat("precipIntensity");
        if (!Assign(precipIntensityValue, state.precipIntensity, error))
        {
            return error;
        }
        auto windSpeedValue = root.RequireFloat("windSpeed");
        if (!Assign(windSpeedValue, state.windSpeed, error))
        {
            return error;
        }
        auto windDirectionValue = root.RequireFloat("windDirectionDeg");
        if (!Assign(windDirectionValue, state.windDirectionDeg, error))
        {
            return error;
        }
        auto gustValue = root.RequireFloat("gustFactor");
        if (!Assign(gustValue, state.gustFactor, error))
        {
            return error;
        }
        auto fogValue = root.RequireFloat("fogDensity");
        if (!Assign(fogValue, state.fogDensity, error))
        {
            return error;
        }
        auto thunderValue = root.RequireFloat("thunderIntensity");
        if (!Assign(thunderValue, state.thunderIntensity, error))
        {
            return error;
        }
        auto temperatureValue = root.RequireFloat("temperatureC");
        if (!Assign(temperatureValue, state.temperatureC, error))
        {
            return error;
        }
        auto humidityValue = root.RequireFloat("humidity");
        if (!Assign(humidityValue, state.humidity, error))
        {
            return error;
        }
        auto wetnessValue = root.RequireFloat("surfaceWetness");
        if (!Assign(wetnessValue, state.surfaceWetness, error))
        {
            return error;
        }
        auto snowValue = root.RequireFloat("snowDepth");
        if (!Assign(snowValue, state.snowDepth, error))
        {
            return error;
        }
        auto rngText = root.RequireString("rngState");
        if (!rngText)
        {
            return rngText.Error();
        }
        auto rngValue = RngStateFromHex(*rngText);
        if (!rngValue)
        {
            return rngValue.Error();
        }
        state.rngState = *rngValue;

        const auto valid = state.Validate();
        if (!valid)
        {
            return valid.Error();
        }
        return state;
    }

    util::Result<std::string> WeatherState::ToJson() const
    {
        const auto valid = Validate();
        if (!valid)
        {
            return valid.Error();
        }

        System::Text::Json::JsonWriterOptions options;
        options.Indented = true;
        System::Text::Json::Utf8JsonWriter writer(options);
        writer.WriteStartObject();
        writer.WriteNumber("cloudCover", static_cast<double>(cloudCover));
        writer.WriteNumber("cloudCumuliform", static_cast<double>(cloudCumuliform));
        writer.WriteString("precipType", std::string(PrecipTypeName(precipType)));
        writer.WriteNumber("precipIntensity", static_cast<double>(precipIntensity));
        writer.WriteNumber("windSpeed", static_cast<double>(windSpeed));
        writer.WriteNumber("windDirectionDeg", static_cast<double>(windDirectionDeg));
        writer.WriteNumber("gustFactor", static_cast<double>(gustFactor));
        writer.WriteNumber("fogDensity", static_cast<double>(fogDensity));
        writer.WriteNumber("thunderIntensity", static_cast<double>(thunderIntensity));
        writer.WriteNumber("temperatureC", static_cast<double>(temperatureC));
        writer.WriteNumber("humidity", static_cast<double>(humidity));
        writer.WriteNumber("surfaceWetness", static_cast<double>(surfaceWetness));
        writer.WriteNumber("snowDepth", static_cast<double>(snowDepth));
        writer.WriteString("rngState", "0x" + util::Rng(rngState).ToHex());
        writer.WriteEndObject();
        return writer.GetString();
    }

} // namespace cnahouse::weather
