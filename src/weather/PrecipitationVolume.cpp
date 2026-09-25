// SPDX-License-Identifier: MIT
#include "cnahouse/weather/PrecipitationVolume.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <numbers>

namespace cnahouse::weather
{
    namespace
    {
        [[nodiscard]] bool Finite(const Microsoft::Xna::Framework::Vector3& value) noexcept
        {
            return std::isfinite(value.X) && std::isfinite(value.Y) && std::isfinite(value.Z);
        }

        [[nodiscard]] float WrapSigned(float value, float halfExtent) noexcept
        {
            const float period = 2.0F * halfExtent;
            float wrapped = std::fmod(value + halfExtent, period);
            if (wrapped < 0.0F)
            {
                wrapped += period;
            }
            return wrapped - halfExtent;
        }
    } // namespace

    Microsoft::Xna::Framework::Vector3 PrecipitationWindVector(const WeatherState& state) noexcept
    {
        if (!std::isfinite(state.windSpeed) || !std::isfinite(state.windDirectionDeg) ||
            state.windSpeed <= 0.0F)
        {
            return {};
        }
        const float radians = state.windDirectionDeg * std::numbers::pi_v<float> / 180.0F;
        return Microsoft::Xna::Framework::Vector3(
            -std::sin(radians) * state.windSpeed, 0.0F, std::cos(radians) * state.windSpeed);
    }

    util::Result<void>
    PrecipitationVolume::Follow(const Microsoft::Xna::Framework::Vector3& cameraEye,
                                const WeatherState& state,
                                std::span<Microsoft::Xna::Framework::Vector3> particlePositions)
    {
        if (!Finite(cameraEye) || !std::isfinite(state.windSpeed) || state.windSpeed < 0.0F ||
            !std::isfinite(state.windDirectionDeg))
        {
            return util::Err(util::ErrorCode::OutOfRange,
                             "camera and wind must be finite, with non-negative wind speed",
                             "weather/precipitation-volume");
        }
        for (const Microsoft::Xna::Framework::Vector3& position : particlePositions)
        {
            if (!Finite(position))
            {
                return util::Err(util::ErrorCode::OutOfRange,
                                 "particle positions must be finite",
                                 "weather/precipitation-volume");
            }
        }

        const Microsoft::Xna::Framework::Vector3 wind = PrecipitationWindVector(state);
        const float horizontalLength = std::sqrt(wind.X * wind.X + wind.Z * wind.Z);
        Microsoft::Xna::Framework::Vector3 nextCentre = cameraEye;
        if (horizontalLength > 0.0F)
        {
            const float scale = kPrecipitationWindOffsetMetres / horizontalLength;
            nextCentre.X += wind.X * scale;
            nextCentre.Z += wind.Z * scale;
        }
        centre_ = nextCentre;
        for (Microsoft::Xna::Framework::Vector3& position : particlePositions)
        {
            position = Wrap(position);
        }
        return util::Ok();
    }

    Microsoft::Xna::Framework::Vector3
    PrecipitationVolume::Wrap(const Microsoft::Xna::Framework::Vector3& position) const noexcept
    {
        if (!Finite(position))
        {
            return centre_;
        }

        Microsoft::Xna::Framework::Vector3 wrapped = position;
        const float halfHeight = 0.5F * kPrecipitationHeightMetres;
        wrapped.Y = centre_.Y + WrapSigned(position.Y - centre_.Y, halfHeight);

        const float localX = position.X - centre_.X;
        const float localZ = position.Z - centre_.Z;
        const float radius = std::sqrt(localX * localX + localZ * localZ);
        if (radius >= kPrecipitationRadiusMetres)
        {
            const float wrappedRadius = WrapSigned(radius, kPrecipitationRadiusMetres);
            const float scale = wrappedRadius / radius;
            wrapped.X = centre_.X + localX * scale;
            wrapped.Z = centre_.Z + localZ * scale;
        }
        return wrapped;
    }

    float
    PrecipitationVolume::BoundaryExcess(const Microsoft::Xna::Framework::Vector3& position) const noexcept
    {
        if (!Finite(position))
        {
            return std::numeric_limits<float>::infinity();
        }
        const float localX = position.X - centre_.X;
        const float localY = position.Y - centre_.Y;
        const float localZ = position.Z - centre_.Z;
        const float radialExcess = std::sqrt(localX * localX + localZ * localZ) - kPrecipitationRadiusMetres;
        const float verticalExcess = std::abs(localY) - 0.5F * kPrecipitationHeightMetres;
        return std::max(radialExcess, verticalExcess);
    }
} // namespace cnahouse::weather
