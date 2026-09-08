// SPDX-License-Identifier: MIT
#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "cnahouse/rendering/Camera.hpp"
#include "cnahouse/rendering/Renderer.hpp"

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

    /// @brief `Pass::OpaqueStatic`: every resident cell's chunks, with `BasicEffect` (`HOUSE-00475`).
    ///
    /// **No culling.** §25's portal traversal is `HOUSE-00485` onwards; this pass draws what
    /// `CellRuntime` has made resident, in the order the chunk file lists it, and counts what it
    /// drew. That is the whole point of drawing the blockout before culling exists: a frame that is
    /// wrong with everything drawn is wrong in the geometry, and a frame that is wrong once culling
    /// arrives is wrong in the culling.
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
        /// @brief Borrows all three; each must outlive this pass.
        StaticGeometryPass(const world::ChunkLibrary& library,
                           const world::CellRuntime& cells,
                           const Camera& camera);
        ~StaticGeometryPass() override;

        void Draw(PassContext& context) override;
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
        std::unique_ptr<Microsoft::Xna::Framework::Graphics::BasicEffect> effect_;
        std::uint32_t chunksDrawn_ = 0u;
        std::uint32_t trianglesDrawn_ = 0u;
    };

} // namespace cnahouse::rendering
