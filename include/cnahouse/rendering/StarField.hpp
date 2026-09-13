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

namespace cnahouse::environment
{
    struct MoonPhase;
    struct MoonPosition;
    struct SimClock;
    struct SunPosition;
} // namespace cnahouse::environment

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

    /// @brief Observer-dependent rotation of the equatorial catalogue into the local sky.
    struct StarOrientation
    {
        double localSiderealTimeDeg = 0.0;
        double latitudeDeg = 0.0;
    };

    /// @brief Section 34's environmental attenuation and twilight limiting magnitude.
    struct StarVisibility
    {
        float twilight = 1.0F;
        float cloudTransmission = 1.0F;
        float moonBrightness = 0.0F;
        float overallAlpha = 1.0F;
        float magnitudeCutoff = 5.5F;
    };

    /// @brief Section 34's authored town glow, shared by the dome and star suppression.
    struct StarLightPollution
    {
        Microsoft::Xna::Framework::Vector3 colour;
        float strength = 0.0F;
        float townAzimuthDeg = 180.0F;
        float azimuthExponent = 4.0F;
        float altitudeExponent = 3.0F;
        float starMagnitudeLoss = 0.0F;
    };

    inline constexpr double kStarCloudExponent = 1.6;
    inline constexpr double kStarMoonSuppression = 0.55;
    inline constexpr double kStarTwinkleStepSeconds = 1.0 / 20.0;
    inline constexpr float kStarZenithTwinkleAmplitude = 0.04F;
    inline constexpr float kStarMaximumTwinkleAmplitude = 0.20F;

    /// @brief Converts a J2000 catalogue position into the unrotated equatorial unit frame.
    ///
    /// +Y is the north celestial pole, +X is RA 0 and -Z is RA 6h. `HOUSE-01611` rotates this
    /// frame into the observer's horizon frame without changing catalogue or billboard geometry.
    [[nodiscard]] Microsoft::Xna::Framework::Vector3 EquatorialDirection(float rightAscensionDeg,
                                                                         float declinationDeg) noexcept;

    /// @brief Derives the field's earth rotation from §35's one simulation clock.
    [[nodiscard]] StarOrientation StarOrientationFor(const environment::SimClock& clock) noexcept;

    /// @brief Converts one catalogue position to world axes: +X east, +Y up, -Z north.
    [[nodiscard]] Microsoft::Xna::Framework::Vector3
    HorizonDirection(const StarCatalogueEntry& star, const StarOrientation& orientation) noexcept;

    /// @brief Evaluates §34's twilight, cloud, moon and limiting-magnitude rules.
    ///
    /// Moon brightness reuses the clear-sky phase/altitude response of §33.4. The caller supplies
    /// the retained catalogue's magnitude endpoints so any future regenerated catalogue still
    /// reveals exactly its brightest-to-faintest range.
    [[nodiscard]] StarVisibility StarVisibilityFor(const environment::SunPosition& sun,
                                                   const environment::MoonPosition& moon,
                                                   const environment::MoonPhase& phase,
                                                   double cloudCover,
                                                   float brightestMagnitude,
                                                   float faintestMagnitude) noexcept;

    /// @brief Section 34's bounded `1/sin(altitude)` twinkle amplitude.
    [[nodiscard]] float StarTwinkleAmplitude(float altitudeSine) noexcept;

    /// @brief Deterministic per-star sinusoid sampled at the field's 20 Hz tick.
    [[nodiscard]] float
    StarTwinkleFactor(std::size_t starIndex, float altitudeSine, std::uint64_t sampleTick) noexcept;

    /// @brief Directional low-horizon lobe: one toward the town, zero away or at zenith.
    [[nodiscard]] float StarLightPollutionFactor(const Microsoft::Xna::Framework::Vector3& direction,
                                                 const StarLightPollution& pollution) noexcept;

    /// @brief Tightens the local limiting magnitude inside the town glow.
    [[nodiscard]] float StarMagnitudeCutoffWithPollution(float baseCutoff,
                                                         const Microsoft::Xna::Framework::Vector3& direction,
                                                         const StarLightPollution& pollution) noexcept;

    /// @brief Samples §34's small B-V LUT and magnitude response.
    [[nodiscard]] StarAppearance AppearanceForStar(float visualMagnitude, float bvColourIndex) noexcept;

    /// @brief Builds one four-vertex camera-facing quad for each catalogue row.
    [[nodiscard]] std::vector<Microsoft::Xna::Framework::Graphics::VertexPositionColor>
    BuildStarVertices(std::span<const StarCatalogueEntry> catalogue);

    /// @brief Builds the same billboards after applying local sidereal time and latitude.
    [[nodiscard]] std::vector<Microsoft::Xna::Framework::Graphics::VertexPositionColor>
    BuildStarVertices(std::span<const StarCatalogueEntry> catalogue, const StarOrientation& orientation);

    /// @brief `Pass::Sky` component that streams and submits the complete catalogue in one draw.
    class StarField final : public IRenderPass
    {
    public:
        StarField(const Camera& camera, StarCatalogue catalogue);
        StarField(const Camera& camera, StarCatalogue catalogue, StarLightPollution lightPollution);
        ~StarField() override;

        /// @brief Rotates the retained catalogue to the clock's current local horizon frame.
        /// @return true when the geometry changed; invalid clock/location data leaves it unchanged.
        bool SetObserver(const environment::SimClock& clock) noexcept;

        /// @brief Applies §34's environmental visibility without changing sidereal orientation.
        bool SetVisibility(const environment::SunPosition& sun,
                           const environment::MoonPosition& moon,
                           const environment::MoonPhase& phase,
                           double cloudCover) noexcept;

        /// @brief Applies one frame's shared celestial state, rebuilding the retained allocation once.
        bool SetCelestial(const environment::SimClock& clock,
                          const environment::SunPosition& sun,
                          const environment::MoonPosition& moon,
                          const environment::MoonPhase& phase,
                          double cloudCover) noexcept;

        /// @brief Advances the deterministic twinkle sampler and rebuilds only on a 20 Hz boundary.
        bool AdvanceTwinkle(double deltaSeconds) noexcept;
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

        [[nodiscard]] const StarOrientation& Orientation() const noexcept
        {
            return orientation_;
        }

        [[nodiscard]] const StarVisibility& Visibility() const noexcept
        {
            return visibility_;
        }

        [[nodiscard]] std::size_t VisibleStarCount() const noexcept
        {
            return visibleStarCount_;
        }

        [[nodiscard]] std::uint64_t GeometryUpdateCount() const noexcept
        {
            return geometryUpdateCount_;
        }

        [[nodiscard]] std::uint64_t TwinkleSampleTick() const noexcept
        {
            return twinkleSampleTick_;
        }

    private:
        class Resources;

        bool ApplyState(const StarOrientation& orientation, const StarVisibility& visibility) noexcept;

        const Camera* camera_ = nullptr;
        StarCatalogue catalogue_;
        std::vector<Microsoft::Xna::Framework::Graphics::VertexPositionColor> vertices_;
        StarOrientation orientation_;
        StarVisibility visibility_;
        StarLightPollution lightPollution_;
        std::unique_ptr<Resources> resources_;
        std::size_t visibleStarCount_ = 0;
        double twinkleAccumulatorSeconds_ = 0.0;
        std::uint64_t twinkleSampleTick_ = 0;
        std::uint64_t geometryUpdateCount_ = 0;
        std::uint64_t uploadCount_ = 0;
        debug::Counters* counterOwner_ = nullptr;
        std::size_t drawsCounter_ = 0;
        std::size_t trianglesCounter_ = 0;
        std::size_t uploadsCounter_ = 0;
        std::size_t twinkleUpdatesCounter_ = 0;
    };
} // namespace cnahouse::rendering
