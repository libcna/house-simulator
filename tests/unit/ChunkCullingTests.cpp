// SPDX-License-Identifier: MIT
//
// `HOUSE-00672`. §25.1's step 3 for static geometry: the chunks of a visible cell, against that
// cell's own cones.
//
// **A cell being visible is not the same as its geometry being visible**, and this is where most
// of the saving is. The portal walk decides which ROOMS contribute; without this step, a room seen
// through a doorway submits every chunk it has, including the wall the doorway is cut in.
#include <algorithm>
#include <cstdio>
#include <filesystem>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "cnahouse/player/FirstPersonCamera.hpp"
#include "cnahouse/visibility/ChunkCulling.hpp"
#include "cnahouse/visibility/VisibilitySystem.hpp"
#include "cnahouse/world/ChunkReader.hpp"
#include "cnahouse/world/WorldData.hpp"
#include "cnahouse/world/WorldLoader.hpp"

#include "System/IO/FileAccess.hpp"
#include "System/IO/FileMode.hpp"
#include "System/IO/FileStream.hpp"

namespace
{
    using cnahouse::app::FrameContext;
    using cnahouse::player::FirstPersonCamera;
    using cnahouse::player::kPlayerEyeHeight;
    using cnahouse::player::PlayerState;
    using cnahouse::util::IdRegistry;
    using cnahouse::visibility::CameraView;
    using cnahouse::visibility::ChunkCuller;
    using cnahouse::visibility::ClipFrustum;
    using cnahouse::visibility::VisibilitySystem;
    using Microsoft::Xna::Framework::Vector3;

    namespace world = cnahouse::world;

    bool ContentIsBuilt()
    {
        return std::filesystem::exists("content/world/layout.cells.json") &&
               std::filesystem::exists("content/world/chunks.bin");
    }

    world::WorldData LoadWorld()
    {
        world::WorldData::Contents contents;
        EXPECT_TRUE(world::WorldLoader::LoadLevels("content/world", contents).HasValue());
        EXPECT_TRUE(world::WorldLoader::LoadCells("content/world", contents).HasValue());
        EXPECT_TRUE(world::WorldLoader::LoadPortals("content/world", contents).HasValue());
        auto built = world::WorldData::Create(std::move(contents));
        EXPECT_TRUE(built) << built.Error().ToString();
        return std::move(built.Value());
    }

    CameraView Standing(const world::WorldData& data, std::string_view cellName, float yawDegrees)
    {
        CameraView view;
        view.cell = cnahouse::util::Intern(std::string(cellName));
        const world::Cell* cell = data.FindCell(view.cell);
        EXPECT_NE(cell, nullptr) << cellName;
        if (cell == nullptr)
        {
            return view;
        }
        const world::Level* level = data.FindLevel(cell->level);
        const world::Footprint& box = cell->boxes.front();

        PlayerState state;
        state.position = Vector3((box.minX + box.maxX) * 0.5F,
                                 (level == nullptr ? 0.0F : level->ffl) + state.Rise(),
                                 (box.minZ + box.maxZ) * 0.5F);
        state.yaw = yawDegrees * 3.14159265F / 180.0F;
        FirstPersonCamera camera;
        camera.SetAspect(16.0F / 9.0F);
        camera.Update(state, kPlayerEyeHeight, 0.0F);

        view.eye = camera.Pose().eye;
        view.viewProjection = camera.View() * camera.Projection();
        view.frustum = ClipFrustum(camera.Frustum());
        view.nearPlane = camera.Frustum().getNearProperty();
        view.farPlane = camera.Frustum().getFarProperty();
        return view;
    }

    /// The chunks belonging to @p cell, by id -- the same mapping `ChunkCuller` builds, done the
    /// slow way so the test does not read the answer off the thing it is testing.
    std::vector<std::uint32_t> ChunkIndicesOf(const world::ChunkLibrary& library, cnahouse::util::Id cell)
    {
        std::vector<std::uint32_t> indices;
        for (std::uint32_t i = 0; i < library.chunks.size(); ++i)
        {
            const world::Chunk& chunk = library.chunks[i];
            if (chunk.cell < library.cells.size() &&
                cnahouse::util::Intern(library.cells[chunk.cell]) == cell)
            {
                indices.push_back(i);
            }
        }
        return indices;
    }

    FrameContext Frame(std::uint64_t index)
    {
        FrameContext frame;
        frame.frameIndex = index;
        return frame;
    }

} // namespace

TEST(ChunkCullingTests, MostOfAVisibleRoomIsStillThrownAway)
{
    IdRegistry::ResetForTesting();
    if (!ContentIsBuilt())
    {
        GTEST_SKIP() << "no deployed world; run tools/ci/build_content.py --only world";
    }
    const world::WorldData data = LoadWorld();
    auto library = world::ChunkReader::ReadFromTitle("content/world/chunks.bin");
    ASSERT_TRUE(library) << library.Error().ToString();

    VisibilitySystem system(data);
    for (const world::Portal& portal : data.Portals())
    {
        system.SetAperture(portal.id, 1.0F);
    }
    system.SetCamera(Standing(data, "L0_HALL", 0.0F));
    system.Update(Frame(1));

    ChunkCuller culler(*library);
    culler.Cull(system.Visible());

    std::printf("  from L0_HALL: %zu of %zu cells have geometry, %d chunks tested, %d drawn, %d culled "
                "(the house has %zu chunks in total)\n",
                static_cast<std::size_t>(culler.Statistics().cellsTested),
                system.Visible().size(),
                culler.Statistics().chunksTested,
                culler.Statistics().chunksDrawn,
                culler.Statistics().chunksCulled,
                library->chunks.size());

    EXPECT_GT(culler.Statistics().chunksTested, 0);
    EXPECT_GT(culler.Statistics().chunksDrawn, 0) << "a visible room drew nothing at all";
    EXPECT_GT(culler.Statistics().chunksCulled, 0)
        << "every chunk of every visible room was submitted, so this step is doing nothing";
    EXPECT_EQ(culler.Statistics().chunksDrawn + culler.Statistics().chunksCulled,
              culler.Statistics().chunksTested);
    // §71.2 budgets 620 draw calls typically. The static shell is one draw per chunk.
    EXPECT_LT(culler.Chunks().size(), 620U);
}

TEST(ChunkCullingTests, EveryChunkDrawnBelongsToAVisibleCellAndIsInOneOfItsCones)
{
    // The two halves of the claim, checked against the walk's own answer rather than against a
    // count: nothing from a culled room, and nothing the room's cones cannot see.
    IdRegistry::ResetForTesting();
    if (!ContentIsBuilt())
    {
        GTEST_SKIP() << "no deployed world";
    }
    const world::WorldData data = LoadWorld();
    auto library = world::ChunkReader::ReadFromTitle("content/world/chunks.bin");
    ASSERT_TRUE(library) << library.Error().ToString();

    VisibilitySystem system(data);
    for (const world::Portal& portal : data.Portals())
    {
        system.SetAperture(portal.id, 1.0F);
    }
    ChunkCuller culler(*library);

    for (const char* room : {"L0_HALL", "L0_KITCHEN", "L1_LANDING", "B1_CINEMA"})
    {
        for (const float yaw : {0.0F, 90.0F, 180.0F, 270.0F})
        {
            system.SetCamera(Standing(data, room, yaw));
            system.Update(Frame(2));
            culler.Cull(system.Visible());

            for (const std::uint32_t index : culler.Chunks())
            {
                const world::Chunk& chunk = library->chunks[index];
                ASSERT_LT(chunk.cell, library->cells.size());
                const cnahouse::util::Id cell = cnahouse::util::Intern(library->cells[chunk.cell]);
                const auto* visible = system.Find(cell);
                ASSERT_NE(visible, nullptr)
                    << IdRegistry::NameOf(cell) << " is not visible but one of its chunks was drawn";

                bool inACone = false;
                for (std::size_t i = 0; i < visible->frustumCount; ++i)
                {
                    inACone = inACone || visible->frusta[i].Intersects(chunk.bounds);
                }
                EXPECT_TRUE(inACone) << "chunk " << index << " of " << IdRegistry::NameOf(cell)
                                     << " is in none of its cell's cones";
            }
        }
    }
}

TEST(ChunkCullingTests, AChunkVisibleThroughTheSECONDDoorwayIsDrawn)
{
    // A room seen through two openings at once keeps up to four cones (`kMaxFrustaPerCell`), and a
    // chunk in ANY of them is on screen. Testing only the first would cull the half of that room
    // which is visible through the other door -- a hole in the world that only appears when two
    // doorways into the same room are in frame, which is most of §12's ground floor.
    IdRegistry::ResetForTesting();
    if (!ContentIsBuilt())
    {
        GTEST_SKIP() << "no deployed world";
    }
    const world::WorldData data = LoadWorld();
    auto library = world::ChunkReader::ReadFromTitle("content/world/chunks.bin");
    ASSERT_TRUE(library) << library.Error().ToString();

    VisibilitySystem system(data);
    for (const world::Portal& portal : data.Portals())
    {
        system.SetAperture(portal.id, 1.0F);
    }

    ChunkCuller culler(*library);
    int onlyLater = 0;
    int multiCone = 0;
    for (const world::Cell& cell : data.Cells())
    {
        for (const float yaw : {0.0F, 90.0F, 180.0F, 270.0F})
        {
            system.SetCamera(Standing(data, IdRegistry::NameOf(cell.id), yaw));
            system.Update(Frame(1));
            culler.Cull(system.Visible());
            const std::span<const std::uint32_t> drawn = culler.Chunks();
            for (const auto& seen : system.Visible())
            {
                if (seen.frustumCount < 2)
                {
                    continue;
                }
                ++multiCone;
                for (const std::uint32_t index : ChunkIndicesOf(*library, seen.cell))
                {
                    const world::Chunk& chunk = library->chunks[index];
                    if (seen.frusta[0].Intersects(chunk.bounds))
                    {
                        continue;
                    }
                    for (std::size_t i = 1; i < seen.frustumCount; ++i)
                    {
                        if (!seen.frusta[i].Intersects(chunk.bounds))
                        {
                            continue;
                        }
                        ++onlyLater;
                        // ...and the CULLER has to have kept it. This is the assertion; the count
                        // above only says the case exists in this house.
                        EXPECT_NE(std::find(drawn.begin(), drawn.end(), index), drawn.end())
                            << "chunk " << index << " of " << IdRegistry::NameOf(seen.cell)
                            << " is visible through cone " << i << " and was not drawn";
                        break;
                    }
                }
            }
        }
    }
    std::printf("  %d cell views kept more than one cone; %d chunk(s) were visible ONLY through a "
                "cone other than the first\n",
                multiCone,
                onlyLater);
    EXPECT_GT(onlyLater, 0) << "no chunk in this house is visible only through a second doorway, so "
                               "testing every cone is untested here";
}

TEST(ChunkCullingTests, TurningRoundChangesWhatIsDrawn)
{
    // The whole point, said as a difference: the same room, two headings, two sets of chunks. A
    // culler that returned everything in the visible cells would return the same list both times.
    IdRegistry::ResetForTesting();
    if (!ContentIsBuilt())
    {
        GTEST_SKIP() << "no deployed world";
    }
    const world::WorldData data = LoadWorld();
    auto library = world::ChunkReader::ReadFromTitle("content/world/chunks.bin");
    ASSERT_TRUE(library) << library.Error().ToString();

    VisibilitySystem system(data);
    ChunkCuller culler(*library);

    system.SetCamera(Standing(data, "L0_KITCHEN", 0.0F));
    system.Update(Frame(1));
    culler.Cull(system.Visible());
    const std::vector<std::uint32_t> north(culler.Chunks().begin(), culler.Chunks().end());

    system.SetCamera(Standing(data, "L0_KITCHEN", 180.0F));
    system.Update(Frame(2));
    culler.Cull(system.Visible());
    const std::vector<std::uint32_t> south(culler.Chunks().begin(), culler.Chunks().end());

    EXPECT_FALSE(north.empty());
    EXPECT_FALSE(south.empty());
    EXPECT_NE(north, south) << "looking the other way drew exactly the same chunks";

    // ...and the same pose twice is the same list: the buffer is reused between frames, so a
    // stale entry would show up here.
    system.SetCamera(Standing(data, "L0_KITCHEN", 0.0F));
    system.Update(Frame(3));
    culler.Cull(system.Visible());
    EXPECT_EQ(std::vector<std::uint32_t>(culler.Chunks().begin(), culler.Chunks().end()), north);
}

TEST(ChunkCullingTests, NothingVisibleDrawsNothing)
{
    IdRegistry::ResetForTesting();
    if (!ContentIsBuilt())
    {
        GTEST_SKIP() << "no deployed world";
    }
    auto library = world::ChunkReader::ReadFromTitle("content/world/chunks.bin");
    ASSERT_TRUE(library) << library.Error().ToString();

    ChunkCuller culler(*library);
    culler.Cull({});
    EXPECT_TRUE(culler.Chunks().empty());
    EXPECT_EQ(culler.Statistics().chunksTested, 0);
    EXPECT_EQ(culler.Statistics().cellsTested, 0);
}
