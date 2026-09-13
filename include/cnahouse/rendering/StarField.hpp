// SPDX-License-Identifier: MIT
#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>
#include <string_view>
#include <vector>

#include "Microsoft/Xna/Framework/Graphics/VertexPositionColor.hpp"
#include "Microsoft/Xna/Framework/Vector3.hpp"

#include "cnahouse/rendering/Renderer.hpp"
#include "cnahouse/util/Result.hpp"

namespace System::IO
{
    class Stream;
}

namespace cnahouse::rendering
{
    struct Camera;

    /// @brief One J2000 record from `CSTR` v1, retained for the later sidereal transform.
    struct StarCatalogueEntry
    {
        float rightAscensionDeg = 0.0F;
        float declinationDeg = 0.0F;
        float visualMagnitude = 0.0F;
        float bvColourIndex = 0.0F;
    };

    using StarCatalogue = std::vector<StarCatalogueEntry>;

    /// @brief Strict reader for `docs/star-catalogue-format.md`'s generated catalogue.
    class StarCatalogueReader
    {
    public:
        static constexpr std::uint32_t kMagic = 0x52545343u; // 'C','S','T','R' little-endian
        static constexpr std::uint32_t kVersion = 1u;
        static constexpr std::uint32_t kStarCount = 1500u;
        static constexpr std::uint32_t kEncodedBytes = 24016u;

        [[nodiscard]] static util::Result<StarCatalogue> Read(System::IO::Stream& stream,
                                                              std::string_view name);
        [[nodiscard]] static util::Result<StarCatalogue> ReadFromTitle(std::string_view contentPath);
    };

    struct StarAppearance
    {
        Microsoft::Xna::Framework::Vector3 colour;
        float alpha = 0.0F;
        float halfSize = 0.0F;
    };

    /// @brief Converts a J2000 catalogue position into the unrotated equatorial unit frame.
    ///
    /// +Y is the north celestial pole, +X is RA 0 and -Z is RA 6h. `HOUSE-01611` rotates this
    /// frame into the observer's horizon frame without changing catalogue or billboard geometry.
    [[nodiscard]] Microsoft::Xna::Framework::Vector3 EquatorialDirection(float rightAscensionDeg,
                                                                         float declinationDeg) noexcept;

    /// @brief Samples §34's small B-V LUT and magnitude response.
    [[nodiscard]] StarAppearance AppearanceForStar(float visualMagnitude, float bvColourIndex) noexcept;

    /// @brief Builds one four-vertex camera-facing quad for each catalogue row.
    [[nodiscard]] std::vector<Microsoft::Xna::Framework::Graphics::VertexPositionColor>
    BuildStarVertices(std::span<const StarCatalogueEntry> catalogue);

    /// @brief `Pass::Sky` component that streams and submits the complete catalogue in one draw.
    class StarField final : public IRenderPass
    {
    public:
        StarField(const Camera& camera, StarCatalogue catalogue);
        ~StarField() override;

        void Draw(PassContext& context) override;

        [[nodiscard]] bool IsActive() const override
        {
            return !catalogue_.empty();
        }

        [[nodiscard]] bool DisturbsDeviceState() const override
        {
            return true;
        }

        [[nodiscard]] const StarCatalogue& Catalogue() const noexcept
        {
            return catalogue_;
        }

        [[nodiscard]] std::span<const Microsoft::Xna::Framework::Graphics::VertexPositionColor>
        Vertices() const noexcept
        {
            return vertices_;
        }

        [[nodiscard]] std::uint64_t UploadCount() const noexcept
        {
            return uploadCount_;
        }

    private:
        class Resources;

        const Camera* camera_ = nullptr;
        StarCatalogue catalogue_;
        std::vector<Microsoft::Xna::Framework::Graphics::VertexPositionColor> vertices_;
        std::unique_ptr<Resources> resources_;
        std::uint64_t uploadCount_ = 0;
        debug::Counters* counterOwner_ = nullptr;
        std::size_t drawsCounter_ = 0;
        std::size_t trianglesCounter_ = 0;
        std::size_t uploadsCounter_ = 0;
    };
} // namespace cnahouse::rendering
