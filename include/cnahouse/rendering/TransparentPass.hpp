// SPDX-License-Identifier: MIT
#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>

#include "Microsoft/Xna/Framework/Vector3.hpp"

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
    struct Light;
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

    /// @brief One §28.6 camera-facing fixture glow after intensity and exposure response.
    struct FixtureGlowVisual
    {
        Microsoft::Xna::Framework::Vector3 centre;
        Microsoft::Xna::Framework::Vector3 tint;
        float radius = 0.0F;
        float alpha = 0.0F;
        bool visible = false;
    };

    /// @brief Shared soft radial profile for fixture glow and §32.4's later flare sprites.
    [[nodiscard]] float GlowQuadRadialOpacity(float normalisedRadius) noexcept;

    /// @brief Resolves one linked fixture into a restrained world-space additive halo.
    ///
    /// Lumens establish source strength, @p groupLevel follows the exact bulb transition, and
    /// @p cameraExposure grows both apparent radius and alpha as the eye opens in a dark space.
    /// An unlinked light is deliberately invisible: canonical light points without a real fixture
    /// must not become floating placeholder orbs.
    [[nodiscard]] FixtureGlowVisual FixtureGlowFor(const world::Light& light,
                                                   float groupLevel,
                                                   const Microsoft::Xna::Framework::Vector3& groupColour,
                                                   float cameraExposure) noexcept;

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

        [[nodiscard]] std::uint32_t GlowsDrawn() const noexcept
        {
            return glowsDrawn_;
        }

    private:
        class GlowResources;

        const world::ChunkLibrary& library_;
        const world::CellRuntime& cells_;
        const world::WorldData& world_;
        const Camera& camera_;
        visibility::RenderList& list_;
        const lighting::LightingSystem* lighting_ = nullptr;
        std::unique_ptr<Microsoft::Xna::Framework::Graphics::BasicEffect> effect_;
        std::unique_ptr<GlowResources> glowResources_;
        std::vector<const world::Light*> glowLights_;
        std::uint32_t chunksDrawn_ = 0u;
        std::uint32_t trianglesDrawn_ = 0u;
        std::uint32_t materialBinds_ = 0u;
        std::uint32_t glowsDrawn_ = 0u;
        debug::Counters* counterOwner_ = nullptr;
        std::size_t chunksCounter_ = 0u;
        std::size_t trianglesCounter_ = 0u;
        std::size_t bindsCounter_ = 0u;
        std::size_t glowsCounter_ = 0u;

        void DrawFixtureGlows(PassContext& context);
    };

} // namespace cnahouse::rendering
