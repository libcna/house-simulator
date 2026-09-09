// SPDX-License-Identifier: MIT
//
// `HOUSE-00676`. `Pass::OpaqueStatic` draws §25.1's step 5 rather than walking the residency map,
// and the two facts that follow from that are checked here against a real device: it submits what
// the LIST holds, and it survives the list holding something residency does not.
//
// An integration test and not a unit one, for `StateTrackerTests`' reason: the pass exists to talk
// to a `GraphicsDevice`, and a mock in place of one would verify only that the mock and the pass
// agree with each other.
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "Microsoft/Xna/Framework/Graphics/GraphicsDevice.hpp"

#include "cnahouse/debug/Counters.hpp"
#include "cnahouse/rendering/Camera.hpp"
#include "cnahouse/rendering/StateTracker.hpp"
#include "cnahouse/rendering/StaticGeometryPass.hpp"
#include "cnahouse/visibility/RenderList.hpp"
#include "cnahouse/world/CellRuntime.hpp"
#include "cnahouse/world/ChunkData.hpp"

#include "integration/DeviceHost.hpp"

namespace
{
    namespace Gfx = Microsoft::Xna::Framework::Graphics;
    using cnahouse::rendering::Camera;
    using cnahouse::rendering::Pass;
    using cnahouse::rendering::StateTracker;
    using cnahouse::rendering::StaticGeometryPass;
    using cnahouse::visibility::RenderItem;
    using cnahouse::visibility::RenderList;
    using cnahouse::world::CellRuntime;
    using cnahouse::world::ChunkLayout;
    using cnahouse::world::ChunkLibrary;

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
                StaticGeometryPass pass(library, cells, camera, list);
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
                StaticGeometryPass pass(library, cells, camera, list);
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

} // namespace
