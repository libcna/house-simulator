// SPDX-License-Identifier: MIT
#include "cnahouse/rendering/SkySystem.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <format>
#include <memory>
#include <numbers>
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
#include "Microsoft/Xna/Framework/Graphics/Texture2D.hpp"
#include "Microsoft/Xna/Framework/Graphics/VertexBuffer.hpp"
#include "Microsoft/Xna/Framework/Graphics/VertexPositionColor.hpp"
#include "Microsoft/Xna/Framework/Graphics/VertexPositionColorTexture.hpp"
#include "Microsoft/Xna/Framework/Graphics/Viewport.hpp"
#include "Microsoft/Xna/Framework/MathHelper.hpp"
#include "Microsoft/Xna/Framework/TitleContainer.hpp"
#include "System/IO/BinaryReader.hpp"
#include "System/IO/Stream.hpp"

#include "cnahouse/debug/Counters.hpp"
#include "cnahouse/environment/MoonLight.hpp"
#include "cnahouse/environment/SimClock.hpp"
#include "cnahouse/environment/SunLight.hpp"
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
        constexpr std::array<std::string_view, 3> kCloudLayerIds{"CL_HIGH", "CL_MID", "CL_LOW"};

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

        Xna::Vector3 AddScaled(const Xna::Vector3& colour, const Xna::Vector3& addition, float scale) noexcept
        {
            return Xna::Vector3(std::clamp(colour.X + addition.X * scale, 0.0F, 1.0F),
                                std::clamp(colour.Y + addition.Y * scale, 0.0F, 1.0F),
                                std::clamp(colour.Z + addition.Z * scale, 0.0F, 1.0F));
        }

        float SunIntensityAt(const SkyColourModel& model, double elevationDeg) noexcept
        {
            if (model.sunIntensity.empty())
            {
                return 0.0F;
            }
            const auto upper = std::lower_bound(model.sunIntensity.begin(),
                                                model.sunIntensity.end(),
                                                elevationDeg,
                                                [](const SkySunIntensityRow& row, double elevation)
                                                { return row.elevationDeg < elevation; });
            if (upper == model.sunIntensity.begin())
            {
                return upper->intensity;
            }
            if (upper == model.sunIntensity.end())
            {
                return model.sunIntensity.back().intensity;
            }
            const SkySunIntensityRow& lower = *(upper - 1);
            const float amount = static_cast<float>((elevationDeg - lower.elevationDeg) /
                                                    (upper->elevationDeg - lower.elevationDeg));
            return lower.intensity + (upper->intensity - lower.intensity) * amount;
        }

        double CircularDifferenceDeg(double a, double b) noexcept
        {
            return std::abs(std::remainder(a - b, 360.0));
        }

        float WrapUv(double value) noexcept
        {
            return static_cast<float>(std::remainder(value, 1.0));
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

        auto sunValue = root.RequireArray("sun");
        if (!sunValue)
        {
            return InFile(sunValue.Error(), name);
        }
        auto sunRows = sunValue->Elements();
        if (!sunRows)
        {
            return InFile(sunRows.Error(), name);
        }
        if (sunRows->size() != kSunIntensityRows)
        {
            return Bad(util::ErrorCode::InvalidData,
                       std::format(
                           "sun has {} rows; the glow curve requires {}", sunRows->size(), kSunIntensityRows),
                       name);
        }
        model.sunIntensity.reserve(sunRows->size());
        for (std::size_t i = 0; i < sunRows->size(); ++i)
        {
            auto elevation = (*sunRows)[i].RequireNumber("elevationDeg");
            auto intensity = (*sunRows)[i].RequireFloat("intensity");
            if (!elevation)
            {
                return InFile(elevation.Error(), name);
            }
            if (!intensity)
            {
                return InFile(intensity.Error(), name);
            }
            if (!std::isfinite(*elevation) || *elevation < -90.0 || *elevation > 90.0 ||
                (!model.sunIntensity.empty() && *elevation <= model.sunIntensity.back().elevationDeg) ||
                !std::isfinite(*intensity) || *intensity < 0.0F || *intensity > 1.0F)
            {
                return Bad(util::ErrorCode::OutOfRange,
                           std::format("sun row {} has unordered elevation or intensity outside 0..1", i),
                           name);
            }
            model.sunIntensity.push_back(SkySunIntensityRow{*elevation, *intensity});
        }

        auto cloudLayersValue = root.RequireArray("cloudLayers");
        if (!cloudLayersValue)
        {
            return InFile(cloudLayersValue.Error(), name);
        }
        auto cloudLayers = cloudLayersValue->Elements();
        if (!cloudLayers)
        {
            return InFile(cloudLayers.Error(), name);
        }
        if (cloudLayers->size() != model.cloudLayers.size())
        {
            return Bad(
                util::ErrorCode::InvalidData,
                std::format("cloudLayers has {} rows; §31.3 requires exactly three", cloudLayers->size()),
                name);
        }
        for (std::size_t i = 0; i < cloudLayers->size(); ++i)
        {
            auto id = (*cloudLayers)[i].RequireString("id");
            auto texture = (*cloudLayers)[i].RequireString("texture");
            auto radius = (*cloudLayers)[i].RequireFloat("altitude");
            auto scrollScale = (*cloudLayers)[i].RequireFloat("scrollScale");
            auto opacity = (*cloudLayers)[i].RequireFloat("opacity");
            if (!id)
            {
                return InFile(id.Error(), name);
            }
            if (!texture)
            {
                return InFile(texture.Error(), name);
            }
            if (!radius)
            {
                return InFile(radius.Error(), name);
            }
            if (!scrollScale)
            {
                return InFile(scrollScale.Error(), name);
            }
            if (!opacity)
            {
                return InFile(opacity.Error(), name);
            }
            if (*id != kCloudLayerIds[i])
            {
                return Bad(util::ErrorCode::InvalidData,
                           std::format("cloud layer {} is '{}', not '{}'", i, *id, kCloudLayerIds[i]),
                           name);
            }
            if (texture->empty() || !std::isfinite(*radius) || *radius <= 0.0F ||
                *radius >= SkyDomeReader::kRadius ||
                (i > 0u && *radius >= model.cloudLayers[i - 1u].radius) || !std::isfinite(*scrollScale) ||
                *scrollScale < 0.0F || !std::isfinite(*opacity) || *opacity < 0.0F || *opacity > 1.0F)
            {
                return Bad(util::ErrorCode::OutOfRange,
                           std::format("cloud layer {} has an empty texture, unordered radius, or "
                                       "scalar outside its supported range",
                                       i),
                           name);
            }
            model.cloudLayers[i] =
                CloudLayer{std::string(*id), std::string(*texture), *radius, *scrollScale, *opacity};
        }

        auto cloudAlphaValue = root.RequireArray("cloudAlpha");
        if (!cloudAlphaValue)
        {
            return InFile(cloudAlphaValue.Error(), name);
        }
        auto cloudAlphaRows = cloudAlphaValue->Elements();
        if (!cloudAlphaRows)
        {
            return InFile(cloudAlphaRows.Error(), name);
        }
        if (cloudAlphaRows->size() < 2u)
        {
            return Bad(util::ErrorCode::InvalidData,
                       "cloudAlpha needs at least two bands for continuous interpolation",
                       name);
        }
        model.cloudAlphaBands.reserve(cloudAlphaRows->size());
        for (std::size_t i = 0; i < cloudAlphaRows->size(); ++i)
        {
            auto coverValue = (*cloudAlphaRows)[i].RequireArray("cloudCover");
            auto high = (*cloudAlphaRows)[i].RequireFloat("high");
            auto mid = (*cloudAlphaRows)[i].RequireFloat("mid");
            auto low = (*cloudAlphaRows)[i].RequireFloat("low");
            if (!coverValue)
            {
                return InFile(coverValue.Error(), name);
            }
            if (!high)
            {
                return InFile(high.Error(), name);
            }
            if (!mid)
            {
                return InFile(mid.Error(), name);
            }
            if (!low)
            {
                return InFile(low.Error(), name);
            }
            auto cover = coverValue->Elements();
            if (!cover || cover->size() != 2u)
            {
                return Bad(util::ErrorCode::InvalidData,
                           std::format("cloudAlpha row {} does not have a two-number cover band", i),
                           name);
            }
            auto minimum = (*cover)[0].AsFloat();
            auto maximum = (*cover)[1].AsFloat();
            if (!minimum)
            {
                return InFile(minimum.Error(), name);
            }
            if (!maximum)
            {
                return InFile(maximum.Error(), name);
            }
            const bool contiguous =
                i == 0u ? *minimum == 0.0F : *minimum == model.cloudAlphaBands.back().maximumCover;
            if (!std::isfinite(*minimum) || !std::isfinite(*maximum) || *minimum < 0.0F || *maximum > 1.0F ||
                *minimum >= *maximum || !contiguous || !std::isfinite(*high) || *high < 0.0F ||
                *high > 1.0F || !std::isfinite(*mid) || *mid < 0.0F || *mid > 1.0F || !std::isfinite(*low) ||
                *low < 0.0F || *low > 1.0F)
            {
                return Bad(util::ErrorCode::OutOfRange,
                           std::format("cloudAlpha row {} is not a contiguous unit-range band", i),
                           name);
            }
            model.cloudAlphaBands.push_back(CloudAlphaBand{*minimum, *maximum, {*high, *mid, *low}});
        }
        if (model.cloudAlphaBands.back().maximumCover != 1.0F)
        {
            return Bad(util::ErrorCode::InvalidData, "cloudAlpha does not cover through 1", name);
        }

        auto stormValue = root.RequireObject("stormCloudAlpha");
        if (!stormValue)
        {
            return InFile(stormValue.Error(), name);
        }
        auto stormHigh = stormValue->RequireFloat("high");
        auto stormMid = stormValue->RequireFloat("mid");
        auto stormLow = stormValue->RequireFloat("low");
        if (!stormHigh)
        {
            return InFile(stormHigh.Error(), name);
        }
        if (!stormMid)
        {
            return InFile(stormMid.Error(), name);
        }
        if (!stormLow)
        {
            return InFile(stormLow.Error(), name);
        }
        model.stormCloudAlpha = {*stormHigh, *stormMid, *stormLow};
        if (std::any_of(model.stormCloudAlpha.begin(),
                        model.stormCloudAlpha.end(),
                        [](float alpha) { return !std::isfinite(alpha) || alpha < 0.0F || alpha > 1.0F; }))
        {
            return Bad(util::ErrorCode::OutOfRange, "stormCloudAlpha has a value outside 0..1", name);
        }
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

    namespace
    {
        class CloudGpuLayer
        {
        public:
            CloudGpuLayer(Gfx::GraphicsDevice& device,
                          std::unique_ptr<Gfx::Texture2D> textureValue,
                          const CloudRingMesh& mesh,
                          std::uint64_t tintRevision)
                : texture(std::move(textureValue))
                , vertices(device,
                           Gfx::VertexPositionColorTexture::getVertexDeclarationStatic(),
                           static_cast<int>(mesh.vertices.size()),
                           Gfx::BufferUsage::WriteOnly)
                , indices(device,
                          Gfx::IndexElementSize::SixteenBits,
                          static_cast<int>(mesh.indices.size()),
                          Gfx::BufferUsage::WriteOnly)
                , effect(device)
                , uploadedVertices(mesh.vertices)
            {
                vertices.SetData(uploadedVertices.data(), static_cast<int>(uploadedVertices.size()));
                indices.SetData(mesh.indices.data(), static_cast<int>(mesh.indices.size()));
                lastOffset = Xna::Vector2(0.0F, 0.0F);
                hasOffset = true;
                lastTintRevision = tintRevision;
                effect.setLightingEnabledProperty(false);
                effect.setTextureEnabledProperty(true);
                effect.setVertexColorEnabledProperty(true);
                effect.setTextureProperty(texture.get());
            }

            bool SetState(const CloudRingMesh& mesh, const Xna::Vector2& offset, std::uint64_t tintRevision)
            {
                if (hasOffset && offset.X == lastOffset.X && offset.Y == lastOffset.Y &&
                    tintRevision == lastTintRevision)
                {
                    return false;
                }
                for (std::size_t i = 0; i < uploadedVertices.size(); ++i)
                {
                    uploadedVertices[i].Color = mesh.vertices[i].Color;
                    uploadedVertices[i].TextureCoordinate =
                        Xna::Vector2(mesh.vertices[i].TextureCoordinate.X + offset.X,
                                     mesh.vertices[i].TextureCoordinate.Y + offset.Y);
                }
                vertices.SetData(uploadedVertices.data(), static_cast<int>(uploadedVertices.size()));
                lastOffset = offset;
                hasOffset = true;
                lastTintRevision = tintRevision;
                return true;
            }

            std::unique_ptr<Gfx::Texture2D> texture;
            Gfx::VertexBuffer vertices;
            Gfx::IndexBuffer indices;
            // Last among the XNA resources, therefore first destroyed: it borrows `texture`.
            Gfx::BasicEffect effect;
            std::vector<Gfx::VertexPositionColorTexture> uploadedVertices;
            Xna::Vector2 lastOffset;
            bool hasOffset = false;
            std::uint64_t lastTintRevision = 0;
        };
    } // namespace

    class SkySystem::Resources
    {
    public:
        Resources(Gfx::GraphicsDevice& device,
                  const SkyDomeMesh& mesh,
                  const std::vector<Gfx::VertexPositionColor>& colouredVertices,
                  const std::array<CloudRingMesh, 3>& cloudRings,
                  std::optional<CloudTextures>& cloudTextures,
                  std::uint64_t cloudTintRevision,
                  std::uint64_t colourRevision)
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
            uploadedColourRevision = colourRevision;

            effect.setLightingEnabledProperty(false);
            effect.setTextureEnabledProperty(false);
            effect.setVertexColorEnabledProperty(true);

            if (cloudTextures.has_value())
            {
                clouds.reserve(cloudRings.size());
                for (std::size_t i = 0; i < cloudRings.size(); ++i)
                {
                    clouds.push_back(std::make_unique<CloudGpuLayer>(
                        device, std::move((*cloudTextures)[i]), cloudRings[i], cloudTintRevision));
                }
                cloudTextures.reset();
            }
        }

        Gfx::VertexBuffer vertices;
        Gfx::IndexBuffer indices;
        Gfx::BasicEffect effect;
        std::vector<std::unique_ptr<CloudGpuLayer>> clouds;
        std::uint64_t uploadedColourRevision = 0;
    };

    SkySystem::SkySystem(const Camera& camera, SkyDomeMesh mesh, SkyColourModel colourModel)
        : camera_(&camera)
        , mesh_(std::move(mesh))
        , colourModel_(std::move(colourModel))
        , cloudRings_(BuildCloudRings(colourModel_.cloudLayers))
        , sunDisc_(camera)
        , moonDisc_(camera)
    {
        colouredVertices_.reserve(mesh_.positions.size());
        for (const Xna::Vector3& position : mesh_.positions)
        {
            colouredVertices_.emplace_back(position, Xna::Color::CornflowerBlue);
        }
        for (std::size_t i = 0; i < cloudAlphas_.size(); ++i)
        {
            cloudAlphas_[i] = colourModel_.cloudLayers[i].opacity;
        }
        environment::SunPosition sun;
        sun.altitudeDeg = -18.0;
        environment::MoonPosition moon;
        moon.altitudeDeg = -90.0;
        RecomputeColours(sun, moon, environment::MoonPhase{}, 0.0);
    }

    SkySystem::SkySystem(const Camera& camera,
                         SkyDomeMesh mesh,
                         SkyColourModel colourModel,
                         CloudTextures cloudTextures)
        : SkySystem(camera, std::move(mesh), std::move(colourModel))
    {
        cloudTextures_.emplace(std::move(cloudTextures));
    }

    SkySystem::SkySystem(const Camera& camera,
                         SkyDomeMesh mesh,
                         SkyColourModel colourModel,
                         CloudTextures cloudTextures,
                         std::unique_ptr<Gfx::Texture2D> moonAlbedo)
        : SkySystem(camera, std::move(mesh), std::move(colourModel), std::move(cloudTextures))
    {
        moonDisc_.SetAlbedo(std::move(moonAlbedo));
    }

    SkySystem::SkySystem(const Camera& camera,
                         SkyDomeMesh mesh,
                         SkyColourModel colourModel,
                         CloudTextures cloudTextures,
                         std::unique_ptr<Gfx::Texture2D> moonAlbedo,
                         StarCatalogue stars)
        : SkySystem(camera,
                    std::move(mesh),
                    std::move(colourModel),
                    std::move(cloudTextures),
                    std::move(moonAlbedo))
    {
        starField_ = std::make_unique<StarField>(camera, std::move(stars));
    }

    SkySystem::~SkySystem() = default;

    void SkySystem::SetSun(const environment::SunPosition& sun, double cloudCover) noexcept
    {
        environment::MoonPosition moon;
        moon.altitudeDeg = -90.0;
        const environment::MoonPhase phase{};
        SetSky(sun, moon, phase, cloudCover);
        sunDisc_.SetSun(sun, cloudCover);
        moonDisc_.SetMoon(moon, phase, sun);
        if (starField_ != nullptr)
        {
            static_cast<void>(starField_->SetVisibility(sun, moon, phase, cloudCover));
        }
    }

    void SkySystem::SetCelestial(const environment::SunPosition& sun,
                                 const environment::MoonPosition& moon,
                                 const environment::MoonPhase& phase,
                                 double cloudCover) noexcept
    {
        SetSky(sun, moon, phase, cloudCover);
        sunDisc_.SetSun(sun, cloudCover);
        moonDisc_.SetMoon(moon, phase, sun);
        if (starField_ != nullptr)
        {
            static_cast<void>(starField_->SetVisibility(sun, moon, phase, cloudCover));
        }
    }

    void SkySystem::SetCelestial(const environment::SimClock& clock,
                                 const environment::SunPosition& sun,
                                 const environment::MoonPosition& moon,
                                 const environment::MoonPhase& phase,
                                 double cloudCover) noexcept
    {
        SetSky(sun, moon, phase, cloudCover);
        sunDisc_.SetSun(sun, cloudCover);
        moonDisc_.SetMoon(moon, phase, sun);
        if (starField_ != nullptr)
        {
            static_cast<void>(starField_->SetCelestial(clock, sun, moon, phase, cloudCover));
        }
    }

    bool SkySystem::SetWind(double speedMetresPerSecond, double directionDegrees) noexcept
    {
        if (!std::isfinite(speedMetresPerSecond) || !std::isfinite(directionDegrees))
        {
            return false;
        }
        windSpeedMetresPerSecond_ = std::clamp(speedMetresPerSecond, 0.0, 30.0);
        windDirectionDegrees_ = std::fmod(directionDegrees, 360.0);
        if (windDirectionDegrees_ < 0.0)
        {
            windDirectionDegrees_ += 360.0;
        }
        return true;
    }

    std::array<float, 3> SkySystem::CloudAlphasFor(const SkyColourModel& model,
                                                   double cloudCover,
                                                   double thunderIntensity) noexcept
    {
        std::array<float, 3> base{};
        if (model.cloudAlphaBands.empty())
        {
            return base;
        }

        const float cover =
            std::isfinite(cloudCover) ? static_cast<float>(std::clamp(cloudCover, 0.0, 1.0)) : 0.0F;
        const auto centre = [](const CloudAlphaBand& band) noexcept
        { return (band.minimumCover + band.maximumCover) * 0.5F; };
        const auto upper =
            std::lower_bound(model.cloudAlphaBands.begin(),
                             model.cloudAlphaBands.end(),
                             cover,
                             [&](const CloudAlphaBand& band, float value) { return centre(band) < value; });
        if (upper == model.cloudAlphaBands.begin())
        {
            base = upper->alpha;
        }
        else if (upper == model.cloudAlphaBands.end())
        {
            base = model.cloudAlphaBands.back().alpha;
        }
        else
        {
            const CloudAlphaBand& lower = *(upper - 1);
            const float lowerCentre = centre(lower);
            const float amount = (cover - lowerCentre) / (centre(*upper) - lowerCentre);
            for (std::size_t i = 0; i < base.size(); ++i)
            {
                base[i] = lower.alpha[i] + (upper->alpha[i] - lower.alpha[i]) * amount;
            }
        }

        const float thunder = std::isfinite(thunderIntensity)
                                  ? static_cast<float>(std::clamp(thunderIntensity, 0.0, 1.0))
                                  : 0.0F;
        for (std::size_t i = 0; i < base.size(); ++i)
        {
            base[i] += (model.stormCloudAlpha[i] - base[i]) * thunder;
        }
        return base;
    }

    bool SkySystem::SetCloudState(double cloudCover, double thunderIntensity) noexcept
    {
        if (!std::isfinite(cloudCover) || !std::isfinite(thunderIntensity))
        {
            return false;
        }
        const std::array<float, 3> next = CloudAlphasFor(colourModel_, cloudCover, thunderIntensity);
        if (next == cloudAlphas_)
        {
            return false;
        }
        cloudAlphas_ = next;
        ++cloudAlphaUpdateCount_;
        return true;
    }

    bool SkySystem::AdvanceClouds(double deltaSeconds) noexcept
    {
        if (!std::isfinite(deltaSeconds) || deltaSeconds <= 0.0 || windSpeedMetresPerSecond_ <= 0.0)
        {
            return false;
        }

        // Meteorological direction names where the wind comes FROM. With north = -Z and east =
        // +X, the air therefore travels (-sin(direction), 0, +cos(direction)). Texture coordinates
        // move the other way so that a feature sampled from the texture visibly follows the air.
        const double radians = windDirectionDegrees_ * std::numbers::pi / 180.0;
        const double sampleU = std::sin(radians) * windSpeedMetresPerSecond_ * deltaSeconds /
                               static_cast<double>(kCloudTextureRepeatMetres);
        const double sampleV = -std::cos(radians) * windSpeedMetresPerSecond_ * deltaSeconds /
                               static_cast<double>(kCloudTextureRepeatMetres);
        for (std::size_t i = 0; i < cloudOffsets_.size(); ++i)
        {
            const double scale = static_cast<double>(colourModel_.cloudLayers[i].scrollScale);
            cloudOffsets_[i].X = WrapUv(static_cast<double>(cloudOffsets_[i].X) + sampleU * scale);
            cloudOffsets_[i].Y = WrapUv(static_cast<double>(cloudOffsets_[i].Y) + sampleV * scale);
        }
        return true;
    }

    bool SkySystem::SetSky(double sunAltitudeDeg, double cloudCover) noexcept
    {
        environment::SunPosition sun;
        sun.altitudeDeg = sunAltitudeDeg;
        sun.azimuthDeg = hasColourState_ ? lastSunAzimuthDeg_ : 0.0;
        environment::MoonPosition moon;
        moon.altitudeDeg = lastMoonAltitudeDeg_;
        environment::MoonPhase phase;
        phase.illuminatedFraction = lastMoonIllumination_;
        return SetSky(sun, moon, phase, cloudCover);
    }

    bool SkySystem::SetSky(const environment::SunPosition& sun,
                           const environment::MoonPosition& moon,
                           const environment::MoonPhase& phase,
                           double cloudCover) noexcept
    {
        if (!std::isfinite(sun.altitudeDeg) || !std::isfinite(sun.azimuthDeg) ||
            !std::isfinite(moon.altitudeDeg) || !std::isfinite(phase.illuminatedFraction) ||
            !std::isfinite(cloudCover))
        {
            return false;
        }
        const double clampedCover = std::clamp(cloudCover, 0.0, 1.0);
        const double clampedIllumination = std::clamp(phase.illuminatedFraction, 0.0, 1.0);
        if (hasColourState_ && std::abs(sun.altitudeDeg - lastSunAltitudeDeg_) <= 0.25 &&
            CircularDifferenceDeg(sun.azimuthDeg, lastSunAzimuthDeg_) <= 1.0 &&
            std::abs(moon.altitudeDeg - lastMoonAltitudeDeg_) <= 1.0 &&
            std::abs(clampedIllumination - lastMoonIllumination_) <= 1.0 / 128.0 &&
            std::abs(clampedCover - lastCloudCover_) <= 0.01)
        {
            return false;
        }
        environment::MoonPhase clampedPhase = phase;
        clampedPhase.illuminatedFraction = clampedIllumination;
        RecomputeColours(sun, moon, clampedPhase, clampedCover);
        return true;
    }

    void SkySystem::RecomputeColours(const environment::SunPosition& sun,
                                     const environment::MoonPosition& moon,
                                     const environment::MoonPhase& phase,
                                     double cloudCover) noexcept
    {
        const auto started = std::chrono::steady_clock::now();
        const auto [zenith, horizon] = GradientAt(colourModel_, sun.altitudeDeg);
        const auto [nightZenith, nightHorizon] = GradientAt(colourModel_, -18.0);
        const float cloudMix = std::pow(static_cast<float>(cloudCover), 1.5F);
        const float clearSky = static_cast<float>((1.0 - cloudCover) * (1.0 - cloudCover));
        const float sunIntensity = SunIntensityAt(colourModel_, sun.altitudeDeg);
        const Xna::Vector3 directionToSun = environment::DirectionToSun(sun);
        const environment::MoonShading moonShading = environment::MoonShadingFor(moon, phase, cloudCover);
        const float nightWeight =
            static_cast<float>(1.0 - environment::TwilightAmbientFactor(sun.altitudeDeg));

        Xna::Vector3 cloudColour = Lerp(horizon, colourModel_.overcastGrey, cloudMix);
        cloudColour =
            Lerp(cloudColour, AddScaled(nightHorizon, moonShading.color, moonShading.intensity), nightWeight);
        const Xna::Color cloudTint(cloudColour);
        for (std::size_t i = 0; i < mesh_.positions.size(); ++i)
        {
            const float altitude = std::clamp(mesh_.positions[i].Y / mesh_.radius, 0.0F, 1.0F);
            const float altitudeBlend = altitude * altitude * (3.0F - 2.0F * altitude);
            const Xna::Vector3 clear = Lerp(horizon, zenith, altitudeBlend);
            Xna::Vector3 colour = Lerp(clear, colourModel_.overcastGrey, cloudMix);

            const Xna::Vector3& position = mesh_.positions[i];
            const double x = static_cast<double>(position.X);
            const double y = static_cast<double>(position.Y);
            const double z = static_cast<double>(position.Z);
            const double sunX = static_cast<double>(directionToSun.X);
            const double sunY = static_cast<double>(directionToSun.Y);
            const double sunZ = static_cast<double>(directionToSun.Z);
            const double length = std::sqrt(x * x + y * y + z * z);
            if (length > 0.0)
            {
                const float dot = static_cast<float>((x * sunX + y * sunY + z * sunZ) / length);
                const float lobe = std::pow(std::max(dot, 0.0F), colourModel_.sunGlowExponent);
                const float glow = colourModel_.sunGlowStrength * sunIntensity * lobe * clearSky;
                colour = AddScaled(colour, colourModel_.sunGlowColor, glow);
            }

            const Xna::Vector3 nightBase = Lerp(nightHorizon, nightZenith, altitudeBlend);
            const Xna::Vector3 night = AddScaled(nightBase, moonShading.color, moonShading.intensity);
            colouredVertices_[i].Color = Xna::Color(Lerp(colour, night, nightWeight));
        }
        for (CloudRingMesh& ring : cloudRings_)
        {
            for (Gfx::VertexPositionColorTexture& vertex : ring.vertices)
            {
                vertex.Color = cloudTint;
            }
        }
        ++cloudTintRevision_;
        ++colourRevision_;
        lastSunAltitudeDeg_ = sun.altitudeDeg;
        lastSunAzimuthDeg_ = sun.azimuthDeg;
        lastMoonAltitudeDeg_ = moon.altitudeDeg;
        lastMoonIllumination_ = phase.illuminatedFraction;
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

    std::array<CloudRingMesh, 3> SkySystem::BuildCloudRings(const std::array<CloudLayer, 3>& layers)
    {
        std::array<CloudRingMesh, 3> result;
        constexpr float kHalfPi = std::numbers::pi_v<float> * 0.5F;
        constexpr float kTwoPi = std::numbers::pi_v<float> * 2.0F;

        for (std::size_t layerIndex = 0; layerIndex < layers.size(); ++layerIndex)
        {
            CloudRingMesh& mesh = result[layerIndex];
            mesh.vertices.reserve(kCloudVerticesPerLayer);
            mesh.indices.reserve(kCloudIndicesPerLayer);
            const float radius = layers[layerIndex].radius;

            // One pole vertex per wedge gives the planar UV projection a continuous top without
            // the zero-area triangles made by a seam-wrapped rectangular grid.
            for (std::uint32_t longitude = 0; longitude < kCloudLongitudeSegments; ++longitude)
            {
                mesh.vertices.emplace_back(
                    Xna::Vector3(0.0F, radius, 0.0F), Xna::Color::White, Xna::Vector2(0.0F, 0.0F));
            }

            for (std::uint32_t latitude = 1; latitude <= kCloudLatitudeSegments; ++latitude)
            {
                const float elevation =
                    kHalfPi * (1.0F - static_cast<float>(latitude) / kCloudLatitudeSegments);
                const float horizontal = radius * std::cos(elevation);
                const float y = radius * std::sin(elevation);
                for (std::uint32_t longitude = 0; longitude <= kCloudLongitudeSegments; ++longitude)
                {
                    const float azimuth = kTwoPi * static_cast<float>(longitude) / kCloudLongitudeSegments;
                    const float x = horizontal * std::sin(azimuth);
                    const float z = -horizontal * std::cos(azimuth);
                    mesh.vertices.emplace_back(
                        Xna::Vector3(x, y, z),
                        Xna::Color::White,
                        Xna::Vector2(x / kCloudTextureRepeatMetres, z / kCloudTextureRepeatMetres));
                }
            }

            const std::uint16_t firstRing = static_cast<std::uint16_t>(kCloudLongitudeSegments);
            for (std::uint16_t longitude = 0; longitude < kCloudLongitudeSegments; ++longitude)
            {
                mesh.indices.push_back(longitude);
                mesh.indices.push_back(static_cast<std::uint16_t>(firstRing + longitude));
                mesh.indices.push_back(static_cast<std::uint16_t>(firstRing + longitude + 1u));
            }
            for (std::uint16_t latitude = 0; latitude + 1u < kCloudLatitudeSegments; ++latitude)
            {
                const std::uint16_t upper =
                    static_cast<std::uint16_t>(firstRing + latitude * (kCloudLongitudeSegments + 1u));
                const std::uint16_t lower = static_cast<std::uint16_t>(upper + kCloudLongitudeSegments + 1u);
                for (std::uint16_t longitude = 0; longitude < kCloudLongitudeSegments; ++longitude)
                {
                    mesh.indices.push_back(static_cast<std::uint16_t>(upper + longitude));
                    mesh.indices.push_back(static_cast<std::uint16_t>(lower + longitude));
                    mesh.indices.push_back(static_cast<std::uint16_t>(lower + longitude + 1u));
                    mesh.indices.push_back(static_cast<std::uint16_t>(upper + longitude));
                    mesh.indices.push_back(static_cast<std::uint16_t>(lower + longitude + 1u));
                    mesh.indices.push_back(static_cast<std::uint16_t>(upper + longitude + 1u));
                }
            }
        }
        return result;
    }

    void SkySystem::Draw(PassContext& context)
    {
        AdvanceClouds(context.deltaSeconds);
        if (resources_ == nullptr)
        {
            resources_ = std::make_unique<Resources>(context.device,
                                                     mesh_,
                                                     colouredVertices_,
                                                     cloudRings_,
                                                     cloudTextures_,
                                                     cloudTintRevision_,
                                                     colourRevision_);
        }

        Resources& resources = *resources_;
        if (resources.uploadedColourRevision != colourRevision_)
        {
            // CNA enforces XNA's resource-binding contract: SetData cannot update the vertex buffer
            // still retained by the previous draw. Upload lazily here, after explicitly unbinding it.
            context.device.SetVertexBuffer(nullptr);
            resources.vertices.SetData(colouredVertices_.data(), static_cast<int>(colouredVertices_.size()));
            resources.uploadedColourRevision = colourRevision_;
        }
        const auto& viewport = context.device.getViewportProperty();
        const float aspect = viewport.getHeightProperty() > 0
                                 ? static_cast<float>(viewport.getWidthProperty()) /
                                       static_cast<float>(viewport.getHeightProperty())
                                 : 1.0F;
        const Xna::Matrix world = DomeWorld(*camera_);
        const Xna::Matrix view = camera_->View();
        const Xna::Matrix projection =
            Xna::Matrix::CreatePerspectiveFieldOfView(Xna::MathHelper::ToRadians(camera_->fieldOfViewDegrees),
                                                      aspect,
                                                      camera_->nearPlane,
                                                      kSkyFarPlane);
        resources.effect.setWorldProperty(world);
        resources.effect.setViewProperty(view);
        resources.effect.setProjectionProperty(projection);

        context.states.SetBlend(Gfx::BlendState::Opaque);
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
            cloudDrawsCounter_ = context.counters.Resolve("sky.cloud.draws");
            cloudTrianglesCounter_ = context.counters.Resolve("sky.cloud.triangles");
            cloudUploadsCounter_ = context.counters.Resolve("sky.cloud.uploads");
            cloudAlphaUpdatesCounter_ = context.counters.Resolve("sky.cloud.alpha_updates");
        }
        context.counters.Set(drawsCounter_, 1);
        context.counters.Set(trianglesCounter_, static_cast<std::int64_t>(mesh_.indices.size() / 3u));
        context.counters.Set(colourUpdatesCounter_, static_cast<std::int64_t>(colourUpdateCount_));
        context.counters.Set(colourMicrosCounter_,
                             static_cast<std::int64_t>(std::lround(lastColourMilliseconds_ * 1000.0)));

        // Celestial layers belong after the opaque dome but before both clouds and world geometry.
        // Stars are first so the nearer moon and sun remain legible; each component owns its
        // additive/no-depth state, and clouds then obscure the complete sky naturally.
        if (starField_ != nullptr)
        {
            starField_->Draw(context);
        }
        moonDisc_.Draw(context);
        sunDisc_.Draw(context);

        std::int64_t cloudDraws = 0;
        std::int64_t cloudTriangles = 0;
        if (!resources.clouds.empty())
        {
            context.states.SetBlend(Gfx::BlendState::AlphaBlend);
            context.states.SetDepthStencil(Gfx::DepthStencilState::None);
            context.states.SetRasterizer(StateFor(CullPolicy::TwoSided));

            for (std::size_t i = 0; i < resources.clouds.size(); ++i)
            {
                CloudGpuLayer& cloud = *resources.clouds[i];
                if (cloud.SetState(cloudRings_[i], cloudOffsets_[i], cloudTintRevision_))
                {
                    ++cloudUploadCount_;
                }
                cloud.effect.setWorldProperty(world);
                cloud.effect.setViewProperty(view);
                cloud.effect.setProjectionProperty(projection);
                cloud.effect.setAlphaProperty(cloudAlphas_[i]);
                context.device.SetVertexBuffer(&cloud.vertices);
                context.device.setIndicesProperty(&cloud.indices);

                Gfx::EffectPassCollection& cloudPasses =
                    cloud.effect.getCurrentTechniqueProperty()->getPassesProperty();
                for (int pass = 0; pass < cloudPasses.getCountProperty(); ++pass)
                {
                    cloudPasses[pass]->Apply();
                    context.device.DrawIndexedPrimitives(
                        Gfx::PrimitiveType::TriangleList,
                        0,
                        0,
                        static_cast<int>(cloudRings_[i].vertices.size()),
                        0,
                        static_cast<int>(cloudRings_[i].indices.size() / 3u));
                    ++cloudDraws;
                    cloudTriangles += static_cast<std::int64_t>(cloudRings_[i].indices.size() / 3u);
                }
            }
        }
        context.counters.Set(cloudDrawsCounter_, cloudDraws);
        context.counters.Set(cloudTrianglesCounter_, cloudTriangles);
        context.counters.Set(cloudUploadsCounter_, static_cast<std::int64_t>(cloudUploadCount_));
        context.counters.Set(cloudAlphaUpdatesCounter_, static_cast<std::int64_t>(cloudAlphaUpdateCount_));
    }

} // namespace cnahouse::rendering
