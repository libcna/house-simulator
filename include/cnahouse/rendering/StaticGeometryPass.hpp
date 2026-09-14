// SPDX-License-Identifier: MIT
#pragma once

#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "cnahouse/rendering/Camera.hpp"
#include "cnahouse/rendering/Renderer.hpp"
#include "cnahouse/visibility/RenderList.hpp"

namespace Microsoft::Xna::Framework::Graphics
{
    class BasicEffect;
    class Texture2D;
} // namespace Microsoft::Xna::Framework::Graphics

namespace cnahouse::lighting
{
    class LightingSystem;
}

namespace cnahouse::world
{
    class CellRuntime;
    class WorldData;
    struct ChunkLibrary;
} // namespace cnahouse::world

namespace cnahouse::rendering
{
    class MaterialBinder;

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
    /// and cell-owned daylight atlas; architectural detail uses stock `BasicEffect` with the same
    /// room's ambient term. `--scene=blockout` retains the old unlit hashed palette deliberately,
    /// so a diagnostic can still separate surface classes without leaking into ordinary play.
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
                           TextureLookup textures);
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
        std::unique_ptr<Microsoft::Xna::Framework::Graphics::BasicEffect> effect_;
        StaticGeometryMode mode_ = StaticGeometryMode::DebugBlockout;
        bool showBackFaces_ = false;
        std::uint32_t chunksDrawn_ = 0u;
        std::uint32_t trianglesDrawn_ = 0u;
        std::uint32_t stateChanges_ = 0u;
    };

} // namespace cnahouse::rendering
