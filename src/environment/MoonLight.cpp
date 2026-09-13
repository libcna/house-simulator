// SPDX-License-Identifier: MIT
#include "cnahouse/environment/MoonLight.hpp"

#include <algorithm>
#include <cmath>

namespace cnahouse::environment
{
    namespace
    {
        constexpr double kDegToRad = 0.017453292519943295769236907684886;

        [[nodiscard]] double ClampUnit(double value) noexcept
        {
            if (!std::isfinite(value))
            {
                return 0.0;
            }
            return std::clamp(value, 0.0, 1.0);
        }
    } // namespace

    double MoonPhaseIllumination(double illuminatedFraction) noexcept
    {
        return std::pow(ClampUnit(illuminatedFraction), 1.8);
    }

    double MoonCloudFactor(double cloudCover) noexcept
    {
        return 1.0 - 0.95 * ClampUnit(cloudCover);
    }

    bool MoonlightMayBeKey(double sunAltitudeDeg) noexcept
    {
        return std::isfinite(sunAltitudeDeg) && sunAltitudeDeg < kMoonlightSunCutoffDeg;
    }

    Microsoft::Xna::Framework::Vector3 DirectionToMoon(const MoonPosition& moon) noexcept
    {
        if (!std::isfinite(moon.altitudeDeg) || !std::isfinite(moon.azimuthDeg))
        {
            return Microsoft::Xna::Framework::Vector3();
        }
        const double altitudeRad = moon.altitudeDeg * kDegToRad;
        const double azimuthRad = moon.azimuthDeg * kDegToRad;
        const double cosAltitude = std::cos(altitudeRad);
        return Microsoft::Xna::Framework::Vector3(static_cast<float>(cosAltitude * std::sin(azimuthRad)),
                                                  static_cast<float>(std::sin(altitudeRad)),
                                                  static_cast<float>(-cosAltitude * std::cos(azimuthRad)));
    }

    Microsoft::Xna::Framework::Vector3 MoonDirection(const MoonPosition& moon) noexcept
    {
        const Microsoft::Xna::Framework::Vector3 toMoon = DirectionToMoon(moon);
        return Microsoft::Xna::Framework::Vector3(-toMoon.X, -toMoon.Y, -toMoon.Z);
    }

    MoonShading MoonShadingFor(const MoonPosition& moon, const MoonPhase& phase, double cloudCover) noexcept
    {
        MoonShading shading;
        if (!std::isfinite(moon.altitudeDeg))
        {
            return shading;
        }
        const double altitudeFactor = std::pow(std::max(0.0, std::sin(moon.altitudeDeg * kDegToRad)), 0.6);
        shading.intensity = static_cast<float>(static_cast<double>(kMoonMaximumIntensity) *
                                               MoonPhaseIllumination(phase.illuminatedFraction) *
                                               altitudeFactor * MoonCloudFactor(cloudCover));
        return shading;
    }

} // namespace cnahouse::environment
