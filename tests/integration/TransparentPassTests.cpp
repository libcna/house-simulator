// SPDX-License-Identifier: MIT
//
// `HOUSE-00898`. The unit suite proves the two-level ordering and material classification; this
// test crosses the other boundary with a real device: the sorted slice is submitted under XNA's
// premultiplied blend state with depth testing on and depth writes off.
#include <array>
#include <string>
#include <string_view>
#include <tuple>

#include <gtest/gtest.h>

#include "Microsoft/Xna/Framework/Graphics/BlendState.hpp"
#include "Microsoft/Xna/Framework/Graphics/DepthStencilState.hpp"
#include "Microsoft/Xna/Framework/Graphics/GraphicsDevice.hpp"
#include "Microsoft/Xna/Framework/Graphics/RasterizerState.hpp"

#include "cnahouse/debug/Counters.hpp"
#include "cnahouse/rendering/Camera.hpp"
#include "cnahouse/rendering/StateTracker.hpp"
#include "cnahouse/rendering/TransparentPass.hpp"
#include "cnahouse/util/Ids.hpp"
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
