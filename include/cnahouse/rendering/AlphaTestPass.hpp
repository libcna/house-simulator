// SPDX-License-Identifier: MIT
#pragma once

#include <cstdint>
#include <functional>
#include <string_view>

#include "cnahouse/rendering/Camera.hpp"
#include "cnahouse/rendering/Renderer.hpp"
#include "cnahouse/visibility/RenderList.hpp"

namespace Microsoft::Xna::Framework::Graphics
{
    class Texture2D;
} // namespace Microsoft::Xna::Framework::Graphics

namespace cnahouse::world
{
    class CellRuntime;
    class WorldData;
    struct ChunkLibrary;
} // namespace cnahouse::world

namespace cnahouse::lighting
{
    class LightingSystem;
    struct ObjectLightAssignment;
} // namespace cnahouse::lighting

namespace cnahouse::rendering
{
    class MaterialBinder;
    struct FogParams;

    /// @brief Share of the sun/moon key a cutout crown receives on average: its leaves face every
    ///        way, and about half of them face the light.
    inline constexpr float kFoliageKeyShare = 0.5F;

    /// @brief Stock-AlphaTestEffect colour for cutout foliage in a sky-open exterior cell.
    ///
    /// XNA's `AlphaTestEffect` has no lights, so `DiffuseColor` is all the light a leaf gets.
    /// Scaling it by the camera exposure alone kept every crown at noon brightness all night
    /// (`HOUSE-03631`). This is the light a BasicEffect receiver in the same cell gets -- the
    /// ambient floor, the open sky, a crown's average share of the sun/moon key, and the active
    /// fixtures' key and spill -- at the exterior's unit exposure, clamped to one per channel.
    [[nodiscard]] Microsoft::Xna::Framework::Vector3
    OutdoorFoliageMultiplier(const Microsoft::Xna::Framework::Vector3& skyAmbient,
                             const Microsoft::Xna::Framework::Vector3& celestialKey,
                             const lighting::ObjectLightAssignment& fixtures) noexcept;

    /// @brief §23.6's alpha-tested static pass, before transparency with full depth writes.
    ///
    /// `RenderList` has already grouped this slice by effect and material. One binder call and one
    /// texture-cache lookup therefore cover a complete material run, while every surviving texel
    /// writes normal opaque depth through `DepthStencilState::Default`. The stock
    /// `AlphaTestEffect` and its exact authored cutoff come from `MaterialBinder` (`HOUSE-00894`).
    class AlphaTestPass final : public IRenderPass
    {
    public:
        using TextureLookup =
            std::function<Microsoft::Xna::Framework::Graphics::Texture2D*(std::string_view contentName)>;

        /// @brief Borrows scene data and @p binder; owns the texture-lookup callable.
        AlphaTestPass(const world::ChunkLibrary& library,
                      const world::CellRuntime& cells,
                      const world::WorldData& world,
                      const Camera& camera,
                      visibility::RenderList& list,
                      MaterialBinder& binder,
                      TextureLookup textures,
                      const lighting::LightingSystem* lighting = nullptr,
                      const FogParams* exteriorFog = nullptr);
        ~AlphaTestPass() override;

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
        MaterialBinder& binder_;
        TextureLookup textures_;
        const lighting::LightingSystem* lighting_ = nullptr;
        const FogParams* exteriorFog_ = nullptr;
        std::uint32_t chunksDrawn_ = 0u;
        std::uint32_t trianglesDrawn_ = 0u;
        std::uint32_t materialBinds_ = 0u;
    };

} // namespace cnahouse::rendering
