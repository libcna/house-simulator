// SPDX-License-Identifier: MIT
#include "cnahouse/rendering/AlphaTestPass.hpp"

#include <cstddef>
#include <utility>

#include "Microsoft/Xna/Framework/Graphics/BlendState.hpp"
#include "Microsoft/Xna/Framework/Graphics/DepthStencilState.hpp"
#include "Microsoft/Xna/Framework/Graphics/Effect.hpp"
#include "Microsoft/Xna/Framework/Graphics/EffectPass.hpp"
#include "Microsoft/Xna/Framework/Graphics/EffectPassCollection.hpp"
#include "Microsoft/Xna/Framework/Graphics/EffectTechnique.hpp"
#include "Microsoft/Xna/Framework/Graphics/GraphicsDevice.hpp"
#include "Microsoft/Xna/Framework/Graphics/IndexBuffer.hpp"
#include "Microsoft/Xna/Framework/Graphics/PrimitiveType.hpp"
#include "Microsoft/Xna/Framework/Graphics/Texture2D.hpp"
#include "Microsoft/Xna/Framework/Graphics/VertexBuffer.hpp"
#include "Microsoft/Xna/Framework/Graphics/Viewport.hpp"

#include "cnahouse/debug/Counters.hpp"
#include "cnahouse/rendering/MaterialBinder.hpp"
#include "cnahouse/rendering/RenderStates.hpp"
#include "cnahouse/rendering/StateTracker.hpp"
#include "cnahouse/util/Ids.hpp"
#include "cnahouse/world/CellRuntime.hpp"
#include "cnahouse/world/ChunkData.hpp"
#include "cnahouse/world/WorldData.hpp"

namespace cnahouse::rendering
{
    namespace Gfx = Microsoft::Xna::Framework::Graphics;

    AlphaTestPass::AlphaTestPass(const world::ChunkLibrary& library,
                                 const world::CellRuntime& cells,
                                 const world::WorldData& world,
                                 const Camera& camera,
                                 visibility::RenderList& list,
                                 MaterialBinder& binder,
                                 TextureLookup textures)
        : library_(library)
        , cells_(cells)
        , world_(world)
        , camera_(camera)
        , list_(list)
        , binder_(binder)
        , textures_(std::move(textures))
    {
    }

    AlphaTestPass::~AlphaTestPass() = default;

    bool AlphaTestPass::IsActive() const
    {
        return static_cast<bool>(textures_) && list_.Has(Pass::AlphaTest);
    }

    void AlphaTestPass::Draw(PassContext& context)
    {
        chunksDrawn_ = 0u;
        trianglesDrawn_ = 0u;
        materialBinds_ = 0u;
        if (!textures_)
        {
            return;
        }

        Gfx::GraphicsDevice& device = context.device;
        const auto& viewport = device.getViewportProperty();
        const float aspect = viewport.getHeightProperty() > 0
                                 ? static_cast<float>(viewport.getWidthProperty()) /
                                       static_cast<float>(viewport.getHeightProperty())
                                 : 0.0F;
        const Microsoft::Xna::Framework::Matrix worldMatrix =
            Microsoft::Xna::Framework::Matrix::getIdentityProperty();
        const Microsoft::Xna::Framework::Matrix view = camera_.View();
        const Microsoft::Xna::Framework::Matrix projection = camera_.Projection(aspect);

        // §23.6: discarded texels write nothing; surviving texels are fully opaque geometry and
        // write the depth that the following transparent pass tests against.
        context.states.SetDepthStencil(Gfx::DepthStencilState::Default);
        context.states.SetBlend(Gfx::BlendState::Opaque);

        const std::span<const visibility::RenderItem> items = list_.ItemsFor(Pass::AlphaTest);
        std::size_t first = 0u;
        while (first < items.size())
        {
            const std::uint16_t materialIndex = items[first].material;
            std::size_t last = first + 1u;
            while (last < items.size() && items[last].effect == items[first].effect &&
                   items[last].material == materialIndex)
            {
                ++last;
            }

            if (materialIndex >= library_.materials.size())
            {
                first = last;
                continue;
            }
            const world::MaterialDef* material =
                world_.FindMaterial(util::Id::Of(library_.materials[materialIndex]));
            if (material == nullptr || material->alphaMode != world::AlphaMode::Mask ||
                material->effectTierS != world::EffectTier::AlphaTest || material->albedo.empty())
            {
                first = last;
                continue;
            }
            Gfx::Texture2D* texture = textures_(material->albedo);
            if (texture == nullptr)
            {
                first = last;
                continue;
            }

            bool hasResident = false;
            for (std::size_t i = first; i < last; ++i)
            {
                if (items[i].geometry < library_.chunks.size() && cells_.Find(items[i].geometry) != nullptr)
                {
                    hasResident = true;
                    break;
                }
            }
            if (!hasResident)
            {
                first = last;
                continue;
            }

            DrawParams draw;
            draw.world = &worldMatrix;
            draw.view = &view;
            draw.projection = &projection;
            draw.diffuse = texture;
            const util::Result<Gfx::Effect*> effectResult = binder_.Bind(material->id, draw);
            const util::Result<CullPolicy> cull = binder_.CullFor(material->id, 1.0F);
            if (!effectResult || !cull)
            {
                first = last;
                continue;
            }
            ++materialBinds_;
            context.states.SetRasterizer(StateFor(cull.Value()));

            Gfx::EffectPassCollection& passes =
                effectResult.Value()->getCurrentTechniqueProperty()->getPassesProperty();
            const int passCount = passes.getCountProperty();
            for (int p = 0; p < passCount; ++p)
            {
                bool applied = false;
                for (std::size_t i = first; i < last; ++i)
                {
                    const visibility::RenderItem& item = items[i];
                    if (item.geometry >= library_.chunks.size())
                    {
                        continue;
                    }
                    const world::CellRuntime::ResidentChunk* resident = cells_.Find(item.geometry);
                    if (resident == nullptr)
                    {
                        continue;
                    }
                    const world::Chunk& chunk = library_.chunks[item.geometry];
                    device.SetVertexBuffer(resident->vertices.get());
                    device.setIndicesProperty(resident->indices.get());
                    if (!applied)
                    {
                        passes[p]->Apply();
                        applied = true;
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
            first = last;
        }

        static const debug::Counters::Handle kChunks = context.counters.Resolve("alpha.chunks");
        static const debug::Counters::Handle kTriangles = context.counters.Resolve("alpha.triangles");
        static const debug::Counters::Handle kBinds = context.counters.Resolve("alpha.materialBinds");
        context.counters.Set(kChunks, static_cast<std::int64_t>(chunksDrawn_));
        context.counters.Set(kTriangles, static_cast<std::int64_t>(trianglesDrawn_));
        context.counters.Set(kBinds, static_cast<std::int64_t>(materialBinds_));
    }

} // namespace cnahouse::rendering
