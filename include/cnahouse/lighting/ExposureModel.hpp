// SPDX-License-Identifier: MIT
#pragma once

#include "cnahouse/lighting/RoomLightState.hpp"

namespace cnahouse::lighting
{

    /// @brief Time to adapt after entering a bright scene, in seconds (§25.7).
    inline constexpr float kBrightSceneAdaptationSeconds = 0.9F;

    /// @brief Time to adapt after entering a dark scene, in seconds (§25.7).
    inline constexpr float kDarkSceneAdaptationSeconds = 2.2F;

    /// @brief Maximum Tier-S exposure lift for an unlit interior.
    ///
    /// Four stops would turn §30's silhouette-only ambient floor into an evenly lit room. Six
    /// TIMES instead keeps it at 0.15 while bringing authored daylight bakes into the useful part
    /// of the framebuffer. The fixed visual-review views are the measured reason for six: at 4x,
    /// foyer and living-room wall planes still quantised into near-black.
    inline constexpr float kMaximumInteriorExposure = 6.0F;

    /// @brief Exterior target: a small eye-constriction that preserves sky highlight headroom.
    inline constexpr float kExteriorExposure = 0.82F;

    /// @brief §28.1's target for one cell, expressed as a display-light multiplier.
    ///
    /// A fully lit interior needs no lift. As the room approaches §30's ambient floor the eye
    /// opens towards 6x; a sky-open exterior constricts independently of room fixtures. The
    /// function is deliberately monotonic and bounded so a switch or cloud transition cannot
    /// produce an exposure discontinuity of its own.
    [[nodiscard]] float ExposureTargetFor(const RoomLightState& room, bool skyOpenExterior) noexcept;

    /// @brief Camera-owned asymmetric adaptation towards the current cell's target.
    class ExposureAdapter
    {
    public:
        /// @brief Advance by simulation seconds; the first valid target establishes the baseline.
        void Advance(float target, float deltaSeconds) noexcept;

        /// @brief Establish a target without a transition (spawn/load/camera-test boundary).
        void Snap(float target) noexcept;

        [[nodiscard]] float Scale() const noexcept
        {
            return scale_;
        }

        /// @brief Effect-side part of Tier S. Values below one are left to the tint quad.
        [[nodiscard]] float EffectScale() const noexcept;

        /// @brief Alpha of the premultiplied black full-screen residual quad.
        [[nodiscard]] float TintAlpha() const noexcept;

        [[nodiscard]] bool Initialized() const noexcept
        {
            return initialized_;
        }

    private:
        float scale_ = 1.0F;
        bool initialized_ = false;
    };

} // namespace cnahouse::lighting
