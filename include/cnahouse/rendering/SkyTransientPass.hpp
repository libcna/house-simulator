// SPDX-License-Identifier: MIT
#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>

#include "Microsoft/Xna/Framework/Graphics/VertexPositionColor.hpp"
#include "Microsoft/Xna/Framework/Vector3.hpp"

#include "cnahouse/rendering/Renderer.hpp"

namespace cnahouse::environment
{
    struct SimClock;
    struct SunPosition;
} // namespace cnahouse::environment

namespace cnahouse::rendering
{
    struct Camera;

    inline constexpr std::size_t kVisibleSatelliteCount = 2u;
    inline constexpr double kMeteorIntervalSimSeconds = 4.0 * 60.0;
    inline constexpr double kMeteorDurationSimSeconds = 8.0;
    inline constexpr double kMeteorMaximumCloudCover = 0.25;

    struct SatelliteSample
    {
        Microsoft::Xna::Framework::Vector3 direction;
        float alpha = 0.0F;
    };

    struct MeteorSample
    {
        Microsoft::Xna::Framework::Vector3 headDirection;
        Microsoft::Xna::Framework::Vector3 tailDirection;
        float alpha = 0.0F;
        std::int64_t eventIndex = 0;
        bool active = false;
    };

    /// @brief One deterministic sample of §34's moving night-sky objects.
    struct SkyTransientFrame
    {
        std::array<SatelliteSample, kVisibleSatelliteCount> satellites{};
        MeteorSample meteor;
        float nightTransmission = 0.0F;
    };

    /// @brief Samples two looping satellite tracks and the four-simulated-minute meteor cadence.
    [[nodiscard]] SkyTransientFrame SkyTransientFrameFor(const environment::SimClock& clock,
                                                         const environment::SunPosition& sun,
                                                         double cloudCover) noexcept;

    /// @brief XNA-only additive pass for the two satellite points and optional meteor streak.
    class SkyTransientPass final : public IRenderPass
    {
    public:
        explicit SkyTransientPass(const Camera& camera) noexcept;
        ~SkyTransientPass() override;

        /// @return false only when the shared clock or environment input is non-finite.
        bool SetCelestial(const environment::SimClock& clock,
                          const environment::SunPosition& sun,
                          double cloudCover) noexcept;

        void Draw(PassContext& context) override;

        [[nodiscard]] bool IsActive() const override
        {
            return primitiveCount_ != 0u;
        }

        [[nodiscard]] bool DisturbsDeviceState() const override
        {
            return true;
        }

        [[nodiscard]] const SkyTransientFrame& Frame() const noexcept
        {
            return frame_;
        }

        [[nodiscard]] std::span<const Microsoft::Xna::Framework::Graphics::VertexPositionColor>
        Vertices() const noexcept
        {
            return std::span(vertices_.data(), vertexCount_);
        }

        [[nodiscard]] std::size_t PrimitiveCount() const noexcept
        {
            return primitiveCount_;
        }

        [[nodiscard]] std::uint64_t UploadCount() const noexcept
        {
            return uploadCount_;
        }

    private:
        class Resources;

        void RebuildVertices() noexcept;

        const Camera* camera_ = nullptr;
        SkyTransientFrame frame_;
        std::array<Microsoft::Xna::Framework::Graphics::VertexPositionColor, 12> vertices_{};
        std::size_t vertexCount_ = 0;
        std::size_t primitiveCount_ = 0;
        std::unique_ptr<Resources> resources_;
        std::uint64_t uploadCount_ = 0;
        debug::Counters* counterOwner_ = nullptr;
        std::size_t drawsCounter_ = 0;
        std::size_t trianglesCounter_ = 0;
        std::size_t uploadsCounter_ = 0;
        std::size_t satellitesCounter_ = 0;
        std::size_t meteorsCounter_ = 0;
    };
} // namespace cnahouse::rendering
