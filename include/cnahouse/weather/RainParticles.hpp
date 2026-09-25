// SPDX-License-Identifier: MIT
#pragma once

#include <array>
#include <cstddef>
#include <span>

#include "Microsoft/Xna/Framework/Vector3.hpp"

#include "cnahouse/rendering/Quality.hpp"
#include "cnahouse/util/Result.hpp"
#include "cnahouse/weather/PrecipitationVolume.hpp"
#include "cnahouse/weather/WeatherState.hpp"

namespace cnahouse::rendering
{
    class ParticleRenderer;
}

namespace cnahouse::weather
{
    class CoverageMask;

    inline constexpr std::size_t kRainMaximumParticleCount = 900u;

    /// @brief §37.1's quality- and intensity-scaled fixed-pool count.
    [[nodiscard]] std::size_t RainParticleCount(const WeatherState& state,
                                                rendering::ParticleQuality quality) noexcept;

    /// @brief §37.1's rate-limited wind contribution plus downward rain velocity.
    [[nodiscard]] Microsoft::Xna::Framework::Vector3 RainParticleVelocity(const WeatherState& state) noexcept;

    /// @brief §37.1's velocity-scaled streak length, clamped to 0.10–0.55 metres.
    [[nodiscard]] float RainStreakLength(const Microsoft::Xna::Framework::Vector3& velocity) noexcept;

    /// @brief Fixed-pool rain motion and submission into the shared particle renderer.
    class RainParticles
    {
    public:
        /// @brief Advances all fixed positions, wraps them and submits the active prefix.
        ///
        /// Rain, sleet and hail use the same compact streak material. None and snow submit no
        /// quads. The renderer is reset even on a dry frame so stale rain cannot survive a phase
        /// change.
        [[nodiscard]] util::Result<void> Update(float deltaSeconds,
                                                const Microsoft::Xna::Framework::Vector3& cameraEye,
                                                const WeatherState& state,
                                                rendering::ParticleQuality quality,
                                                rendering::ParticleRenderer& renderer,
                                                const CoverageMask* coverage = nullptr);

        [[nodiscard]] std::span<const Microsoft::Xna::Framework::Vector3> Positions() const noexcept
        {
            return positions_;
        }

        [[nodiscard]] std::size_t ActiveCount() const noexcept
        {
            return activeCount_;
        }

    private:
        PrecipitationVolume volume_;
        std::array<Microsoft::Xna::Framework::Vector3, kRainMaximumParticleCount> positions_{};
        bool initialised_ = false;
        std::size_t activeCount_ = 0u;

        void InitialisePositions() noexcept;
    };
} // namespace cnahouse::weather
