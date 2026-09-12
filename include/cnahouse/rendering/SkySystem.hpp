// SPDX-License-Identifier: MIT
#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string_view>
#include <vector>

#include "Microsoft/Xna/Framework/Matrix.hpp"
#include "Microsoft/Xna/Framework/Vector3.hpp"

#include "cnahouse/rendering/Renderer.hpp"
#include "cnahouse/rendering/SunDiscPass.hpp"
#include "cnahouse/util/Result.hpp"

namespace System::IO
{
    class Stream;
}

namespace cnahouse::environment
{
    struct SunPosition;
}

namespace cnahouse::rendering
{

    struct Camera;

    /// @brief CPU-side contents of `docs/sky-dome-format.md`'s `CSKY` v1 file.
    ///
    /// Positions remain resident after upload because `HOUSE-01644` colours them from altitude
    /// whenever the sky state changes. Indices never change.
    struct SkyDomeMesh
    {
        std::uint32_t longitudeSegments = 0;
        std::uint32_t latitudeSegments = 0;
        std::uint32_t domeVertexCount = 0;
        float radius = 0.0F;
        float skirtDepth = 0.0F;
        std::vector<Microsoft::Xna::Framework::Vector3> positions;
        std::vector<std::uint16_t> indices;
    };

    /// @brief Strict reader for the generated `CSKY` v1 sky dome.
    class SkyDomeReader
    {
    public:
        static constexpr std::uint32_t kMagic = 0x594B5343u; // 'C','S','K','Y' little-endian
        static constexpr std::uint32_t kVersion = 1u;
        static constexpr std::uint32_t kLongitudeSegments = 32u;
        static constexpr std::uint32_t kLatitudeSegments = 18u;
        static constexpr std::uint32_t kDomeVertexCount = 577u;
        static constexpr std::uint32_t kVertexCount = 610u;
        static constexpr std::uint32_t kIndexCount = 3648u;
        static constexpr float kRadius = 900.0F;
        static constexpr float kSkirtDepth = 90.0F;
        static constexpr std::uint32_t kEncodedBytes = 14656u;

        [[nodiscard]] static util::Result<SkyDomeMesh> Read(System::IO::Stream& stream,
                                                            std::string_view name);
        [[nodiscard]] static util::Result<SkyDomeMesh> ReadFromTitle(std::string_view contentPath);
    };

    /// @brief §31's complete `Pass::Sky` owner: camera-following dome, then celestial overlays.
    ///
    /// `HOUSE-01643` supplies the geometry and its bootstrap colour. `HOUSE-01644` replaces that
    /// colour from the generated LUT only when the sky state changes materially. Keeping the sun
    /// pass inside this object is necessary because `Renderer` deliberately owns one implementation
    /// per pass; installing two independent `Pass::Sky` objects would silently replace the first.
    class SkySystem final : public IRenderPass
    {
    public:
        SkySystem(const Camera& camera, SkyDomeMesh mesh);
        ~SkySystem() override;

        void SetSun(const environment::SunPosition& sun, double cloudCover) noexcept;
        void Draw(PassContext& context) override;

        [[nodiscard]] bool DisturbsDeviceState() const override
        {
            return true;
        }

        [[nodiscard]] const SkyDomeMesh& Mesh() const noexcept
        {
            return mesh_;
        }

        /// @brief The dome follows all three camera axes, so its 900 m shell can never be reached.
        [[nodiscard]] static Microsoft::Xna::Framework::Matrix DomeWorld(const Camera& camera) noexcept;

    private:
        class Resources;

        const Camera* camera_ = nullptr;
        SkyDomeMesh mesh_;
        SunDiscPass sunDisc_;
        std::unique_ptr<Resources> resources_;
        debug::Counters* counterOwner_ = nullptr;
        std::size_t drawsCounter_ = 0;
        std::size_t trianglesCounter_ = 0;
    };

} // namespace cnahouse::rendering
