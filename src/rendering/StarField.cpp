// SPDX-License-Identifier: MIT
#include "cnahouse/rendering/StarField.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <format>
#include <numbers>
#include <utility>

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
#include "Microsoft/Xna/Framework/TitleContainer.hpp"
#include "Microsoft/Xna/Framework/Vector4.hpp"
#include "System/IO/BinaryReader.hpp"
#include "System/IO/Stream.hpp"

#include "cnahouse/debug/Counters.hpp"
#include "cnahouse/environment/SimClock.hpp"
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
        constexpr float kDistance = 880.0F;
        constexpr float kSkyFarPlane = 1000.0F;
        constexpr float kMinimumMagnitude = -2.0F;
        constexpr float kMaximumMagnitude = 5.5F;
        constexpr float kMinimumBv = -0.5F;
        constexpr float kMaximumBv = 3.0F;
        constexpr double kVectorEpsilonSquared = 1.0e-12;

        struct BvLutEntry
        {
            float bv;
            Xna::Vector3 colour;
        };

        const std::array<BvLutEntry, 8> kBvLut{{
            {-0.40F, Xna::Vector3(0.61F, 0.70F, 1.00F)},
            {-0.20F, Xna::Vector3(0.70F, 0.78F, 1.00F)},
            {0.00F, Xna::Vector3(0.83F, 0.87F, 1.00F)},
            {0.30F, Xna::Vector3(0.95F, 0.95F, 1.00F)},
            {0.60F, Xna::Vector3(1.00F, 0.97F, 0.88F)},
            {1.00F, Xna::Vector3(1.00F, 0.86F, 0.65F)},
            {1.50F, Xna::Vector3(1.00F, 0.70F, 0.42F)},
            {2.00F, Xna::Vector3(1.00F, 0.55F, 0.28F)},
        }};

        util::Error Bad(util::ErrorCode code, std::string message, std::string_view name)
        {
            return util::Err(code, std::move(message), std::string(name));
        }

        [[nodiscard]] double Dot(const Xna::Vector3& a, const Xna::Vector3& b) noexcept
        {
            return static_cast<double>(a.X) * static_cast<double>(b.X) +
                   static_cast<double>(a.Y) * static_cast<double>(b.Y) +
                   static_cast<double>(a.Z) * static_cast<double>(b.Z);
        }

        [[nodiscard]] Xna::Vector3 Cross(const Xna::Vector3& a, const Xna::Vector3& b) noexcept
        {
            return Xna::Vector3(a.Y * b.Z - a.Z * b.Y, a.Z * b.X - a.X * b.Z, a.X * b.Y - a.Y * b.X);
        }

        [[nodiscard]] Xna::Vector3 Scaled(const Xna::Vector3& value, float scale) noexcept
        {
            return Xna::Vector3(value.X * scale, value.Y * scale, value.Z * scale);
        }

        [[nodiscard]] Xna::Vector3 Add(const Xna::Vector3& a, const Xna::Vector3& b) noexcept
        {
            return Xna::Vector3(a.X + b.X, a.Y + b.Y, a.Z + b.Z);
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

        [[nodiscard]] Xna::Vector3 BvColour(float bv) noexcept
        {
            const float bounded = std::clamp(bv, kBvLut.front().bv, kBvLut.back().bv);
            const auto upper =
                std::lower_bound(kBvLut.begin(),
                                 kBvLut.end(),
                                 bounded,
                                 [](const BvLutEntry& entry, float value) { return entry.bv < value; });
            if (upper == kBvLut.begin())
            {
                return upper->colour;
            }
            if (upper == kBvLut.end())
            {
                return kBvLut.back().colour;
            }
            const BvLutEntry& lower = *(upper - 1);
            const float amount = (bounded - lower.bv) / (upper->bv - lower.bv);
            return Xna::Vector3(lower.colour.X + (upper->colour.X - lower.colour.X) * amount,
                                lower.colour.Y + (upper->colour.Y - lower.colour.Y) * amount,
                                lower.colour.Z + (upper->colour.Z - lower.colour.Z) * amount);
        }

        void FillStarVertices(std::span<const StarCatalogueEntry> catalogue,
                              const StarOrientation* orientation,
                              std::vector<Gfx::VertexPositionColor>& vertices)
        {
            vertices.clear();
            vertices.reserve(catalogue.size() * 4u);
            for (const StarCatalogueEntry& star : catalogue)
            {
                const Xna::Vector3 direction =
                    orientation == nullptr ? EquatorialDirection(star.rightAscensionDeg, star.declinationDeg)
                                           : HorizonDirection(star, *orientation);
                const Xna::Vector3 centre = Scaled(direction, kDistance);
                Xna::Vector3 right = Cross(Xna::Vector3::Up, direction);
                if (Dot(right, right) <= kVectorEpsilonSquared)
                {
                    right = Xna::Vector3(1.0F, 0.0F, 0.0F);
                }
                else
                {
                    right = Normalised(right, Xna::Vector3(1.0F, 0.0F, 0.0F));
                }
                const Xna::Vector3 up = Normalised(Cross(direction, right), Xna::Vector3::Up);
                const StarAppearance appearance = AppearanceForStar(star.visualMagnitude, star.bvColourIndex);
                const Xna::Vector3 horizontal = Scaled(right, appearance.halfSize);
                const Xna::Vector3 vertical = Scaled(up, appearance.halfSize);
                const Xna::Color colour(Xna::Vector4(
                    appearance.colour.X, appearance.colour.Y, appearance.colour.Z, appearance.alpha));
                vertices.emplace_back(Add(Add(centre, Scaled(horizontal, -1.0F)), Scaled(vertical, -1.0F)),
                                      colour);
                vertices.emplace_back(Add(Add(centre, horizontal), Scaled(vertical, -1.0F)), colour);
                vertices.emplace_back(Add(Add(centre, horizontal), vertical), colour);
                vertices.emplace_back(Add(Add(centre, Scaled(horizontal, -1.0F)), vertical), colour);
            }
        }
    } // namespace

    util::Result<StarCatalogue> StarCatalogueReader::Read(System::IO::Stream& stream, std::string_view name)
    {
        try
        {
            if (stream.getLengthProperty() != static_cast<int>(kEncodedBytes))
            {
                return Bad(util::ErrorCode::InvalidData,
                           std::format("file has {} bytes; CSTR v1 requires exactly {}",
                                       stream.getLengthProperty(),
                                       kEncodedBytes),
                           name);
            }
            System::IO::BinaryReader reader(&stream, true);
            const std::uint32_t magic = reader.ReadUInt32();
            if (magic != kMagic)
            {
                return Bad(util::ErrorCode::InvalidData,
                           std::format("magic is {:#010x}, not 'CSTR' ({:#010x})", magic, kMagic),
                           name);
            }
            const std::uint32_t version = reader.ReadUInt32();
            if (version != kVersion)
            {
                return Bad(util::ErrorCode::VersionMismatch,
                           std::format(
                               "version {} is not supported; this build reads version {}", version, kVersion),
                           name);
            }
            const std::uint32_t flags = reader.ReadUInt32();
            if (flags != 0u)
            {
                return Bad(util::ErrorCode::VersionMismatch,
                           std::format("reserved header flags {:#010x} are set", flags),
                           name);
            }
            const std::uint32_t count = reader.ReadUInt32();
            if (count != kStarCount)
            {
                return Bad(util::ErrorCode::InvalidData,
                           std::format("catalogue declares {} stars, not {}", count, kStarCount),
                           name);
            }

            StarCatalogue catalogue;
            catalogue.reserve(count);
            float previousMagnitude = kMinimumMagnitude;
            for (std::uint32_t i = 0; i < count; ++i)
            {
                StarCatalogueEntry star;
                star.rightAscensionDeg = reader.ReadSingle();
                star.declinationDeg = reader.ReadSingle();
                star.visualMagnitude = reader.ReadSingle();
                star.bvColourIndex = reader.ReadSingle();
                if (!std::isfinite(star.rightAscensionDeg) || !std::isfinite(star.declinationDeg) ||
                    !std::isfinite(star.visualMagnitude) || !std::isfinite(star.bvColourIndex))
                {
                    return Bad(util::ErrorCode::InvalidData,
                               std::format("star {} contains a non-finite field", i),
                               name);
                }
                if (star.rightAscensionDeg < 0.0F || star.rightAscensionDeg >= 360.0F ||
                    star.declinationDeg < -90.0F || star.declinationDeg > 90.0F ||
                    star.visualMagnitude < kMinimumMagnitude || star.visualMagnitude > kMaximumMagnitude ||
                    star.bvColourIndex < kMinimumBv || star.bvColourIndex > kMaximumBv)
                {
                    return Bad(util::ErrorCode::OutOfRange,
                               std::format("star {} is outside CSTR v1's coordinate/photometry bounds", i),
                               name);
                }
                if (i != 0u && star.visualMagnitude < previousMagnitude)
                {
                    return Bad(util::ErrorCode::InvalidData,
                               std::format("star {} is brighter than the preceding record", i),
                               name);
                }
                previousMagnitude = star.visualMagnitude;
                catalogue.push_back(star);
            }
            return catalogue;
        }
        catch (const std::exception& e)
        {
            return Bad(util::ErrorCode::InvalidData,
                       std::format("the file ended early or could not be read: {}", e.what()),
                       name);
        }
    }

    util::Result<StarCatalogue> StarCatalogueReader::ReadFromTitle(std::string_view contentPath)
    {
        try
        {
            std::unique_ptr<System::IO::Stream> stream =
                Xna::TitleContainer::OpenStream(std::string(contentPath));
            if (stream == nullptr)
            {
                return Bad(util::ErrorCode::NotFound, "the file could not be opened", contentPath);
            }
            return Read(*stream, contentPath);
        }
        catch (const std::exception& e)
        {
            return Bad(util::ErrorCode::NotFound, e.what(), contentPath);
        }
    }

    Xna::Vector3 EquatorialDirection(float rightAscensionDeg, float declinationDeg) noexcept
    {
        const float ra = rightAscensionDeg * std::numbers::pi_v<float> / 180.0F;
        const float dec = declinationDeg * std::numbers::pi_v<float> / 180.0F;
        const float cosDec = std::cos(dec);
        return Xna::Vector3(cosDec * std::cos(ra), std::sin(dec), -cosDec * std::sin(ra));
    }

    StarOrientation StarOrientationFor(const environment::SimClock& clock) noexcept
    {
        StarOrientation result;
        result.latitudeDeg = clock.latitudeDeg;
        const double daysSinceJ2000 =
            environment::DaysSinceJ2000ForEpochSeconds(clock.CivilEpochSeconds(), clock.utcOffsetMinutes);
        result.localSiderealTimeDeg = environment::LocalSiderealTimeDeg(daysSinceJ2000, clock.longitudeDeg);
        return result;
    }

    Xna::Vector3 HorizonDirection(const StarCatalogueEntry& star, const StarOrientation& orientation) noexcept
    {
        if (!std::isfinite(star.rightAscensionDeg) || !std::isfinite(star.declinationDeg) ||
            !std::isfinite(orientation.localSiderealTimeDeg) || !std::isfinite(orientation.latitudeDeg))
        {
            return Xna::Vector3::Forward;
        }
        const double hourAngleRad =
            (orientation.localSiderealTimeDeg - static_cast<double>(star.rightAscensionDeg)) *
            std::numbers::pi / 180.0;
        const double declinationRad = static_cast<double>(star.declinationDeg) * std::numbers::pi / 180.0;
        const double latitudeRad = orientation.latitudeDeg * std::numbers::pi / 180.0;
        const double sinHourAngle = std::sin(hourAngleRad);
        const double cosHourAngle = std::cos(hourAngleRad);
        const double sinDeclination = std::sin(declinationRad);
        const double cosDeclination = std::cos(declinationRad);
        const double sinLatitude = std::sin(latitudeRad);
        const double cosLatitude = std::cos(latitudeRad);
        return Xna::Vector3(
            static_cast<float>(-cosDeclination * sinHourAngle),
            static_cast<float>(sinLatitude * sinDeclination + cosLatitude * cosDeclination * cosHourAngle),
            static_cast<float>(sinLatitude * cosDeclination * cosHourAngle - cosLatitude * sinDeclination));
    }

    StarAppearance AppearanceForStar(float visualMagnitude, float bvColourIndex) noexcept
    {
        const float magnitude = std::clamp(visualMagnitude, kMinimumMagnitude, kMaximumMagnitude);
        const float brightness =
            std::clamp((kMaximumMagnitude - magnitude) / (kMaximumMagnitude - kMinimumMagnitude), 0.0F, 1.0F);
        const float shaped = std::sqrt(brightness);
        // At 60 degrees vertical FOV these span roughly 0.5..1.7 pixels at 720p. Brighter stars
        // therefore read as points with a larger additive core instead of oversized discs.
        const float angularRadiusDeg = 0.020F + 0.055F * shaped;
        StarAppearance result;
        result.colour = BvColour(bvColourIndex);
        result.alpha = 0.18F + 0.82F * shaped;
        result.halfSize = kDistance * std::tan(angularRadiusDeg * std::numbers::pi_v<float> / 180.0F);
        return result;
    }

    std::vector<Gfx::VertexPositionColor> BuildStarVertices(std::span<const StarCatalogueEntry> catalogue)
    {
        std::vector<Gfx::VertexPositionColor> vertices;
        FillStarVertices(catalogue, nullptr, vertices);
        return vertices;
    }

    std::vector<Gfx::VertexPositionColor> BuildStarVertices(std::span<const StarCatalogueEntry> catalogue,
                                                            const StarOrientation& orientation)
    {
        std::vector<Gfx::VertexPositionColor> vertices;
        FillStarVertices(catalogue, &orientation, vertices);
        return vertices;
    }

    class StarField::Resources
    {
    public:
        Resources(Gfx::GraphicsDevice& device, std::size_t starCount)
            : vertices(device,
                       Gfx::VertexPositionColor::getVertexDeclarationStatic(),
                       static_cast<int>(starCount * 4u),
                       Gfx::BufferUsage::WriteOnly)
            , indices(device,
                      Gfx::IndexElementSize::SixteenBits,
                      static_cast<int>(starCount * 6u),
                      Gfx::BufferUsage::WriteOnly)
            , effect(device)
        {
            std::vector<std::uint16_t> data;
            data.reserve(starCount * 6u);
            for (std::size_t i = 0; i < starCount; ++i)
            {
                const auto base = static_cast<std::uint16_t>(i * 4u);
                data.insert(data.end(),
                            {base,
                             static_cast<std::uint16_t>(base + 1u),
                             static_cast<std::uint16_t>(base + 2u),
                             base,
                             static_cast<std::uint16_t>(base + 2u),
                             static_cast<std::uint16_t>(base + 3u)});
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

    StarField::StarField(const Camera& camera, StarCatalogue catalogue)
        : camera_(&camera)
        , catalogue_(std::move(catalogue))
    {
        orientation_ = StarOrientationFor(environment::SimClock{});
        FillStarVertices(catalogue_, &orientation_, vertices_);
        geometryUpdateCount_ = 1;
    }

    StarField::~StarField() = default;

    bool StarField::SetObserver(const environment::SimClock& clock) noexcept
    {
        const double civilEpochSeconds = clock.CivilEpochSeconds();
        if (!std::isfinite(civilEpochSeconds) || !std::isfinite(clock.latitudeDeg) ||
            !std::isfinite(clock.longitudeDeg) || clock.latitudeDeg < -90.0 || clock.latitudeDeg > 90.0 ||
            clock.longitudeDeg < -180.0 || clock.longitudeDeg > 180.0)
        {
            return false;
        }
        const StarOrientation next = StarOrientationFor(clock);
        if (next.localSiderealTimeDeg == orientation_.localSiderealTimeDeg &&
            next.latitudeDeg == orientation_.latitudeDeg)
        {
            return false;
        }
        orientation_ = next;
        FillStarVertices(catalogue_, &orientation_, vertices_);
        ++geometryUpdateCount_;
        return true;
    }

    void StarField::Draw(PassContext& context)
    {
        if (!IsActive())
        {
            return;
        }
        if (resources_ == nullptr)
        {
            resources_ = std::make_unique<Resources>(context.device, catalogue_.size());
        }
        Resources& resources = *resources_;
        resources.vertices.SetData(
            vertices_.data(), 0, static_cast<int>(vertices_.size()), Gfx::SetDataOptions::Discard);
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
            passes[pass].Apply();
            context.device.DrawIndexedPrimitives(Gfx::PrimitiveType::TriangleList,
                                                 0,
                                                 0,
                                                 static_cast<int>(vertices_.size()),
                                                 0,
                                                 static_cast<int>(catalogue_.size() * 2u));
        }

        if (counterOwner_ != &context.counters)
        {
            counterOwner_ = &context.counters;
            drawsCounter_ = context.counters.Resolve("stars.draws");
            trianglesCounter_ = context.counters.Resolve("stars.triangles");
            uploadsCounter_ = context.counters.Resolve("stars.uploads");
        }
        context.counters.Set(drawsCounter_, 1);
        context.counters.Set(trianglesCounter_, static_cast<std::int64_t>(catalogue_.size() * 2u));
        context.counters.Set(uploadsCounter_, static_cast<std::int64_t>(uploadCount_));
    }
} // namespace cnahouse::rendering
