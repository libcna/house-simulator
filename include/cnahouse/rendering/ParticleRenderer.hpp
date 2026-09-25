// SPDX-License-Identifier: MIT
#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>

#include "Microsoft/Xna/Framework/Color.hpp"
#include "Microsoft/Xna/Framework/Graphics/VertexPositionColorTexture.hpp"
#include "Microsoft/Xna/Framework/Vector2.hpp"
#include "Microsoft/Xna/Framework/Vector3.hpp"

#include "cnahouse/rendering/Camera.hpp"
#include "cnahouse/rendering/Quality.hpp"
#include "cnahouse/rendering/Renderer.hpp"

namespace Microsoft::Xna::Framework::Graphics
{
    class Texture2D;
}

namespace cnahouse::rendering
{
    inline constexpr std::size_t kMaximumParticleQuads = 2000u;
    inline constexpr std::size_t kMaximumParticleMaterials = 6u;

    /// @brief The fixed-pool particle budget selected by each existing graphics preset.
    [[nodiscard]] std::size_t ParticleLimitFor(ParticleQuality quality) noexcept;

    /// @brief One world-space quad submitted to the shared particle stream.
    struct ParticleQuad
    {
        Microsoft::Xna::Framework::Vector3 centre;
        /// World-space direction of the long edge. Zero keeps the ordinary screen-up billboard.
        Microsoft::Xna::Framework::Vector3 elongationAxis;
        Microsoft::Xna::Framework::Vector2 halfSize;
        Microsoft::Xna::Framework::Color colour;
        std::uint8_t material = 0u;
    };

    /// @brief Expands a particle to a camera-facing, textured two-triangle quad.
    [[nodiscard]] std::array<Microsoft::Xna::Framework::Graphics::VertexPositionColorTexture, 4>
    ParticleBillboardVertices(const ParticleQuad& particle, const Camera& camera) noexcept;

    /// @brief Fixed-capacity XNA particle stream shared by the retained weather effects.
    ///
    /// Simulation submits small `ParticleQuad` records into an in-object array. `Draw` groups them
    /// into a second fixed array, uploads one `DynamicVertexBuffer` with `Discard`, then issues one
    /// indexed draw for each used material. Materials and the camera are borrowed; GPU resources
    /// are created once on first use and retained. No frame operation grows a container.
    class ParticleRenderer
    {
    public:
        explicit ParticleRenderer(const Camera& camera) noexcept;
        ~ParticleRenderer();

        ParticleRenderer(const ParticleRenderer&) = delete;
        ParticleRenderer& operator=(const ParticleRenderer&) = delete;

        /// @brief Clears the fixed pool and selects the preset's submission ceiling.
        void BeginFrame(ParticleQuality quality) noexcept;

        /// @brief Borrows one texture for a stable material slot; null unregisters the slot.
        [[nodiscard]] bool SetMaterial(std::size_t material,
                                       Microsoft::Xna::Framework::Graphics::Texture2D* texture) noexcept;

        /// @return false for invalid geometry/material data or when the selected preset is full.
        [[nodiscard]] bool Submit(const ParticleQuad& particle) noexcept;

        void Draw(PassContext& context);

        [[nodiscard]] bool IsActive() const noexcept
        {
            return particleCount_ != 0u;
        }

        [[nodiscard]] std::span<const ParticleQuad> Particles() const noexcept
        {
            return std::span(particles_.data(), particleCount_);
        }

        [[nodiscard]] std::size_t Capacity() const noexcept
        {
            return particleLimit_;
        }

        [[nodiscard]] std::uint32_t LastDrawCount() const noexcept
        {
            return lastDrawCount_;
        }

        [[nodiscard]] std::uint64_t UploadCount() const noexcept
        {
            return uploadCount_;
        }

        [[nodiscard]] std::uint32_t RejectedCount() const noexcept
        {
            return rejectedCount_;
        }

    private:
        class Resources;

        const Camera* camera_ = nullptr;
        std::array<Microsoft::Xna::Framework::Graphics::Texture2D*, kMaximumParticleMaterials> materials_{};
        std::array<ParticleQuad, kMaximumParticleQuads> particles_{};
        std::array<Microsoft::Xna::Framework::Graphics::VertexPositionColorTexture,
                   kMaximumParticleQuads * 4u>
            vertices_{};
        std::array<std::size_t, kMaximumParticleMaterials> firstQuadByMaterial_{};
        std::array<std::size_t, kMaximumParticleMaterials> quadCountByMaterial_{};
        std::size_t particleCount_ = 0u;
        std::size_t particleLimit_ = kMaximumParticleQuads;
        std::size_t drawableQuadCount_ = 0u;
        std::unique_ptr<Resources> resources_;
        std::uint32_t lastDrawCount_ = 0u;
        std::uint32_t rejectedCount_ = 0u;
        std::uint64_t uploadCount_ = 0u;
        debug::Counters* counterOwner_ = nullptr;
        std::size_t drawsCounter_ = 0u;
        std::size_t quadsCounter_ = 0u;
        std::size_t uploadsCounter_ = 0u;
        std::size_t rejectedCounter_ = 0u;

        void BuildVertices() noexcept;
    };
} // namespace cnahouse::rendering
