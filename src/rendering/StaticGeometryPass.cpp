// SPDX-License-Identifier: MIT
#include "cnahouse/rendering/StaticGeometryPass.hpp"

#include <cmath>

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
#include "cnahouse/rendering/StateTracker.hpp"
#include "cnahouse/world/CellRuntime.hpp"
#include "cnahouse/world/ChunkData.hpp"

namespace cnahouse::rendering
{
    namespace Gfx = Microsoft::Xna::Framework::Graphics;
    using Microsoft::Xna::Framework::Vector3;

    namespace
    {
        /// FNV-1a over the name. A fixed, stated algorithm rather than `std::hash`, whose value is
        /// allowed to differ between standard libraries -- and a render test compares pixels.
        std::uint32_t Fnv1a(const std::string& text) noexcept
        {
            std::uint32_t hash = 2166136261u;
            for (const char character : text)
            {
                hash ^= static_cast<std::uint8_t>(character);
                hash *= 16777619u;
            }
            return hash;
        }

        /// One turn of the hue circle at fixed saturation and value, so every colour is equally
        /// readable and none of them is black or white.
        Vector3 HueToRgb(float hue, float saturation, float value) noexcept
        {
            const float sector = hue * 6.0f;
            const float fraction = sector - std::floor(sector);
            const float p = value * (1.0f - saturation);
            const float q = value * (1.0f - saturation * fraction);
            const float t = value * (1.0f - saturation * (1.0f - fraction));
            switch (static_cast<int>(sector) % 6)
            {
                case 0:
                    return Vector3(value, t, p);
                case 1:
                    return Vector3(q, value, p);
                case 2:
                    return Vector3(p, value, t);
                case 3:
                    return Vector3(p, q, value);
                case 4:
                    return Vector3(t, p, value);
                default:
                    return Vector3(value, p, q);
            }
        }

    } // namespace

    Vector3 StaticGeometryPass::BlockoutColour(const std::string& material)
    {
        const std::uint32_t hash = Fnv1a(material);
        // The hue comes from the whole hash and the two remaining knobs from separate bytes of it,
        // so two names that happen to land on a similar hue still differ in weight.
        const float hue = static_cast<float>(hash % 3600u) / 3600.0f;
        const float saturation = 0.35f + static_cast<float>((hash >> 12) % 40u) / 100.0f;
        const float value = 0.55f + static_cast<float>((hash >> 20) % 35u) / 100.0f;
        return HueToRgb(hue, saturation, value);
    }

    StaticGeometryPass::StaticGeometryPass(const world::ChunkLibrary& library,
                                           const world::CellRuntime& cells,
                                           const Camera& camera,
                                           visibility::RenderList& list)
        : library_(library)
        , cells_(cells)
        , camera_(camera)
        , list_(list)
    {
    }

    StaticGeometryPass::~StaticGeometryPass() = default;

    bool StaticGeometryPass::IsActive() const
    {
        // What the LIST holds, not what is resident: residency is what the pass can draw and the
        // list is what it was asked to. A frame whose list is empty has nothing for this pass even
        // with the whole house on the GPU, and saying so is what makes "ran" and "had nothing to
        // do" different numbers in the overlay.
        return list_.Has(Pass::OpaqueStatic);
    }

    void StaticGeometryPass::Draw(PassContext& context)
    {
        chunksDrawn_ = 0u;
        trianglesDrawn_ = 0u;
        stateChanges_ = 0u;
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
                                 : 0.0f;
        effect_->setViewProperty(camera_.View());
        effect_->setProjectionProperty(camera_.Projection(aspect));

        // Opaque geometry, seen from inside and outside: `CullClockwise` is §14's convention for
        // everything this project generates, depth writes on, no blending.
        context.states.SetRasterizer(showBackFaces_ ? Gfx::RasterizerState::CullCounterClockwise
                                                    : Gfx::RasterizerState::CullClockwise);
        context.states.SetDepthStencil(Gfx::DepthStencilState::Default);
        context.states.SetBlend(Gfx::BlendState::Opaque);

        Gfx::EffectPassCollection& passes = effect_->getCurrentTechniqueProperty()->getPassesProperty();

        // §25.1's step 5 sorted this by material, so the blockout colour is written once per RUN of
        // chunks that share one rather than once per chunk -- which is the whole return on the
        // sort, and what §71.2 counts as a state change.
        //
        // The pass loop is the OUTER one, which is what lets `Apply` be hoisted at all: each pass's
        // `Apply` has to precede its own draw, so a technique with two of them must submit the run
        // twice. `BasicEffect` has one, so this loop turns once -- but written the other way round
        // it would be a hoist that silently drew the house under the last pass only.
        const int passCount = passes.getCountProperty();
        for (int p = 0; p < passCount; ++p)
        {
            bool bound = false;
            std::uint16_t boundMaterial = 0u;
            for (const visibility::RenderItem& item : list_.ItemsFor(Pass::OpaqueStatic))
            {
                const world::CellRuntime::ResidentChunk* resident = cells_.Find(item.geometry);
                if (resident == nullptr)
                {
                    // The list named a chunk whose cell is not resident. Skipped and not fatal:
                    // residency and visibility are two answers arriving from different systems, and
                    // the frame in between must draw the house it has rather than stop.
                    continue;
                }
                const world::Chunk& chunk = library_.chunks[item.geometry];
                device.SetVertexBuffer(resident->vertices.get());
                device.setIndicesProperty(resident->indices.get());
                if (!bound || item.material != boundMaterial)
                {
                    effect_->setDiffuseColorProperty(BlockoutColour(library_.materials[item.material]));
                    // `Apply` is what copies the effect's parameters to the device, so it belongs
                    // with the parameter that changed and nowhere else. AFTER the buffers, which is
                    // the order the pass has always bound them in.
                    passes[p].Apply();
                    boundMaterial = item.material;
                    bound = true;
                    ++stateChanges_;
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

        static const debug::Counters::Handle kChunks = context.counters.Resolve("static.chunks");
        static const debug::Counters::Handle kTriangles = context.counters.Resolve("static.triangles");
        static const debug::Counters::Handle kStates = context.counters.Resolve("static.stateChanges");
        context.counters.Set(kChunks, static_cast<std::int64_t>(chunksDrawn_));
        context.counters.Set(kTriangles, static_cast<std::int64_t>(trianglesDrawn_));
        context.counters.Set(kStates, static_cast<std::int64_t>(stateChanges_));
    }

} // namespace cnahouse::rendering
