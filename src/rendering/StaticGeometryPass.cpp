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
#include "cnahouse/visibility/ExteriorScene.hpp"
#include "cnahouse/world/CellRuntime.hpp"
#include "cnahouse/world/ChunkData.hpp"
#include "cnahouse/world/WorldData.hpp"

namespace cnahouse::rendering
{
    namespace Gfx = Microsoft::Xna::Framework::Graphics;
    using Microsoft::Xna::Framework::Vector3;

    float OpaqueReceiverEffectExposure(world::CellKind cellKind,
                                       bool exteriorFacing,
                                       float cameraEffectExposure) noexcept
    {
        return cellKind == world::CellKind::Exterior || exteriorFacing ? 1.0F : cameraEffectExposure;
    }

    float BasicCelestialKeyScale(bool exteriorWindow,
                                 bool skyOpen,
                                 float effectExposure,
                                 float roomDaylight) noexcept
    {
        constexpr float kExteriorWindowKey = 0.30F;
        constexpr float kIndoorWindowKey = 0.10F;
        if (exteriorWindow)
        {
            return effectExposure * kExteriorWindowKey;
        }
        return effectExposure * (skyOpen ? 1.0F : kIndoorWindowKey * roomDaylight);
    }

    util::Id FixtureGroupForChunk(const world::Chunk& chunk, std::span<const world::Light> lights) noexcept
    {
        util::Id group;
        for (const world::ChunkSubRange& range : chunk.subRanges)
        {
            const util::Id prop = util::Id::Of(range.source);
            for (const world::Light& light : lights)
            {
                if (light.fixtureProp != prop || light.emissiveMaterialSlot.empty())
                {
                    continue;
                }
                if (group.IsValid() && group != light.group)
                {
                    return {};
                }
                group = light.group;
            }
        }
        return group;
    }

    Vector3
    FixtureEmissiveMultiplier(const Vector3& groupColour, float groupLevel, float effectExposure) noexcept
    {
        const float level = std::isfinite(groupLevel) ? std::clamp(groupLevel, 0.0F, 1.0F) : 0.0F;
        const float exposure = std::isfinite(effectExposure) ? std::clamp(effectExposure, 0.0F, 6.0F) : 1.0F;
        const float reflected = 0.06F * exposure;
        const float emitted = level * (0.38F + 0.92F * std::min(exposure, 1.5F));
        return Vector3(reflected + emitted * std::max(groupColour.X, 0.0F),
                       reflected + emitted * std::max(groupColour.Y, 0.0F),
                       reflected + emitted * std::max(groupColour.Z, 0.0F));
    }

    namespace
    {
        // Non-lightmapped Basic detail has no per-surface LM_DAY attenuation. The first fixed
        // furniture captures measured the room's full sky term * 5.875 eye exposure plus XNA's
        // constructor-white key, clipping every pale seat. Keep the attenuated daylight terms,
        // while an *active* authored fixture supplies enough room bounce for close furniture and
        // trim to read: the foyer chair's fixed pixel rose from RGB(17,14,12) to RGB(53,37,23)
        // without altering any artificial-lightmapped architectural receiver.
        constexpr float kBasicSkyBounce = 0.18F;
        constexpr float kBasicFixtureAmbient = 0.20F;
        constexpr float kBasicFixtureKey = 0.22F;

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
        fixtureGroups_.reserve(library_.chunks.size());
        for (const world::Chunk& chunk : library_.chunks)
        {
            fixtureGroups_.push_back(FixtureGroupForChunk(chunk, world.Lights()));
        }
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
        const float cameraExposure = lighting_->CameraEffectExposure();

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
            const util::Id leaderFixtureGroup =
                leader.geometry < fixtureGroups_.size() ? fixtureGroups_[leader.geometry] : util::Id{};
            std::size_t last = first + 1u;
            while (last < items.size() && items[last].effect == leader.effect &&
                   items[last].material == leader.material && items[last].geometry < library_.chunks.size())
            {
                const world::Chunk& candidate = library_.chunks[items[last].geometry];
                if (candidate.cell != leaderChunk.cell || candidate.layout != leaderChunk.layout)
                {
                    break;
                }
                const util::Id candidateFixtureGroup = items[last].geometry < fixtureGroups_.size()
                                                           ? fixtureGroups_[items[last].geometry]
                                                           : util::Id{};
                if (candidateFixtureGroup != leaderFixtureGroup)
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
            Vector3 runMin;
            Vector3 runMax;
            for (std::size_t index = first; index < last; ++index)
            {
                if (cells_.Find(items[index].geometry) == nullptr)
                {
                    continue;
                }
                const world::Chunk& chunk = library_.chunks[items[index].geometry];
                if (!hasResident)
                {
                    runMin = chunk.bounds.Min;
                    runMax = chunk.bounds.Max;
                    hasResident = true;
                    continue;
                }
                runMin.X = std::min(runMin.X, chunk.bounds.Min.X);
                runMin.Y = std::min(runMin.Y, chunk.bounds.Min.Y);
                runMin.Z = std::min(runMin.Z, chunk.bounds.Min.Z);
                runMax.X = std::max(runMax.X, chunk.bounds.Max.X);
                runMax.Y = std::max(runMax.Y, chunk.bounds.Max.Y);
                runMax.Z = std::max(runMax.Z, chunk.bounds.Max.Z);
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

            const bool exteriorSkin = visibility::IsExteriorSkinMaterial(library_.materials[leader.material]);
            const bool exteriorWindow =
                visibility::IsExteriorWindowMaterial(library_.materials[leader.material]);
            const bool exteriorDoor = visibility::IsExteriorDoorMaterial(library_.materials[leader.material]);
            const bool weatherFacingDetail = exteriorWindow || exteriorDoor;
            const float exposure =
                OpaqueReceiverEffectExposure(cell->kind, exteriorSkin || weatherFacingDetail, cameraExposure);

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
                if (exteriorSkin)
                {
                    // The shell keeps its adjacent room as the residency owner, but the outside
                    // face is not illuminated through that room's windows and must never receive
                    // its lamps. Its own LM_DAY islands contain the uniform-sky bake and preserve
                    // the eave/reveal occlusion; only the live intensity comes from the globally
                    // shared outdoor sky. This remains the stock DualTexture path.
                    DrawParams draw = common;
                    float scale = exposure;
                    if (cell->lightmaps.daylight.has_value())
                    {
                        draw.lightmap = textures_(cell->lightmaps.daylight->contentName);
                        scale = 0.5F * cell->lightmaps.daylight->scale * exposure;
                    }
                    else
                    {
                        draw.lightmap = textures_(kNeutralLightmap);
                    }
                    const Vector3& sky = lighting_->OutdoorSkyIrradianceColor();
                    draw.colourMultiplier = Vector3(scale * sky.X, scale * sky.Y, scale * sky.Z);
                    if (!submit(draw, false, true))
                    {
                        first = last;
                        continue;
                    }

                    // An outer-skin run must not inherit the owning room's lamps, but a fixed
                    // lamp in a neighbouring cell can have an explicit bake on this receiver.
                    // Those cross-cell products are exactly the bindings not present in the
                    // cell's control index. They consume the source group's global envelope;
                    // adding that group to `cell.lightGroups` would incorrectly brighten the
                    // foyer state/exposure and violate the world validator's ownership rule.
                    for (const world::CellLightmapGroup& group : cell->lightmaps.artificial)
                    {
                        if (std::find(cell->lightGroups.begin(), cell->lightGroups.end(), group.group) !=
                            cell->lightGroups.end())
                        {
                            continue;
                        }
                        const float groupLevel = lighting_->GroupOutputLevel(group.group);
                        if (groupLevel <= 0.0F)
                        {
                            continue;
                        }
                        DrawParams spill = common;
                        spill.lightmap = textures_(group.texture.contentName);
                        const Vector3 groupColour = lighting_->GroupColor(group.group);
                        const float groupScale = 0.5F * group.texture.scale * groupLevel * exposure;
                        spill.colourMultiplier = Vector3(groupScale * groupColour.X,
                                                         groupScale * groupColour.Y,
                                                         groupScale * groupColour.Z);
                        submit(spill, true, false);
                    }
                    first = last;
                    continue;
                }

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
                    bool hasMappedGroup = false;
                    for (const util::Id group : cell->lightGroups)
                    {
                        if (bindingFor(group) != nullptr)
                        {
                            hasMappedGroup = true;
                            break;
                        }
                    }
                    if (!hasMappedGroup)
                    {
                        first = last;
                        continue;
                    }
                    // A lamp's spatial bake is close to black away from the fixture. Multiplying
                    // the unlit ambient floor by that bake made a daylight room's walls disappear
                    // when the switch was off (L0_LIVING: peak 10.55, mean 0.011 irradiance).
                    // Half-grey is neutral under DualTextureEffect's measured x2 product: it
                    // writes the true 0.025 floor and depth, while each *active* group retains
                    // its authored UV2 bake in an additive depth-equal pass.
                    DrawParams ambient = common;
                    ambient.lightmap = textures_(kNeutralLightmap);
                    ambient.colourMultiplier = Vector3(lighting::kAmbientFloor * exposure,
                                                       lighting::kAmbientFloor * exposure,
                                                       lighting::kAmbientFloor * exposure);
                    if (!submit(ambient, false, true))
                    {
                        first = last;
                        continue;
                    }

                    // No group with zero contribution costs a draw. Active groups repeat the
                    // identical geometry/transform against the depth written above.
                    for (const util::Id groupId : cell->lightGroups)
                    {
                        const world::CellLightmapGroup* group = bindingFor(groupId);
                        if (group == nullptr)
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
                        const float groupScale = 0.5F * group->texture.scale * groupLevel * exposure;
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
                    draw.colourMultiplier = Vector3(exposure, exposure, exposure);
                    if (cell->lightmaps.daylight.has_value())
                    {
                        draw.colourMultiplier = Vector3(lighting::kAmbientFloor * exposure,
                                                        lighting::kAmbientFloor * exposure,
                                                        lighting::kAmbientFloor * exposure);
                    }
                    if (!submit(draw, false, true))
                    {
                        first = last;
                        continue;
                    }
                }

                if (cell->lightmaps.daylight.has_value() && room != nullptr &&
                    (room->daylightTint.X > 0.0F || room->daylightTint.Y > 0.0F ||
                     room->daylightTint.Z > 0.0F))
                {
                    const world::CellLightmapTexture& lightmap = *cell->lightmaps.daylight;
                    DrawParams daylight = common;
                    daylight.lightmap = textures_(lightmap.contentName);
                    const float scale = 0.5F * lightmap.scale * exposure;
                    daylight.colourMultiplier = Vector3(scale * room->daylightTint.X,
                                                        scale * room->daylightTint.Y,
                                                        scale * room->daylightTint.Z);
                    // `daylightTint` is already sky colour times the room's live daylight level.
                    // Re-evaluating either here would let the dome and the shell disagree. It is
                    // the final additive pass so the opaque/artificial depth remains authoritative.
                    submit(daylight, true, false);
                }
            }
            else
            {
                DrawParams draw = common;
                if (room != nullptr)
                {
                    const bool skyOpen =
                        weatherFacingDetail || (cell->kind == world::CellKind::Exterior &&
                                                cell->visibilityHint == world::VisibilityHint::Open);
                    const float artificial =
                        weatherFacingDetail ? 0.0F : room->artificial * kBasicFixtureAmbient;
                    const Vector3& sky = skyOpen ? lighting_->SkyAmbientColor() : room->skyAmbientColor;
                    const float skyScale = skyOpen ? 1.0F : kBasicSkyBounce;
                    draw.ambientLight =
                        Vector3(exposure * std::min(1.0F,
                                                    lighting::kAmbientFloor + skyScale * sky.X +
                                                        artificial * room->artificialColor.X),
                                exposure * std::min(1.0F,
                                                    lighting::kAmbientFloor + skyScale * sky.Y +
                                                        artificial * room->artificialColor.Y),
                                exposure * std::min(1.0F,
                                                    lighting::kAmbientFloor + skyScale * sky.Z +
                                                        artificial * room->artificialColor.Z));
                    const bool celestial = lighting_->CelestialKeyForCell(cell->id) != nullptr;
                    const Vector3 objectCentre = (runMin + runMax) * 0.5F;
                    lighting::ObjectLightAssignment objectLights =
                        exteriorDoor && !celestial
                            ? lighting_->CrossCellReceiverLightsForObject(cell->id, objectCentre)
                            : lighting_->StaticDetailLightsForObject(cell->id, objectCentre);
                    // A weather-facing leaf may be paired either with a baked facade source
                    // (`bakeCells`, the front entry) or with an explicitly unbaked practical
                    // (`spillCells`, the outward-aimed garage flood). Prefer the matching baked
                    // source when it exists, but do not make an unbaked fixed receiver invisible
                    // merely because exterior doors use this narrower lighting boundary.
                    if (exteriorDoor && !celestial && !objectLights.slots[0].has_value())
                    {
                        const lighting::ObjectLightAssignment spillLights =
                            lighting_->StaticDetailLightsForObject(cell->id, objectCentre);
                        const Vector3& spill = spillLights.spillDiffuseColor;
                        if (spill.X > 0.0F || spill.Y > 0.0F || spill.Z > 0.0F)
                        {
                            objectLights = spillLights;
                        }
                    }
                    if (!celestial)
                    {
                        const Vector3& spill = objectLights.spillDiffuseColor;
                        draw.ambientLight = Vector3(
                            std::min(1.0F, draw.ambientLight.X + exposure * kBasicFixtureAmbient * spill.X),
                            std::min(1.0F, draw.ambientLight.Y + exposure * kBasicFixtureAmbient * spill.Y),
                            std::min(1.0F, draw.ambientLight.Z + exposure * kBasicFixtureAmbient * spill.Z));
                    }
                    if (exteriorDoor && !celestial)
                    {
                        // Wall-mounted sources graze a coplanar door, so their direct Lambert term
                        // is deliberately small. Reuse the same restrained fixture-bounce factor
                        // as indoor Basic detail to represent the lit porch enclosure around it.
                        Vector3 localBounce;
                        for (std::size_t slot = 0; slot < 2; ++slot)
                        {
                            if (!objectLights.slots[slot].has_value())
                            {
                                continue;
                            }
                            const Vector3& source = objectLights.slots[slot]->diffuseColor;
                            localBounce.X += source.X;
                            localBounce.Y += source.Y;
                            localBounce.Z += source.Z;
                        }
                        draw.ambientLight = Vector3(
                            std::min(1.0F,
                                     draw.ambientLight.X + exposure * kBasicFixtureAmbient * localBounce.X),
                            std::min(1.0F,
                                     draw.ambientLight.Y + exposure * kBasicFixtureAmbient * localBounce.Y),
                            std::min(1.0F,
                                     draw.ambientLight.Z + exposure * kBasicFixtureAmbient * localBounce.Z));
                    }
                    const float directionalScale =
                        celestial ? BasicCelestialKeyScale(exteriorWindow, skyOpen, exposure, room->daylight)
                                  : exposure * kBasicFixtureKey;
                    for (std::size_t slot = 0; slot < objectLights.slots.size(); ++slot)
                    {
                        if (!objectLights.slots[slot].has_value() || (exteriorWindow && !celestial))
                        {
                            continue;
                        }
                        const lighting::ObjectDirectionalLight& light = *objectLights.slots[slot];
                        draw.directionalLights[slot] =
                            StockDirectionalLight{light.direction,
                                                  Vector3(directionalScale * light.diffuseColor.X,
                                                          directionalScale * light.diffuseColor.Y,
                                                          directionalScale * light.diffuseColor.Z),
                                                  Vector3(0.0F, 0.0F, 0.0F)};
                    }
                }
                const MaterialDesc* description = binder_->Find(material->id);
                if (material->materialClass == world::MaterialClass::Emissive && leaderFixtureGroup.IsValid())
                {
                    draw.colourMultiplier =
                        FixtureEmissiveMultiplier(lighting_->GroupColor(leaderFixtureGroup),
                                                  lighting_->GroupOutputLevel(leaderFixtureGroup),
                                                  exposure);
                }
                else if (description != nullptr && !description->lightingEnabled)
                {
                    draw.colourMultiplier = Vector3(exposure, exposure, exposure);
                }
                submit(draw, false, true);
            }
            first = last;
        }
    }

} // namespace cnahouse::rendering
