// SPDX-License-Identifier: MIT
#include "cnahouse/rendering/SunDiscPass.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <numbers>

#include "Microsoft/Xna/Framework/Color.hpp"
#include "Microsoft/Xna/Framework/Graphics/BasicEffect.hpp"
#include "Microsoft/Xna/Framework/Graphics/BlendState.hpp"
#include "Microsoft/Xna/Framework/Graphics/BufferUsage.hpp"
#include "Microsoft/Xna/Framework/Graphics/DepthStencilState.hpp"
#include "Microsoft/Xna/Framework/Graphics/EffectPass.hpp"
#include "Microsoft/Xna/Framework/Graphics/EffectPassCollection.hpp"
#include "Microsoft/Xna/Framework/Graphics/EffectTechnique.hpp"
#include "Microsoft/Xna/Framework/Graphics/GraphicsDevice.hpp"
#include "Microsoft/Xna/Framework/Graphics/IndexBuffer.hpp"
#include "Microsoft/Xna/Framework/Graphics/IndexElementSize.hpp"
#include "Microsoft/Xna/Framework/Graphics/PrimitiveType.hpp"
#include "Microsoft/Xna/Framework/Graphics/RasterizerState.hpp"
#include "Microsoft/Xna/Framework/Graphics/SamplerState.hpp"
#include "Microsoft/Xna/Framework/Graphics/Texture2D.hpp"
#include "Microsoft/Xna/Framework/Graphics/VertexBuffer.hpp"
#include "Microsoft/Xna/Framework/Graphics/VertexPositionTexture.hpp"
#include "Microsoft/Xna/Framework/Graphics/Viewport.hpp"
#include "Microsoft/Xna/Framework/MathHelper.hpp"
#include "Microsoft/Xna/Framework/Matrix.hpp"
#include "Microsoft/Xna/Framework/Vector2.hpp"

#include "cnahouse/debug/Counters.hpp"
#include "cnahouse/environment/SunLight.hpp"
#include "cnahouse/rendering/Camera.hpp"
#include "cnahouse/rendering/RenderStates.hpp"
#include "cnahouse/rendering/StateTracker.hpp"

namespace cnahouse::rendering
{
    namespace Xna = Microsoft::Xna::Framework;
    namespace Gfx = Microsoft::Xna::Framework::Graphics;

    namespace
    {
        constexpr float kDistance = 890.0F;
        constexpr float kAngularDiameterDegrees = 0.53F;
        constexpr float kMaximumHorizonScale = 2.6F;
        constexpr double kFullSizeAltitudeDegrees = 10.0;
        constexpr float kSkyFarPlane = 1000.0F;
        constexpr int kTextureSize = 64;

        float Smoothstep(float value) noexcept
        {
            const float t = std::clamp(value, 0.0F, 1.0F);
            return t * t * (3.0F - 2.0F * t);
        }
    } // namespace

    float SunDiscHorizonScale(double altitudeDeg) noexcept
    {
        if (!std::isfinite(altitudeDeg) || altitudeDeg <= 0.0)
        {
            return kMaximumHorizonScale;
        }
        if (altitudeDeg >= kFullSizeAltitudeDegrees)
        {
            return 1.0F;
        }
        const float awayFromHorizon = Smoothstep(static_cast<float>(altitudeDeg / kFullSizeAltitudeDegrees));
        return kMaximumHorizonScale + (1.0F - kMaximumHorizonScale) * awayFromHorizon;
    }

    float SunDiscRadialOpacity(float normalisedRadius) noexcept
    {
        constexpr float kSoftLimbStarts = 0.72F;
        if (!std::isfinite(normalisedRadius) || normalisedRadius >= 1.0F)
        {
            return 0.0F;
        }
        if (normalisedRadius <= kSoftLimbStarts)
        {
            return 1.0F;
        }
        return Smoothstep((1.0F - normalisedRadius) / (1.0F - kSoftLimbStarts));
    }

    SunDiscFrame
    BuildSunDiscFrame(const Camera& camera, const environment::SunPosition& sun, double cloudCover) noexcept
    {
        SunDiscFrame frame;
        frame.horizonScale = SunDiscHorizonScale(sun.altitudeDeg);
        const float halfAngleRadians = kAngularDiameterDegrees * 0.5F * std::numbers::pi_v<float> / 180.0F;
        frame.radius = kDistance * std::tan(halfAngleRadians) * frame.horizonScale;

        const Xna::Vector3 toSun = environment::DirectionToSun(sun);
        frame.centre = Xna::Vector3(camera.eye.X + toSun.X * kDistance,
                                    camera.eye.Y + toSun.Y * kDistance,
                                    camera.eye.Z + toSun.Z * kDistance);

        const environment::SunShading shading = environment::SunShadingFor(sun, cloudCover);
        frame.tint = shading.color;
        frame.opacity = shading.directIntensity;
        frame.visible = std::isfinite(sun.altitudeDeg) &&
                        sun.altitudeDeg >= environment::kRefractedHorizonDeg && frame.opacity > 0.0F;
        return frame;
    }

    class SunDiscPass::Resources
    {
    public:
        explicit Resources(Gfx::GraphicsDevice& device)
            : texture(device, kTextureSize, kTextureSize)
            , vertices(device,
                       Gfx::VertexPositionTexture::getVertexDeclarationStatic(),
                       4,
                       Gfx::BufferUsage::WriteOnly)
            , indices(device, Gfx::IndexElementSize::SixteenBits, 6, Gfx::BufferUsage::WriteOnly)
            , effect(device)
        {
            std::array<Xna::Color, kTextureSize * kTextureSize> texels;
            for (int y = 0; y < kTextureSize; ++y)
            {
                for (int x = 0; x < kTextureSize; ++x)
                {
                    const float nx = (static_cast<float>(x) + 0.5F) * (2.0F / kTextureSize) - 1.0F;
                    const float ny = (static_cast<float>(y) + 0.5F) * (2.0F / kTextureSize) - 1.0F;
                    const float alpha = SunDiscRadialOpacity(std::sqrt(nx * nx + ny * ny));
                    const int alphaByte = static_cast<int>(std::lround(alpha * 255.0F));
                    texels[static_cast<std::size_t>(y * kTextureSize + x)] =
                        Xna::Color(255, 255, 255, alphaByte);
                }
            }
            texture.SetData(texels.data(), static_cast<int>(texels.size()));

            const std::array<Gfx::VertexPositionTexture, 4> quad{{
                {Xna::Vector3(-1.0F, -1.0F, 0.0F), Xna::Vector2(0.0F, 1.0F)},
                {Xna::Vector3(1.0F, -1.0F, 0.0F), Xna::Vector2(1.0F, 1.0F)},
                {Xna::Vector3(1.0F, 1.0F, 0.0F), Xna::Vector2(1.0F, 0.0F)},
                {Xna::Vector3(-1.0F, 1.0F, 0.0F), Xna::Vector2(0.0F, 0.0F)},
            }};
            constexpr std::array<std::uint16_t, 6> kIndices{{0, 1, 2, 0, 2, 3}};
            vertices.SetData(quad.data(), static_cast<int>(quad.size()));
            indices.SetData(kIndices.data(), static_cast<int>(kIndices.size()));

            effect.setLightingEnabledProperty(false);
            effect.setVertexColorEnabledProperty(false);
            effect.setTextureEnabledProperty(true);
            effect.setTextureProperty(&texture);
        }

        Gfx::Texture2D texture;
        Gfx::VertexBuffer vertices;
        Gfx::IndexBuffer indices;
        // Last constructed, therefore first destroyed: it borrows `texture` through
        // `setTextureProperty` and must not outlive it even during teardown.
        Gfx::BasicEffect effect;
    };

    SunDiscPass::SunDiscPass(const Camera& camera) noexcept
        : camera_(&camera)
    {
    }

    SunDiscPass::~SunDiscPass() = default;

    void SunDiscPass::SetSun(const environment::SunPosition& sun, double cloudCover) noexcept
    {
        frame_ = BuildSunDiscFrame(*camera_, sun, cloudCover);
    }

    void SunDiscPass::Draw(PassContext& context)
    {
        if (!frame_.visible)
        {
            return;
        }
        if (resources_ == nullptr)
        {
            resources_ = std::make_unique<Resources>(context.device);
        }

        const auto& viewport = context.device.getViewportProperty();
        const float aspect = viewport.getHeightProperty() > 0
                                 ? static_cast<float>(viewport.getWidthProperty()) /
                                       static_cast<float>(viewport.getHeightProperty())
                                 : 1.0F;

        Resources& resources = *resources_;
        const Xna::Vector3 forward(camera_->target.X - camera_->eye.X,
                                   camera_->target.Y - camera_->eye.Y,
                                   camera_->target.Z - camera_->eye.Z);
        const Xna::Matrix billboard =
            Xna::Matrix::CreateBillboard(frame_.centre, camera_->eye, Xna::Vector3::Up, forward);
        resources.effect.setWorldProperty(Xna::Matrix::CreateScale(frame_.radius) * billboard);
        resources.effect.setViewProperty(camera_->View());
        resources.effect.setProjectionProperty(
            Xna::Matrix::CreatePerspectiveFieldOfView(Xna::MathHelper::ToRadians(camera_->fieldOfViewDegrees),
                                                      aspect,
                                                      camera_->nearPlane,
                                                      kSkyFarPlane));
        resources.effect.setDiffuseColorProperty(frame_.tint);
        resources.effect.setAlphaProperty(frame_.opacity);

        context.states.SetBlend(Gfx::BlendState::Additive);
        context.states.SetDepthStencil(Gfx::DepthStencilState::None);
        context.states.SetRasterizer(StateFor(CullPolicy::TwoSided));
        context.states.SetSampler(0, Gfx::SamplerState::LinearClamp);
        context.device.SetVertexBuffer(&resources.vertices);
        context.device.setIndicesProperty(&resources.indices);

        Gfx::EffectPassCollection& passes =
            resources.effect.getCurrentTechniqueProperty()->getPassesProperty();
        for (int pass = 0; pass < passes.getCountProperty(); ++pass)
        {
            passes[pass].Apply();
            context.device.DrawIndexedPrimitives(Gfx::PrimitiveType::TriangleList, 0, 0, 4, 0, 2);
        }

        // A pass normally sees one registry for its lifetime. Remember WHICH registry as well as
        // its handles, though: integration hosts create short-lived registries in one process and
        // an index resolved in an earlier one has no meaning in the next.
        if (counterOwner_ != &context.counters)
        {
            counterOwner_ = &context.counters;
            drawsCounter_ = context.counters.Resolve("sun.disc.draws");
            scaleCounter_ = context.counters.Resolve("sun.disc.scaleMilli");
        }
        context.counters.Set(drawsCounter_, 1);
        context.counters.Set(scaleCounter_,
                             static_cast<std::int64_t>(std::lround(frame_.horizonScale * 1000.0F)));
    }

} // namespace cnahouse::rendering
