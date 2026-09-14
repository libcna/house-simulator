// SPDX-License-Identifier: MIT
#include "cnahouse/rendering/StaticGeometryPass.hpp"

#include <algorithm>
#include <cmath>
#include <utility>

#include "Microsoft/Xna/Framework/Graphics/BasicEffect.hpp"
#include "Microsoft/Xna/Framework/Graphics/BlendState.hpp"
#include "Microsoft/Xna/Framework/Graphics/DepthStencilState.hpp"
#include "Microsoft/Xna/Framework/Graphics/Effect.hpp"
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
#include "cnahouse/lighting/RoomLightState.hpp"
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
                                           visibility::RenderList& list,
                                           StaticGeometryMode mode)
        : library_(library)
        , cells_(cells)
        , camera_(camera)
        , list_(list)
        , mode_(mode)
    {
    }

    StaticGeometryPass::StaticGeometryPass(const world::ChunkLibrary& library,
                                           const world::CellRuntime& cells,
                                           const world::WorldData& world,
                                           const lighting::LightingSystem& lighting,
                                           const Camera& camera,
                                           visibility::RenderList& list,
                                           MaterialBinder& binder,
                                           TextureLookup textures)
        : library_(library)
        , cells_(cells)
        , world_(&world)
        , lighting_(&lighting)
        , camera_(camera)
        , list_(list)
        , binder_(&binder)
        , textures_(std::move(textures))
        , mode_(StaticGeometryMode::ProductionMaterials)
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

        if (mode_ == StaticGeometryMode::ProductionMaterials)
        {
            DrawProduction(context);
        }
        else
        {
            DrawDebug(context);
        }

        static const debug::Counters::Handle kChunks = context.counters.Resolve("static.chunks");
        static const debug::Counters::Handle kTriangles = context.counters.Resolve("static.triangles");
        static const debug::Counters::Handle kStates = context.counters.Resolve("static.stateChanges");
        context.counters.Set(kChunks, static_cast<std::int64_t>(chunksDrawn_));
        context.counters.Set(kTriangles, static_cast<std::int64_t>(trianglesDrawn_));
        context.counters.Set(kStates, static_cast<std::int64_t>(stateChanges_));
    }

    void StaticGeometryPass::DrawDebug(PassContext& context)
    {
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
                    passes[p]->Apply();
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
    }

    void StaticGeometryPass::DrawProduction(PassContext& context)
    {
        if (world_ == nullptr || lighting_ == nullptr || binder_ == nullptr || !textures_)
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

        constexpr std::string_view kNeutralLightmap = "Textures/Fallback/grey";
        const std::span<const visibility::RenderItem> items = list_.ItemsFor(Pass::OpaqueStatic);
        std::size_t first = 0u;
        while (first < items.size())
        {
            const visibility::RenderItem& leader = items[first];
            if (leader.geometry >= library_.chunks.size())
            {
                ++first;
                continue;
            }
            const world::Chunk& leaderChunk = library_.chunks[leader.geometry];
            std::size_t last = first + 1u;
            while (last < items.size() && items[last].effect == leader.effect &&
                   items[last].material == leader.material && items[last].geometry < library_.chunks.size())
            {
                const world::Chunk& candidate = library_.chunks[items[last].geometry];
                if (candidate.cell != leaderChunk.cell || candidate.layout != leaderChunk.layout)
                {
                    break;
                }
                ++last;
            }

            if (leader.material >= library_.materials.size() || leaderChunk.cell >= library_.cells.size())
            {
                first = last;
                continue;
            }
            bool hasResident = false;
            for (std::size_t index = first; index < last; ++index)
            {
                if (cells_.Find(items[index].geometry) != nullptr)
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

            const world::MaterialDef* material =
                world_->FindMaterial(util::Id::Of(library_.materials[leader.material]));
            const world::Cell* cell = world_->FindCell(util::Id::Of(library_.cells[leaderChunk.cell]));
            if (material == nullptr || cell == nullptr ||
                material->effectTierS != visibility::EffectForLayout(leaderChunk.layout))
            {
                first = last;
                continue;
            }

            DrawParams common;
            common.world = &worldMatrix;
            common.view = &view;
            common.projection = &projection;
            if (!material->albedo.empty())
            {
                common.diffuse = textures_(material->albedo);
            }

            const lighting::RoomLightState* room = lighting_->FindCell(cell->id);
            const util::Result<CullPolicy> cull = binder_->CullFor(material->id, 1.0F);
            if (!cull)
            {
                first = last;
                continue;
            }
            context.states.SetRasterizer(showBackFaces_ ? Gfx::RasterizerState::CullCounterClockwise
                                                        : StateFor(cull.Value()));

            auto submit = [&](const DrawParams& draw, bool additive, bool countGeometry)
            {
                const util::Result<Gfx::Effect*> effectResult = binder_->Bind(material->id, draw);
                if (!effectResult)
                {
                    return false;
                }
                ++stateChanges_;
                context.states.SetBlend(additive ? Gfx::BlendState::Additive : Gfx::BlendState::Opaque);
                context.states.SetDepthStencil(additive ? DepthEqualReadOnly()
                                                        : Gfx::DepthStencilState::Default);

                Gfx::EffectPassCollection& passes =
                    effectResult.Value()->getCurrentTechniqueProperty()->getPassesProperty();
                const int passCount = passes.getCountProperty();
                for (int p = 0; p < passCount; ++p)
                {
                    bool applied = false;
                    for (std::size_t index = first; index < last; ++index)
                    {
                        const visibility::RenderItem& item = items[index];
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
                        if (countGeometry && p == 0)
                        {
                            ++chunksDrawn_;
                            trianglesDrawn_ += resident->primitiveCount;
                        }
                    }
                }
                return true;
            };

            if (leaderChunk.layout == world::ChunkLayout::Dual)
            {
                if (!cell->lightmaps.artificial.empty())
                {
                    auto bindingFor = [&](util::Id group) -> const world::CellLightmapGroup*
                    {
                        const auto found = std::find_if(cell->lightmaps.artificial.begin(),
                                                        cell->lightmaps.artificial.end(),
                                                        [group](const world::CellLightmapGroup& candidate)
                                                        { return candidate.group == group; });
                        return found == cell->lightmaps.artificial.end() ? nullptr : &*found;
                    };
                    const world::CellLightmapGroup* base = nullptr;
                    for (const util::Id group : cell->lightGroups)
                    {
                        base = bindingFor(group);
                        if (base != nullptr)
                        {
                            break;
                        }
                    }
                    if (base == nullptr)
                    {
                        first = last;
                        continue;
                    }
                    DrawParams draw = common;
                    draw.lightmap = textures_(base->texture.contentName);
                    const float level = lighting_->GroupLevelInCell(cell->id, base->group);
                    const Vector3 colour = lighting_->GroupColor(base->group);
                    const float scale = 0.5F * base->texture.scale;
                    draw.colourMultiplier = Vector3(scale * (level * colour.X + lighting::kAmbientFloor),
                                                    scale * (level * colour.Y + lighting::kAmbientFloor),
                                                    scale * (level * colour.Z + lighting::kAmbientFloor));
                    if (!submit(draw, false, true))
                    {
                        first = last;
                        continue;
                    }

                    // The first group establishes colour and depth even when its switch is off.
                    // Later groups with no contribution cost no draw; active ones repeat exactly
                    // the same geometry under the depth-equal state measured by HOUSE-00079.
                    for (const util::Id groupId : cell->lightGroups)
                    {
                        const world::CellLightmapGroup* group = bindingFor(groupId);
                        if (group == nullptr || group == base)
                        {
                            continue;
                        }
                        const float groupLevel = lighting_->GroupLevelInCell(cell->id, group->group);
                        if (groupLevel <= 0.0F)
                        {
                            continue;
                        }
                        DrawParams additional = common;
                        additional.lightmap = textures_(group->texture.contentName);
                        const Vector3 groupColour = lighting_->GroupColor(group->group);
                        const float groupScale = 0.5F * group->texture.scale * groupLevel;
                        additional.colourMultiplier = Vector3(groupScale * groupColour.X,
                                                              groupScale * groupColour.Y,
                                                              groupScale * groupColour.Z);
                        submit(additional, true, false);
                    }
                }
                else
                {
                    // Exterior surfaces have no room bake. Half-grey is neutral under
                    // DualTextureEffect's measured x2 product. An interior receiver with no
                    // artificial group uses the neutral map for its ambient floor and still writes
                    // the depth HOUSE-01264's daylight pass needs.
                    DrawParams draw = common;
                    draw.lightmap = textures_(kNeutralLightmap);
                    if (cell->lightmaps.daylight.has_value())
                    {
                        draw.colourMultiplier = Vector3(
                            lighting::kAmbientFloor, lighting::kAmbientFloor, lighting::kAmbientFloor);
                    }
                    submit(draw, false, true);
                }
            }
            else
            {
                DrawParams draw = common;
                if (room != nullptr)
                {
                    draw.ambientLight =
                        Vector3(std::min(1.0F, room->skyAmbientColor.X + lighting::kAmbientFloor),
                                std::min(1.0F, room->skyAmbientColor.Y + lighting::kAmbientFloor),
                                std::min(1.0F, room->skyAmbientColor.Z + lighting::kAmbientFloor));
                }
                submit(draw, false, true);
            }
            first = last;
        }
    }

} // namespace cnahouse::rendering
