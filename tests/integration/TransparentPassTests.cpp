// SPDX-License-Identifier: MIT
//
// `HOUSE-00898`. The unit suite proves the two-level ordering and material classification; this
// test crosses the other boundary with a real device: the sorted slice is submitted under XNA's
// premultiplied blend state with depth testing on and depth writes off.
#include <algorithm>
#include <array>
#include <limits>
#include <span>
#include <string>
#include <string_view>
#include <tuple>
#include <vector>

#include <gtest/gtest.h>

#include "Microsoft/Xna/Framework/Color.hpp"
#include "Microsoft/Xna/Framework/Graphics/BlendState.hpp"
#include "Microsoft/Xna/Framework/Graphics/DepthFormat.hpp"
#include "Microsoft/Xna/Framework/Graphics/DepthStencilState.hpp"
#include "Microsoft/Xna/Framework/Graphics/GraphicsDevice.hpp"
#include "Microsoft/Xna/Framework/Graphics/RasterizerState.hpp"
#include "Microsoft/Xna/Framework/Graphics/RenderTarget2D.hpp"
#include "Microsoft/Xna/Framework/Graphics/RenderTargetUsage.hpp"
#include "Microsoft/Xna/Framework/Graphics/SurfaceFormat.hpp"

#include "cnahouse/app/FrameTimer.hpp"
#include "cnahouse/debug/Counters.hpp"
#include "cnahouse/environment/SimClock.hpp"
#include "cnahouse/lighting/LightingSystem.hpp"
#include "cnahouse/lighting/ShadingGrid.hpp"
#include "cnahouse/rendering/Camera.hpp"
#include "cnahouse/rendering/StateTracker.hpp"
#include "cnahouse/rendering/TransparentPass.hpp"
#include "cnahouse/util/Ids.hpp"
#include "cnahouse/visibility/PortalRuntime.hpp"
#include "cnahouse/visibility/RenderList.hpp"
#include "cnahouse/world/CellRuntime.hpp"
#include "cnahouse/world/ChunkData.hpp"
#include "cnahouse/world/WorldData.hpp"

#include "integration/DeviceHost.hpp"

namespace
{
    namespace Gfx = Microsoft::Xna::Framework::Graphics;
    namespace world = cnahouse::world;
    using Microsoft::Xna::Framework::Vector3;

    world::WorldData TransparentMaterials()
    {
        world::WorldData::Contents contents;
        for (const auto& [name, tint, alpha] : {std::tuple<std::string_view, Vector3, float>{
                                                    "MAT_TEST_CLEAR", Vector3(0.1F, 0.3F, 0.5F), 0.12F},
                                                std::tuple<std::string_view, Vector3, float>{
                                                    "MAT_TEST_OBSCURED", Vector3(0.4F, 0.2F, 0.1F), 0.32F}})
        {
            world::MaterialDef material;
            material.id = cnahouse::util::Intern(name);
            material.alphaMode = world::AlphaMode::Blend;
            material.alpha = alpha;
            material.tint = tint;
            contents.materials.push_back(material);
        }
        auto built = world::WorldData::Create(std::move(contents));
        EXPECT_TRUE(built) << built.Error().ToString();
        return std::move(built.Value());
    }

    world::ChunkLibrary TinyGlassHouse()
    {
        world::ChunkLibrary library;
        library.cells = {"FAR_ROOM", "NEAR_ROOM"};
        library.materials = {"MAT_TEST_CLEAR", "MAT_TEST_OBSCURED"};
        auto add = [&](std::uint16_t cell, std::uint16_t material, float x)
        {
            world::Chunk chunk;
            chunk.cell = cell;
            chunk.material = material;
            chunk.layout = world::ChunkLayout::Basic;
            chunk.bounds = Microsoft::Xna::Framework::BoundingBox(Vector3(x - 0.5F, 0.0F, -0.5F),
                                                                  Vector3(x + 0.5F, 1.0F, 0.5F));
            chunk.vertexCount = 3u;
            chunk.vertices.assign(3u * world::ChunkVertexStride(world::ChunkLayout::Basic), 0u);
            chunk.indexCount = 3u;
            chunk.indices.assign(3u * sizeof(std::uint16_t), 0u);
            library.chunks.push_back(std::move(chunk));
        };
        add(0u, 0u, 8.0F);
        add(0u, 1u, 10.0F);
        add(1u, 0u, 2.0F);
        return library;
    }

    TEST(TransparentPassTests, GlassTintDoesNotBecomeAnEmitterWhenTheEyeAdaptsToADarkRoom)
    {
        using cnahouse::rendering::TransparentTintExposure;
        EXPECT_FLOAT_EQ(TransparentTintExposure(world::MaterialClass::Glass, 6.0F), 1.0F);
        EXPECT_FLOAT_EQ(TransparentTintExposure(world::MaterialClass::Glass, 1.0F), 1.0F);
        EXPECT_FLOAT_EQ(TransparentTintExposure(world::MaterialClass::Water, 6.0F), 6.0F);
        EXPECT_FLOAT_EQ(TransparentTintExposure(world::MaterialClass::Fabric, 2.5F), 2.5F);
    }

    TEST(TransparentPassTests, FixtureGlowTracksFluxSwitchEnvelopeAndEyeAdaptation)
    {
        using cnahouse::rendering::FixtureGlowFor;
        using cnahouse::rendering::GlowQuadRadialOpacity;

        EXPECT_FLOAT_EQ(GlowQuadRadialOpacity(-1.0F), 1.0F);
        EXPECT_FLOAT_EQ(GlowQuadRadialOpacity(0.0F), 1.0F);
        EXPECT_GT(GlowQuadRadialOpacity(0.25F), GlowQuadRadialOpacity(0.75F));
        EXPECT_FLOAT_EQ(GlowQuadRadialOpacity(1.0F), 0.0F);
        EXPECT_FLOAT_EQ(GlowQuadRadialOpacity(std::numeric_limits<float>::quiet_NaN()), 0.0F);

        world::Light light;
        light.position = Vector3(1.0F, 2.0F, 3.0F);
        light.intensityLm = 400.0F;
        light.fixtureProp = cnahouse::util::Id::Of("PROP_TEST_LANTERN");
        light.emissiveMaterialSlot = "LanternShade";
        const Vector3 warm(1.1F, 0.65F, -0.1F);

        const auto off = FixtureGlowFor(light, 0.0F, warm, 1.0F);
        const auto dim = FixtureGlowFor(light, 0.25F, warm, 1.0F);
        const auto on = FixtureGlowFor(light, 1.0F, warm, 1.0F);
        const auto darkAdapted = FixtureGlowFor(light, 1.0F, warm, 6.0F);
        EXPECT_FALSE(off.visible);
        ASSERT_TRUE(dim.visible);
        ASSERT_TRUE(on.visible);
        ASSERT_TRUE(darkAdapted.visible);
        EXPECT_EQ(on.centre, light.position);
        EXPECT_FLOAT_EQ(on.tint.X, 1.0F);
        EXPECT_FLOAT_EQ(on.tint.Y, 0.65F);
        EXPECT_FLOAT_EQ(on.tint.Z, 0.0F);
        EXPECT_LT(dim.radius, on.radius);
        EXPECT_LT(dim.alpha, on.alpha);
        EXPECT_LT(on.radius, darkAdapted.radius);
        EXPECT_LT(on.alpha, darkAdapted.alpha);

        light.intensityLm = 1600.0F;
        const auto brighter = FixtureGlowFor(light, 1.0F, warm, 1.0F);
        EXPECT_GT(brighter.radius, on.radius);
        EXPECT_GT(brighter.alpha, on.alpha);

        light.fixtureProp = {};
        EXPECT_FALSE(FixtureGlowFor(light, 1.0F, warm, 1.0F).visible)
            << "canonical light points without a physical fixture must never become floating orbs";
    }

    TEST(TransparentPassTests, ItDrawsOnlyLiveLinkedFixtureGlowsWithAdditiveReadOnlyDepth)
    {
        world::WorldData::Contents contents;
        world::Light light;
        light.id = cnahouse::util::Id::Of("LIGHT_TEST_LANTERN");
        light.cell = cnahouse::util::Id::Of("EXTERIOR_TEST");
        light.group = cnahouse::util::Id::Of("LG_TEST_LANTERN");
        light.type = world::LightType::Point;
        light.bulbClass = world::BulbClass::Led;
        light.position = Vector3(0.0F, 0.0F, 0.0F);
        light.colorK = 2400.0F;
        light.intensityLm = 400.0F;
        light.fixtureProp = cnahouse::util::Id::Of("PROP_TEST_LANTERN");
        light.emissiveMaterialSlot = "LanternShade";
        light.defaultOn = true;
        contents.lights.push_back(light);
        auto built = world::WorldData::Create(std::move(contents));
        ASSERT_TRUE(built) << built.Error().ToString();
        const world::WorldData fixtureWorld = std::move(built.Value());

        const world::ChunkLibrary library;
        std::uint32_t glows = 0u;
        int litPixels = 0;
        int brightestRed = 0;
        cnahouse::testsupport::DeviceHost host(
            [&](Gfx::GraphicsDevice& device)
            {
                world::CellRuntime cells(device, library);
                cnahouse::lighting::ShadingGrid shading = cnahouse::lighting::ShadingGrid::Unshaded();
                cnahouse::environment::SimClock clock;
                const std::vector<cnahouse::visibility::PortalRuntime> portals;
                cnahouse::lighting::LightingSystem lighting(fixtureWorld, shading, clock, std::span(portals));
                cnahouse::app::FrameContext frame;
                frame.deltaSeconds = 1.0F / 60.0F;
                frame.frameIndex = 1u;
                lighting.Update(frame);

                cnahouse::rendering::Camera camera;
                camera.eye = Vector3(0.0F, 0.0F, 3.0F);
                camera.target = Vector3(0.0F, 0.0F, 0.0F);
                cnahouse::visibility::RenderList list;
                cnahouse::rendering::StateTracker states(device);
                cnahouse::debug::Counters counters;
                cnahouse::rendering::TransparentPass pass(
                    library, cells, fixtureWorld, camera, list, &lighting);
                ASSERT_TRUE(pass.IsActive());
                cnahouse::rendering::PassContext context{device, states, counters, 1.0F / 60.0F};
                Gfx::RenderTarget2D target(device,
                                           320,
                                           240,
                                           false,
                                           Gfx::SurfaceFormat::Color,
                                           Gfx::DepthFormat::Depth24,
                                           0,
                                           Gfx::RenderTargetUsage::PreserveContents);
                device.SetRenderTarget(&target);
                device.Clear(Microsoft::Xna::Framework::Color::Black);
                states.Invalidate();
                pass.Draw(context);
                device.SetRenderTarget(nullptr);

                std::vector<Microsoft::Xna::Framework::Color> pixels(320u * 240u);
                target.GetData(pixels.data(), static_cast<int>(pixels.size()));
                for (const Microsoft::Xna::Framework::Color& pixel : pixels)
                {
                    const int red = pixel.getRProperty();
                    litPixels += red != 0 || pixel.getGProperty() != 0 || pixel.getBProperty() != 0 ? 1 : 0;
                    brightestRed = std::max(brightestRed, red);
                }

                glows = pass.GlowsDrawn();
                EXPECT_EQ(glows, 1u);
                const Gfx::BlendState& blend = device.getBlendStateProperty();
                EXPECT_EQ(blend.getColorSourceBlendProperty(),
                          Gfx::BlendState::Additive.getColorSourceBlendProperty());
                EXPECT_EQ(blend.getColorDestinationBlendProperty(),
                          Gfx::BlendState::Additive.getColorDestinationBlendProperty());
                const Gfx::DepthStencilState& depth = device.getDepthStencilStateProperty();
                EXPECT_TRUE(depth.getDepthBufferEnableProperty());
                EXPECT_FALSE(depth.getDepthBufferWriteEnableProperty());
                ASSERT_NE(counters.Find("transparent.fixtureGlows"), nullptr);
                EXPECT_EQ(counters.Find("transparent.fixtureGlows")->current, 1);

                ASSERT_TRUE(lighting.SetGroupOn(light.group, false));
                frame.frameIndex = 2u;
                lighting.Update(frame);
                EXPECT_FALSE(pass.IsActive());
            });
        host.Run();

        ASSERT_TRUE(host.Ran());
        ASSERT_EQ(host.Failure(), "") << "the real device rejected the additive fixture glow";
        EXPECT_EQ(glows, 1u);
        EXPECT_GT(litPixels, 100) << "the counted glow did not reach the colour target";
        EXPECT_GT(brightestRed, 10) << "the radial texture or alpha suppressed the live source";
    }

    TEST(TransparentPassTests, ItDrawsCellThenObjectWithAlphaBlendAndReadOnlyDepth)
    {
        const world::WorldData materials = TransparentMaterials();
        const world::ChunkLibrary library = TinyGlassHouse();
        std::uint32_t drawn = 0u;
        std::uint32_t triangles = 0u;
        std::uint32_t binds = 0u;

        cnahouse::testsupport::DeviceHost host(
            [&](Gfx::GraphicsDevice& device)
            {
                world::CellRuntime cells(device, library);
                ASSERT_TRUE(cells.Load("FAR_ROOM"));
                ASSERT_TRUE(cells.Load("NEAR_ROOM"));

                cnahouse::visibility::RenderList list;
                const std::array<std::uint32_t, 3> chunks{0u, 1u, 2u};
                list.AddChunks(library, chunks, Vector3(0.0F, 0.5F, 0.0F), &materials);
                const auto items = list.ItemsFor(cnahouse::rendering::Pass::Transparent);
                ASSERT_EQ(items.size(), 3U);
                EXPECT_EQ(items[0].geometry, 1u);
                EXPECT_EQ(items[1].geometry, 0u);
                EXPECT_EQ(items[2].geometry, 2u);

                cnahouse::rendering::Camera camera;
                cnahouse::rendering::StateTracker states(device);
                cnahouse::debug::Counters counters;
                cnahouse::rendering::TransparentPass pass(library, cells, materials, camera, list);
                ASSERT_TRUE(pass.IsActive());
                cnahouse::rendering::PassContext context{device, states, counters, 1.0F / 60.0F};
                pass.Draw(context);

                drawn = pass.ChunksDrawn();
                triangles = pass.TrianglesDrawn();
                binds = pass.MaterialBinds();
                const Gfx::BlendState& blend = device.getBlendStateProperty();
                EXPECT_EQ(blend.getColorSourceBlendProperty(),
                          Gfx::BlendState::AlphaBlend.getColorSourceBlendProperty());
                EXPECT_EQ(blend.getColorDestinationBlendProperty(),
                          Gfx::BlendState::AlphaBlend.getColorDestinationBlendProperty());
                const Gfx::DepthStencilState& depth = device.getDepthStencilStateProperty();
                EXPECT_TRUE(depth.getDepthBufferEnableProperty());
                EXPECT_FALSE(depth.getDepthBufferWriteEnableProperty());
                EXPECT_EQ(device.getRasterizerStateProperty().getCullModeProperty(),
                          Gfx::RasterizerState::CullNone.getCullModeProperty());

                ASSERT_NE(counters.Find("transparent.chunks"), nullptr);
                ASSERT_NE(counters.Find("transparent.triangles"), nullptr);
                ASSERT_NE(counters.Find("transparent.materialBinds"), nullptr);
                EXPECT_EQ(counters.Find("transparent.chunks")->current, 3);
                EXPECT_EQ(counters.Find("transparent.triangles")->current, 3);
                EXPECT_EQ(counters.Find("transparent.materialBinds")->current, 2);
            });
        host.Run();

        ASSERT_TRUE(host.Ran());
        ASSERT_EQ(host.Failure(), "") << "the real device rejected the transparent pass";
        EXPECT_EQ(drawn, 3u);
        EXPECT_EQ(triangles, 3u);
        EXPECT_EQ(binds, 2u) << "adjacent equal materials need no redundant parameter apply";
    }

} // namespace
