// SPDX-License-Identifier: MIT
#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "Microsoft/Xna/Framework/Graphics/Texture2D.hpp"
#include "Microsoft/Xna/Framework/Graphics/VertexPositionColor.hpp"
#include "Microsoft/Xna/Framework/Graphics/VertexPositionColorTexture.hpp"
#include "Microsoft/Xna/Framework/Matrix.hpp"
#include "Microsoft/Xna/Framework/Vector2.hpp"
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

    struct SkyGradientRow
    {
        double sunElevationDeg = 0.0;
        Microsoft::Xna::Framework::Vector3 zenith;
        Microsoft::Xna::Framework::Vector3 horizon;
    };

    /// @brief One of §31.3's three authored cloud shells.
    struct CloudLayer
    {
        std::string id;
        std::string texture;
        float radius = 0.0F;
        float scrollScale = 0.0F;
        float opacity = 0.0F;
    };

    /// @brief CPU geometry for one 48 × 8 cloud hemisphere.
    struct CloudRingMesh
    {
        std::vector<Microsoft::Xna::Framework::Graphics::VertexPositionColorTexture> vertices;
        std::vector<std::uint16_t> indices;
    };

    /// @brief One contiguous `cloudCover` band and its three §31.3 layer alphas.
    struct CloudAlphaBand
    {
        float minimumCover = 0.0F;
        float maximumCover = 0.0F;
        std::array<float, 3> alpha{};
    };

    /// @brief The compact 32-row colour model generated into `layout.sky.json`.
    struct SkyColourModel
    {
        std::vector<SkyGradientRow> gradient;
        Microsoft::Xna::Framework::Vector3 overcastGrey;
        Microsoft::Xna::Framework::Vector3 sunGlowColor;
        float sunGlowStrength = 0.0F;
        float sunGlowExponent = 0.0F;
        std::uint32_t cloudCoverSamples = 0;
        std::uint32_t azimuthOffsetSamples = 0;
        std::array<CloudLayer, 3> cloudLayers;
        std::vector<CloudAlphaBand> cloudAlphaBands;
        std::array<float, 3> stormCloudAlpha{};
    };

    /// @brief Reads only §31.2's generated colour block from `layout.sky.json`.
    class SkyColourModelReader
    {
    public:
        static constexpr std::size_t kGradientRows = 32u;
        static constexpr std::uint32_t kCloudCoverSamples = 8u;
        static constexpr std::uint32_t kAzimuthOffsetSamples = 16u;

        [[nodiscard]] static util::Result<SkyColourModel> Read(std::string_view json, std::string name);
        [[nodiscard]] static util::Result<SkyColourModel> ReadFromTitle(std::string_view contentPath);
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
    /// `HOUSE-01643` supplies the dome and `HOUSE-01644` its live colour. `HOUSE-01647` adds the
    /// three textured, wind-scrolling cloud shells after the sun. Keeping every layer inside this
    /// object is necessary because `Renderer` deliberately owns one implementation per pass;
    /// installing independent `Pass::Sky` objects would silently replace one another.
    class SkySystem final : public IRenderPass
    {
    public:
        using CloudTextures = std::array<Microsoft::Xna::Framework::Graphics::Texture2D, 3>;

        static constexpr std::uint32_t kCloudLongitudeSegments = 48u;
        static constexpr std::uint32_t kCloudLatitudeSegments = 8u;
        static constexpr std::uint32_t kCloudVerticesPerLayer = 440u;
        static constexpr std::uint32_t kCloudIndicesPerLayer = 2160u;
        static constexpr float kCloudTextureRepeatMetres = 240.0F;

        SkySystem(const Camera& camera, SkyDomeMesh mesh, SkyColourModel colourModel);
        SkySystem(const Camera& camera,
                  SkyDomeMesh mesh,
                  SkyColourModel colourModel,
                  CloudTextures cloudTextures);
        ~SkySystem() override;

        void SetSun(const environment::SunPosition& sun, double cloudCover) noexcept;
        /// @brief Sets §36.1's meteorological wind (`direction` is where it comes from).
        bool SetWind(double speedMetresPerSecond, double directionDegrees) noexcept;
        /// @brief Maps continuous cover and thunder into the three live layer alphas.
        bool SetCloudState(double cloudCover, double thunderIntensity) noexcept;
        /// @brief Advances the bounded UV offsets without requiring a graphics device.
        bool AdvanceClouds(double deltaSeconds) noexcept;
        /// @brief Recomputes and uploads colours only past §31.2's material-change thresholds.
        /// @return true when an update occurred.
        bool SetSky(double sunAltitudeDeg, double cloudCover) noexcept;
        void Draw(PassContext& context) override;

        [[nodiscard]] bool DisturbsDeviceState() const override
        {
            return true;
        }

        [[nodiscard]] const SkyDomeMesh& Mesh() const noexcept
        {
            return mesh_;
        }

        [[nodiscard]] std::span<const Microsoft::Xna::Framework::Graphics::VertexPositionColor>
        ColouredVertices() const noexcept
        {
            return colouredVertices_;
        }

        [[nodiscard]] const std::array<CloudRingMesh, 3>& CloudRings() const noexcept
        {
            return cloudRings_;
        }

        [[nodiscard]] const std::array<Microsoft::Xna::Framework::Vector2, 3>& CloudOffsets() const noexcept
        {
            return cloudOffsets_;
        }

        [[nodiscard]] const std::array<float, 3>& CloudAlphas() const noexcept
        {
            return cloudAlphas_;
        }

        [[nodiscard]] std::uint64_t ColourUpdateCount() const noexcept
        {
            return colourUpdateCount_;
        }

        [[nodiscard]] double LastColourMilliseconds() const noexcept
        {
            return lastColourMilliseconds_;
        }

        /// @brief The dome follows all three camera axes, so its 900 m shell can never be reached.
        [[nodiscard]] static Microsoft::Xna::Framework::Matrix DomeWorld(const Camera& camera) noexcept;

        /// @brief Builds §31.3's non-degenerate 48 × 8 hemispheres from the authored radii.
        [[nodiscard]] static std::array<CloudRingMesh, 3>
        BuildCloudRings(const std::array<CloudLayer, 3>& layers);

        /// @brief Pure form of §31.3's continuous cover/thunder interpolation.
        [[nodiscard]] static std::array<float, 3>
        CloudAlphasFor(const SkyColourModel& model, double cloudCover, double thunderIntensity) noexcept;

    private:
        class Resources;

        void RecomputeColours(double sunAltitudeDeg, double cloudCover) noexcept;

        const Camera* camera_ = nullptr;
        SkyDomeMesh mesh_;
        SkyColourModel colourModel_;
        std::vector<Microsoft::Xna::Framework::Graphics::VertexPositionColor> colouredVertices_;
        std::array<CloudRingMesh, 3> cloudRings_;
        std::array<Microsoft::Xna::Framework::Vector2, 3> cloudOffsets_{};
        std::array<float, 3> cloudAlphas_{};
        std::optional<CloudTextures> cloudTextures_;
        SunDiscPass sunDisc_;
        std::unique_ptr<Resources> resources_;
        double windSpeedMetresPerSecond_ = 0.0;
        double windDirectionDegrees_ = 0.0;
        double lastSunAltitudeDeg_ = 0.0;
        double lastCloudCover_ = 0.0;
        bool hasColourState_ = false;
        std::uint64_t colourUpdateCount_ = 0;
        double lastColourMilliseconds_ = 0.0;
        debug::Counters* counterOwner_ = nullptr;
        std::size_t drawsCounter_ = 0;
        std::size_t trianglesCounter_ = 0;
        std::size_t colourUpdatesCounter_ = 0;
        std::size_t colourMicrosCounter_ = 0;
        std::size_t cloudDrawsCounter_ = 0;
        std::size_t cloudTrianglesCounter_ = 0;
        std::size_t cloudUploadsCounter_ = 0;
        std::size_t cloudAlphaUpdatesCounter_ = 0;
        std::uint64_t cloudUploadCount_ = 0;
        std::uint64_t cloudAlphaUpdateCount_ = 0;
    };

} // namespace cnahouse::rendering
