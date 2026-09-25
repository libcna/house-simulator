// SPDX-License-Identifier: MIT
#include "cnahouse/rendering/TransparentPass.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>

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
#include "Microsoft/Xna/Framework/Graphics/Texture2D.hpp"
#include "Microsoft/Xna/Framework/Graphics/VertexBuffer.hpp"
#include "Microsoft/Xna/Framework/Graphics/VertexPositionTexture.hpp"
#include "Microsoft/Xna/Framework/Graphics/Viewport.hpp"
#include "Microsoft/Xna/Framework/Matrix.hpp"
#include "Microsoft/Xna/Framework/Vector2.hpp"

#include "cnahouse/debug/Counters.hpp"
#include "cnahouse/lighting/LightingSystem.hpp"
#include "cnahouse/rendering/ParticleRenderer.hpp"
#include "cnahouse/rendering/StateTracker.hpp"
#include "cnahouse/util/Ids.hpp"
#include "cnahouse/world/CellRuntime.hpp"
#include "cnahouse/world/ChunkData.hpp"
#include "cnahouse/world/WorldData.hpp"

namespace cnahouse::rendering
{
    namespace Gfx = Microsoft::Xna::Framework::Graphics;
    using Microsoft::Xna::Framework::Vector3;

    float TransparentTintExposure(world::MaterialClass materialClass, float cameraEffectExposure) noexcept
    {
        return materialClass == world::MaterialClass::Glass ? 1.0F : cameraEffectExposure;
    }

    float GlowQuadRadialOpacity(float normalisedRadius) noexcept
    {
        if (!std::isfinite(normalisedRadius) || normalisedRadius >= 1.0F)
        {
            return 0.0F;
        }
        if (normalisedRadius <= 0.0F)
        {
            return 1.0F;
        }
        // A squared parabola has no hard inner disc and reaches zero with a flat derivative. The
        // same profile is suitable for §32.4's future flare sprites, so it is public pure maths.
        const float fade = 1.0F - normalisedRadius * normalisedRadius;
        return fade * fade;
    }

    FixtureGlowVisual FixtureGlowFor(const world::Light& light,
                                     float groupLevel,
                                     const Vector3& groupColour,
                                     float cameraExposure) noexcept
    {
        FixtureGlowVisual visual;
        visual.centre = light.position;
        visual.tint = Vector3(std::clamp(groupColour.X, 0.0F, 1.0F),
                              std::clamp(groupColour.Y, 0.0F, 1.0F),
                              std::clamp(groupColour.Z, 0.0F, 1.0F));
        if (!light.fixtureProp.IsValid() || light.emissiveMaterialSlot.empty() ||
            !std::isfinite(groupLevel) || !std::isfinite(light.intensityLm) || light.intensityLm <= 0.0F)
        {
            return visual;
        }

        const float level = std::clamp(groupLevel, 0.0F, 1.0F);
        if (level <= 0.0F)
        {
            return visual;
        }
        const float exposure = std::isfinite(cameraExposure) ? std::clamp(cameraExposure, 0.5F, 6.0F) : 1.0F;
        const float relativeLumens = std::clamp(light.intensityLm / 400.0F, 0.0F, 16.0F);
        const float fluxForRadius = std::sqrt(std::sqrt(relativeLumens));
        const float exposureForRadius = std::clamp(std::pow(exposure, 0.2F), 0.85F, 1.45F);
        // A spot's reflector and visor expose much less apparent emitter area than an unshielded
        // globe at the same delivered lumens. Its receiver contribution still uses all authored
        // lumens; only the camera-facing presentation halo is restrained here.
        const float opticRadius = light.type == world::LightType::Spot ? 0.72F : 1.0F;
        const float opticAlpha = light.type == world::LightType::Spot ? 0.72F : 1.0F;
        visual.radius = 0.42F * opticRadius * fluxForRadius * std::sqrt(level) * exposureForRadius;
        visual.alpha = std::clamp(
            0.34F * opticAlpha * level * std::sqrt(relativeLumens) * std::sqrt(exposure), 0.0F, 0.85F);
        visual.visible = visual.radius >= 0.01F && visual.alpha >= 0.001F &&
                         (visual.tint.X > 0.0F || visual.tint.Y > 0.0F || visual.tint.Z > 0.0F);
        return visual;
    }

    class TransparentPass::GlowResources
    {
    public:
        static constexpr int kTextureSize = 64;

        explicit GlowResources(Gfx::GraphicsDevice& device)
            : texture(device, kTextureSize, kTextureSize)
            , vertices(device,
                       Gfx::VertexPositionTexture::getVertexDeclarationStatic(),
                       4,
                       Gfx::BufferUsage::WriteOnly)
            , indices(device, Gfx::IndexElementSize::SixteenBits, 6, Gfx::BufferUsage::WriteOnly)
            , effect(device)
        {
            std::array<Microsoft::Xna::Framework::Color, kTextureSize * kTextureSize> texels;
            for (int y = 0; y < kTextureSize; ++y)
            {
                for (int x = 0; x < kTextureSize; ++x)
                {
                    const float nx = (static_cast<float>(x) + 0.5F) * (2.0F / kTextureSize) - 1.0F;
                    const float ny = (static_cast<float>(y) + 0.5F) * (2.0F / kTextureSize) - 1.0F;
                    const float alpha = GlowQuadRadialOpacity(std::sqrt(nx * nx + ny * ny));
                    const int alphaByte = static_cast<int>(std::lround(alpha * 255.0F));
                    texels[static_cast<std::size_t>(y * kTextureSize + x)] =
                        Microsoft::Xna::Framework::Color(255, 255, 255, alphaByte);
                }
            }
            texture.SetData(texels.data(), static_cast<int>(texels.size()));

            const std::array<Gfx::VertexPositionTexture, 4> quad{{
                {Vector3(-1.0F, -1.0F, 0.0F), Microsoft::Xna::Framework::Vector2(0.0F, 1.0F)},
                {Vector3(1.0F, -1.0F, 0.0F), Microsoft::Xna::Framework::Vector2(1.0F, 1.0F)},
                {Vector3(1.0F, 1.0F, 0.0F), Microsoft::Xna::Framework::Vector2(1.0F, 0.0F)},
                {Vector3(-1.0F, 1.0F, 0.0F), Microsoft::Xna::Framework::Vector2(0.0F, 0.0F)},
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
        // Last constructed, therefore first destroyed: the effect borrows the texture.
        Gfx::BasicEffect effect;
    };

    TransparentPass::TransparentPass(const world::ChunkLibrary& library,
                                     const world::CellRuntime& cells,
                                     const world::WorldData& world,
                                     const Camera& camera,
                                     visibility::RenderList& list,
                                     const lighting::LightingSystem* lighting,
                                     ParticleRenderer* particles)
        : library_(library)
        , cells_(cells)
        , world_(world)
        , camera_(camera)
        , list_(list)
        , lighting_(lighting)
        , particles_(particles)
    {
        glowLights_.reserve(world.Lights().size());
        for (const world::Light& light : world.Lights())
        {
            if (light.fixtureProp.IsValid() && !light.emissiveMaterialSlot.empty())
            {
                glowLights_.push_back(&light);
            }
        }
    }

    TransparentPass::~TransparentPass() = default;

    bool TransparentPass::IsActive() const
    {
        if (list_.Has(Pass::Transparent))
        {
            return true;
        }
        if (particles_ != nullptr && particles_->IsActive())
        {
            return true;
        }
        if (lighting_ != nullptr)
        {
            for (const world::Light* light : glowLights_)
            {
                if (lighting_->GroupOutputLevel(light->group) > 0.0F)
                {
                    return true;
                }
            }
        }
        return false;
    }

    void TransparentPass::Draw(PassContext& context)
    {
        chunksDrawn_ = 0u;
        trianglesDrawn_ = 0u;
        materialBinds_ = 0u;
        glowsDrawn_ = 0u;
        Gfx::GraphicsDevice& device = context.device;

        if (effect_ == nullptr)
        {
            effect_ = std::make_unique<Gfx::BasicEffect>(device);
            effect_->setLightingEnabledProperty(false);
            effect_->setTextureEnabledProperty(false);
            effect_->setVertexColorEnabledProperty(false);
            effect_->setWorldProperty(Microsoft::Xna::Framework::Matrix::getIdentityProperty());
        }

        const auto& viewport = device.getViewportProperty();
        const float aspect = viewport.getHeightProperty() > 0
                                 ? static_cast<float>(viewport.getWidthProperty()) /
                                       static_cast<float>(viewport.getHeightProperty())
                                 : 0.0F;
        effect_->setViewProperty(camera_.View());
        effect_->setProjectionProperty(camera_.Projection(aspect));

        // §23.6: premultiplied alpha; opaque and alpha-tested depth is tested but transparent
        // geometry cannot hide a later transparent draw by writing its own depth.
        context.states.SetRasterizer(Gfx::RasterizerState::CullNone);
        context.states.SetDepthStencil(Gfx::DepthStencilState::DepthRead);
        context.states.SetBlend(Gfx::BlendState::AlphaBlend);

        Gfx::EffectPassCollection& passes = effect_->getCurrentTechniqueProperty()->getPassesProperty();
        const int passCount = passes.getCountProperty();
        for (int p = 0; p < passCount; ++p)
        {
            bool bound = false;
            std::uint16_t boundMaterial = 0u;
            for (const visibility::RenderItem& item : list_.ItemsFor(Pass::Transparent))
            {
                if (item.geometry >= library_.chunks.size() || item.material >= library_.materials.size())
                {
                    continue;
                }
                const world::CellRuntime::ResidentChunk* resident = cells_.Find(item.geometry);
                if (resident == nullptr)
                {
                    continue;
                }
                const world::MaterialDef* material =
                    world_.FindMaterial(util::Id::Of(library_.materials[item.material]));
                if (material == nullptr || material->alphaMode != world::AlphaMode::Blend)
                {
                    // `RenderList` normally makes this impossible. Keep the draw boundary strict:
                    // a manually added item cannot turn an opaque material translucent by merely
                    // putting it in this pass.
                    continue;
                }

                const world::Chunk& chunk = library_.chunks[item.geometry];
                device.SetVertexBuffer(resident->vertices.get());
                device.setIndicesProperty(resident->indices.get());
                if (!bound || item.material != boundMaterial)
                {
                    const float adapted = lighting_ == nullptr ? 1.0F : lighting_->CameraEffectExposure();
                    const float exposure = TransparentTintExposure(material->materialClass, adapted);
                    effect_->setDiffuseColorProperty(Vector3(material->tint.X * exposure,
                                                             material->tint.Y * exposure,
                                                             material->tint.Z * exposure));
                    effect_->setAlphaProperty(material->alpha);
                    passes[p]->Apply();
                    bound = true;
                    boundMaterial = item.material;
                    ++materialBinds_;
                }
                device.DrawIndexedPrimitives(Gfx::PrimitiveType::TriangleList,
                                             0,
                                             0,
                                             static_cast<int>(chunk.vertexCount),
                                             0,
                                             static_cast<int>(resident->primitiveCount));
                if (p == 0)
                {
                    ++chunksDrawn_;
                    trianglesDrawn_ += resident->primitiveCount;
                }
            }
        }

        // Weather particles share the transparent pass and draw before additive fixture glows.
        // Their own renderer restores alpha blend and read-only depth, while the glow path below
        // explicitly selects its additive state if it has anything to draw.
        if (particles_ != nullptr)
        {
            particles_->Draw(context);
        }
        DrawFixtureGlows(context);

        if (counterOwner_ != &context.counters)
        {
            counterOwner_ = &context.counters;
            chunksCounter_ = context.counters.Resolve("transparent.chunks");
            trianglesCounter_ = context.counters.Resolve("transparent.triangles");
            bindsCounter_ = context.counters.Resolve("transparent.materialBinds");
            glowsCounter_ = context.counters.Resolve("transparent.fixtureGlows");
        }
        context.counters.Set(chunksCounter_, static_cast<std::int64_t>(chunksDrawn_));
        context.counters.Set(trianglesCounter_, static_cast<std::int64_t>(trianglesDrawn_));
        context.counters.Set(bindsCounter_, static_cast<std::int64_t>(materialBinds_));
        context.counters.Set(glowsCounter_, static_cast<std::int64_t>(glowsDrawn_));
    }

    void TransparentPass::DrawFixtureGlows(PassContext& context)
    {
        if (lighting_ == nullptr || glowLights_.empty())
        {
            return;
        }

        const float exposure = lighting_->CameraExposureScale();
        bool prepared = false;
        const Vector3 forward(camera_.target.X - camera_.eye.X,
                              camera_.target.Y - camera_.eye.Y,
                              camera_.target.Z - camera_.eye.Z);
        for (const world::Light* light : glowLights_)
        {
            // The camera-facing clearance below can otherwise lift a ceiling optic's halo
            // through the structural deck into the storey above. The fixture's own emissive
            // material is still depth-tested normally; only this presentation billboard is
            // suppressed when the eye is outside its owning storey's vertical envelope.
            const world::Cell* owner = world_.FindCell(light->cell);
            const world::Level* level = owner != nullptr ? world_.FindLevel(owner->level) : nullptr;
            if (level != nullptr && level->ceiling.has_value() &&
                (camera_.eye.Y > *level->ceiling + level->structureDepth ||
                 camera_.eye.Y < level->ffl - level->structureDepth))
            {
                continue;
            }
            const FixtureGlowVisual glow = FixtureGlowFor(*light,
                                                          lighting_->GroupOutputLevel(light->group),
                                                          lighting_->GroupColor(light->group),
                                                          exposure);
            if (!glow.visible)
            {
                continue;
            }
            if (glowResources_ == nullptr)
            {
                glowResources_ = std::make_unique<GlowResources>(context.device);
            }
            GlowResources& resources = *glowResources_;
            if (!prepared)
            {
                const auto& viewport = context.device.getViewportProperty();
                const float aspect = viewport.getHeightProperty() > 0
                                         ? static_cast<float>(viewport.getWidthProperty()) /
                                               static_cast<float>(viewport.getHeightProperty())
                                         : 1.0F;
                resources.effect.setViewProperty(camera_.View());
                resources.effect.setProjectionProperty(camera_.Projection(aspect));
                context.states.SetRasterizer(Gfx::RasterizerState::CullNone);
                context.states.SetDepthStencil(Gfx::DepthStencilState::DepthRead);
                context.states.SetBlend(Gfx::BlendState::Additive);
                context.device.SetVertexBuffer(&resources.vertices);
                context.device.setIndicesProperty(&resources.indices);
                prepared = true;
            }

            // Canonical point lights sit at the optical centre of their wall fixtures, commonly
            // behind the front glass and frame. A depth-tested quad at that exact point is
            // swallowed by the shade or wall it is meant to soften. Move only the presentation
            // quad toward the eye by a radius-scaled fixture clearance; the physical light and
            // baked receiver remain canonical. DepthRead still lets geometry in front of the
            // whole fixture occlude it.
            const Vector3 toEye(
                camera_.eye.X - glow.centre.X, camera_.eye.Y - glow.centre.Y, camera_.eye.Z - glow.centre.Z);
            const float eyeDistance = std::sqrt(toEye.X * toEye.X + toEye.Y * toEye.Y + toEye.Z * toEye.Z);
            Vector3 presentationCentre = glow.centre;
            if (eyeDistance > 0.001F)
            {
                const float fixtureClearance = std::clamp(glow.radius * 1.25F, 0.18F, 0.45F);
                const float scale = fixtureClearance / eyeDistance;
                presentationCentre = Vector3(glow.centre.X + toEye.X * scale,
                                             glow.centre.Y + toEye.Y * scale,
                                             glow.centre.Z + toEye.Z * scale);
            }
            const Microsoft::Xna::Framework::Matrix billboard =
                Microsoft::Xna::Framework::Matrix::CreateBillboard(
                    presentationCentre, camera_.eye, Vector3::Up, forward);
            resources.effect.setWorldProperty(Microsoft::Xna::Framework::Matrix::CreateScale(glow.radius) *
                                              billboard);
            resources.effect.setDiffuseColorProperty(glow.tint);
            resources.effect.setAlphaProperty(glow.alpha);
            Gfx::EffectPassCollection& passes =
                resources.effect.getCurrentTechniqueProperty()->getPassesProperty();
            for (int pass = 0; pass < passes.getCountProperty(); ++pass)
            {
                passes[pass]->Apply();
                context.device.DrawIndexedPrimitives(Gfx::PrimitiveType::TriangleList, 0, 0, 4, 0, 2);
            }
            ++glowsDrawn_;
        }
    }

} // namespace cnahouse::rendering
