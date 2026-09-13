// SPDX-License-Identifier: MIT
#pragma once

#include "Microsoft/Xna/Framework/Vector3.hpp"

#include "cnahouse/environment/MoonModel.hpp"

namespace cnahouse::environment
{

    /// @brief §33.4's maximum moonlight relative to full daylight.
    inline constexpr float kMoonMaximumIntensity = 0.0022F;
    /// @brief The artistic solar-altitude boundary below which moonlight may become the key.
    inline constexpr double kMoonlightSunCutoffDeg = -4.0;

    /// @brief §33.4's opposition-weighted phase curve, `illumination^1.8`.
    [[nodiscard]] double MoonPhaseIllumination(double illuminatedFraction) noexcept;

    /// @brief §33.4's moonlight cloud transmission, `1 − 0.95·cloudCover`.
    [[nodiscard]] double MoonCloudFactor(double cloudCover) noexcept;

    /// @brief Whether the sun is strictly below §33.4's −4° moon-key boundary.
    [[nodiscard]] bool MoonlightMayBeKey(double sunAltitudeDeg) noexcept;

    /// @brief World-space unit vector from the observer towards @p moon.
    [[nodiscard]] Microsoft::Xna::Framework::Vector3 DirectionToMoon(const MoonPosition& moon) noexcept;

    /// @brief Direction in which the moonlight travels, for XNA's `DirectionalLight`.
    [[nodiscard]] Microsoft::Xna::Framework::Vector3 MoonDirection(const MoonPosition& moon) noexcept;

    struct MoonShading
    {
        Microsoft::Xna::Framework::Vector3 color{0.62F, 0.70F, 1.00F};
        float intensity = 0.0F;
    };

    /// @brief Evaluate §33.4's blue moonlight from phase, altitude and cloud cover.
    ///
    /// A non-finite altitude or illumination fails closed. Cloud cover is clamped to `[0,1]` and,
    /// like `SunShadingFor`, a non-finite cover is treated as clear; a moon at or below the
    /// geometric horizon contributes no directional light.
    [[nodiscard]] MoonShading
    MoonShadingFor(const MoonPosition& moon, const MoonPhase& phase, double cloudCover) noexcept;

} // namespace cnahouse::environment
