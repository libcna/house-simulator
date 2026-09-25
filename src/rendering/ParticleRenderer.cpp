// SPDX-License-Identifier: MIT
#include "cnahouse/rendering/ParticleRenderer.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>

#include "Microsoft/Xna/Framework/Graphics/BasicEffect.hpp"
#include "Microsoft/Xna/Framework/Graphics/BlendState.hpp"
#include "Microsoft/Xna/Framework/Graphics/BufferUsage.hpp"
#include "Microsoft/Xna/Framework/Graphics/DepthStencilState.hpp"
#include "Microsoft/Xna/Framework/Graphics/DynamicVertexBuffer.hpp"
#include "Microsoft/Xna/Framework/Graphics/EffectPass.hpp"
#include "Microsoft/Xna/Framework/Graphics/EffectPassCollection.hpp"
#include "Microsoft/Xna/Framework/Graphics/EffectTechnique.hpp"
#include "Microsoft/Xna/Framework/Graphics/GraphicsDevice.hpp"
#include "Microsoft/Xna/Framework/Graphics/IndexBuffer.hpp"
#include "Microsoft/Xna/Framework/Graphics/IndexElementSize.hpp"
#include "Microsoft/Xna/Framework/Graphics/PrimitiveType.hpp"
#include "Microsoft/Xna/Framework/Graphics/RasterizerState.hpp"
#include "Microsoft/Xna/Framework/Graphics/SetDataOptions.hpp"
#include "Microsoft/Xna/Framework/Graphics/Texture2D.hpp"
#include "Microsoft/Xna/Framework/Graphics/Viewport.hpp"
#include "Microsoft/Xna/Framework/Matrix.hpp"

#include "cnahouse/debug/Counters.hpp"
#include "cnahouse/rendering/StateTracker.hpp"

namespace cnahouse::rendering
{
    namespace Xna = Microsoft::Xna::Framework;
    namespace Gfx = Microsoft::Xna::Framework::Graphics;

    namespace
    {
        constexpr double kDirectionEpsilonSquared = 1.0e-12;

        [[nodiscard]] double LengthSquared(const Xna::Vector3& value) noexcept
        {
            return static_cast<double>(value.X) * static_cast<double>(value.X) +
                   static_cast<double>(value.Y) * static_cast<double>(value.Y) +
                   static_cast<double>(value.Z) * static_cast<double>(value.Z);
        }

        [[nodiscard]] Xna::Vector3 Scaled(const Xna::Vector3& value, float scale) noexcept
        {
            return Xna::Vector3(value.X * scale, value.Y * scale, value.Z * scale);
        }

        [[nodiscard]] Xna::Vector3 Add(const Xna::Vector3& a, const Xna::Vector3& b) noexcept
        {
            return Xna::Vector3(a.X + b.X, a.Y + b.Y, a.Z + b.Z);
        }

        [[nodiscard]] Xna::Vector3 Subtract(const Xna::Vector3& a, const Xna::Vector3& b) noexcept
        {
            return Xna::Vector3(a.X - b.X, a.Y - b.Y, a.Z - b.Z);
        }

        [[nodiscard]] Xna::Vector3 Cross(const Xna::Vector3& a, const Xna::Vector3& b) noexcept
        {
            return Xna::Vector3(a.Y * b.Z - a.Z * b.Y, a.Z * b.X - a.X * b.Z, a.X * b.Y - a.Y * b.X);
        }

        [[nodiscard]] float Dot(const Xna::Vector3& a, const Xna::Vector3& b) noexcept
        {
            return a.X * b.X + a.Y * b.Y + a.Z * b.Z;
        }

        [[nodiscard]] Xna::Vector3 Normalised(const Xna::Vector3& value,
                                              const Xna::Vector3& fallback) noexcept
        {
            const double lengthSquared = LengthSquared(value);
            if (!std::isfinite(lengthSquared) || lengthSquared <= kDirectionEpsilonSquared)
            {
                return fallback;
            }
            return Scaled(value, static_cast<float>(1.0 / std::sqrt(lengthSquared)));
        }

        [[nodiscard]] bool Finite(const ParticleQuad& particle) noexcept
        {
            return std::isfinite(particle.centre.X) && std::isfinite(particle.centre.Y) &&
                   std::isfinite(particle.centre.Z) && std::isfinite(particle.halfSize.X) &&
                   std::isfinite(particle.halfSize.Y) && particle.halfSize.X > 0.0F &&
                   particle.halfSize.Y > 0.0F && std::isfinite(particle.elongationAxis.X) &&
                   std::isfinite(particle.elongationAxis.Y) && std::isfinite(particle.elongationAxis.Z);
        }
    } // namespace

    std::size_t ParticleLimitFor(ParticleQuality quality) noexcept
    {
        switch (quality)
        {
            case ParticleQuality::Low:
                return 500u;
            case ParticleQuality::Medium:
                return 1000u;
            case ParticleQuality::High:
                return kMaximumParticleQuads;
        }
        return 500u;
    }

    std::array<Gfx::VertexPositionColorTexture, 4> ParticleBillboardVertices(const ParticleQuad& particle,
                                                                             const Camera& camera) noexcept
    {
        const Xna::Vector3 forward =
            Normalised(Subtract(camera.target, camera.eye), Xna::Vector3(0.0F, 0.0F, -1.0F));
        const Xna::Vector3 fallbackRight =
            Normalised(Cross(forward, Xna::Vector3::Up), Xna::Vector3(1.0F, 0.0F, 0.0F));
        const Xna::Vector3 fallbackUp = Normalised(Cross(fallbackRight, forward), Xna::Vector3::Up);
        const Xna::Vector3 projectedAxis =
            Subtract(particle.elongationAxis, Scaled(forward, Dot(particle.elongationAxis, forward)));
        const Xna::Vector3 requestedUp = Normalised(projectedAxis, fallbackUp);
        const Xna::Vector3 right = Normalised(Cross(forward, requestedUp), fallbackRight);
        const Xna::Vector3 up = Normalised(Cross(right, forward), fallbackUp);
        const Xna::Vector3 horizontal = Scaled(right, particle.halfSize.X);
        const Xna::Vector3 vertical = Scaled(up, particle.halfSize.Y);

        return {{{Subtract(Subtract(particle.centre, horizontal), vertical),
                  particle.colour,
                  Xna::Vector2(0.0F, 1.0F)},
                 {Add(Subtract(particle.centre, vertical), horizontal),
                  particle.colour,
                  Xna::Vector2(1.0F, 1.0F)},
                 {Add(Add(particle.centre, horizontal), vertical), particle.colour, Xna::Vector2(1.0F, 0.0F)},
                 {Add(Subtract(particle.centre, horizontal), vertical),
                  particle.colour,
                  Xna::Vector2(0.0F, 0.0F)}}};
    }

    class ParticleRenderer::Resources
    {
    public:
        explicit Resources(Gfx::GraphicsDevice& device)
            : vertices(device,
                       Gfx::VertexPositionColorTexture::getVertexDeclarationStatic(),
                       static_cast<int>(kMaximumParticleQuads * 4u),
                       Gfx::BufferUsage::WriteOnly)
            , indices(device,
                      Gfx::IndexElementSize::SixteenBits,
                      static_cast<int>(kMaximumParticleQuads * 6u),
                      Gfx::BufferUsage::WriteOnly)
            , effect(device)
        {
            std::array<std::uint16_t, kMaximumParticleQuads * 6u> data{};
            for (std::size_t quad = 0u; quad < kMaximumParticleQuads; ++quad)
            {
                const auto base = static_cast<std::uint16_t>(quad * 4u);
                const std::size_t index = quad * 6u;
                data[index] = base;
                data[index + 1u] = static_cast<std::uint16_t>(base + 1u);
                data[index + 2u] = static_cast<std::uint16_t>(base + 2u);
                data[index + 3u] = base;
                data[index + 4u] = static_cast<std::uint16_t>(base + 2u);
                data[index + 5u] = static_cast<std::uint16_t>(base + 3u);
            }
            indices.SetData(data.data(), static_cast<int>(data.size()));
            effect.setWorldProperty(Xna::Matrix::getIdentityProperty());
            effect.setLightingEnabledProperty(false);
            effect.setTextureEnabledProperty(true);
            effect.setVertexColorEnabledProperty(true);
            effect.setFogEnabledProperty(false);
            effect.setDiffuseColorProperty(Xna::Vector3(1.0F, 1.0F, 1.0F));
            const float alpha = 1.0F;
            effect.setAlphaProperty(alpha);
        }

        Gfx::DynamicVertexBuffer vertices;
        Gfx::IndexBuffer indices;
        Gfx::BasicEffect effect;
    };

    ParticleRenderer::ParticleRenderer(const Camera& camera) noexcept
        : camera_(&camera)
    {
    }

    ParticleRenderer::~ParticleRenderer() = default;

    void ParticleRenderer::BeginFrame(ParticleQuality quality) noexcept
    {
        particleCount_ = 0u;
        particleLimit_ = ParticleLimitFor(quality);
        drawableQuadCount_ = 0u;
        firstQuadByMaterial_.fill(0u);
        quadCountByMaterial_.fill(0u);
        lastDrawCount_ = 0u;
        rejectedCount_ = 0u;
    }

    bool ParticleRenderer::SetMaterial(std::size_t material, Gfx::Texture2D* texture) noexcept
    {
        if (material >= materials_.size())
        {
            return false;
        }
        materials_[material] = texture;
        return true;
    }

    bool ParticleRenderer::Submit(const ParticleQuad& particle) noexcept
    {
        if (particle.material >= materials_.size() || !Finite(particle) || particleCount_ >= particleLimit_)
        {
            ++rejectedCount_;
            return false;
        }
        particles_[particleCount_++] = particle;
        return true;
    }

    void ParticleRenderer::BuildVertices() noexcept
    {
        drawableQuadCount_ = 0u;
        firstQuadByMaterial_.fill(0u);
        quadCountByMaterial_.fill(0u);
        for (std::size_t material = 0u; material < materials_.size(); ++material)
        {
            firstQuadByMaterial_[material] = drawableQuadCount_;
            if (materials_[material] == nullptr)
            {
                continue;
            }
            for (std::size_t particleIndex = 0u; particleIndex < particleCount_; ++particleIndex)
            {
                const ParticleQuad& particle = particles_[particleIndex];
                if (particle.material != material)
                {
                    continue;
                }
                const auto quad = ParticleBillboardVertices(particle, *camera_);
                std::copy(quad.begin(),
                          quad.end(),
                          vertices_.begin() + static_cast<std::ptrdiff_t>(drawableQuadCount_ * 4u));
                ++drawableQuadCount_;
                ++quadCountByMaterial_[material];
            }
        }
    }

    void ParticleRenderer::Draw(PassContext& context)
    {
        if (counterOwner_ != &context.counters)
        {
            counterOwner_ = &context.counters;
            drawsCounter_ = context.counters.Resolve("particles.draws");
            quadsCounter_ = context.counters.Resolve("particles.quads");
            uploadsCounter_ = context.counters.Resolve("particles.uploads");
            rejectedCounter_ = context.counters.Resolve("particles.rejected");
        }

        lastDrawCount_ = 0u;
        BuildVertices();
        if (drawableQuadCount_ == 0u)
        {
            context.counters.Set(drawsCounter_, 0);
            context.counters.Set(quadsCounter_, 0);
            context.counters.Set(uploadsCounter_, static_cast<std::int64_t>(uploadCount_));
            context.counters.Set(rejectedCounter_, static_cast<std::int64_t>(rejectedCount_));
            return;
        }

        if (resources_ == nullptr)
        {
            resources_ = std::make_unique<Resources>(context.device);
        }
        Resources& resources = *resources_;
        context.device.SetVertexBuffer(nullptr);
        resources.vertices.SetData(
            vertices_.data(), 0, static_cast<int>(drawableQuadCount_ * 4u), Gfx::SetDataOptions::Discard);
        ++uploadCount_;

        const auto& viewport = context.device.getViewportProperty();
        const float aspect = viewport.getHeightProperty() > 0
                                 ? static_cast<float>(viewport.getWidthProperty()) /
                                       static_cast<float>(viewport.getHeightProperty())
                                 : 1.0F;
        resources.effect.setViewProperty(camera_->View());
        resources.effect.setProjectionProperty(camera_->Projection(aspect));

        context.states.SetRasterizer(Gfx::RasterizerState::CullNone);
        context.states.SetDepthStencil(Gfx::DepthStencilState::DepthRead);
        context.states.SetBlend(Gfx::BlendState::AlphaBlend);
        context.device.SetVertexBuffer(&resources.vertices);
        context.device.setIndicesProperty(&resources.indices);

        for (std::size_t material = 0u; material < materials_.size(); ++material)
        {
            const std::size_t quadCount = quadCountByMaterial_[material];
            if (quadCount == 0u)
            {
                continue;
            }
            resources.effect.setTextureProperty(materials_[material]);
            Gfx::EffectPassCollection& passes =
                resources.effect.getCurrentTechniqueProperty()->getPassesProperty();
            for (int pass = 0; pass < passes.getCountProperty(); ++pass)
            {
                passes[pass]->Apply();
                const std::size_t firstQuad = firstQuadByMaterial_[material];
                context.device.DrawIndexedPrimitives(Gfx::PrimitiveType::TriangleList,
                                                     0,
                                                     static_cast<int>(firstQuad * 4u),
                                                     static_cast<int>(quadCount * 4u),
                                                     static_cast<int>(firstQuad * 6u),
                                                     static_cast<int>(quadCount * 2u));
                ++lastDrawCount_;
            }
        }

        context.counters.Set(drawsCounter_, static_cast<std::int64_t>(lastDrawCount_));
        context.counters.Set(quadsCounter_, static_cast<std::int64_t>(drawableQuadCount_));
        context.counters.Set(uploadsCounter_, static_cast<std::int64_t>(uploadCount_));
        context.counters.Set(rejectedCounter_, static_cast<std::int64_t>(rejectedCount_));
    }
} // namespace cnahouse::rendering
