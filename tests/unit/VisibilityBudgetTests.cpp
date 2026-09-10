// SPDX-License-Identifier: MIT
//
// `HOUSE-00690`. §70.3: *"With every door open, the visible cell count from 12 named poses stays
// within budget."*
//
// **With every door OPEN, which is the arrangement no player will ever make.** §65.6 starts them
// shut and a player opens the two or three they walk through; a house with all 62 open is the
// upper bound on an upper bound, and a budget that survives it survives anything the game can
// produce. §71.2 allows 9 typical, 22 worst case and 30 as a hard fail.
#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <iterator>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "cnahouse/player/FirstPersonCamera.hpp"
#include "cnahouse/visibility/ChunkCulling.hpp"
#include "cnahouse/visibility/ExteriorCulling.hpp"
#include "cnahouse/visibility/ExteriorScene.hpp"
#include "cnahouse/visibility/RenderList.hpp"
#include "cnahouse/visibility/VisibilitySystem.hpp"
#include "cnahouse/world/ChunkReader.hpp"
#include "cnahouse/world/WorldData.hpp"
#include "cnahouse/world/WorldLoader.hpp"

#include "unit/VisibilityPoses.hpp"

namespace
{
    namespace world = cnahouse::world;
    namespace visibility = cnahouse::visibility;
    using cnahouse::app::FrameContext;
    using cnahouse::player::FirstPersonCamera;
    using cnahouse::player::kPlayerEyeHeight;
    using cnahouse::player::PlayerState;
    using cnahouse::testsupport::kVisibilityPoses;
    using cnahouse::testsupport::VisibilityPose;
    using cnahouse::util::IdRegistry;
    using cnahouse::visibility::CameraView;
    using cnahouse::visibility::ChunkCuller;
    using cnahouse::visibility::ClipFrustum;
    using cnahouse::visibility::RenderList;
    using cnahouse::visibility::VisibilitySystem;
    using Microsoft::Xna::Framework::Vector3;

    bool ContentIsBuilt()
    {
        return std::filesystem::exists("content/world/layout.cells.json");
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

    CameraView Look(const VisibilityPose& pose)
    {
        PlayerState state;
        state.position = Vector3(pose.x, pose.y + state.Rise(), pose.z);
        state.yaw = pose.yawDegrees * 3.14159265F / 180.0F;
        FirstPersonCamera camera;
        camera.SetAspect(16.0F / 9.0F);
        camera.Update(state, kPlayerEyeHeight, 0.0F);

        CameraView view;
        view.cell = cnahouse::util::Intern(std::string(pose.cell));
        view.eye = camera.Pose().eye;
        view.viewProjection = camera.View() * camera.Projection();
        view.frustum = ClipFrustum(camera.Frustum());
        view.nearPlane = camera.Frustum().getNearProperty();
        view.farPlane = camera.Frustum().getFarProperty();
        return view;
    }

} // namespace

TEST(VisibilityBudgetTests, TheTwelveBudgetPosesStayInsideSection71Point2)
{
    if (!ContentIsBuilt())
    {
        GTEST_SKIP() << "no deployed world; run tools/ci/build_content.py --only world";
    }
    IdRegistry::ResetForTesting();
    const world::WorldData data = LoadWorld();
    VisibilitySystem system(data);
    // Every leafed portal in the house, open.
    for (const world::Portal& portal : data.Portals())
    {
        system.SetAperture(portal.id, 1.0F);
    }

    int poses = 0;
    std::size_t worst = 0;
    const char* worstName = "-";
    std::size_t total = 0;
    for (const VisibilityPose& pose : kVisibilityPoses)
    {
        if (!pose.budget)
        {
            continue;
        }
        ++poses;
        system.SetCamera(Look(pose));
        FrameContext frame;
        frame.frameIndex = 1;
        system.Update(frame);

        const std::size_t cells = system.Visible().size();
        total += cells;
        if (cells > worst)
        {
            worst = cells;
            worstName = pose.name.data();
        }
        // §71.2's hard fail, per pose. The typical and worst-case columns are a budget for the
        // FRAME the game actually draws; this is every door in the house open at the worst corner
        // of it, so the number to hold against is the one that is a failure at any time.
        EXPECT_LE(cells, 30U) << pose.name << " sees " << cells
                              << " cells with every door open, over §71.2's hard fail of 30";
        EXPECT_EQ(system.Stats().cellsDropped, 0)
            << pose.name << " hit §25's own cap and threw a VISIBLE cell away";
    }
    // ...and the doors really ARE open. Run with them shut every count above falls, every
    // assertion still passes, and the clause this test is named after would be doing nothing.
    std::size_t shutTotal = 0;
    for (const world::Portal& portal : data.Portals())
    {
        system.SetAperture(portal.id, 0.0F);
    }
    for (const VisibilityPose& pose : kVisibilityPoses)
    {
        if (!pose.budget)
        {
            continue;
        }
        system.SetCamera(Look(pose));
        FrameContext frame;
        frame.frameIndex = 2;
        system.Update(frame);
        shutTotal += system.Visible().size();
    }
    EXPECT_GT(total, shutTotal) << "the twelve see no more with every door open than with them "
                                   "shut, so they were not run with them open";

    std::printf("  12 budget poses with every door in the house open: worst %s at %zu cells, "
                "mean %.1f (%zu cells in all, against %zu with them shut) "
                "(§71.2: 9 typical, 22 worst, 30 hard)\n",
                worstName,
                worst,
                static_cast<double>(total) / static_cast<double>(poses),
                total,
                shutTotal);
    EXPECT_EQ(poses, 12) << "§70.4 names twelve budget poses and the fixture marks " << poses;
    // And the WORST of the twelve is inside the worst-case column, not merely the hard fail --
    // which is the claim that says the house is built to the budget rather than around it.
    EXPECT_LE(worst, 22U) << worstName << " is over §71.2's worst case of 22";
}

TEST(VisibilityBudgetTests, AndTheDrawCallsAndStateChangesThatFollowFromThem)
{
    // The cells are what §25 controls; the draw calls are what they COST. §71.2 budgets 620 draw
    // calls and 90 state changes, and the two numbers only mean something together: a visible set
    // inside budget whose chunks are not is a frame that still misses.
    if (!ContentIsBuilt())
    {
        GTEST_SKIP() << "no deployed world";
    }
    if (!std::filesystem::exists("content/world/chunks.bin"))
    {
        GTEST_SKIP() << "no chunks.bin";
    }
    IdRegistry::ResetForTesting();
    const world::WorldData data = LoadWorld();
    auto library = world::ChunkReader::ReadFromTitle("content/world/chunks.bin");
    ASSERT_TRUE(library) << library.Error().ToString();

    VisibilitySystem system(data);
    for (const world::Portal& portal : data.Portals())
    {
        system.SetAperture(portal.id, 1.0F);
    }
    ChunkCuller culler(*library);
    RenderList list;
    // §25.6's half of the frame (`HOUSE-00700`). Until it existed the draw list WAS the walk's
    // chunks and this test could ask the chunk cull alone; now the outdoors is a second structure
    // over the same file, and a budget measured without it is a budget measured on part of the
    // frame. The union subtracts what the walk already found, exactly as `CnaHouseGame` does.
    const visibility::ExteriorScene scene = visibility::BuildExteriorScene(*library, data);
    visibility::ExteriorCuller exterior;
    std::vector<visibility::ClipFrustum> cones;
    std::vector<std::uint32_t> outdoors;
    std::vector<std::uint32_t> already;
    std::vector<std::uint32_t> extra;

    int worstDraws = 0;
    int worstStates = 0;
    int worstIndoors = 0;
    int posesWithOutdoors = 0;
    std::size_t outdoorChunks = 0;
    const char* worstName = "-";
    for (const VisibilityPose& pose : kVisibilityPoses)
    {
        if (!pose.budget)
        {
            continue;
        }
        const CameraView view = Look(pose);
        system.SetCamera(view);
        FrameContext frame;
        frame.frameIndex = 1;
        system.Update(frame);
        culler.Cull(system.Visible());
        list.Clear();
        list.AddChunks(*library, culler.Chunks(), view.eye);
        visibility::GatherExteriorCones(data, system.Visible(), cones);
        outdoors.clear();
        if (!cones.empty() && !scene.Empty())
        {
            exterior.Cull(scene.bvh, cones, view.eye);
            for (const std::uint32_t instance : exterior.Instances())
            {
                outdoors.push_back(scene.ChunkOf(instance));
            }
            std::sort(outdoors.begin(), outdoors.end());
        }
        already.assign(culler.Chunks().begin(), culler.Chunks().end());
        std::sort(already.begin(), already.end());
        extra.clear();
        std::set_difference(
            outdoors.begin(), outdoors.end(), already.begin(), already.end(), std::back_inserter(extra));
        const int indoors = list.DrawCalls();
        list.AddChunks(*library, extra, view.eye);
        list.Sort();
        posesWithOutdoors += extra.empty() ? 0 : 1;
        outdoorChunks += extra.size();

        EXPECT_LE(list.DrawCalls(), 620) << pose.name << " is over §71.2's 620 draw calls";
        EXPECT_LE(list.StateChanges(), 90) << pose.name << " is over §71.2's 90 state changes";
        if (list.DrawCalls() > worstDraws)
        {
            worstDraws = list.DrawCalls();
            worstStates = list.StateChanges();
            worstIndoors = indoors;
            worstName = pose.name.data();
        }
    }
    std::printf("  the worst of the twelve is %s: %d draw call(s) and %d state change(s) "
                "(§71.2: 620 and 90); %d of those are §25.6's outdoors, which reached %d of the "
                "12 poses for %zu chunk(s) in all\n",
                worstName,
                worstDraws,
                worstStates,
                worstDraws - worstIndoors,
                posesWithOutdoors,
                outdoorChunks);
    EXPECT_GT(worstDraws, 0) << "no pose drew anything, so nothing was budgeted";
    // Both halves of the frame are in that number. A run where the outdoors reached nobody would
    // be measuring the walk alone and would pass every budget above without saying so.
    EXPECT_GT(posesWithOutdoors, 0) << "§25.6 contributed to no budget pose, so this measures half "
                                       "the frame";
    EXPECT_GT(outdoorChunks, 0U);
    // ...and it reached the WORST pose, which is the one the budget is read off. Counting what
    // §25.6 found is not the same as drawing it: this compares the list's own two totals, so a
    // union that computes the right set and then submits nothing fails here and nowhere else.
    EXPECT_GT(worstDraws, worstIndoors)
        << worstName << " draws " << worstDraws << " and the walk alone accounts for all of them, "
        << "so the outdoors was gathered and not submitted";
    // A worst pose that drew most of the house would mean the walk is not earning its keep at the
    // poses a budget is measured at. The house's chunk count is ASKED of the file rather than
    // written down (`HOUSE-00485`): the shell is generated, and a number in a test is a number the
    // next person to change a wall has to edit.
    const int whole = static_cast<int>(library->chunks.size());
    ASSERT_GT(whole, 100) << "the chunk library is not the house";
    EXPECT_LT(worstDraws, whole / 2) << "the busiest budget pose draws half the house";
}
