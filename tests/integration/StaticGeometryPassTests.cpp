// SPDX-License-Identifier: MIT
//
// `HOUSE-00676`. `Pass::OpaqueStatic` draws §25.1's step 5 rather than walking the residency map,
// and the two facts that follow from that are checked here against a real device: it submits what
// the LIST holds, and it survives the list holding something residency does not.
//
// An integration test and not a unit one, for `StateTrackerTests`' reason: the pass exists to talk
// to a `GraphicsDevice`, and a mock in place of one would verify only that the mock and the pass
// agree with each other.
#include <algorithm>
#include <filesystem>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "Microsoft/Xna/Framework/Graphics/BlendState.hpp"
#include "Microsoft/Xna/Framework/Graphics/CompareFunction.hpp"
#include "Microsoft/Xna/Framework/Graphics/DepthStencilState.hpp"
#include "Microsoft/Xna/Framework/Graphics/GraphicsDevice.hpp"
#include "Microsoft/Xna/Framework/Graphics/Texture2D.hpp"

#include "cnahouse/app/FrameTimer.hpp"
#include "cnahouse/debug/Counters.hpp"
#include "cnahouse/environment/SimClock.hpp"
#include "cnahouse/lighting/LightingSystem.hpp"
#include "cnahouse/lighting/ShadingGrid.hpp"
#include "cnahouse/rendering/Camera.hpp"
#include "cnahouse/rendering/MaterialBinder.hpp"
#include "cnahouse/rendering/StateTracker.hpp"
#include "cnahouse/rendering/StaticGeometryPass.hpp"
#include "cnahouse/visibility/RenderList.hpp"
#include "cnahouse/visibility/VisibilitySystem.hpp"
#include "cnahouse/world/CellRuntime.hpp"
#include "cnahouse/world/ChunkData.hpp"
#include "cnahouse/world/WorldLoader.hpp"

#include "integration/DeviceHost.hpp"

namespace
{
    namespace Gfx = Microsoft::Xna::Framework::Graphics;
    using cnahouse::rendering::Camera;
    using cnahouse::rendering::Pass;
    using cnahouse::rendering::StateTracker;
    using cnahouse::rendering::StaticGeometryMode;
    using cnahouse::rendering::StaticGeometryPass;
    using cnahouse::visibility::RenderItem;
    using cnahouse::visibility::RenderList;
    using cnahouse::world::CellRuntime;
    using cnahouse::world::ChunkLayout;
    using cnahouse::world::ChunkLibrary;

    TEST(StaticGeometryPassTests, OutdoorReceiversDoNotInheritAnIndoorCameraLift)
    {
        using cnahouse::rendering::OpaqueReceiverEffectExposure;
        using cnahouse::world::CellKind;
        EXPECT_FLOAT_EQ(OpaqueReceiverEffectExposure(CellKind::Exterior, false, 6.0F), 1.0F);
        EXPECT_FLOAT_EQ(OpaqueReceiverEffectExposure(CellKind::Room, true, 6.0F), 1.0F);
        EXPECT_FLOAT_EQ(OpaqueReceiverEffectExposure(CellKind::Room, false, 6.0F), 6.0F);
        EXPECT_FLOAT_EQ(OpaqueReceiverEffectExposure(CellKind::Garage, false, 2.5F), 2.5F);
        EXPECT_FLOAT_EQ(OpaqueReceiverEffectExposure(CellKind::Exterior, false, 1.0F), 1.0F);
    }

    /// Two cells, two materials, one triangle each -- enough to be drawn and to be counted.
    ChunkLibrary TinyHouse()
    {
        ChunkLibrary library;
        library.cells = {"A_ROOM", "B_ROOM"};
        library.materials = {"BLOCKOUT_floor", "BLOCKOUT_wall"};
        for (const std::uint16_t cell : {std::uint16_t{0}, std::uint16_t{0}, std::uint16_t{1}})
        {
            cnahouse::world::Chunk chunk;
            chunk.cell = cell;
            chunk.material = static_cast<std::uint16_t>(library.chunks.size() % 2u);
            chunk.layout = ChunkLayout::Basic;
            chunk.vertexCount = 3u;
            chunk.vertices.assign(3u * cnahouse::world::ChunkVertexStride(ChunkLayout::Basic), 0u);
            chunk.indexCount = 3u;
            chunk.indices.assign(3u * sizeof(std::uint16_t), 0u);
            library.chunks.push_back(std::move(chunk));
        }
        return library;
    }

    RenderItem StaticItem(std::uint32_t chunk, std::uint16_t material)
    {
        RenderItem item;
        item.pass = Pass::OpaqueStatic;
        item.effect = cnahouse::world::EffectTier::Basic;
        item.material = material;
        item.geometry = chunk;
        return item;
    }

    TEST(StaticGeometryPassTests, ItDrawsWhatTheListHoldsAndSkipsWhatResidencyDoesNot)
    {
        const ChunkLibrary library = TinyHouse();
        std::string failure;
        std::uint32_t drawn = 0u;
        std::uint32_t states = 0u;

        cnahouse::testsupport::DeviceHost host(
            [&](Gfx::GraphicsDevice& device)
            {
                CellRuntime cells(device, library);
                ASSERT_TRUE(cells.Load("A_ROOM")) << "the fixture's first cell did not upload";
                // B_ROOM is deliberately NOT resident, and its chunk is in the list anyway.
                ASSERT_EQ(cells.ResidentChunks(), 2u);

                Camera camera;
                RenderList list;
                list.Add(StaticItem(0u, 0u));
                list.Add(StaticItem(1u, 1u));
                list.Add(StaticItem(2u, 1u)); // B_ROOM: in the list, not on the GPU.
                list.Sort();

                StateTracker tracker(device);
                cnahouse::debug::Counters counters;
                StaticGeometryPass pass(library, cells, camera, list, StaticGeometryMode::DebugBlockout);
                ASSERT_TRUE(pass.IsActive()) << "a list with static items in it is work to do";

                cnahouse::rendering::PassContext context{device, tracker, counters, 1.0f / 60.0f};
                pass.Draw(context);
                drawn = pass.ChunksDrawn();
                states = pass.StateChanges();
            });
        host.Run();
        ASSERT_TRUE(host.Ran()) << "the frame that does the drawing never ran";
        ASSERT_EQ(host.Failure(), "") << "the device rejected something the pass submitted";

        // Two of the three: the third named a chunk whose cell is not resident, and residency and
        // visibility are two answers from different systems -- the frame in between draws the house
        // it has rather than dereferencing a null buffer.
        EXPECT_EQ(drawn, 2u);
        // Two materials, so two binds. Not three, which is what one-per-chunk would be.
        EXPECT_EQ(states, 2u);
    }

    TEST(StaticGeometryPassTests, AnEmptyListIsNotWorkToDo)
    {
        const ChunkLibrary library = TinyHouse();
        bool activeWithNothing = true;
        bool activeWithAnotherPassOnly = true;

        cnahouse::testsupport::DeviceHost host(
            [&](Gfx::GraphicsDevice& device)
            {
                CellRuntime cells(device, library);
                ASSERT_TRUE(cells.Load("A_ROOM"));
                Camera camera;
                RenderList list;
                StaticGeometryPass pass(library, cells, camera, list, StaticGeometryMode::DebugBlockout);
                // The whole house on the GPU and nothing asked for: "ran" and "had nothing to do"
                // are different numbers in §71's overlay, and this is the difference.
                activeWithNothing = pass.IsActive();

                RenderItem other = StaticItem(0u, 0u);
                other.pass = Pass::Transparent;
                list.Add(other);
                activeWithAnotherPassOnly = pass.IsActive();
            });
        host.Run();
        ASSERT_TRUE(host.Ran());
        ASSERT_EQ(host.Failure(), "");
        EXPECT_FALSE(activeWithNothing);
        EXPECT_FALSE(activeWithAnotherPassOnly) << "another pass's item made this one active";
    }

    TEST(StaticGeometryPassTests, ProductionComposesArtificialAndDaylightAtEqualDepth)
    {
        const std::string worldPath = std::string(CNAHOUSE_TEST_CONTENT_ROOT) + "/world";
        if (!std::filesystem::exists(worldPath + "/layout.cells.json"))
        {
            GTEST_SKIP() << "no deployed world; run tools/ci/build_content.py --only world";
        }
        cnahouse::world::WorldData::Contents contents;
        ASSERT_TRUE(cnahouse::world::WorldLoader::LoadLevels(worldPath, contents));
        ASSERT_TRUE(cnahouse::world::WorldLoader::LoadMaterials(worldPath, contents));
        ASSERT_TRUE(cnahouse::world::WorldLoader::LoadCells(worldPath, contents));
        ASSERT_TRUE(cnahouse::world::WorldLoader::LoadPortals(worldPath, contents));
        ASSERT_TRUE(cnahouse::world::WorldLoader::LoadOpenings(worldPath, contents));
        ASSERT_TRUE(cnahouse::world::WorldLoader::LoadLights(worldPath, contents));
        ASSERT_TRUE(cnahouse::world::WorldLoader::LoadInteractables(worldPath, contents));
        ASSERT_TRUE(cnahouse::world::WorldLoader::LoadInitialState(worldPath, contents));
        auto loaded = cnahouse::world::WorldData::Create(std::move(contents));
        ASSERT_TRUE(loaded) << loaded.Error().ToString();
        cnahouse::world::WorldData world = std::move(loaded.Value());
        const cnahouse::world::Cell* kitchen = world.FindCell(cnahouse::util::Id::Of("L0_KITCHEN"));
        ASSERT_NE(kitchen, nullptr);
        ASSERT_TRUE(kitchen->lightmaps.daylight.has_value());
        ASSERT_GE(kitchen->lightmaps.artificial.size(), 2U);
        ASSERT_FALSE(kitchen->lightGroups.empty());
        ASSERT_EQ(kitchen->lightGroups.size(), 4U)
            << "the four-group kitchen is the Tier-S receiver pass-budget worst case";
        const auto primary = std::find_if(kitchen->lightmaps.artificial.begin(),
                                          kitchen->lightmaps.artificial.end(),
                                          [&](const cnahouse::world::CellLightmapGroup& binding)
                                          { return binding.group == kitchen->lightGroups.front(); });
        ASSERT_NE(primary, kitchen->lightmaps.artificial.end());
        const auto secondary = std::find_if(kitchen->lightmaps.artificial.begin(),
                                            kitchen->lightmaps.artificial.end(),
                                            [&](const cnahouse::world::CellLightmapGroup& binding)
                                            { return binding.group != primary->group; });
        ASSERT_NE(secondary, kitchen->lightmaps.artificial.end());
        const cnahouse::world::MaterialDef* floor = world.FindMaterial(kitchen->floorMaterial);
        ASSERT_NE(floor, nullptr);
        ASSERT_EQ(floor->effectTierS, cnahouse::world::EffectTier::DualTexture);

        ChunkLibrary library;
        library.cells = {"L0_KITCHEN"};
        library.materials = {std::string(cnahouse::util::IdRegistry::NameOf(floor->id))};
        cnahouse::world::Chunk chunk;
        chunk.cell = 0U;
        chunk.material = 0U;
        chunk.layout = ChunkLayout::Dual;
        chunk.vertexCount = 3U;
        chunk.vertices.assign(3U * cnahouse::world::ChunkVertexStride(ChunkLayout::Dual), 0U);
        chunk.indexCount = 3U;
        chunk.indices.assign(3U * sizeof(std::uint16_t), 0U);
        library.chunks.push_back(std::move(chunk));

        std::vector<std::string> requested;
        std::vector<std::string> onRequested;
        std::vector<std::string> offRequested;
        std::vector<std::string> allRequested;
        std::uint32_t drawn = 0U;
        cnahouse::testsupport::DeviceHost host(
            [&](Gfx::GraphicsDevice& device)
            {
                CellRuntime cells(device, library);
                ASSERT_TRUE(cells.Load("L0_KITCHEN"));
                cnahouse::visibility::VisibilitySystem visibility(world);
                cnahouse::lighting::ShadingGrid shading = cnahouse::lighting::ShadingGrid::Unshaded();
                cnahouse::environment::SimClock clock;
                cnahouse::environment::CivilTime noon;
                noon.month = 3;
                noon.day = 20;
                noon.hour = 12;
                clock.SetStandard(noon);
                cnahouse::lighting::LightingSystem lighting(world, shading, clock, visibility.Portals());
                ASSERT_TRUE(lighting.SetGroupOn(primary->group, true));
                ASSERT_TRUE(lighting.SetGroupOn(secondary->group, true));
                cnahouse::app::FrameContext frame;
                frame.frameIndex = 1U;
                lighting.Update(frame);

                cnahouse::rendering::MaterialBinder binder(device);
                ASSERT_TRUE(binder.RegisterAll(world.Materials()));
                Gfx::Texture2D albedo(device, 2, 2);
                Gfx::Texture2D lightmap(device, 2, 2);
                Camera camera;
                RenderList list;
                RenderItem item = StaticItem(0U, 0U);
                item.effect = cnahouse::world::EffectTier::DualTexture;
                list.Add(item);
                StateTracker tracker(device);
                cnahouse::debug::Counters counters;
                StaticGeometryPass pass(library,
                                        cells,
                                        world,
                                        lighting,
                                        camera,
                                        list,
                                        binder,
                                        [&](std::string_view name)
                                        {
                                            requested.emplace_back(name);
                                            return name == floor->albedo ? &albedo : &lightmap;
                                        });
                cnahouse::rendering::PassContext context{device, tracker, counters, 1.0F / 60.0F};
                pass.Draw(context);
                drawn = pass.ChunksDrawn();
                EXPECT_EQ(pass.StateChanges(), 4U)
                    << "one neutral opaque floor, two active artificial groups and daylight";
                const Gfx::BlendState& blend = device.getBlendStateProperty();
                EXPECT_EQ(blend.getColorSourceBlendProperty(),
                          Gfx::BlendState::Additive.getColorSourceBlendProperty());
                EXPECT_EQ(blend.getColorDestinationBlendProperty(),
                          Gfx::BlendState::Additive.getColorDestinationBlendProperty());
                const Gfx::DepthStencilState& depth = device.getDepthStencilStateProperty();
                EXPECT_TRUE(depth.getDepthBufferEnableProperty());
                EXPECT_FALSE(depth.getDepthBufferWriteEnableProperty());
                EXPECT_EQ(depth.getDepthBufferFunctionProperty(), Gfx::CompareFunction::Equal);

                onRequested = requested;
                requested.clear();
                ASSERT_TRUE(lighting.SetGroupOn(primary->group, false));
                ASSERT_TRUE(lighting.SetGroupOn(secondary->group, false));
                frame.frameIndex = 2U;
                lighting.Update(frame);
                pass.Draw(context);
                EXPECT_EQ(pass.StateChanges(), 2U)
                    << "with both switches off, only neutral ambient and live daylight remain";
                offRequested = requested;

                requested.clear();
                for (const cnahouse::world::CellLightmapGroup& binding : kitchen->lightmaps.artificial)
                {
                    ASSERT_TRUE(lighting.SetGroupOn(binding.group, true));
                }
                frame.frameIndex = 3U;
                frame.deltaSeconds = 0.5F;
                lighting.Update(frame);
                pass.Draw(context);
                EXPECT_EQ(pass.StateChanges(), 6U)
                    << "the four-group kitchen at noon is floor + four lamps + daylight";
                allRequested = requested;
            });
        host.Run();
        ASSERT_TRUE(host.Ran());
        ASSERT_EQ(host.Failure(), "");
        EXPECT_EQ(drawn, 1U);
        ASSERT_GE(onRequested.size(), 5U);
        EXPECT_NE(std::find(onRequested.begin(), onRequested.end(), floor->albedo), onRequested.end());
        EXPECT_EQ(onRequested[1], "Textures/Fallback/grey")
            << "the opaque ambient floor cannot multiply the primary lamp's spatial bake";
        EXPECT_NE(std::find(onRequested.begin(), onRequested.end(), primary->texture.contentName),
                  onRequested.end())
            << "the active primary lamp still needs its authored UV2 bake";
        EXPECT_NE(std::find(onRequested.begin(), onRequested.end(), secondary->texture.contentName),
                  onRequested.end());
        EXPECT_NE(std::find(onRequested.begin(), onRequested.end(), kitchen->lightmaps.daylight->contentName),
                  onRequested.end())
            << "the live daylight pass did not request the cell-owned LM_DAY atlas";
        ASSERT_GE(offRequested.size(), 3U);
        EXPECT_EQ(offRequested[1], "Textures/Fallback/grey");
        EXPECT_EQ(std::find(offRequested.begin(), offRequested.end(), primary->texture.contentName),
                  offRequested.end())
            << "the dark primary bake must not carry ambient when its switch is off";
        EXPECT_EQ(std::find(offRequested.begin(), offRequested.end(), secondary->texture.contentName),
                  offRequested.end());
        EXPECT_NE(
            std::find(offRequested.begin(), offRequested.end(), kitchen->lightmaps.daylight->contentName),
            offRequested.end());
        ASSERT_GE(allRequested.size(), 7U);
        EXPECT_EQ(allRequested[1], "Textures/Fallback/grey");
        for (const cnahouse::world::CellLightmapGroup& binding : kitchen->lightmaps.artificial)
        {
            EXPECT_NE(std::find(allRequested.begin(), allRequested.end(), binding.texture.contentName),
                      allRequested.end());
        }
        EXPECT_NE(
            std::find(allRequested.begin(), allRequested.end(), kitchen->lightmaps.daylight->contentName),
            allRequested.end());
    }

    TEST(StaticGeometryPassTests, ProductionOuterSkinUsesOnlyItsOutdoorDaylightBake)
    {
        const std::string worldPath = std::string(CNAHOUSE_TEST_CONTENT_ROOT) + "/world";
        if (!std::filesystem::exists(worldPath + "/layout.cells.json"))
        {
            GTEST_SKIP() << "no deployed world; run tools/ci/build_content.py --only world";
        }
        cnahouse::world::WorldData::Contents contents;
        ASSERT_TRUE(cnahouse::world::WorldLoader::LoadLevels(worldPath, contents));
        ASSERT_TRUE(cnahouse::world::WorldLoader::LoadMaterials(worldPath, contents));
        ASSERT_TRUE(cnahouse::world::WorldLoader::LoadCells(worldPath, contents));
        ASSERT_TRUE(cnahouse::world::WorldLoader::LoadPortals(worldPath, contents));
        ASSERT_TRUE(cnahouse::world::WorldLoader::LoadOpenings(worldPath, contents));
        ASSERT_TRUE(cnahouse::world::WorldLoader::LoadLights(worldPath, contents));
        ASSERT_TRUE(cnahouse::world::WorldLoader::LoadInteractables(worldPath, contents));
        ASSERT_TRUE(cnahouse::world::WorldLoader::LoadInitialState(worldPath, contents));
        auto loaded = cnahouse::world::WorldData::Create(std::move(contents));
        ASSERT_TRUE(loaded) << loaded.Error().ToString();
        cnahouse::world::WorldData world = std::move(loaded.Value());
        const cnahouse::world::Cell* foyer = world.FindCell(cnahouse::util::Id::Of("L0_FOYER"));
        const cnahouse::world::MaterialDef* siding =
            world.FindMaterial(cnahouse::util::Id::Of("MAT_SIDING_WARM_WHITE"));
        ASSERT_NE(foyer, nullptr);
        ASSERT_NE(siding, nullptr);
        ASSERT_TRUE(foyer->lightmaps.daylight.has_value());
        ASSERT_FALSE(foyer->lightmaps.artificial.empty())
            << "the test cannot prove room lamps were bypassed without one to bypass";
        ASSERT_EQ(siding->effectTierS, cnahouse::world::EffectTier::DualTexture);

        ChunkLibrary library;
        library.cells = {"L0_FOYER"};
        library.materials = {"MAT_SIDING_WARM_WHITE"};
        cnahouse::world::Chunk chunk;
        chunk.cell = 0U;
        chunk.material = 0U;
        chunk.layout = ChunkLayout::Dual;
        chunk.vertexCount = 3U;
        chunk.vertices.assign(3U * cnahouse::world::ChunkVertexStride(ChunkLayout::Dual), 0U);
        chunk.indexCount = 3U;
        chunk.indices.assign(3U * sizeof(std::uint16_t), 0U);
        library.chunks.push_back(std::move(chunk));

        std::vector<std::string> requested;
        std::uint32_t drawn = 0U;
        std::uint32_t states = 0U;
        Gfx::Blend blend = Gfx::Blend::Zero;
        bool depthWrites = false;
        cnahouse::testsupport::DeviceHost host(
            [&](Gfx::GraphicsDevice& device)
            {
                CellRuntime cells(device, library);
                ASSERT_TRUE(cells.Load("L0_FOYER"));
                cnahouse::visibility::VisibilitySystem visibility(world);
                cnahouse::lighting::ShadingGrid shading = cnahouse::lighting::ShadingGrid::Unshaded();
                cnahouse::environment::SimClock clock;
                cnahouse::environment::CivilTime noon;
                noon.month = 3;
                noon.day = 20;
                noon.hour = 12;
                clock.SetStandard(noon);
                cnahouse::lighting::LightingSystem lighting(world, shading, clock, visibility.Portals());
                for (const cnahouse::util::Id group : foyer->lightGroups)
                {
                    lighting.SetGroupOn(group, true);
                }
                cnahouse::app::FrameContext frame;
                frame.frameIndex = 1U;
                lighting.Update(frame);
                ASSERT_GT(lighting.SkyAmbientColor().X, 0.0F);

                cnahouse::rendering::MaterialBinder binder(device);
                ASSERT_TRUE(binder.RegisterAll(world.Materials()));
                Gfx::Texture2D albedo(device, 2, 2);
                Gfx::Texture2D lightmap(device, 2, 2);
                Camera camera;
                RenderList list;
                RenderItem item = StaticItem(0U, 0U);
                item.effect = cnahouse::world::EffectTier::DualTexture;
                list.Add(item);
                StateTracker tracker(device);
                cnahouse::debug::Counters counters;
                StaticGeometryPass pass(library,
                                        cells,
                                        world,
                                        lighting,
                                        camera,
                                        list,
                                        binder,
                                        [&](std::string_view name)
                                        {
                                            requested.emplace_back(name);
                                            return name == siding->albedo ? &albedo : &lightmap;
                                        });
                cnahouse::rendering::PassContext context{device, tracker, counters, 1.0F / 60.0F};
                pass.Draw(context);
                drawn = pass.ChunksDrawn();
                states = pass.StateChanges();
                blend = device.getBlendStateProperty().getColorDestinationBlendProperty();
                depthWrites = device.getDepthStencilStateProperty().getDepthBufferWriteEnableProperty();
            });
        host.Run();
        ASSERT_TRUE(host.Ran());
        ASSERT_EQ(host.Failure(), "");
        EXPECT_EQ(drawn, 1U);
        EXPECT_EQ(states, 1U) << "room lamps or additive interior daylight reached the outer skin";
        ASSERT_EQ(requested.size(), 2U);
        EXPECT_EQ(requested[0], siding->albedo);
        EXPECT_EQ(requested[1], foyer->lightmaps.daylight->contentName);
        EXPECT_EQ(blend, Gfx::Blend::Zero) << "the outdoor daylight bake was not the opaque base";
        EXPECT_TRUE(depthWrites);
    }

} // namespace
