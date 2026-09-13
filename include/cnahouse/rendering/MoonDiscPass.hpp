// SPDX-License-Identifier: MIT
#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>

#include "Microsoft/Xna/Framework/Vector3.hpp"

#include "cnahouse/environment/MoonModel.hpp"
#include "cnahouse/environment/SunModel.hpp"
#include "cnahouse/rendering/MoonMask.hpp"
#include "cnahouse/rendering/Renderer.hpp"

namespace Microsoft::Xna::Framework::Graphics
{
    class Texture2D;
}

namespace cnahouse::rendering
{

    struct Camera;

    /// @brief The complete device-independent answer for §33.3's lunar quad this frame.
    struct MoonDiscFrame
    {
        Microsoft::Xna::Framework::Vector3 centre;
        Microsoft::Xna::Framework::Vector3 tint;
        float radius = 0.0F;
        float horizonScale = 1.0F;
        double brightLimbAngleRadians = 0.0;
        bool visible = false;
    };

    /// @brief Position angle of the projected sun direction in the moon billboard's plane.
    ///
    /// Zero points along the quad's +U axis; positive angles rotate counter-clockwise towards +V,
    /// exactly matching `MoonMask`'s convention. At new/full moon the projection can vanish and
    /// zero is the stable answer because the terminator orientation is then invisible.
    [[nodiscard]] double MoonBrightLimbAngle(const environment::MoonPosition& moon,
                                             const environment::SunPosition& sun) noexcept;

    /// @brief Builds §33.3's position, 0.52-degree angular size and shared horizon treatment.
    [[nodiscard]] MoonDiscFrame BuildMoonDiscFrame(const Camera& camera,
                                                   const environment::MoonPosition& moon,
                                                   const environment::SunPosition& sun) noexcept;

    /// @brief `Pass::Sky` component for the masked, albedo-textured additive lunar quad.
    ///
    /// The albedo comes from the content pipeline; the second texture is `MoonMask`'s 128-square
    /// CPU output. `SetMoon` samples the shared ephemerides and updates the CPU cache. `Draw`
    /// uploads only after the cache's >1/128 phase threshold and owns no non-XNA graphics API.
    class MoonDiscPass final : public IRenderPass
    {
    public:
        explicit MoonDiscPass(const Camera& camera) noexcept;
        MoonDiscPass(const Camera& camera, Microsoft::Xna::Framework::Graphics::Texture2D albedo);
        ~MoonDiscPass() override;

        void SetAlbedo(Microsoft::Xna::Framework::Graphics::Texture2D albedo);
        void SetMoon(const environment::MoonPosition& moon,
                     const environment::MoonPhase& phase,
                     const environment::SunPosition& sun) noexcept;
        void Draw(PassContext& context) override;

        [[nodiscard]] bool IsActive() const override;

        [[nodiscard]] bool DisturbsDeviceState() const override
        {
            return true;
        }

        [[nodiscard]] const MoonDiscFrame& Frame() const noexcept
        {
            return frame_;
        }

        [[nodiscard]] std::uint64_t MaskGenerationCount() const noexcept
        {
            return mask_.GenerationCount();
        }

    private:
        class Resources;

        const Camera* camera_ = nullptr;
        MoonDiscFrame frame_;
        MoonMask mask_;
        std::unique_ptr<Microsoft::Xna::Framework::Graphics::Texture2D> albedo_;
        std::unique_ptr<Resources> resources_;
        std::uint64_t uploadedGeneration_ = 0;
        std::uint64_t maskUploadCount_ = 0;
        debug::Counters* counterOwner_ = nullptr;
        std::size_t drawsCounter_ = 0;
        std::size_t scaleCounter_ = 0;
        std::size_t maskUploadsCounter_ = 0;
    };

} // namespace cnahouse::rendering
