// SPDX-License-Identifier: MIT
#include "cnahouse/audio/AmbienceDirector.hpp"

#include <algorithm>
#include <cmath>

namespace cnahouse::audio
{

    float AmbienceDirector::DayMix(double sunAltitudeDeg) noexcept
    {
        if (!std::isfinite(sunAltitudeDeg))
        {
            return 0.0F;
        }
        const double span = kDayAltitudeDeg - kNightAltitudeDeg;
        return static_cast<float>(std::clamp((sunAltitudeDeg - kNightAltitudeDeg) / span, 0.0, 1.0));
    }

    AmbienceMix AmbienceDirector::Advance(world::CellKind listenerKind,
                                          double sunAltitudeDeg,
                                          float deltaSeconds) noexcept
    {
        const float target = listenerKind == world::CellKind::Exterior ? 1.0F : 0.0F;
        if (!initialized_)
        {
            exterior_ = target;
            initialized_ = true;
        }
        else if (std::isfinite(deltaSeconds) && deltaSeconds > 0.0F)
        {
            const float step = deltaSeconds / kCellCrossfadeSeconds;
            exterior_ += std::clamp(target - exterior_, -step, step);
        }

        const float day = DayMix(sunAltitudeDeg);
        return AmbienceMix{
            .interior = 1.0F - exterior_,
            .exteriorDay = exterior_ * day,
            .exteriorNight = exterior_ * (1.0F - day),
        };
    }

    void AmbienceDirector::Reset() noexcept
    {
        initialized_ = false;
        exterior_ = 0.0F;
        weatherInitialized_ = false;
        weatherMix_ = {};
    }

    WeatherMix AmbienceDirector::AdvanceWeather(world::CellKind listenerKind,
                                                float skyExposure,
                                                float roofDistance,
                                                float undergroundDepth,
                                                const weather::WeatherState& weather,
                                                float deltaSeconds) noexcept
    {
        const auto fraction = [](float value)
        { return std::isfinite(value) ? std::clamp(value, 0.0F, 1.0F) : 0.0F; };
        const bool exterior = listenerKind == world::CellKind::Exterior;
        const float sky = fraction(skyExposure);
        const float roof =
            std::isfinite(roofDistance) && roofDistance >= 0.0F ? fraction(1.0F - roofDistance / 8.0F) : 0.0F;
        const float aboveGround =
            std::isfinite(undergroundDepth) ? fraction(1.0F - std::max(0.0F, undergroundDepth) / 0.8F) : 0.0F;
        // One retained rain layer also carries its roof transmission. No roof/window voices.
        // A small transmitted bed survives in windowless above-ground rooms, not the basement.
        const float rainGain =
            exterior ? 1.0F : aboveGround * std::min(0.92F, 0.30F + 1.40F * std::sqrt(sky) + roof);
        const float windGain = exterior ? 1.0F : aboveGround * std::min(0.8F, 1.6F * std::sqrt(sky));
        const bool liquid = weather.precipType == weather::PrecipType::Rain ||
                            weather.precipType == weather::PrecipType::Sleet ||
                            weather.precipType == weather::PrecipType::Hail;
        const float rain = liquid ? fraction(weather.precipIntensity) : 0.0F;
        const float strongRain = fraction((rain - 0.25F) / 0.5F);
        const float wind = fraction(weather.windSpeed / 12.0F);
        const float strongWind = fraction((wind - 0.2F) / 0.6F);
        const WeatherMix target{{rainGain * rain * (1.0F - strongRain),
                                 rainGain * rain * strongRain,
                                 windGain * wind * (1.0F - strongWind),
                                 windGain * wind * strongWind},
                                exterior ? 0.0F : 1.0F - sky};
        if (!weatherInitialized_)
        {
            weatherInitialized_ = true;
            weatherMix_ = target;
        }
        else if (std::isfinite(deltaSeconds) && deltaSeconds > 0.0F)
        {
            const float step = deltaSeconds / kCellCrossfadeSeconds;
            for (std::size_t index = 0; index < target.layers.size(); ++index)
            {
                weatherMix_.layers[index] +=
                    std::clamp(target.layers[index] - weatherMix_.layers[index], -step, step);
            }
            weatherMix_.dull += std::clamp(target.dull - weatherMix_.dull, -step, step);
        }
        return weatherMix_;
    }

} // namespace cnahouse::audio
