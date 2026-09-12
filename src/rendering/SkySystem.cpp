// SPDX-License-Identifier: MIT
#include "cnahouse/rendering/SkySystem.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <format>
#include <memory>
#include <utility>

#include "Microsoft/Xna/Framework/Color.hpp"
#include "Microsoft/Xna/Framework/Graphics/BasicEffect.hpp"
#include "Microsoft/Xna/Framework/Graphics/BlendState.hpp"
#include "Microsoft/Xna/Framework/Graphics/BufferUsage.hpp"
#include "Microsoft/Xna/Framework/Graphics/DepthStencilState.hpp"
#include "Microsoft/Xna/Framework/Graphics/EffectPass.hpp"
#include "Microsoft/Xna/Framework/Graphics/EffectPassCollection.hpp"
#include "Microsoft/Xna/Framework/Graphics/EffectTechnique.hpp"
#include "Microsoft/Xna/Framework/Graphics/GraphicsDevice.hpp"
#include "Microsoft/Xna/Framework/Graphics/IndexBuffer.hpp"
#include "Microsoft/Xna/Framework/Graphics/IndexElementSize.hpp"
#include "Microsoft/Xna/Framework/Graphics/PrimitiveType.hpp"
#include "Microsoft/Xna/Framework/Graphics/VertexBuffer.hpp"
#include "Microsoft/Xna/Framework/Graphics/VertexPositionColor.hpp"
#include "Microsoft/Xna/Framework/Graphics/Viewport.hpp"
#include "Microsoft/Xna/Framework/MathHelper.hpp"
#include "Microsoft/Xna/Framework/TitleContainer.hpp"
#include "System/IO/BinaryReader.hpp"
#include "System/IO/Stream.hpp"

#include "cnahouse/debug/Counters.hpp"
#include "cnahouse/rendering/Camera.hpp"
#include "cnahouse/rendering/RenderStates.hpp"
#include "cnahouse/rendering/StateTracker.hpp"
#include "cnahouse/util/Json.hpp"

namespace cnahouse::rendering
{
    namespace Xna = Microsoft::Xna::Framework;
    namespace Gfx = Microsoft::Xna::Framework::Graphics;

    namespace
    {
        constexpr float kSkyFarPlane = 1000.0F;

        util::Error Bad(util::ErrorCode code, std::string message, std::string_view name)
        {
            return util::Err(code, std::move(message), std::string(name));
        }

        util::Error InFile(const util::Error& error, std::string_view name)
        {
            return error.WithContext(name);
        }

        bool IsUnitColour(const Xna::Vector3& colour) noexcept
        {
            return std::isfinite(colour.X) && std::isfinite(colour.Y) && std::isfinite(colour.Z) &&
                   colour.X >= 0.0F && colour.X <= 1.0F && colour.Y >= 0.0F && colour.Y <= 1.0F &&
                   colour.Z >= 0.0F && colour.Z <= 1.0F;
        }

        Xna::Vector3 Lerp(const Xna::Vector3& from, const Xna::Vector3& to, float amount) noexcept
        {
            return Xna::Vector3(from.X + (to.X - from.X) * amount,
                                from.Y + (to.Y - from.Y) * amount,
                                from.Z + (to.Z - from.Z) * amount);
        }

        std::pair<Xna::Vector3, Xna::Vector3> GradientAt(const SkyColourModel& model,
                                                         double sunAltitudeDeg) noexcept
        {
            const auto upper = std::lower_bound(model.gradient.begin(),
                                                model.gradient.end(),
                                                sunAltitudeDeg,
                                                [](const SkyGradientRow& row, double elevation)
                                                { return row.sunElevationDeg < elevation; });
            if (upper == model.gradient.begin())
            {
                return {upper->zenith, upper->horizon};
            }
            if (upper == model.gradient.end())
            {
                const SkyGradientRow& row = model.gradient.back();
                return {row.zenith, row.horizon};
            }
            const SkyGradientRow& lower = *(upper - 1);
            const float amount = static_cast<float>((sunAltitudeDeg - lower.sunElevationDeg) /
                                                    (upper->sunElevationDeg - lower.sunElevationDeg));
            return {Lerp(lower.zenith, upper->zenith, amount), Lerp(lower.horizon, upper->horizon, amount)};
        }
    } // namespace

    util::Result<SkyColourModel> SkyColourModelReader::Read(std::string_view json, std::string name)
    {
        auto document = util::JsonDocument::Parse(json, name);
        if (!document)
        {
            return document.Error();
        }
        const util::JsonValue& root = document->Root();
        auto schema = root.RequireString("schema");
        if (!schema)
        {
            return InFile(schema.Error(), name);
        }
        if (*schema != "cna-house/sky/1")
        {
            return Bad(util::ErrorCode::SchemaMismatch,
                       std::format("schema is '{}', not 'cna-house/sky/1'", *schema),
                       name);
        }

        auto gradientValue = root.RequireArray("gradient");
        if (!gradientValue)
        {
            return InFile(gradientValue.Error(), name);
        }
        auto rows = gradientValue->Elements();
        if (!rows)
        {
            return InFile(rows.Error(), name);
        }
        if (rows->size() != kGradientRows)
        {
            return Bad(util::ErrorCode::InvalidData,
                       std::format("gradient has {} rows; the generated LUT requires {}",
                                   rows->size(),
                                   kGradientRows),
                       name);
        }

        SkyColourModel model;
        model.gradient.reserve(rows->size());
        for (std::size_t i = 0; i < rows->size(); ++i)
        {
            auto elevation = (*rows)[i].RequireNumber("sunElevationDeg");
            auto zenith = (*rows)[i].RequireVector3("zenith");
            auto horizon = (*rows)[i].RequireVector3("horizon");
            if (!elevation)
            {
                return InFile(elevation.Error(), name);
            }
            if (!zenith)
            {
                return InFile(zenith.Error(), name);
            }
            if (!horizon)
            {
                return InFile(horizon.Error(), name);
            }
            if (!std::isfinite(*elevation) || *elevation < -90.0 || *elevation > 90.0 ||
                (!model.gradient.empty() && *elevation <= model.gradient.back().sunElevationDeg))
            {
                return Bad(util::ErrorCode::InvalidData,
                           std::format("gradient row {} has elevation {}, which is non-finite, "
                                       "outside -90..90 or not strictly ascending",
                                       i,
                                       *elevation),
                           name);
            }
            if (!IsUnitColour(*zenith) || !IsUnitColour(*horizon))
            {
                return Bad(util::ErrorCode::OutOfRange,
                           std::format("gradient row {} has a colour outside 0..1", i),
                           name);
            }
            model.gradient.push_back(SkyGradientRow{*elevation, *zenith, *horizon});
        }

        auto colourValue = root.RequireObject("colourModel");
        if (!colourValue)
        {
            return InFile(colourValue.Error(), name);
        }
        auto cloudSamples = colourValue->RequireInt("cloudCoverSamples");
        auto azimuthSamples = colourValue->RequireInt("azimuthOffsetSamples");
        auto overcast = colourValue->RequireVector3("overcastGrey");
        auto glowColour = colourValue->RequireVector3("sunGlowColor");
        auto glowStrength = colourValue->RequireFloat("sunGlowStrength");
        auto glowExponent = colourValue->RequireFloat("sunGlowExponent");
        if (!cloudSamples)
        {
            return InFile(cloudSamples.Error(), name);
        }
        if (!azimuthSamples)
        {
            return InFile(azimuthSamples.Error(), name);
        }
        if (!overcast)
        {
            return InFile(overcast.Error(), name);
        }
        if (!glowColour)
        {
            return InFile(glowColour.Error(), name);
        }
        if (!glowStrength)
        {
            return InFile(glowStrength.Error(), name);
        }
        if (!glowExponent)
        {
            return InFile(glowExponent.Error(), name);
        }
        if (*cloudSamples != kCloudCoverSamples || *azimuthSamples != kAzimuthOffsetSamples)
        {
            return Bad(util::ErrorCode::InvalidData,
                       std::format("colourModel samples are {}x{}, not {}x{}",
                                   *cloudSamples,
                                   *azimuthSamples,
                                   kCloudCoverSamples,
                                   kAzimuthOffsetSamples),
                       name);
        }
        if (!IsUnitColour(*overcast) || !IsUnitColour(*glowColour) || !std::isfinite(*glowStrength) ||
            *glowStrength < 0.0F || *glowStrength > 1.0F || !std::isfinite(*glowExponent) ||
            *glowExponent <= 0.0F)
        {
            return Bad(util::ErrorCode::OutOfRange,
                       "colourModel has a colour or scalar outside its supported range",
                       name);
        }
        model.cloudCoverSamples = static_cast<std::uint32_t>(*cloudSamples);
        model.azimuthOffsetSamples = static_cast<std::uint32_t>(*azimuthSamples);
        model.overcastGrey = *overcast;
        model.sunGlowColor = *glowColour;
        model.sunGlowStrength = *glowStrength;
        model.sunGlowExponent = *glowExponent;
        return model;
    }

    util::Result<SkyColourModel> SkyColourModelReader::ReadFromTitle(std::string_view contentPath)
    {
        try
        {
            std::unique_ptr<System::IO::Stream> stream =
                Xna::TitleContainer::OpenStream(std::string(contentPath));
            if (stream == nullptr)
            {
                return Bad(util::ErrorCode::NotFound, "the file could not be opened", contentPath);
            }
            System::IO::BinaryReader reader(stream.get(), true);
            const int length = stream->getLengthProperty();
            if (length <= 0)
            {
                return Bad(util::ErrorCode::InvalidData, "the file is empty", contentPath);
            }
            const std::vector<std::uint8_t> bytes = reader.ReadBytes(length);
            if (bytes.size() != static_cast<std::size_t>(length))
            {
                return Bad(util::ErrorCode::InvalidData, "the file ended early", contentPath);
            }
            return Read(std::string_view(reinterpret_cast<const char*>(bytes.data()), bytes.size()),
                        std::string(contentPath));
        }
        catch (const std::exception& e)
        {
            return Bad(util::ErrorCode::NotFound, e.what(), contentPath);
        }
    }

    util::Result<SkyDomeMesh> SkyDomeReader::Read(System::IO::Stream& stream, std::string_view name)
    {
        try
        {
            if (stream.getLengthProperty() != static_cast<int>(kEncodedBytes))
            {
                return Bad(util::ErrorCode::InvalidData,
                           std::format("file has {} bytes; CSKY v1 requires exactly {}",
                                       stream.getLengthProperty(),
                                       kEncodedBytes),
                           name);
            }

            System::IO::BinaryReader reader(&stream, true);
            const std::uint32_t magic = reader.ReadUInt32();
            if (magic != kMagic)
            {
                return Bad(util::ErrorCode::InvalidData,
                           std::format("magic is {:#010x}, not 'CSKY' ({:#010x})", magic, kMagic),
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

            SkyDomeMesh mesh;
            mesh.longitudeSegments = reader.ReadUInt32();
            mesh.radius = reader.ReadSingle();
            mesh.skirtDepth = reader.ReadSingle();
            mesh.latitudeSegments = reader.ReadUInt32();
            mesh.domeVertexCount = reader.ReadUInt32();
            const std::uint32_t vertexCount = reader.ReadUInt32();
            const std::uint32_t indexCount = reader.ReadUInt32();

            if (mesh.longitudeSegments != kLongitudeSegments || mesh.latitudeSegments != kLatitudeSegments ||
                mesh.domeVertexCount != kDomeVertexCount || vertexCount != kVertexCount ||
                indexCount != kIndexCount || mesh.radius != kRadius || mesh.skirtDepth != kSkirtDepth)
            {
                return Bad(util::ErrorCode::InvalidData,
                           std::format("geometry header is not the 32x18, radius 900, skirt 90 "
                                       "CSKY v1 layout (got {}x{}, radius {}, skirt {}, {}/{}/{} "
                                       "dome/vertices/indices)",
                                       mesh.longitudeSegments,
                                       mesh.latitudeSegments,
                                       mesh.radius,
                                       mesh.skirtDepth,
                                       mesh.domeVertexCount,
                                       vertexCount,
                                       indexCount),
                           name);
            }

            mesh.positions.reserve(vertexCount);
            for (std::uint32_t i = 0; i < vertexCount; ++i)
            {
                Xna::Vector3 position(reader.ReadSingle(), reader.ReadSingle(), reader.ReadSingle());
                if (!std::isfinite(position.X) || !std::isfinite(position.Y) || !std::isfinite(position.Z))
                {
                    return Bad(util::ErrorCode::InvalidData,
                               std::format("vertex {} has a non-finite position", i),
                               name);
                }
                mesh.positions.push_back(position);
            }

            mesh.indices.reserve(indexCount);
            for (std::uint32_t i = 0; i < indexCount; ++i)
            {
                const std::uint16_t index = reader.ReadUInt16();
                if (index >= vertexCount)
                {
                    return Bad(util::ErrorCode::OutOfRange,
                               std::format("index {} addresses vertex {} of {}", i, index, vertexCount),
                               name);
                }
                mesh.indices.push_back(index);
            }
            return mesh;
        }
        catch (const std::exception& e)
        {
            return Bad(util::ErrorCode::InvalidData,
                       std::format("the file ended early or could not be read: {}", e.what()),
                       name);
        }
    }

    util::Result<SkyDomeMesh> SkyDomeReader::ReadFromTitle(std::string_view contentPath)
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

    class SkySystem::Resources
    {
    public:
        Resources(Gfx::GraphicsDevice& device,
                  const SkyDomeMesh& mesh,
                  const std::vector<Gfx::VertexPositionColor>& colouredVertices)
            : vertices(device,
                       Gfx::VertexPositionColor::getVertexDeclarationStatic(),
                       static_cast<int>(mesh.positions.size()),
                       Gfx::BufferUsage::WriteOnly)
            , indices(device,
                      Gfx::IndexElementSize::SixteenBits,
                      static_cast<int>(mesh.indices.size()),
                      Gfx::BufferUsage::WriteOnly)
            , effect(device)
        {
            vertices.SetData(colouredVertices.data(), static_cast<int>(colouredVertices.size()));
            indices.SetData(mesh.indices.data(), static_cast<int>(mesh.indices.size()));

            effect.setLightingEnabledProperty(false);
            effect.setTextureEnabledProperty(false);
            effect.setVertexColorEnabledProperty(true);
        }

        Gfx::VertexBuffer vertices;
        Gfx::IndexBuffer indices;
        Gfx::BasicEffect effect;
    };

    SkySystem::SkySystem(const Camera& camera, SkyDomeMesh mesh, SkyColourModel colourModel)
        : camera_(&camera)
        , mesh_(std::move(mesh))
        , colourModel_(std::move(colourModel))
        , sunDisc_(camera)
    {
        colouredVertices_.reserve(mesh_.positions.size());
        for (const Xna::Vector3& position : mesh_.positions)
        {
            colouredVertices_.emplace_back(position, Xna::Color::CornflowerBlue);
        }
        RecomputeColours(-18.0, 0.0);
    }

    SkySystem::~SkySystem() = default;

    void SkySystem::SetSun(const environment::SunPosition& sun, double cloudCover) noexcept
    {
        SetSky(sun.altitudeDeg, cloudCover);
        sunDisc_.SetSun(sun, cloudCover);
    }

    bool SkySystem::SetSky(double sunAltitudeDeg, double cloudCover) noexcept
    {
        if (!std::isfinite(sunAltitudeDeg) || !std::isfinite(cloudCover))
        {
            return false;
        }
        const double clampedCover = std::clamp(cloudCover, 0.0, 1.0);
        if (hasColourState_ && std::abs(sunAltitudeDeg - lastSunAltitudeDeg_) <= 0.25 &&
            std::abs(clampedCover - lastCloudCover_) <= 0.01)
        {
            return false;
        }
        RecomputeColours(sunAltitudeDeg, clampedCover);
        return true;
    }

    void SkySystem::RecomputeColours(double sunAltitudeDeg, double cloudCover) noexcept
    {
        const auto started = std::chrono::steady_clock::now();
        const auto [zenith, horizon] = GradientAt(colourModel_, sunAltitudeDeg);
        const float cloudMix = std::pow(static_cast<float>(cloudCover), 1.5F);
        for (std::size_t i = 0; i < mesh_.positions.size(); ++i)
        {
            const float altitude = std::clamp(mesh_.positions[i].Y / mesh_.radius, 0.0F, 1.0F);
            const float altitudeBlend = altitude * altitude * (3.0F - 2.0F * altitude);
            const Xna::Vector3 clear = Lerp(horizon, zenith, altitudeBlend);
            colouredVertices_[i].Color = Xna::Color(Lerp(clear, colourModel_.overcastGrey, cloudMix));
        }
        if (resources_ != nullptr)
        {
            resources_->vertices.SetData(colouredVertices_.data(),
                                         static_cast<int>(colouredVertices_.size()));
        }
        lastSunAltitudeDeg_ = sunAltitudeDeg;
        lastCloudCover_ = cloudCover;
        hasColourState_ = true;
        ++colourUpdateCount_;
        lastColourMilliseconds_ =
            std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - started).count();
    }

    Xna::Matrix SkySystem::DomeWorld(const Camera& camera) noexcept
    {
        return Xna::Matrix::CreateTranslation(camera.eye);
    }

    void SkySystem::Draw(PassContext& context)
    {
        if (resources_ == nullptr)
        {
            resources_ = std::make_unique<Resources>(context.device, mesh_, colouredVertices_);
        }

        Resources& resources = *resources_;
        const auto& viewport = context.device.getViewportProperty();
        const float aspect = viewport.getHeightProperty() > 0
                                 ? static_cast<float>(viewport.getWidthProperty()) /
                                       static_cast<float>(viewport.getHeightProperty())
                                 : 1.0F;
        resources.effect.setWorldProperty(DomeWorld(*camera_));
        resources.effect.setViewProperty(camera_->View());
        resources.effect.setProjectionProperty(
            Xna::Matrix::CreatePerspectiveFieldOfView(Xna::MathHelper::ToRadians(camera_->fieldOfViewDegrees),
                                                      aspect,
                                                      camera_->nearPlane,
                                                      kSkyFarPlane));

        context.states.SetBlend(Gfx::BlendState::Opaque);
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
                                                 static_cast<int>(mesh_.positions.size()),
                                                 0,
                                                 static_cast<int>(mesh_.indices.size() / 3u));
        }

        if (counterOwner_ != &context.counters)
        {
            counterOwner_ = &context.counters;
            drawsCounter_ = context.counters.Resolve("sky.dome.draws");
            trianglesCounter_ = context.counters.Resolve("sky.dome.triangles");
            colourUpdatesCounter_ = context.counters.Resolve("sky.colour.updates");
            colourMicrosCounter_ = context.counters.Resolve("sky.colour.micros");
        }
        context.counters.Set(drawsCounter_, 1);
        context.counters.Set(trianglesCounter_, static_cast<std::int64_t>(mesh_.indices.size() / 3u));
        context.counters.Set(colourUpdatesCounter_, static_cast<std::int64_t>(colourUpdateCount_));
        context.counters.Set(colourMicrosCounter_,
                             static_cast<std::int64_t>(std::lround(lastColourMilliseconds_ * 1000.0)));

        // Celestial layers belong after the opaque dome but before world geometry. The existing sun
        // pass retains its own additive blend and no-depth state and lazily owns its texture.
        sunDisc_.Draw(context);
    }

} // namespace cnahouse::rendering
