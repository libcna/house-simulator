// SPDX-License-Identifier: MIT
#pragma once

#include <cstdint>
#include <memory>

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
    class WorldData;
    enum class MaterialClass : std::uint8_t;
    struct ChunkLibrary;
} // namespace cnahouse::world

namespace cnahouse::lighting
{
    class LightingSystem;
}

namespace cnahouse::rendering
{

    /// @brief Glass is a scene-referred filter, not a camera-exposure-scaled emitter.
    ///
    /// The stock unlit BasicEffect applies its own Alpha to DiffuseColor. Multiplying glass tint
    /// by dark-room exposure before that blend whitens an unexposed sky behind a clear pane; all
    /// non-glass transparent materials retain the existing effect-side camera exposure.
    [[nodiscard]] float TransparentTintExposure(world::MaterialClass materialClass,
                                                float cameraEffectExposure) noexcept;

    /// @brief §23.6's static transparent submission: cell then object, back to front.
    ///
    /// The order belongs to `RenderList`; this pass consumes its `Pass::Transparent` slice without
    /// regrouping by effect or material. It consequently accepts extra material binds in exchange
    /// for correct blending. `DepthRead` tests opaque and alpha-tested geometry but never writes,
    /// while XNA's premultiplied `AlphaBlend` composes each farther surface before the nearer one.
    ///
    /// This is the current static-chunk producer. Future transparent dynamic objects add their own
    /// submission path to the same sorted slice; an item that is not a resident static chunk is
    /// skipped rather than interpreted as one.
    class TransparentPass final : public IRenderPass
    {
    public:
        /// @brief Borrows all five; each must outlive this pass.
        TransparentPass(const world::ChunkLibrary& library,
                        const world::CellRuntime& cells,
                        const world::WorldData& world,
                        const Camera& camera,
                        visibility::RenderList& list,
                        const lighting::LightingSystem* lighting = nullptr);
        ~TransparentPass() override;

        void Draw(PassContext& context) override;
        [[nodiscard]] bool IsActive() const override;

        [[nodiscard]] std::uint32_t ChunksDrawn() const noexcept
        {
            return chunksDrawn_;
        }

        [[nodiscard]] std::uint32_t TrianglesDrawn() const noexcept
        {
            return trianglesDrawn_;
        }

        /// @brief Parameter applications in the sorted order, not distinct material count.
        [[nodiscard]] std::uint32_t MaterialBinds() const noexcept
        {
            return materialBinds_;
        }

    private:
        const world::ChunkLibrary& library_;
        const world::CellRuntime& cells_;
        const world::WorldData& world_;
        const Camera& camera_;
        visibility::RenderList& list_;
        const lighting::LightingSystem* lighting_ = nullptr;
        std::unique_ptr<Microsoft::Xna::Framework::Graphics::BasicEffect> effect_;
        std::uint32_t chunksDrawn_ = 0u;
        std::uint32_t trianglesDrawn_ = 0u;
        std::uint32_t materialBinds_ = 0u;
    };

} // namespace cnahouse::rendering
