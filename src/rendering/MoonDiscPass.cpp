// SPDX-License-Identifier: MIT
#include "cnahouse/rendering/MoonDiscPass.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <numbers>
#include <span>
#include <utility>

#include "Microsoft/Xna/Framework/Color.hpp"
#include "Microsoft/Xna/Framework/Graphics/BlendState.hpp"
#include "Microsoft/Xna/Framework/Graphics/BufferUsage.hpp"
#include "Microsoft/Xna/Framework/Graphics/DepthStencilState.hpp"
#include "Microsoft/Xna/Framework/Graphics/DualTextureEffect.hpp"
#include "Microsoft/Xna/Framework/Graphics/EffectPass.hpp"
#include "Microsoft/Xna/Framework/Graphics/EffectPassCollection.hpp"
#include "Microsoft/Xna/Framework/Graphics/EffectTechnique.hpp"
#include "Microsoft/Xna/Framework/Graphics/GraphicsDevice.hpp"
#include "Microsoft/Xna/Framework/Graphics/IndexBuffer.hpp"
#include "Microsoft/Xna/Framework/Graphics/IndexElementSize.hpp"
#include "Microsoft/Xna/Framework/Graphics/PrimitiveType.hpp"
#include "Microsoft/Xna/Framework/Graphics/Texture2D.hpp"
#include "Microsoft/Xna/Framework/Graphics/TextureCollection.hpp"
#include "Microsoft/Xna/Framework/Graphics/VertexBuffer.hpp"
#include "Microsoft/Xna/Framework/Graphics/VertexDeclaration.hpp"
#include "Microsoft/Xna/Framework/Graphics/VertexElement.hpp"
#include "Microsoft/Xna/Framework/Graphics/VertexElementFormat.hpp"
#include "Microsoft/Xna/Framework/Graphics/VertexElementUsage.hpp"
#include "Microsoft/Xna/Framework/Graphics/Viewport.hpp"
#include "Microsoft/Xna/Framework/MathHelper.hpp"
#include "Microsoft/Xna/Framework/Matrix.hpp"
#include "Microsoft/Xna/Framework/Vector2.hpp"

#include "cnahouse/debug/Counters.hpp"
#include "cnahouse/environment/MoonLight.hpp"
#include "cnahouse/environment/SunLight.hpp"
#include "cnahouse/rendering/Camera.hpp"
#include "cnahouse/rendering/RenderStates.hpp"
#include "cnahouse/rendering/StateTracker.hpp"
#include "cnahouse/rendering/SunDiscPass.hpp"

namespace cnahouse::rendering
{
    namespace Xna = Microsoft::Xna::Framework;
    namespace Gfx = Microsoft::Xna::Framework::Graphics;

    namespace
    {
        constexpr float kDistance = 890.0F;
        constexpr float kAngularDiameterDegrees = 0.52F;
        constexpr float kSkyFarPlane = 1000.0F;
        constexpr double kVectorEpsilonSquared = 1.0e-12;

        struct VertexDualTexture
        {
            Xna::Vector3 position;
            Xna::Vector2 albedoUv;
            Xna::Vector2 maskUv;
        };

        [[nodiscard]] double Dot(const Xna::Vector3& a, const Xna::Vector3& b) noexcept
        {
            return static_cast<double>(a.X) * static_cast<double>(b.X) +
                   static_cast<double>(a.Y) * static_cast<double>(b.Y) +
                   static_cast<double>(a.Z) * static_cast<double>(b.Z);
        }

        [[nodiscard]] Xna::Vector3 Cross(const Xna::Vector3& a, const Xna::Vector3& b) noexcept
        {
            return Xna::Vector3(a.Y * b.Z - a.Z * b.Y, a.Z * b.X - a.X * b.Z, a.X * b.Y - a.Y * b.X);
        }

        [[nodiscard]] Xna::Vector3 Scaled(const Xna::Vector3& value, double scale) noexcept
        {
            return Xna::Vector3(static_cast<float>(static_cast<double>(value.X) * scale),
                                static_cast<float>(static_cast<double>(value.Y) * scale),
                                static_cast<float>(static_cast<double>(value.Z) * scale));
        }

        [[nodiscard]] Xna::Vector3 Subtract(const Xna::Vector3& a, const Xna::Vector3& b) noexcept
        {
            return Xna::Vector3(a.X - b.X, a.Y - b.Y, a.Z - b.Z);
        }

        [[nodiscard]] Xna::Vector3 Normalised(const Xna::Vector3& value,
                                              const Xna::Vector3& fallback) noexcept
        {
            const double lengthSquared = Dot(value, value);
            if (!std::isfinite(lengthSquared) || lengthSquared <= kVectorEpsilonSquared)
            {
                return fallback;
            }
            return Scaled(value, 1.0 / std::sqrt(lengthSquared));
        }

        struct DiscBasis
        {
            Xna::Vector3 forward;
            Xna::Vector3 right;
            Xna::Vector3 up;
        };

        [[nodiscard]] DiscBasis BasisFor(const Xna::Vector3& direction) noexcept
        {
            const Xna::Vector3 forward = Normalised(direction, Xna::Vector3::Forward);
            Xna::Vector3 right = Cross(Xna::Vector3::Up, forward);
            if (Dot(right, right) <= kVectorEpsilonSquared)
            {
                right = Xna::Vector3(1.0F, 0.0F, 0.0F);
            }
            else
            {
                right = Normalised(right, Xna::Vector3(1.0F, 0.0F, 0.0F));
            }
            return DiscBasis{forward, right, Cross(forward, right)};
        }

        [[nodiscard]] Xna::Matrix Billboard(const MoonDiscFrame& frame,
                                            const Xna::Vector3& direction) noexcept
        {
            const DiscBasis basis = BasisFor(direction);
            Xna::Matrix result = Xna::Matrix::getIdentityProperty();
            result.M11 = basis.right.X;
            result.M12 = basis.right.Y;
            result.M13 = basis.right.Z;
            result.M21 = basis.up.X;
            result.M22 = basis.up.Y;
            result.M23 = basis.up.Z;
            result.M31 = basis.forward.X;
            result.M32 = basis.forward.Y;
            result.M33 = basis.forward.Z;
            result.M41 = frame.centre.X;
            result.M42 = frame.centre.Y;
            result.M43 = frame.centre.Z;
            return result;
        }
    } // namespace

    double MoonBrightLimbAngle(const environment::MoonPosition& moon,
                               const environment::SunPosition& sun) noexcept
    {
        if (!std::isfinite(moon.altitudeDeg) || !std::isfinite(moon.azimuthDeg) ||
            !std::isfinite(sun.altitudeDeg) || !std::isfinite(sun.azimuthDeg))
        {
            return 0.0;
        }
        const Xna::Vector3 moonDirection = environment::DirectionToMoon(moon);
        const Xna::Vector3 sunDirection = environment::DirectionToSun(sun);
        const DiscBasis basis = BasisFor(moonDirection);
        const Xna::Vector3 projected =
            Subtract(sunDirection, Scaled(basis.forward, Dot(sunDirection, basis.forward)));
        if (Dot(projected, projected) <= kVectorEpsilonSquared)
        {
            return 0.0;
        }
        return std::atan2(Dot(projected, basis.up), Dot(projected, basis.right));
    }

    MoonDiscFrame BuildMoonDiscFrame(const Camera& camera,
                                     const environment::MoonPosition& moon,
                                     const environment::SunPosition& sun) noexcept
    {
        MoonDiscFrame frame;
        frame.horizonScale = SunDiscHorizonScale(moon.altitudeDeg);
        const float halfAngleRadians = kAngularDiameterDegrees * 0.5F * std::numbers::pi_v<float> / 180.0F;
        frame.radius = kDistance * std::tan(halfAngleRadians) * frame.horizonScale;
        const Xna::Vector3 toMoon = environment::DirectionToMoon(moon);
        frame.centre = Xna::Vector3(camera.eye.X + toMoon.X * kDistance,
                                    camera.eye.Y + toMoon.Y * kDistance,
                                    camera.eye.Z + toMoon.Z * kDistance);
        frame.tint = environment::SunLightingAt(moon.altitudeDeg).color;
        frame.brightLimbAngleRadians = MoonBrightLimbAngle(moon, sun);
        frame.visible = std::isfinite(moon.altitudeDeg) && std::isfinite(moon.azimuthDeg) &&
                        moon.altitudeDeg >= environment::kMoonRefractedHorizonDeg;
        return frame;
    }

    class MoonDiscPass::Resources
    {
    public:
        Resources(Gfx::GraphicsDevice& device, Gfx::Texture2D& albedo, std::span<const Xna::Color> maskPixels)
            : mask(device, kMoonMaskSize, kMoonMaskSize)
            , vertices(device,
                       Gfx::VertexDeclaration({
                           Gfx::VertexElement(
                               0, Gfx::VertexElementFormat::Vector3, Gfx::VertexElementUsage::Position, 0),
                           Gfx::VertexElement(12,
                                              Gfx::VertexElementFormat::Vector2,
                                              Gfx::VertexElementUsage::TextureCoordinate,
                                              0),
                           Gfx::VertexElement(20,
                                              Gfx::VertexElementFormat::Vector2,
                                              Gfx::VertexElementUsage::TextureCoordinate,
                                              1),
                       }),
                       4,
                       Gfx::BufferUsage::WriteOnly)
            , indices(device, Gfx::IndexElementSize::SixteenBits, 6, Gfx::BufferUsage::WriteOnly)
            , effect(device)
        {
            mask.SetData(maskPixels.data(), static_cast<int>(maskPixels.size()));
            const std::array<VertexDualTexture, 4> quad{{
                {Xna::Vector3(-1.0F, -1.0F, 0.0F), Xna::Vector2(0.0F, 1.0F), Xna::Vector2(0.0F, 1.0F)},
                {Xna::Vector3(1.0F, -1.0F, 0.0F), Xna::Vector2(1.0F, 1.0F), Xna::Vector2(1.0F, 1.0F)},
                {Xna::Vector3(1.0F, 1.0F, 0.0F), Xna::Vector2(1.0F, 0.0F), Xna::Vector2(1.0F, 0.0F)},
                {Xna::Vector3(-1.0F, 1.0F, 0.0F), Xna::Vector2(0.0F, 0.0F), Xna::Vector2(0.0F, 0.0F)},
            }};
            constexpr std::array<std::uint16_t, 6> kIndices{{0, 1, 2, 0, 2, 3}};
            vertices.SetData(quad.data(), static_cast<int>(quad.size()));
            indices.SetData(kIndices.data(), static_cast<int>(kIndices.size()));
            effect.setTextureProperty(&albedo);
            effect.setTexture2Property(&mask);
            effect.setVertexColorEnabledProperty(false);
            effect.setFogEnabledProperty(false);
        }

        Gfx::Texture2D mask;
        Gfx::VertexBuffer vertices;
        Gfx::IndexBuffer indices;
        // Last among the XNA resources, therefore first destroyed: it borrows both textures.
        Gfx::DualTextureEffect effect;
    };

    MoonDiscPass::MoonDiscPass(const Camera& camera) noexcept
        : camera_(&camera)
    {
    }

    MoonDiscPass::MoonDiscPass(const Camera& camera, std::unique_ptr<Gfx::Texture2D> albedo) noexcept
        : camera_(&camera)
        , albedo_(std::move(albedo))
    {
    }

    MoonDiscPass::MoonDiscPass(const Camera& camera, Gfx::Texture2D* borrowedAlbedo) noexcept
        : camera_(&camera)
        , borrowedAlbedo_(borrowedAlbedo)
    {
    }

    MoonDiscPass::~MoonDiscPass() = default;

    void MoonDiscPass::SetAlbedo(std::unique_ptr<Gfx::Texture2D> albedo) noexcept
    {
        resources_.reset();
        albedo_ = std::move(albedo);
        borrowedAlbedo_ = nullptr;
        uploadedGeneration_ = 0;
    }

    void MoonDiscPass::SetMoon(const environment::MoonPosition& moon,
                               const environment::MoonPhase& phase,
                               const environment::SunPosition& sun) noexcept
    {
        frame_ = BuildMoonDiscFrame(*camera_, moon, sun);
        frame_.visible = frame_.visible && std::isfinite(phase.phase);
        static_cast<void>(mask_.Update(phase.phase, frame_.brightLimbAngleRadians));
    }

    bool MoonDiscPass::IsActive() const
    {
        return frame_.visible && mask_.HasPixels() &&
               (resources_ != nullptr || albedo_ != nullptr || borrowedAlbedo_ != nullptr);
    }

    void MoonDiscPass::Draw(PassContext& context)
    {
        if (!IsActive())
        {
            return;
        }
        if (resources_ == nullptr)
        {
            Gfx::Texture2D* albedo = borrowedAlbedo_ != nullptr ? borrowedAlbedo_ : albedo_.get();
            resources_ = std::make_unique<Resources>(context.device, *albedo, mask_.Pixels());
            uploadedGeneration_ = mask_.GenerationCount();
            ++maskUploadCount_;
        }
        else if (uploadedGeneration_ != mask_.GenerationCount())
        {
            // XNA refuses SetData on a texture the device still has bound (`AM4-203`), and the
            // previous frame's DualTextureEffect pass left the mask on a sampler slot.
            Gfx::TextureCollection& textures = context.device.getTexturesProperty();
            for (int slot = 0; slot < 2; ++slot)
            {
                if (textures[slot] == &resources_->mask)
                {
                    textures(slot, nullptr);
                }
            }
            resources_->mask.SetData(mask_.Pixels().data(), static_cast<int>(mask_.Pixels().size()));
            uploadedGeneration_ = mask_.GenerationCount();
            ++maskUploadCount_;
        }

        const auto& viewport = context.device.getViewportProperty();
        const float aspect = viewport.getHeightProperty() > 0
                                 ? static_cast<float>(viewport.getWidthProperty()) /
                                       static_cast<float>(viewport.getHeightProperty())
                                 : 1.0F;
        Resources& resources = *resources_;
        const Xna::Vector3 direction = Xna::Vector3((frame_.centre.X - camera_->eye.X) / kDistance,
                                                    (frame_.centre.Y - camera_->eye.Y) / kDistance,
                                                    (frame_.centre.Z - camera_->eye.Z) / kDistance);
        const Xna::Matrix billboard = Billboard(frame_, direction);
        resources.effect.setWorldProperty(Xna::Matrix::CreateScale(frame_.radius) * billboard);
        resources.effect.setViewProperty(camera_->View());
        resources.effect.setProjectionProperty(
            Xna::Matrix::CreatePerspectiveFieldOfView(Xna::MathHelper::ToRadians(camera_->fieldOfViewDegrees),
                                                      aspect,
                                                      camera_->nearPlane,
                                                      kSkyFarPlane));
        // XNA's DualTextureEffect computes `2 * texture0 * texture1 * diffuse`. Half tint cancels
        // that built-in lightmap factor, leaving exactly albedo * phase mask * atmospheric tint.
        resources.effect.setDiffuseColorProperty(
            Xna::Vector3(frame_.tint.X * 0.5F, frame_.tint.Y * 0.5F, frame_.tint.Z * 0.5F));
        resources.effect.setAlphaProperty(1.0F);

        context.states.SetBlend(Gfx::BlendState::Additive);
        context.states.SetDepthStencil(Gfx::DepthStencilState::None);
        context.states.SetRasterizer(StateFor(CullPolicy::TwoSided));
        context.device.SetVertexBuffer(&resources.vertices);
        context.device.setIndicesProperty(&resources.indices);

        Gfx::EffectPassCollection& passes =
            resources.effect.getCurrentTechniqueProperty()->getPassesProperty();
        for (int pass = 0; pass < passes.getCountProperty(); ++pass)
        {
            passes[pass]->Apply();
            context.device.DrawIndexedPrimitives(Gfx::PrimitiveType::TriangleList, 0, 0, 4, 0, 2);
        }

        if (counterOwner_ != &context.counters)
        {
            counterOwner_ = &context.counters;
            drawsCounter_ = context.counters.Resolve("moon.disc.draws");
            scaleCounter_ = context.counters.Resolve("moon.disc.scaleMilli");
            maskUploadsCounter_ = context.counters.Resolve("moon.disc.mask_uploads");
        }
        context.counters.Set(drawsCounter_, 1);
        context.counters.Set(scaleCounter_,
                             static_cast<std::int64_t>(std::lround(frame_.horizonScale * 1000.0F)));
        context.counters.Set(maskUploadsCounter_, static_cast<std::int64_t>(maskUploadCount_));
    }

} // namespace cnahouse::rendering
