// SPDX-License-Identifier: MIT
#include "cnahouse/weather/RainParticles.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <numbers>

#include "Microsoft/Xna/Framework/Color.hpp"
#include "Microsoft/Xna/Framework/Vector2.hpp"

#include "cnahouse/rendering/ParticleRenderer.hpp"
#include "cnahouse/weather/CoverageMask.hpp"

namespace cnahouse::weather
{
    namespace Xna = Microsoft::Xna::Framework;

    namespace
    {
        constexpr std::uint8_t kRainMaterialSlot = 0u;

        [[nodiscard]] std::uint32_t Hash(std::uint32_t value) noexcept
        {
            value ^= value >> 16u;
            value *= 0x7feb352du;
            value ^= value >> 15u;
            value *= 0x846ca68bu;
            value ^= value >> 16u;
            return value;
        }

        [[nodiscard]] float Unit(std::uint32_t value) noexcept
        {
            return static_cast<float>(Hash(value) & 0x00ffffffu) / 16777216.0F;
        }

        [[nodiscard]] float QualityScale(rendering::ParticleQuality quality) noexcept
        {
            switch (quality)
            {
                case rendering::ParticleQuality::Low:
                    return 0.55F;
                case rendering::ParticleQuality::Medium:
                    return 0.80F;
                case rendering::ParticleQuality::High:
                    return 1.0F;
            }
            return 0.55F;
        }

        [[nodiscard]] float PhaseScale(PrecipType type) noexcept
        {
            switch (type)
            {
                case PrecipType::Rain:
                case PrecipType::Sleet:
                case PrecipType::Hail:
                    return 1.0F;
                case PrecipType::None:
                case PrecipType::Snow:
                    return 0.0F;
            }
            return 0.0F;
        }
    } // namespace

    std::size_t RainParticleCount(const WeatherState& state, rendering::ParticleQuality quality) noexcept
    {
        if (!std::isfinite(state.precipIntensity))
        {
            return 0u;
        }
        const float intensity = std::clamp(state.precipIntensity, 0.0F, 1.0F);
        const float count = static_cast<float>(kRainMaximumParticleCount) * std::pow(intensity, 0.8F) *
                            QualityScale(quality) * PhaseScale(state.precipType);
        return static_cast<std::size_t>(std::lround(count));
    }

    Xna::Vector3 RainParticleVelocity(const WeatherState& state) noexcept
    {
        const Xna::Vector3 wind = PrecipitationWindVector(state);
        const float intensity = std::clamp(state.precipIntensity, 0.0F, 1.0F);
        return Xna::Vector3(wind.X * 0.55F, -(6.0F + 3.0F * intensity), wind.Z * 0.55F);
    }

    float RainStreakLength(const Xna::Vector3& velocity) noexcept
    {
        const float speed =
            std::sqrt(velocity.X * velocity.X + velocity.Y * velocity.Y + velocity.Z * velocity.Z);
        return std::clamp(speed * 0.045F, 0.10F, 0.55F);
    }

    void RainParticles::InitialisePositions() noexcept
    {
        const Xna::Vector3 centre = volume_.Centre();
        for (std::size_t index = 0u; index < positions_.size(); ++index)
        {
            const std::uint32_t seed = static_cast<std::uint32_t>(index) + 1u;
            const float radius = kPrecipitationRadiusMetres * std::sqrt(Unit(seed * 3u));
            const float angle = 2.0F * std::numbers::pi_v<float> * Unit(seed * 3u + 1u);
            const float height = (Unit(seed * 3u + 2u) - 0.5F) * kPrecipitationHeightMetres;
            positions_[index] = Xna::Vector3(
                centre.X + std::cos(angle) * radius, centre.Y + height, centre.Z + std::sin(angle) * radius);
        }
        initialised_ = true;
    }

    util::Result<void> RainParticles::Update(float deltaSeconds,
                                             const Xna::Vector3& cameraEye,
                                             const WeatherState& state,
                                             rendering::ParticleQuality quality,
                                             rendering::ParticleRenderer& renderer,
                                             const CoverageMask* coverage)
    {
        renderer.BeginFrame(quality);
        activeCount_ = 0u;
        if (!std::isfinite(deltaSeconds) || deltaSeconds < 0.0F || !std::isfinite(state.precipIntensity) ||
            state.precipIntensity < 0.0F || state.precipIntensity > 1.0F)
        {
            return util::Err(util::ErrorCode::OutOfRange,
                             "rain delta and precipitation intensity must be finite and non-negative",
                             "weather/rain-particles");
        }

        const util::Result<void> followed = volume_.Follow(
            cameraEye, state, initialised_ ? std::span<Xna::Vector3>(positions_) : std::span<Xna::Vector3>());
        if (!followed)
        {
            return followed;
        }
        if (!initialised_)
        {
            InitialisePositions();
        }

        const Xna::Vector3 velocity = RainParticleVelocity(state);
        for (Xna::Vector3& position : positions_)
        {
            position = volume_.Wrap(Xna::Vector3(position.X + velocity.X * deltaSeconds,
                                                 position.Y + velocity.Y * deltaSeconds,
                                                 position.Z + velocity.Z * deltaSeconds));
            if (coverage != nullptr)
            {
                coverage->TeleportSheltered(position, volume_.Centre().Y + 0.5F * kPrecipitationHeightMetres);
            }
        }

        const std::size_t requested = RainParticleCount(state, quality);
        const std::size_t count = std::min(requested, renderer.Capacity());
        const float halfLength = 0.5F * RainStreakLength(velocity);
        const int alpha =
            static_cast<int>(std::lround(105.0F + 70.0F * std::clamp(state.precipIntensity, 0.0F, 1.0F)));
        for (std::size_t index = 0u; index < count; ++index)
        {
            if (coverage != nullptr && !coverage->IsExposed(positions_[index]))
            {
                continue;
            }
            rendering::ParticleQuad quad;
            quad.centre = positions_[index];
            quad.elongationAxis = velocity;
            quad.halfSize = Xna::Vector2(0.014F, halfLength);
            quad.colour = Xna::Color(alpha, alpha, alpha, alpha);
            quad.material = kRainMaterialSlot;
            if (!renderer.Submit(quad))
            {
                return util::Err(util::ErrorCode::Unknown,
                                 "validated rain particle was rejected by the shared renderer",
                                 "weather/rain-particles");
            }
        }
        activeCount_ = renderer.Particles().size();
        return util::Ok();
    }
} // namespace cnahouse::weather
