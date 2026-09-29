// SPDX-License-Identifier: MIT
#pragma once

#include <cstdint>
#include <functional>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "cnahouse/rendering/Camera.hpp"
#include "cnahouse/rendering/Renderer.hpp"
#include "cnahouse/util/Ids.hpp"
#include "cnahouse/visibility/RenderList.hpp"

namespace Microsoft::Xna::Framework::Graphics
{
    class BasicEffect;
    class Texture2D;
} // namespace Microsoft::Xna::Framework::Graphics

namespace cnahouse::lighting
{
    class LightingSystem;
    struct RoomLightState;
} // namespace cnahouse::lighting

namespace cnahouse::world
{
    class CellRuntime;
    class WorldData;
    enum class CellKind : std::uint8_t;
    struct Cell;
    struct Chunk;
    struct ChunkLibrary;
    struct Light;
    struct MaterialDef;
} // namespace cnahouse::world

namespace cnahouse::rendering
{

    /// @brief Ambient share of an active fixture's colour on unbaked (Basic) detail.
    inline constexpr float kBasicFixtureAmbient = 0.20F;

    /// @brief Direct share of an active fixture's colour in a Basic detail light slot.
    inline constexpr float kBasicFixtureKey = 0.22F;

    /// @brief Outdoor opaque receivers share the sky's scene-referred Tier-S exposure domain.
    ///
    /// Interior receivers continue following the camera's adapted stock-effect multiplier. An
    /// exterior cell, or a weather-facing chunk resident in a room, must not become a bright emitter
    /// merely because the camera is in a dark room and the sky dome behind it remains unexposed.
    [[nodiscard]] float OpaqueReceiverEffectExposure(world::CellKind cellKind,
                                                     bool exteriorFacing,
                                                     float cameraEffectExposure) noexcept;

    /// @brief Stock-BasicEffect celestial-key scale for opaque architectural detail.
    ///
    /// Weather-facing window frames already receive the full outdoor sky ambient. Their pale
    /// painted albedo therefore needs a restrained direct key to retain moulding detail instead
    /// of clipping to emissive white. Other open-sky detail keeps the full key, while indoor
    /// detail retains its daylight-gated window contribution.
    [[nodiscard]] float BasicCelestialKeyScale(bool exteriorWindow,
                                               bool skyOpen,
                                               float effectExposure,
                                               float roomDaylight) noexcept;

    /// @brief Cell-wide irradiance sampled from the same bake as the nearby shell receiver.
    ///
    /// This is the low-cost HOUSE-03402 path for static detail without UV1: daylight and active
    /// owned fixture products contribute their baked non-padding means. The ambient floor remains
    /// present. Foreign facade bindings are excluded because they are not the interior receiver.
    [[nodiscard]] Microsoft::Xna::Framework::Vector3
    BakedReceiverAmbientFor(const world::Cell& cell,
                            const lighting::RoomLightState& room,
                            const lighting::LightingSystem& lighting) noexcept;

    /// @brief Bounded indirect shell fill from active owned fixtures, never from daylight.
    /// An unlit receiver retains only the existing ambient floor; strong lamps cannot add
    /// more than 0.09 per channel. The caller supplies the bake-derived fixture irradiance.
    [[nodiscard]] Microsoft::Xna::Framework::Vector3
    ArtificialShellBounceFor(const Microsoft::Xna::Framework::Vector3& fixtureIrradiance) noexcept;

    /// @brief The one switch group shared by every linked fixture prop in @p chunk.
    ///
    /// The content build keeps independently switched emissive slots in separate chunks. An
    /// invalid id means the chunk is ordinary geometry, unlinked, or violates that contract.
    [[nodiscard]] util::Id FixtureGroupForChunk(const world::Chunk& chunk,
                                                std::span<const world::Light> lights) noexcept;

    /// @brief Stock-BasicEffect colour for a physical diffuser at its live transition level.
    [[nodiscard]] Microsoft::Xna::Framework::Vector3
    FixtureEmissiveMultiplier(const Microsoft::Xna::Framework::Vector3& groupColour,
                              float groupLevel,
                              float effectExposure) noexcept;

    /// @brief Tier S changes to the fully-wet endpoint at this integrated wetness.
    inline constexpr float kTierSWetSwapThreshold = 0.5F;

    /// @brief Finds the validated `_WET` endpoint for one authored dry material name.
    ///
    /// `HOUSE-00905` authored the fourteen pairs with a stable suffix. The lookup happens once
    /// when the static pass is constructed; frame drawing retains only the two hashed ids.
    [[nodiscard]] util::Id FindTierSWetVariant(std::span<const world::MaterialDef> materials,
                                               std::string_view dryName);

    /// @brief Chooses one prevalidated Tier-S endpoint from the live integrated wetness.
    [[nodiscard]] util::Id SelectTierSMaterial(util::Id dry, util::Id wet, float surfaceWetness) noexcept;
    class MaterialBinder;
    struct FogParams;

    enum class StaticGeometryMode
    {
        /// Canonical albedo plus cell-owned baked lighting. The normal playable presentation.
        ProductionMaterials,
        /// Stable hashed colours for geometry/material diagnosis. Explicit debug presentation.
        DebugBlockout,
    };

    /// @brief `Pass::OpaqueStatic`: production shell materials, or the explicit debug blockout.
    ///
    /// **This pass no longer decides what to draw.** It walked the residency map itself until
    /// `HOUSE-00676`; now it draws `RenderList::ItemsFor(Pass::OpaqueStatic)` and whoever built the
    /// list decided. That is §25.1's shape -- step 5 produces a sorted list and the passes submit
    /// it. Production binds a material/lightmap once per contiguous material-and-cell run; the
    /// debug mode binds one hashed colour per material run.
    ///
    /// **No culling here either.** What the list holds is somebody else's answer; today the
    /// blockout and walk scenes put every resident chunk in it, and §25's visible set replaces
    /// that source without this pass changing at all -- which is the point of taking the decision
    /// out of it.
    ///
    /// In production, receiver chunks use stock XNA `DualTextureEffect` with their authored albedo
    /// and cell-owned baked-light atlases. The first artificial group is opaque; each other active
    /// group repeats identical geometry with additive blending and depth-equal/no-write; the live
    /// sky-tinted daylight atlas is the final additive pass. Room-resident outer skin instead uses
    /// its daylight atlas once as an opaque base under the unattenuated outdoor sky, never under
    /// room lamps or window attenuation. Room-resident exterior window and door detail likewise
    /// uses the outdoor sky and celestial key instead of the owning room's dim indirect term. An
    /// explicitly baked cross-cell group may still add its
    /// fixed spill to that outside face and supplies the matching stock-BasicEffect approximation
    /// to its door detail (for example the porch lanterns on the foyer facade).
    /// Detail uses stock `BasicEffect`.
    /// `--scene=blockout` retains the old unlit hashed palette
    /// deliberately, so a diagnostic can still separate surface classes without leaking into
    /// ordinary play.
    class StaticGeometryPass final : public IRenderPass
    {
    public:
        using TextureLookup =
            std::function<Microsoft::Xna::Framework::Graphics::Texture2D*(std::string_view contentName)>;

        /// @brief Explicit debug constructor. Borrows all four; each must outlive this pass.
        ///
        /// @param list the frame's draw list. Non-const because the first pass to ask for its own
        ///        slice sorts it (`RenderList::ItemsFor`), which is what makes a caller that
        ///        forgot to sort impossible rather than merely unlucky.
        StaticGeometryPass(const world::ChunkLibrary& library,
                           const world::CellRuntime& cells,
                           const Camera& camera,
                           visibility::RenderList& list,
                           StaticGeometryMode mode);

        /// @brief Production constructor. All borrowed services must outlive this pass.
        StaticGeometryPass(const world::ChunkLibrary& library,
                           const world::CellRuntime& cells,
                           const world::WorldData& world,
                           const lighting::LightingSystem& lighting,
                           const Camera& camera,
                           visibility::RenderList& list,
                           MaterialBinder& binder,
                           TextureLookup textures,
                           const FogParams* exteriorFog = nullptr,
                           const float* surfaceWetness = nullptr);
        ~StaticGeometryPass() override;

        void Draw(PassContext& context) override;

        /// @brief Reverses the culling, so that only BACK faces are drawn (`HOUSE-00478`).
        ///
        /// §14: front faces are counter-clockwise and the game binds `CullClockwise`. Bind the
        /// opposite and every face that is drawn is one you should never have been able to see --
        /// so a frame that is nearly empty is a frame with nothing inside out in it. That is a
        /// normal-visualisation pass that needs no custom effect, which Tier S could not have
        /// (ADR-0003), and it says a thing a colour ramp does not: not "which way does this face
        /// point" but "is this face pointing at me when it should not be".
        void SetShowBackFaces(bool value) noexcept
        {
            showBackFaces_ = value;
        }

        [[nodiscard]] bool IsActive() const override;

        [[nodiscard]] StaticGeometryMode Mode() const noexcept
        {
            return mode_;
        }

        /// @brief Chunks and triangles submitted by the last `Draw`.
        [[nodiscard]] std::uint32_t ChunksDrawn() const noexcept
        {
            return chunksDrawn_;
        }

        [[nodiscard]] std::uint32_t TrianglesDrawn() const noexcept
        {
            return trianglesDrawn_;
        }

        /// @brief Effect-parameter applications by the last `Draw` -- §71.2's *state changes*.
        ///
        /// One per run of chunks sharing a material, which over a sorted list is one per material
        /// and over an unsorted one is very nearly one per chunk. Counted rather than assumed,
        /// because it is the only number that says whether §25.1's step 5 bought anything.
        [[nodiscard]] std::uint32_t StateChanges() const noexcept
        {
            return stateChanges_;
        }

        /// @brief The blockout colour of @p material: stable, and derived from the name alone.
        ///
        /// A hash rather than a table, because a table here would be a second copy of
        /// `house_shell_gen.py`'s placeholder palette and the two would drift. Deterministic across
        /// runs and platforms -- a render test compares pixels — and spread over the hue circle so
        /// that two materials of one cell are told apart rather than being two greys.
        [[nodiscard]] static Microsoft::Xna::Framework::Vector3 BlockoutColour(const std::string& material);

    private:
        void DrawDebug(PassContext& context);
        void DrawProduction(PassContext& context);

        const world::ChunkLibrary& library_;
        const world::CellRuntime& cells_;
        const world::WorldData* world_ = nullptr;
        const lighting::LightingSystem* lighting_ = nullptr;
        const Camera& camera_;
        visibility::RenderList& list_;
        MaterialBinder* binder_ = nullptr;
        TextureLookup textures_;
        const FogParams* exteriorFog_ = nullptr;
        const float* surfaceWetness_ = nullptr;
        std::unique_ptr<Microsoft::Xna::Framework::Graphics::BasicEffect> effect_;
        std::vector<util::Id> fixtureGroups_;
        /// One validated wet endpoint per chunk-library material, or invalid for no endpoint.
        std::vector<util::Id> wetMaterials_;
        StaticGeometryMode mode_ = StaticGeometryMode::DebugBlockout;
        bool showBackFaces_ = false;
        std::uint32_t chunksDrawn_ = 0u;
        std::uint32_t trianglesDrawn_ = 0u;
        std::uint32_t stateChanges_ = 0u;
    };

} // namespace cnahouse::rendering
