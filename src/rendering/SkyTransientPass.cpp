// SPDX-License-Identifier: MIT
#include "cnahouse/rendering/SkyTransientPass.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <numbers>
#include <vector>

#include "Microsoft/Xna/Framework/Color.hpp"
#include "Microsoft/Xna/Framework/Graphics/BasicEffect.hpp"
#include "Microsoft/Xna/Framework/Graphics/BlendState.hpp"
#include "Microsoft/Xna/Framework/Graphics/BufferUsage.hpp"
#include "Microsoft/Xna/Framework/Graphics/DepthStencilState.hpp"
#include "Microsoft/Xna/Framework/Graphics/DynamicVertexBuffer.hpp"
#include "Microsoft/Xna/Framework/Graphics/EffectPass.hpp"
#include "Microsoft/Xna/Framework/Graphics/EffectPassCollection.hpp"
#include "Microsoft/Xna/Framework/Graphics/EffectTechnique.hpp"
#include "Microsoft/Xna/Framework/Graphics/GraphicsDevice.hpp"
#include "Microsoft/Xna/Framework/Graphics/IndexBuffer.hpp"
#include "Microsoft/Xna/Framework/Graphics/IndexElementSize.hpp"
#include "Microsoft/Xna/Framework/Graphics/PrimitiveType.hpp"
#include "Microsoft/Xna/Framework/Graphics/SetDataOptions.hpp"
#include "Microsoft/Xna/Framework/Graphics/Viewport.hpp"
#include "Microsoft/Xna/Framework/MathHelper.hpp"
#include "Microsoft/Xna/Framework/Matrix.hpp"
#include "Microsoft/Xna/Framework/Vector4.hpp"

#include "cnahouse/debug/Counters.hpp"
#include "cnahouse/environment/SimClock.hpp"
#include "cnahouse/environment/SunLight.hpp"
#include "cnahouse/environment/SunModel.hpp"
#include "cnahouse/rendering/Camera.hpp"
#include "cnahouse/rendering/RenderStates.hpp"
#include "cnahouse/rendering/StateTracker.hpp"

namespace cnahouse::rendering
{
    namespace Xna = Microsoft::Xna::Framework;
    namespace Gfx = Microsoft::Xna::Framework::Graphics;

    namespace
    {
        constexpr float kDistance = 875.0F;
        constexpr float kSkyFarPlane = 1000.0F;
        constexpr float kSatelliteHalfSize = 0.48F;
        constexpr float kMeteorHalfWidth = 0.42F;
        constexpr double kVectorEpsilonSquared = 1.0e-12;

        struct SatelliteTrack
        {
            double periodSeconds;
            double phaseOffset;
            double startAzimuthDeg;
            double azimuthTravelDeg;
            double maximumAltitudeDeg;
            float brightness;
        };

        constexpr std::array<SatelliteTrack, kVisibleSatelliteCount> kSatelliteTracks{{
            {420.0, 0.19, 248.0, -142.0, 68.0, 0.82F},
            {660.0, 0.57, 322.0, 171.0, 56.0, 0.68F},
        }};

        [[nodiscard]] double PositiveFraction(double value) noexcept
        {
            return value - std::floor(value);
        }

        [[nodiscard]] float Smoothstep(float value) noexcept
        {
            const float t = std::clamp(value, 0.0F, 1.0F);
            return t * t * (3.0F - 2.0F * t);
        }

        [[nodiscard]] double Dot(const Xna::Vector3& a, const Xna::Vector3& b) noexcept
        {
            return static_cast<double>(a.X) * static_cast<double>(b.X) +
                   static_cast<double>(a.Y) * static_cast<double>(b.Y) +
                   static_cast<double>(a.Z) * static_cast<double>(b.Z);
        }

        [[nodiscard]] Xna::Vector3 Scaled(const Xna::Vector3& value, float scale) noexcept
        {
            return Xna::Vector3(value.X * scale, value.Y * scale, value.Z * scale);
        }

        [[nodiscard]] Xna::Vector3 Add(const Xna::Vector3& a, const Xna::Vector3& b) noexcept
        {
            return Xna::Vector3(a.X + b.X, a.Y + b.Y, a.Z + b.Z);
        }

        [[nodiscard]] Xna::Vector3 Subtract(const Xna::Vector3& a, const Xna::Vector3& b) noexcept
        {
            return Xna::Vector3(a.X - b.X, a.Y - b.Y, a.Z - b.Z);
        }

        [[nodiscard]] Xna::Vector3 Cross(const Xna::Vector3& a, const Xna::Vector3& b) noexcept
        {
            return Xna::Vector3(a.Y * b.Z - a.Z * b.Y, a.Z * b.X - a.X * b.Z, a.X * b.Y - a.Y * b.X);
        }

        [[nodiscard]] Xna::Vector3 Normalised(const Xna::Vector3& value,
                                              const Xna::Vector3& fallback) noexcept
        {
            const double lengthSquared = Dot(value, value);
            if (!std::isfinite(lengthSquared) || lengthSquared <= kVectorEpsilonSquared)
            {
                return fallback;
            }
            return Scaled(value, static_cast<float>(1.0 / std::sqrt(lengthSquared)));
        }

        [[nodiscard]] Xna::Vector3 SkyDirection(double azimuthDeg, double altitudeDeg) noexcept
        {
            const double azimuth = azimuthDeg * std::numbers::pi / 180.0;
            const double altitude = altitudeDeg * std::numbers::pi / 180.0;
            const double horizontal = std::cos(altitude);
            return Xna::Vector3(static_cast<float>(horizontal * std::sin(azimuth)),
                                static_cast<float>(std::sin(altitude)),
                                static_cast<float>(-horizontal * std::cos(azimuth)));
        }

        [[nodiscard]] std::uint32_t HashEvent(std::int64_t eventIndex) noexcept
        {
            std::uint32_t hash = static_cast<std::uint32_t>(eventIndex) ^ 0x9E3779B9u;
            hash ^= hash >> 16u;
            hash *= 0x7FEB352Du;
            hash ^= hash >> 15u;
            hash *= 0x846CA68Bu;
            hash ^= hash >> 16u;
            return hash;
        }

        [[nodiscard]] double HashUnit(std::uint32_t hash, unsigned shift) noexcept
        {
            return static_cast<double>((hash >> shift) & 0xFFu) / 255.0;
        }

        void AppendPoint(std::array<Gfx::VertexPositionColor, 12>& vertices,
                         std::size_t& count,
                         const SatelliteSample& sample) noexcept
        {
            if (!(sample.alpha > 0.0F) || count + 4u > vertices.size())
            {
                return;
            }
            const Xna::Vector3 centre = Scaled(sample.direction, kDistance);
            Xna::Vector3 right =
                Normalised(Cross(Xna::Vector3::Up, sample.direction), Xna::Vector3(1.0F, 0.0F, 0.0F));
            const Xna::Vector3 up = Normalised(Cross(sample.direction, right), Xna::Vector3::Up);
            right = Scaled(right, kSatelliteHalfSize);
            const Xna::Vector3 vertical = Scaled(up, kSatelliteHalfSize);
            const Xna::Color colour(Xna::Vector4(0.82F, 0.90F, 1.0F, sample.alpha));
            vertices[count++] = Gfx::VertexPositionColor(
                Add(Add(centre, Scaled(right, -1.0F)), Scaled(vertical, -1.0F)), colour);
            vertices[count++] =
                Gfx::VertexPositionColor(Add(Add(centre, right), Scaled(vertical, -1.0F)), colour);
            vertices[count++] = Gfx::VertexPositionColor(Add(Add(centre, right), vertical), colour);
            vertices[count++] =
                Gfx::VertexPositionColor(Add(Add(centre, Scaled(right, -1.0F)), vertical), colour);
        }

        void AppendMeteor(std::array<Gfx::VertexPositionColor, 12>& vertices,
                          std::size_t& count,
                          const MeteorSample& sample) noexcept
        {
            if (!sample.active || !(sample.alpha > 0.0F) || count + 4u > vertices.size())
            {
                return;
            }
            const Xna::Vector3 head = Scaled(sample.headDirection, kDistance);
            const Xna::Vector3 tail = Scaled(sample.tailDirection, kDistance);
            const Xna::Vector3 along = Subtract(head, tail);
            const Xna::Vector3 radial =
                Normalised(Add(sample.headDirection, sample.tailDirection), sample.headDirection);
            const Xna::Vector3 side =
                Scaled(Normalised(Cross(radial, along), Xna::Vector3(1.0F, 0.0F, 0.0F)), kMeteorHalfWidth);
            const Xna::Color tailColour(Xna::Vector4(0.35F, 0.55F, 1.0F, 0.0F));
            const Xna::Color headColour(Xna::Vector4(0.85F, 0.93F, 1.0F, sample.alpha));
            vertices[count++] = Gfx::VertexPositionColor(Add(tail, Scaled(side, -1.0F)), tailColour);
            vertices[count++] = Gfx::VertexPositionColor(Add(tail, side), tailColour);
            vertices[count++] = Gfx::VertexPositionColor(Add(head, side), headColour);
            vertices[count++] = Gfx::VertexPositionColor(Add(head, Scaled(side, -1.0F)), headColour);
        }
    } // namespace

    SkyTransientFrame SkyTransientFrameFor(const environment::SimClock& clock,
                                           const environment::SunPosition& sun,
                                           double cloudCover) noexcept
    {
        SkyTransientFrame frame;
        if (!std::isfinite(clock.epochSeconds) || !std::isfinite(sun.altitudeDeg) ||
            !std::isfinite(cloudCover))
        {
            return frame;
        }

        const double boundedCover = std::clamp(cloudCover, 0.0, 1.0);
        const float twilight = static_cast<float>(environment::StarVisibilityForSunAltitude(sun.altitudeDeg));
        const float cloudTransmission = static_cast<float>(std::pow(1.0 - boundedCover, 1.6));
        frame.nightTransmission = twilight * cloudTransmission;

        for (std::size_t i = 0; i < frame.satellites.size(); ++i)
        {
            const SatelliteTrack& track = kSatelliteTracks[i];
            const double phase =
                PositiveFraction(clock.epochSeconds / track.periodSeconds + track.phaseOffset);
            const double altitude = std::sin(std::numbers::pi * phase) * track.maximumAltitudeDeg;
            const double azimuth = track.startAzimuthDeg + track.azimuthTravelDeg * phase;
            const float edgeFade = Smoothstep(static_cast<float>(phase / 0.08)) *
                                   Smoothstep(static_cast<float>((1.0 - phase) / 0.08));
            frame.satellites[i].direction = SkyDirection(azimuth, altitude);
            frame.satellites[i].alpha = frame.nightTransmission * edgeFade * track.brightness;
        }

        const double eventNumber = std::floor(clock.epochSeconds / kMeteorIntervalSimSeconds);
        const double eventStart = eventNumber * kMeteorIntervalSimSeconds;
        const double eventAge = clock.epochSeconds - eventStart;
        frame.meteor.eventIndex = static_cast<std::int64_t>(eventNumber);
        if (sun.altitudeDeg <= environment::kStarsFullAtSunAltitudeDeg &&
            boundedCover <= kMeteorMaximumCloudCover && eventAge > 0.0 &&
            eventAge < kMeteorDurationSimSeconds)
        {
            const double progress = eventAge / kMeteorDurationSimSeconds;
            const double tailProgress = std::max(0.0, progress - 0.18);
            const std::uint32_t hash = HashEvent(frame.meteor.eventIndex);
            const double startAzimuth = 20.0 + 320.0 * HashUnit(hash, 0u);
            const double startAltitude = 38.0 + 34.0 * HashUnit(hash, 8u);
            const double azimuthSign = (hash & 0x10000u) == 0u ? -1.0 : 1.0;
            const double azimuthTravel = azimuthSign * (18.0 + 22.0 * HashUnit(hash, 17u));
            const double altitudeTravel = -(10.0 + 16.0 * HashUnit(hash, 24u));
            frame.meteor.headDirection = SkyDirection(startAzimuth + azimuthTravel * progress,
                                                      startAltitude + altitudeTravel * progress);
            frame.meteor.tailDirection = SkyDirection(startAzimuth + azimuthTravel * tailProgress,
                                                      startAltitude + altitudeTravel * tailProgress);
            frame.meteor.alpha =
                frame.nightTransmission * static_cast<float>(std::sin(std::numbers::pi * progress));
            frame.meteor.active = frame.meteor.alpha > 0.0F;
        }
        return frame;
    }

    class SkyTransientPass::Resources
    {
    public:
        explicit Resources(Gfx::GraphicsDevice& device)
            : vertices(device,
                       Gfx::VertexPositionColor::getVertexDeclarationStatic(),
                       12,
                       Gfx::BufferUsage::WriteOnly)
            , indices(device, Gfx::IndexElementSize::SixteenBits, 18, Gfx::BufferUsage::WriteOnly)
            , effect(device)
        {
            std::array<std::uint16_t, 18> data{};
            for (std::uint16_t primitive = 0; primitive < 3u; ++primitive)
            {
                const std::uint16_t base = static_cast<std::uint16_t>(primitive * 4u);
                const std::size_t index = static_cast<std::size_t>(primitive) * 6u;
                data[index] = base;
                data[index + 1u] = static_cast<std::uint16_t>(base + 1u);
                data[index + 2u] = static_cast<std::uint16_t>(base + 2u);
                data[index + 3u] = base;
                data[index + 4u] = static_cast<std::uint16_t>(base + 2u);
                data[index + 5u] = static_cast<std::uint16_t>(base + 3u);
            }
            indices.SetData(data.data(), static_cast<int>(data.size()));
            effect.setLightingEnabledProperty(false);
            effect.setTextureEnabledProperty(false);
            effect.setVertexColorEnabledProperty(true);
            effect.setFogEnabledProperty(false);
        }

        Gfx::DynamicVertexBuffer vertices;
        Gfx::IndexBuffer indices;
        Gfx::BasicEffect effect;
    };

    SkyTransientPass::SkyTransientPass(const Camera& camera) noexcept
        : camera_(&camera)
    {
    }

    SkyTransientPass::~SkyTransientPass() = default;

    bool SkyTransientPass::SetCelestial(const environment::SimClock& clock,
                                        const environment::SunPosition& sun,
                                        double cloudCover) noexcept
    {
        if (!std::isfinite(clock.epochSeconds) || !std::isfinite(sun.altitudeDeg) ||
            !std::isfinite(cloudCover))
        {
            return false;
        }
        frame_ = SkyTransientFrameFor(clock, sun, cloudCover);
        RebuildVertices();
        return true;
    }

    void SkyTransientPass::RebuildVertices() noexcept
    {
        vertexCount_ = 0u;
        primitiveCount_ = 0u;
        for (const SatelliteSample& satellite : frame_.satellites)
        {
            const std::size_t before = vertexCount_;
            AppendPoint(vertices_, vertexCount_, satellite);
            primitiveCount_ += vertexCount_ != before ? 1u : 0u;
        }
        const std::size_t before = vertexCount_;
        AppendMeteor(vertices_, vertexCount_, frame_.meteor);
        primitiveCount_ += vertexCount_ != before ? 1u : 0u;
    }

    void SkyTransientPass::Draw(PassContext& context)
    {
        if (counterOwner_ != &context.counters)
        {
            counterOwner_ = &context.counters;
            drawsCounter_ = context.counters.Resolve("sky.transients.draws");
            trianglesCounter_ = context.counters.Resolve("sky.transients.triangles");
            uploadsCounter_ = context.counters.Resolve("sky.transients.uploads");
            satellitesCounter_ = context.counters.Resolve("sky.transients.satellites");
            meteorsCounter_ = context.counters.Resolve("sky.transients.meteors");
        }
        const std::int64_t visibleSatellites = static_cast<std::int64_t>(
            std::count_if(frame_.satellites.begin(),
                          frame_.satellites.end(),
                          [](const SatelliteSample& sample) { return sample.alpha > 0.0F; }));
        context.counters.Set(satellitesCounter_, visibleSatellites);
        context.counters.Set(meteorsCounter_, frame_.meteor.active ? 1 : 0);
        if (!IsActive())
        {
            context.counters.Set(drawsCounter_, 0);
            context.counters.Set(trianglesCounter_, 0);
            context.counters.Set(uploadsCounter_, static_cast<std::int64_t>(uploadCount_));
            return;
        }
        if (resources_ == nullptr)
        {
            resources_ = std::make_unique<Resources>(context.device);
        }
        Resources& resources = *resources_;
        context.device.SetVertexBuffer(nullptr);
        resources.vertices.SetData(
            vertices_.data(), 0, static_cast<int>(vertexCount_), Gfx::SetDataOptions::Discard);
        ++uploadCount_;

        const auto& viewport = context.device.getViewportProperty();
        const float aspect = viewport.getHeightProperty() > 0
                                 ? static_cast<float>(viewport.getWidthProperty()) /
                                       static_cast<float>(viewport.getHeightProperty())
                                 : 1.0F;
        resources.effect.setWorldProperty(Xna::Matrix::CreateTranslation(camera_->eye));
        resources.effect.setViewProperty(camera_->View());
        resources.effect.setProjectionProperty(
            Xna::Matrix::CreatePerspectiveFieldOfView(Xna::MathHelper::ToRadians(camera_->fieldOfViewDegrees),
                                                      aspect,
                                                      camera_->nearPlane,
                                                      kSkyFarPlane));

        context.states.SetBlend(Gfx::BlendState::Additive);
        context.states.SetDepthStencil(Gfx::DepthStencilState::None);
        context.states.SetRasterizer(StateFor(CullPolicy::TwoSided));
        context.device.SetVertexBuffer(&resources.vertices);
        context.device.setIndicesProperty(&resources.indices);

        Gfx::EffectPassCollection& passes =
            resources.effect.getCurrentTechniqueProperty()->getPassesProperty();
        for (int pass = 0; pass < passes.getCountProperty(); ++pass)
        {
            passes[pass]->Apply();
            context.device.DrawIndexedPrimitives(Gfx::PrimitiveType::TriangleList,
                                                 0,
                                                 0,
                                                 static_cast<int>(vertexCount_),
                                                 0,
                                                 static_cast<int>(primitiveCount_ * 2u));
        }

        context.counters.Set(drawsCounter_, 1);
        context.counters.Set(trianglesCounter_, static_cast<std::int64_t>(primitiveCount_ * 2u));
        context.counters.Set(uploadsCounter_, static_cast<std::int64_t>(uploadCount_));
    }
} // namespace cnahouse::rendering
