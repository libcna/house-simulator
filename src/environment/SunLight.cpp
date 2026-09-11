// SPDX-License-Identifier: MIT
#include "cnahouse/environment/SunLight.hpp"

#include <cmath>

namespace cnahouse::environment
{
    namespace
    {
        using Microsoft::Xna::Framework::Vector3;

        constexpr double kDegToRad = 0.017453292519943295769236907684886;

        [[nodiscard]] float Lerp(float from, float to, float t) noexcept
        {
            return from + (to - from) * t;
        }

        /// @brief `kSunLightingAnchors` evaluated at @p altitudeDeg, clamped at both ends.
        [[nodiscard]] SunLighting FromAnchors(float altitudeDeg) noexcept
        {
            const auto& anchors = kSunLightingAnchors;
            if (altitudeDeg <= anchors.front().altitudeDeg)
            {
                const auto& low = anchors.front();
                return SunLighting{Vector3(low.red, low.green, low.blue), low.intensity};
            }
            if (altitudeDeg >= anchors.back().altitudeDeg)
            {
                const auto& high = anchors.back();
                return SunLighting{Vector3(high.red, high.green, high.blue), high.intensity};
            }
            for (std::size_t index = 1; index < anchors.size(); ++index)
            {
                const SunLightingAnchor& upper = anchors[index];
                if (altitudeDeg <= upper.altitudeDeg)
                {
                    const SunLightingAnchor& lower = anchors[index - 1];
                    const float span = upper.altitudeDeg - lower.altitudeDeg;
                    const float t = (altitudeDeg - lower.altitudeDeg) / span;
                    return SunLighting{Vector3(Lerp(lower.red, upper.red, t),
                                               Lerp(lower.green, upper.green, t),
                                               Lerp(lower.blue, upper.blue, t)),
                                       Lerp(lower.intensity, upper.intensity, t)};
                }
            }
            const auto& high = anchors.back();
            return SunLighting{Vector3(high.red, high.green, high.blue), high.intensity};
        }

        /// @brief The 64 entries, built once from the anchors.
        ///
        /// Entry `i` is the altitude `kSunLutLowestDeg + i · step`, so entry 0 and entry 63 sit
        /// exactly on the first and last anchor and the table covers the whole interpolated range
        /// with no entry wasted on the constant tails.
        [[nodiscard]] const std::array<SunLighting, kSunLutSize>& Table() noexcept
        {
            static const std::array<SunLighting, kSunLutSize> table = []
            {
                std::array<SunLighting, kSunLutSize> built{};
                const float step =
                    (kSunLutHighestDeg - kSunLutLowestDeg) / static_cast<float>(kSunLutSize - 1);
                for (std::size_t index = 0; index < kSunLutSize; ++index)
                {
                    built[index] = FromAnchors(kSunLutLowestDeg + step * static_cast<float>(index));
                }
                return built;
            }();
            return table;
        }

        [[nodiscard]] double ClampUnit(double value) noexcept
        {
            if (!std::isfinite(value))
            {
                return 0.0;
            }
            return value < 0.0 ? 0.0 : (value > 1.0 ? 1.0 : value);
        }

    } // namespace

    Vector3 DirectionToSun(const SunPosition& sun) noexcept
    {
        const double altitudeRad = sun.altitudeDeg * kDegToRad;
        const double azimuthRad = sun.azimuthDeg * kDegToRad;
        const double cosAltitude = std::cos(altitudeRad);
        return Vector3(static_cast<float>(cosAltitude * std::sin(azimuthRad)),
                       static_cast<float>(std::sin(altitudeRad)),
                       static_cast<float>(-cosAltitude * std::cos(azimuthRad)));
    }

    Vector3 SunDirection(const SunPosition& sun) noexcept
    {
        const Vector3 toSun = DirectionToSun(sun);
        return Vector3(-toSun.X, -toSun.Y, -toSun.Z);
    }

    SunLighting SunLightingAt(double altitudeDeg) noexcept
    {
        if (!std::isfinite(altitudeDeg))
        {
            // A NaN altitude reaching a directional light is a black or a white screen with no
            // error message. Night is the safe answer: it is what the caller gets anyway when the
            // sun is down, so nothing downstream has a case it has not already handled.
            return Table().front();
        }
        const auto& table = Table();
        const double lowest = static_cast<double>(kSunLutLowestDeg);
        const double highest = static_cast<double>(kSunLutHighestDeg);
        if (altitudeDeg <= lowest)
        {
            return table.front();
        }
        if (altitudeDeg >= highest)
        {
            return table.back();
        }
        const double position =
            (altitudeDeg - lowest) / (highest - lowest) * static_cast<double>(kSunLutSize - 1);
        const auto index = static_cast<std::size_t>(position);
        // `position` is strictly inside the range, so `index + 1` exists; the guard is against the
        // one value of `altitudeDeg` where rounding could put it on the last entry.
        const std::size_t next = index + 1 < kSunLutSize ? index + 1 : index;
        const auto t = static_cast<float>(position - static_cast<double>(index));
        const SunLighting& low = table[index];
        const SunLighting& high = table[next];
        return SunLighting{Vector3(Lerp(low.color.X, high.color.X, t),
                                   Lerp(low.color.Y, high.color.Y, t),
                                   Lerp(low.color.Z, high.color.Z, t)),
                           Lerp(low.intensity, high.intensity, t)};
    }

    const std::array<SunLighting, kSunLutSize>& SunLightingTable() noexcept
    {
        return Table();
    }

    double DirectCloudFactor(double cloudCover) noexcept
    {
        return 1.0 - 0.85 * ClampUnit(cloudCover);
    }

    double SkyDiffuseCloudFactor(double cloudCover) noexcept
    {
        return 1.0 - 0.35 * ClampUnit(cloudCover);
    }

    SunShading SunShadingFor(const SunPosition& sun, double cloudCover) noexcept
    {
        const SunLighting lighting = SunLightingAt(sun.altitudeDeg);
        SunShading shading;
        shading.color = lighting.color;
        shading.directIntensity =
            static_cast<float>(static_cast<double>(lighting.intensity) * DirectCloudFactor(cloudCover));
        shading.skyDiffuseIntensity =
            static_cast<float>(static_cast<double>(lighting.intensity) * SkyDiffuseCloudFactor(cloudCover));
        return shading;
    }

} // namespace cnahouse::environment
