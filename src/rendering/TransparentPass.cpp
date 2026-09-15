// SPDX-License-Identifier: MIT
#include "cnahouse/rendering/TransparentPass.hpp"

#include "Microsoft/Xna/Framework/Graphics/BasicEffect.hpp"
#include "Microsoft/Xna/Framework/Graphics/BlendState.hpp"
#include "Microsoft/Xna/Framework/Graphics/DepthStencilState.hpp"
#include "Microsoft/Xna/Framework/Graphics/EffectPass.hpp"
#include "Microsoft/Xna/Framework/Graphics/EffectPassCollection.hpp"
#include "Microsoft/Xna/Framework/Graphics/EffectTechnique.hpp"
#include "Microsoft/Xna/Framework/Graphics/GraphicsDevice.hpp"
#include "Microsoft/Xna/Framework/Graphics/IndexBuffer.hpp"
#include "Microsoft/Xna/Framework/Graphics/PrimitiveType.hpp"
#include "Microsoft/Xna/Framework/Graphics/RasterizerState.hpp"
#include "Microsoft/Xna/Framework/Graphics/VertexBuffer.hpp"
#include "Microsoft/Xna/Framework/Graphics/Viewport.hpp"

#include "cnahouse/debug/Counters.hpp"
#include "cnahouse/lighting/LightingSystem.hpp"
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

    TransparentPass::TransparentPass(const world::ChunkLibrary& library,
                                     const world::CellRuntime& cells,
                                     const world::WorldData& world,
                                     const Camera& camera,
                                     visibility::RenderList& list,
                                     const lighting::LightingSystem* lighting)
        : library_(library)
        , cells_(cells)
        , world_(world)
        , camera_(camera)
        , list_(list)
        , lighting_(lighting)
    {
    }

    TransparentPass::~TransparentPass() = default;

    bool TransparentPass::IsActive() const
    {
        return list_.Has(Pass::Transparent);
    }

    void TransparentPass::Draw(PassContext& context)
    {
        chunksDrawn_ = 0u;
        trianglesDrawn_ = 0u;
        materialBinds_ = 0u;
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

        static const debug::Counters::Handle kChunks = context.counters.Resolve("transparent.chunks");
        static const debug::Counters::Handle kTriangles = context.counters.Resolve("transparent.triangles");
        static const debug::Counters::Handle kBinds = context.counters.Resolve("transparent.materialBinds");
        context.counters.Set(kChunks, static_cast<std::int64_t>(chunksDrawn_));
        context.counters.Set(kTriangles, static_cast<std::int64_t>(trianglesDrawn_));
        context.counters.Set(kBinds, static_cast<std::int64_t>(materialBinds_));
    }

} // namespace cnahouse::rendering
