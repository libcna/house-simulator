// SPDX-License-Identifier: MIT
#include "cnahouse/rendering/SkySystem.hpp"

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
    } // namespace

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
        Resources(Gfx::GraphicsDevice& device, const SkyDomeMesh& mesh)
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
            // `HOUSE-01644` replaces this bootstrap blue from the LUT. It is intentionally a real
            // visible colour rather than transparent or green: a loaded dome must be distinguishable
            // from both a missing pass and the old blockout clear colour during this task.
            std::vector<Gfx::VertexPositionColor> coloured;
            coloured.reserve(mesh.positions.size());
            for (const Xna::Vector3& position : mesh.positions)
            {
                coloured.emplace_back(position, Xna::Color::CornflowerBlue);
            }
            vertices.SetData(coloured.data(), static_cast<int>(coloured.size()));
            indices.SetData(mesh.indices.data(), static_cast<int>(mesh.indices.size()));

            effect.setLightingEnabledProperty(false);
            effect.setTextureEnabledProperty(false);
            effect.setVertexColorEnabledProperty(true);
        }

        Gfx::VertexBuffer vertices;
        Gfx::IndexBuffer indices;
        Gfx::BasicEffect effect;
    };

    SkySystem::SkySystem(const Camera& camera, SkyDomeMesh mesh)
        : camera_(&camera)
        , mesh_(std::move(mesh))
        , sunDisc_(camera)
    {
    }

    SkySystem::~SkySystem() = default;

    void SkySystem::SetSun(const environment::SunPosition& sun, double cloudCover) noexcept
    {
        sunDisc_.SetSun(sun, cloudCover);
    }

    Xna::Matrix SkySystem::DomeWorld(const Camera& camera) noexcept
    {
        return Xna::Matrix::CreateTranslation(camera.eye);
    }

    void SkySystem::Draw(PassContext& context)
    {
        if (resources_ == nullptr)
        {
            resources_ = std::make_unique<Resources>(context.device, mesh_);
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
        }
        context.counters.Set(drawsCounter_, 1);
        context.counters.Set(trianglesCounter_, static_cast<std::int64_t>(mesh_.indices.size() / 3u));

        // Celestial layers belong after the opaque dome but before world geometry. The existing sun
        // pass retains its own additive blend and no-depth state and lazily owns its texture.
        sunDisc_.Draw(context);
    }

} // namespace cnahouse::rendering
