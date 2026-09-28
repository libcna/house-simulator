// SPDX-License-Identifier: MIT
//
// `HOUSE-00899`. `MaterialBinderTests` pins AlphaTestEffect's cutoff and texture parameters; this
// test crosses the pass boundary with a live device and proves the surviving texels use opaque
// depth semantics before transparency.
#include <array>
#include <string_view>

#include <gtest/gtest.h>

#include "Microsoft/Xna/Framework/Graphics/BlendState.hpp"
#include "Microsoft/Xna/Framework/Graphics/DepthStencilState.hpp"
#include "Microsoft/Xna/Framework/Graphics/GraphicsDevice.hpp"
#include "Microsoft/Xna/Framework/Graphics/RasterizerState.hpp"
#include "Microsoft/Xna/Framework/Graphics/Texture2D.hpp"

#include "cnahouse/debug/Counters.hpp"
#include "cnahouse/rendering/AlphaTestPass.hpp"
#include "cnahouse/rendering/Camera.hpp"
#include "cnahouse/rendering/MaterialBinder.hpp"
#include "cnahouse/rendering/StateTracker.hpp"
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

    world::WorldData MaskMaterial()
    {
        world::WorldData::Contents contents;
        world::MaterialDef material;
        material.id = cnahouse::util::Intern("MAT_TEST_LEAVES");
        material.albedo = "Textures/Test/leaves";
        material.alphaMode = world::AlphaMode::Mask;
        material.alphaCutoff = 0.5F;
        material.twoSided = true;
        material.effectTierS = world::EffectTier::AlphaTest;
        contents.materials.push_back(material);
        auto built = world::WorldData::Create(std::move(contents));
        EXPECT_TRUE(built) << built.Error().ToString();
        return std::move(built.Value());
    }

    world::ChunkLibrary TwoLeafCards()
    {
        world::ChunkLibrary library;
        library.cells = {"GARDEN"};
        library.materials = {"MAT_TEST_LEAVES"};
        for (float x : {1.0F, 2.0F})
        {
            world::Chunk chunk;
            chunk.cell = 0u;
            chunk.material = 0u;
            chunk.layout = world::ChunkLayout::AlphaTest;
            chunk.bounds =
                Microsoft::Xna::Framework::BoundingBox(Vector3(x, 0.0F, 0.0F), Vector3(x + 0.5F, 1.0F, 0.0F));
            chunk.vertexCount = 3u;
            chunk.vertices.assign(3u * world::ChunkVertexStride(world::ChunkLayout::AlphaTest), 0u);
            chunk.indexCount = 3u;
            chunk.indices.assign(3u * sizeof(std::uint16_t), 0u);
            library.chunks.push_back(std::move(chunk));
        }
        return library;
    }

    TEST(AlphaTestPassTests, ItBindsOncePerMaterialAndWritesFullOpaqueDepth)
    {
        const world::WorldData materials = MaskMaterial();
        const world::ChunkLibrary library = TwoLeafCards();
        std::uint32_t drawn = 0u;
        std::uint32_t binds = 0u;
        int textureLookups = 0;

        cnahouse::testsupport::DeviceHost host(
            [&](Gfx::GraphicsDevice& device)
            {
                world::CellRuntime cells(device, library);
                ASSERT_TRUE(cells.Load("GARDEN"));
                cnahouse::visibility::RenderList list;
                const std::array<std::uint32_t, 2> chunks{0u, 1u};
                list.AddChunks(library, chunks, Vector3(0.0F, 0.0F, 0.0F), &materials);
                ASSERT_EQ(list.ItemsFor(cnahouse::rendering::Pass::AlphaTest).size(), 2U);

                Gfx::Texture2D albedo(device, 2, 2);
                cnahouse::rendering::Camera camera;
                cnahouse::rendering::StateTracker states(device);
                cnahouse::debug::Counters counters;
                cnahouse::rendering::MaterialBinder binder(device);
                ASSERT_TRUE(binder.RegisterAll(materials.Materials()));
                cnahouse::rendering::AlphaTestPass pass(library,
                                                        cells,
                                                        materials,
                                                        camera,
                                                        list,
                                                        binder,
                                                        [&](std::string_view name)
                                                        {
                                                            ++textureLookups;
                                                            EXPECT_EQ(name, "Textures/Test/leaves");
                                                            return &albedo;
                                                        });
                ASSERT_TRUE(pass.IsActive());
                cnahouse::rendering::PassContext context{device, states, counters, 1.0F / 60.0F};
                pass.Draw(context);

                drawn = pass.ChunksDrawn();
                binds = pass.MaterialBinds();
                const Gfx::DepthStencilState& depth = device.getDepthStencilStateProperty();
                EXPECT_TRUE(depth.getDepthBufferEnableProperty());
                EXPECT_TRUE(depth.getDepthBufferWriteEnableProperty());
                const Gfx::BlendState& blend = device.getBlendStateProperty();
                EXPECT_EQ(blend.getColorSourceBlendProperty(),
                          Gfx::BlendState::Opaque.getColorSourceBlendProperty());
                EXPECT_EQ(blend.getColorDestinationBlendProperty(),
                          Gfx::BlendState::Opaque.getColorDestinationBlendProperty());
                EXPECT_EQ(device.getRasterizerStateProperty().getCullModeProperty(),
                          Gfx::RasterizerState::CullNone.getCullModeProperty());

                ASSERT_NE(counters.Find("alpha.chunks"), nullptr);
                ASSERT_NE(counters.Find("alpha.triangles"), nullptr);
                ASSERT_NE(counters.Find("alpha.materialBinds"), nullptr);
                EXPECT_EQ(counters.Find("alpha.chunks")->current, 2);
                EXPECT_EQ(counters.Find("alpha.triangles")->current, 2);
                EXPECT_EQ(counters.Find("alpha.materialBinds")->current, 1);

                cnahouse::debug::Counters second;
                const auto sentinel = second.Resolve("other.owner");
                second.Set(sentinel, 73);
                cnahouse::rendering::PassContext next{device, states, second, 1.0F / 60.0F};
                pass.Draw(next);
                ASSERT_NE(second.Find("alpha.chunks"), nullptr);
                ASSERT_NE(second.Find("alpha.triangles"), nullptr);
                ASSERT_NE(second.Find("alpha.materialBinds"), nullptr);
                EXPECT_EQ(second.Find("alpha.chunks")->current, 2);
                EXPECT_EQ(second.Find("alpha.triangles")->current, 2);
                EXPECT_EQ(second.Find("alpha.materialBinds")->current, 1);
                EXPECT_EQ(second.Find("other.owner")->current, 73);
            });
        host.Run();

        ASSERT_TRUE(host.Ran());
        ASSERT_EQ(host.Failure(), "") << "the real device rejected the alpha-test pass";
        EXPECT_EQ(drawn, 2u);
        EXPECT_EQ(binds, 1u);
        EXPECT_EQ(textureLookups, 2) << "one lookup per sorted material run on each draw";
    }

} // namespace
