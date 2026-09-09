// SPDX-License-Identifier: MIT
#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "cnahouse/rendering/Camera.hpp"
#include "cnahouse/rendering/Renderer.hpp"
#include "cnahouse/visibility/RenderList.hpp"

namespace Microsoft::Xna::Framework::Graphics
{
    class BasicEffect;
}

namespace cnahouse::world
{
    class CellRuntime;
    struct ChunkLibrary;
} // namespace cnahouse::world

namespace cnahouse::rendering
{

    /// @brief `Pass::OpaqueStatic`: the draw list's static slice, with `BasicEffect`
    ///        (`HOUSE-00475`, `HOUSE-00676`).
    ///
    /// **This pass no longer decides what to draw.** It walked the residency map itself until
    /// `HOUSE-00676`; now it draws `RenderList::ItemsFor(Pass::OpaqueStatic)` and whoever built the
    /// list decided. That is §25.1's shape -- step 5 produces a sorted list and the passes submit
    /// it -- and it is what lets the sort do something: the list arrives grouped by material, so
    /// the blockout colour is written and `Apply`d once per material instead of once per chunk.
    ///
    /// **No culling here either.** What the list holds is somebody else's answer; today the
    /// blockout and walk scenes put every resident chunk in it, and §25's visible set replaces
    /// that source without this pass changing at all -- which is the point of taking the decision
    /// out of it.
    ///
    /// **`BasicEffect` with lighting off, one flat colour per material.** §22.2 puts the receivers
    /// on `DualTextureEffect` and the detail on `BasicEffect`, and neither can draw anything yet:
    /// `HOUSE-00296` acquires the first texture and `HOUSE-00490` bakes the first lightmap. Until
    /// then a lit blockout would be a lie about the lighting and a single grey one would be a
    /// silhouette. So each material gets a stable colour derived from its own name -- a colour to
    /// tell a wall from a floor by, stated to be nothing more. Lighting is off because 60 % of the
    /// chunks are `dual` and carry no normal at all (`docs/chunk-format.md` §4b).
    class StaticGeometryPass final : public IRenderPass
    {
    public:
        /// @brief Borrows all four; each must outlive this pass.
        ///
        /// @param list the frame's draw list. Non-const because the first pass to ask for its own
        ///        slice sorts it (`RenderList::ItemsFor`), which is what makes a caller that
        ///        forgot to sort impossible rather than merely unlucky.
        StaticGeometryPass(const world::ChunkLibrary& library,
                           const world::CellRuntime& cells,
                           const Camera& camera,
                           visibility::RenderList& list);
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
        const world::ChunkLibrary& library_;
        const world::CellRuntime& cells_;
        const Camera& camera_;
        visibility::RenderList& list_;
        std::unique_ptr<Microsoft::Xna::Framework::Graphics::BasicEffect> effect_;
        bool showBackFaces_ = false;
        std::uint32_t chunksDrawn_ = 0u;
        std::uint32_t trianglesDrawn_ = 0u;
        std::uint32_t stateChanges_ = 0u;
    };

} // namespace cnahouse::rendering
