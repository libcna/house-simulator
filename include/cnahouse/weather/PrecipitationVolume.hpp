// SPDX-License-Identifier: MIT
#pragma once

#include <span>

#include "Microsoft/Xna/Framework/Vector3.hpp"

#include "cnahouse/util/Result.hpp"
#include "cnahouse/weather/WeatherState.hpp"

namespace cnahouse::weather
{
    inline constexpr float kPrecipitationRadiusMetres = 12.0F;
    inline constexpr float kPrecipitationHeightMetres = 14.0F;
    inline constexpr float kPrecipitationWindOffsetMetres = 3.0F;

    /// @brief The horizontal air velocity described by the live meteorological wind state.
    ///
    /// Authored direction is where wind comes from. In the project's +X east / -Z north world,
    /// the air therefore travels `(-sin(direction), 0, +cos(direction)) * speed`. Gust factor is
    /// deliberately absent: the compact environment retains no separate gust model.
    [[nodiscard]] Microsoft::Xna::Framework::Vector3
    PrecipitationWindVector(const WeatherState& state) noexcept;

    /// @brief Camera-relative §37.1 precipitation cylinder with allocation-free particle wrapping.
    class PrecipitationVolume
    {
    public:
        /// @brief Follows @p cameraEye, offset three metres downwind, then wraps every point.
        /// @return an error without mutation when the camera or wind inputs are non-finite.
        [[nodiscard]] util::Result<void>
        Follow(const Microsoft::Xna::Framework::Vector3& cameraEye,
               const WeatherState& state,
               std::span<Microsoft::Xna::Framework::Vector3> particlePositions);

        /// @brief Wraps one point through the opposite vertical/circular boundary.
        [[nodiscard]] Microsoft::Xna::Framework::Vector3
        Wrap(const Microsoft::Xna::Framework::Vector3& position) const noexcept;

        /// @brief Negative inside the cylinder, zero on it, positive outside it.
        [[nodiscard]] float BoundaryExcess(const Microsoft::Xna::Framework::Vector3& position) const noexcept;

        [[nodiscard]] const Microsoft::Xna::Framework::Vector3& Centre() const noexcept
        {
            return centre_;
        }

    private:
        Microsoft::Xna::Framework::Vector3 centre_;
    };
} // namespace cnahouse::weather
