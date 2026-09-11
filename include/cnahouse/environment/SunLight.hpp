// SPDX-License-Identifier: MIT
#pragma once

#include <array>
#include <cstddef>

#include "Microsoft/Xna/Framework/Vector3.hpp"

#include "cnahouse/environment/SunModel.hpp"

namespace cnahouse::environment
{

    /// @brief §32.2's world-space unit vector pointing AT the sun.
    ///
    /// `(cos(alt)·sin(az), sin(alt), −cos(alt)·cos(az))`, which is §10.1's axes written out:
    /// azimuth is measured from NORTH clockwise through east, north is `−Z` and east is `+X`, so
    /// the horizontal part is `sin(az)` east plus `cos(az)` north and the north component carries
    /// the sign. At sunrise (azimuth 90°, altitude 0) this is `+X`; at northern noon (azimuth 180°)
    /// it leans `+Z`, which is south, because at 40° N the noon sun IS in the south.
    [[nodiscard]] Microsoft::Xna::Framework::Vector3 DirectionToSun(const SunPosition& sun) noexcept;

    /// @brief §32.2's `sunDirection`: the direction the light TRAVELS, which is `−DirectionToSun`.
    ///
    /// This is the one an XNA `DirectionalLight` wants, and the sign is the single most likely
    /// thing in §32 to be wrong in a way that looks almost right — shadows fall the correct
    /// distance and point the wrong way. `SunLightTests` asserts both vectors against the compass
    /// rather than against each other.
    [[nodiscard]] Microsoft::Xna::Framework::Vector3 SunDirection(const SunPosition& sun) noexcept;

    /// @brief What §32.2's LUT holds at one altitude: a colour and a relative intensity.
    struct SunLighting
    {
        Microsoft::Xna::Framework::Vector3 color{0.0F, 0.0F, 0.0F};
        /// @brief Relative to full daylight, `[0, 1]`.
        float intensity = 0.0F;
    };

    /// @brief §32.2's table, verbatim. Six anchors; the LUT interpolates between them.
    struct SunLightingAnchor
    {
        float altitudeDeg;
        float red;
        float green;
        float blue;
        float intensity;
    };

    /// @brief §32.2's six rows, in the order the section gives them.
    ///
    /// The lowest is CIVIL TWILIGHT and not the horizon, and the highest is +60° rather than the
    /// zenith: at §33's 40.05° N the sun never gets past 73.4°, so everything above the last anchor
    /// is one colour and one intensity anyway.
    inline constexpr std::array<SunLightingAnchor, 6> kSunLightingAnchors{{
        {-6.00F, 0.22F, 0.24F, 0.38F, 0.02F},
        {-0.83F, 1.00F, 0.42F, 0.16F, 0.10F},
        {2.00F, 1.00F, 0.62F, 0.34F, 0.35F},
        {10.00F, 1.00F, 0.86F, 0.68F, 0.72F},
        {30.00F, 1.00F, 0.96F, 0.90F, 0.94F},
        {60.00F, 1.00F, 1.00F, 0.99F, 1.00F},
    }};

    /// @brief §32.2: *"a 64-entry LUT over altitude"*.
    inline constexpr std::size_t kSunLutSize = 64;

    /// @brief The altitudes the LUT spans: the first anchor to the last.
    ///
    /// Outside them the answer is constant, so there is nothing for a table entry to say. **Below
    /// −6° that constant is the twilight row itself and not black**, which is deliberate: §32.2
    /// feeds this LUT to the sky-diffuse term as well as the direct one, and a dim blue ambient is
    /// what a night sky is. Clamping to zero instead would put a 2 % step at civil twilight into
    /// every frame of every dusk.
    inline constexpr float kSunLutLowestDeg = kSunLightingAnchors.front().altitudeDeg;
    inline constexpr float kSunLutHighestDeg = kSunLightingAnchors.back().altitudeDeg;

    /// @brief §32.2's LUT, looked up and interpolated at @p altitudeDeg.
    ///
    /// The table is built once from `kSunLightingAnchors`; this is a lookup and a lerp between two
    /// neighbouring entries, which is what makes a LUT worth having over evaluating the anchors
    /// directly every frame.
    [[nodiscard]] SunLighting SunLightingAt(double altitudeDeg) noexcept;

    /// @brief The LUT itself, for a test or a debug overlay that wants to see the whole curve.
    [[nodiscard]] const std::array<SunLighting, kSunLutSize>& SunLightingTable() noexcept;

    /// @brief §32.2's cloud factor for the DIRECT term: `1 − 0.85·cloudCover`.
    ///
    /// Heavy overcast leaves 15 % of the beam, which is why a cloudy noon still casts a faint
    /// shadow rather than none.
    [[nodiscard]] double DirectCloudFactor(double cloudCover) noexcept;

    /// @brief §32.2's cloud factor for the SKY-DIFFUSE term: `1 − 0.35·cloudCover`.
    ///
    /// Much gentler than the direct one, and that is the whole look of an overcast day: the beam
    /// goes away and the ambient barely does, so the world flattens instead of darkening.
    [[nodiscard]] double SkyDiffuseCloudFactor(double cloudCover) noexcept;

    /// @brief The lighting the sun contributes at @p sun under @p cloudCover.
    ///
    /// **The cloud factor multiplies the INTENSITY and not the colour.** A renderer uses the two
    /// together as `color × intensity`, so scaling both would apply the cloud twice; and §32.2's
    /// colours are normalised hues rather than radiances, which is what makes the intensity the
    /// place the attenuation belongs.
    struct SunShading
    {
        /// @brief The LUT's colour, unattenuated. Shared by both terms.
        Microsoft::Xna::Framework::Vector3 color{0.0F, 0.0F, 0.0F};
        float directIntensity = 0.0F;
        float skyDiffuseIntensity = 0.0F;
    };

    /// @brief @p cloudCover is clamped to `[0, 1]`; a non-finite one is treated as clear sky.
    [[nodiscard]] SunShading SunShadingFor(const SunPosition& sun, double cloudCover) noexcept;

} // namespace cnahouse::environment
