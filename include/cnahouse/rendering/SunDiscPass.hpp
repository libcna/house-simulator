// SPDX-License-Identifier: MIT
#pragma once

#include <cstddef>
#include <memory>

#include "Microsoft/Xna/Framework/Vector3.hpp"

#include "cnahouse/environment/SunModel.hpp"
#include "cnahouse/rendering/Renderer.hpp"

namespace cnahouse::rendering
{

    struct Camera;

    /// @brief The complete value-shaped answer for §32.3's sun quad this frame.
    struct SunDiscFrame
    {
        Microsoft::Xna::Framework::Vector3 centre;
        Microsoft::Xna::Framework::Vector3 tint;
        float radius = 0.0F;
        float opacity = 0.0F;
        float horizonScale = 1.0F;
        bool visible = false;
    };

    /// @brief §32.3's apparent-size multiplier.
    ///
    /// The section fixes both endpoints but not a discontinuity: the disc is 2.6x at and below
    /// the horizon, eases to its true size over the warm 0..10 degree LUT band, and is 1x above it.
    /// A smoothstep keeps neither endpoint visible as a change of speed during sunrise.
    [[nodiscard]] float SunDiscHorizonScale(double altitudeDeg) noexcept;

    /// @brief Opacity of §32.3's soft radial texture at a normalised radius.
    ///
    /// Solid through 72% of the radius, then smooth to transparent at the limb. Values outside the
    /// disc are zero. This is public pure arithmetic so the generated texture is testable without a
    /// graphics device.
    [[nodiscard]] float SunDiscRadialOpacity(float normalisedRadius) noexcept;

    /// @brief Builds §32.3's position, angular size, LUT tint and direct-light attenuation.
    [[nodiscard]] SunDiscFrame
    BuildSunDiscFrame(const Camera& camera, const environment::SunPosition& sun, double cloudCover) noexcept;

    /// @brief `Pass::Sky` implementation for §32.3's camera-facing additive sun quad.
    ///
    /// GPU resources are created once, on the first visible frame. `SetSun` copies the lighting
    /// stage's one solar answer; `Draw` never evaluates a second sun model and never allocates.
    class SunDiscPass final : public IRenderPass
    {
    public:
        explicit SunDiscPass(const Camera& camera) noexcept;
        ~SunDiscPass() override;

        void SetSun(const environment::SunPosition& sun, double cloudCover) noexcept;
        void Draw(PassContext& context) override;

        [[nodiscard]] bool IsActive() const override
        {
            return frame_.visible;
        }

        [[nodiscard]] bool DisturbsDeviceState() const override
        {
            return true;
        }

        [[nodiscard]] const SunDiscFrame& Frame() const noexcept
        {
            return frame_;
        }

    private:
        class Resources;

        const Camera* camera_ = nullptr;
        SunDiscFrame frame_;
        std::unique_ptr<Resources> resources_;
        debug::Counters* counterOwner_ = nullptr;
        std::size_t drawsCounter_ = 0;
        std::size_t scaleCounter_ = 0;
    };

} // namespace cnahouse::rendering
