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
                                           const Camera& camera)
        : library_(library)
        , cells_(cells)
        , camera_(camera)
    {
    }

    StaticGeometryPass::~StaticGeometryPass() = default;

    bool StaticGeometryPass::IsActive() const
    {
        return cells_.ResidentChunks() > 0;
    }

    void StaticGeometryPass::Draw(PassContext& context)
    {
        chunksDrawn_ = 0u;
        trianglesDrawn_ = 0u;
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
        context.states.SetRasterizer(Gfx::RasterizerState::CullClockwise);
        context.states.SetDepthStencil(Gfx::DepthStencilState::Default);
        context.states.SetBlend(Gfx::BlendState::Opaque);

        Gfx::EffectPassCollection& passes = effect_->getCurrentTechniqueProperty()->getPassesProperty();

        for (const std::string& cell : library_.cells)
        {
            const auto* chunks = cells_.Chunks(cell);
            if (chunks == nullptr)
            {
                continue;
            }
            for (const auto& resident : *chunks)
            {
                const world::Chunk& chunk = library_.chunks[resident.chunk];
                effect_->setDiffuseColorProperty(BlockoutColour(library_.materials[chunk.material]));
                device.SetVertexBuffer(resident.vertices.get());
                device.setIndicesProperty(resident.indices.get());
                for (int p = 0; p < passes.getCountProperty(); ++p)
                {
                    // The colour changes per chunk, so `Apply` is per chunk too: it is what copies
                    // the effect's parameters to the device, and hoisting it out of this loop would
                    // draw the whole house in whatever colour the first chunk happened to be.
                    passes[p].Apply();
                    device.DrawIndexedPrimitives(Gfx::PrimitiveType::TriangleList,
                                                 0,
                                                 0,
                                                 static_cast<int>(chunk.vertexCount),
                                                 0,
                                                 static_cast<int>(resident.primitiveCount));
                }
                ++chunksDrawn_;
                trianglesDrawn_ += resident.primitiveCount;
            }
        }

        static const debug::Counters::Handle kChunks = context.counters.Resolve("static.chunks");
        static const debug::Counters::Handle kTriangles = context.counters.Resolve("static.triangles");
        context.counters.Set(kChunks, static_cast<std::int64_t>(chunksDrawn_));
        context.counters.Set(kTriangles, static_cast<std::int64_t>(trianglesDrawn_));
    }

} // namespace cnahouse::rendering
