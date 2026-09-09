// SPDX-License-Identifier: MIT
//
// `HOUSE-00673`. §25.1's step 3 for the things that move: a door swinging, a drawer, a chair the
// player has nudged. §17.4 batches the static props of a cell into chunks and excludes these,
// so each one is a draw call of its own and each one is worth a test of its own.
//
// The fixture puts an instance in the middle of every cell of §12's house -- 96 of them, which is
// half of §71.2's typical 180 -- and asks which survive from a given pose.
#include <algorithm>
#include <cstdio>
#include <filesystem>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "cnahouse/player/FirstPersonCamera.hpp"
#include "cnahouse/visibility/InstanceCulling.hpp"
#include "cnahouse/visibility/VisibilitySystem.hpp"
#include "cnahouse/world/WorldData.hpp"
#include "cnahouse/world/WorldLoader.hpp"

namespace
{
    using cnahouse::app::FrameContext;
    using cnahouse::player::FirstPersonCamera;
    using cnahouse::player::kPlayerEyeHeight;
    using cnahouse::player::PlayerState;
    using cnahouse::util::IdRegistry;
    using cnahouse::visibility::CameraView;
    using cnahouse::visibility::ClipFrustum;
    using cnahouse::visibility::DynamicInstance;
    using cnahouse::visibility::InstanceCuller;
    using cnahouse::visibility::VisibilitySystem;
    using Microsoft::Xna::Framework::BoundingSphere;
    using Microsoft::Xna::Framework::Vector3;

    namespace world = cnahouse::world;

    bool WorldIsDeployed()
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

    /// A 0.4 m sphere in the middle of every cell: a chair, roughly, in every room of the house.
    std::vector<DynamicInstance> ChairInEveryRoom(const world::WorldData& data)
    {
        std::vector<DynamicInstance> instances;
        for (const world::Cell& cell : data.Cells())
        {
            const world::Level* level = data.FindLevel(cell.level);
            if (cell.boxes.empty() || level == nullptr)
            {
                continue;
            }
            const world::Footprint& box = cell.boxes.front();
            DynamicInstance instance;
            instance.id = cell.id;
            instance.cell = cell.id;
            instance.bounds = BoundingSphere(
                Vector3((box.minX + box.maxX) * 0.5F, level->ffl + 0.45F, (box.minZ + box.maxZ) * 0.5F),
                0.4F);
            instances.push_back(instance);
        }
        return instances;
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

    FrameContext Frame(std::uint64_t index)
    {
        FrameContext frame;
        frame.frameIndex = index;
        return frame;
    }

} // namespace

TEST(InstanceCullingTests, MostOfTheHousesInstancesAreGoneBeforeAnyPlaneIsTested)
{
    IdRegistry::ResetForTesting();
    if (!WorldIsDeployed())
    {
        GTEST_SKIP() << "no deployed world";
    }
    const world::WorldData data = LoadWorld();
    const std::vector<DynamicInstance> chairs = ChairInEveryRoom(data);
    ASSERT_GT(chairs.size(), 80U) << "§16 has 96 cells and this fixture is one instance per cell";

    VisibilitySystem system(data);
    for (const world::Portal& portal : data.Portals())
    {
        system.SetAperture(portal.id, 1.0F);
    }
    system.SetCamera(Standing(data, "L0_HALL", 0.0F));
    system.Update(Frame(1));

    InstanceCuller culler;
    culler.Cull(system.Visible(), chairs);

    std::printf("  a chair in every room, seen from L0_HALL: %d tested, %d drawn, %d culled with the "
                "room they are in, %d by the room's own cones\n",
                culler.Statistics().tested,
                culler.Statistics().drawn,
                culler.Statistics().culledByCell,
                culler.Statistics().culledByCone);

    EXPECT_EQ(culler.Statistics().tested, static_cast<int>(chairs.size()));
    EXPECT_EQ(culler.Statistics().drawn + culler.Statistics().culledByCell + culler.Statistics().culledByCone,
              culler.Statistics().tested);
    // §25's order is rooms first and things second, and this is why: nearly everything goes with
    // the room, at one id comparison rather than four plane tests.
    EXPECT_GT(culler.Statistics().culledByCell, culler.Statistics().drawn * 4);
    EXPECT_GT(culler.Statistics().drawn, 0) << "the room the camera is standing in has a chair in it";
    // §71.2 budgets 180 dynamic instances typically and 700 at the hard limit.
    EXPECT_LT(culler.Instances().size(), 180U);
}

TEST(InstanceCullingTests, WhatIsDrawnIsInAVisibleCellAndInsideOneOfItsCones)
{
    IdRegistry::ResetForTesting();
    if (!WorldIsDeployed())
    {
        GTEST_SKIP() << "no deployed world";
    }
    const world::WorldData data = LoadWorld();
    const std::vector<DynamicInstance> chairs = ChairInEveryRoom(data);
    VisibilitySystem system(data);
    for (const world::Portal& portal : data.Portals())
    {
        system.SetAperture(portal.id, 1.0F);
    }
    InstanceCuller culler;

    for (const char* room : {"L0_HALL", "L0_KITCHEN", "L1_LANDING", "B1_CINEMA", "EXT_BACKYARD"})
    {
        for (const float yaw : {0.0F, 90.0F, 180.0F, 270.0F})
        {
            system.SetCamera(Standing(data, room, yaw));
            system.Update(Frame(2));
            culler.Cull(system.Visible(), chairs);

            for (const std::uint32_t index : culler.Instances())
            {
                const DynamicInstance& instance = chairs[index];
                const auto* cell = system.Find(instance.cell);
                ASSERT_NE(cell, nullptr) << IdRegistry::NameOf(instance.cell) << " is not visible";
                bool inACone = false;
                for (std::size_t i = 0; i < cell->frustumCount; ++i)
                {
                    inACone = inACone || cell->frusta[i].Intersects(instance.bounds);
                }
                EXPECT_TRUE(inACone) << IdRegistry::NameOf(instance.id) << " is in no cone of its cell";
            }
        }
    }
}

TEST(InstanceCullingTests, AnInstanceVisibleOnlyThroughTheSecondDoorwayIsDrawn)
{
    // The same claim as `HOUSE-00672`'s for chunks: a room seen through two openings keeps up to
    // four cones, and a thing in ANY of them is on screen. Five instances a cell -- the middle and
    // the four corners -- because a chair in the middle of a room is in every cone that sees the
    // room at all, and the interesting case is the one in the corner by the second door.
    IdRegistry::ResetForTesting();
    if (!WorldIsDeployed())
    {
        GTEST_SKIP() << "no deployed world";
    }
    const world::WorldData data = LoadWorld();

    std::vector<DynamicInstance> chairs;
    for (const world::Cell& cell : data.Cells())
    {
        const world::Level* level = data.FindLevel(cell.level);
        if (cell.boxes.empty() || level == nullptr)
        {
            continue;
        }
        const world::Footprint& box = cell.boxes.front();
        for (const auto& [x, z] : {std::pair{0.5F, 0.5F},
                                   std::pair{0.15F, 0.15F},
                                   std::pair{0.85F, 0.15F},
                                   std::pair{0.15F, 0.85F},
                                   std::pair{0.85F, 0.85F}})
        {
            DynamicInstance instance;
            instance.id = cell.id;
            instance.cell = cell.id;
            instance.bounds = BoundingSphere(Vector3(box.minX + (box.maxX - box.minX) * x,
                                                     level->ffl + 0.45F,
                                                     box.minZ + (box.maxZ - box.minZ) * z),
                                             0.4F);
            chairs.push_back(instance);
        }
    }

    VisibilitySystem system(data);
    for (const world::Portal& portal : data.Portals())
    {
        system.SetAperture(portal.id, 1.0F);
    }
    InstanceCuller culler;

    int onlyLater = 0;
    for (const world::Cell& cell : data.Cells())
    {
        for (const float yaw : {0.0F, 90.0F, 180.0F, 270.0F})
        {
            system.SetCamera(Standing(data, IdRegistry::NameOf(cell.id), yaw));
            system.Update(Frame(1));
            culler.Cull(system.Visible(), chairs);
            const std::span<const std::uint32_t> drawn = culler.Instances();

            for (std::uint32_t index = 0; index < chairs.size(); ++index)
            {
                const auto* seen = system.Find(chairs[index].cell);
                if (seen == nullptr || seen->frustumCount < 2 ||
                    seen->frusta[0].Intersects(chairs[index].bounds))
                {
                    continue;
                }
                for (std::size_t i = 1; i < seen->frustumCount; ++i)
                {
                    if (!seen->frusta[i].Intersects(chairs[index].bounds))
                    {
                        continue;
                    }
                    ++onlyLater;
                    EXPECT_NE(std::find(drawn.begin(), drawn.end(), index), drawn.end())
                        << "the instance at index " << index << " in "
                        << IdRegistry::NameOf(chairs[index].cell) << " is visible through cone " << i
                        << " and was culled";
                    break;
                }
            }
        }
    }
    std::printf("  %zu instances over 384 poses: %d were visible ONLY through a cone other than the "
                "first\n",
                chairs.size(),
                onlyLater);
    EXPECT_GT(onlyLater, 0) << "no instance in this house is visible only through a second doorway";
}

TEST(InstanceCullingTests, AChairBehindTheCameraGoesAndTheOneInFrontStays)
{
    // The cone test doing its own work, in the room the camera is standing in -- where the cell
    // test cannot help, because that room is always visible.
    IdRegistry::ResetForTesting();
    if (!WorldIsDeployed())
    {
        GTEST_SKIP() << "no deployed world";
    }
    const world::WorldData data = LoadWorld();
    VisibilitySystem system(data);
    system.SetCamera(Standing(data, "L0_HALL", 0.0F));
    system.Update(Frame(1));

    const world::Cell* hall = data.FindCell(cnahouse::util::Intern("L0_HALL"));
    ASSERT_NE(hall, nullptr);
    const world::Level* level = data.FindLevel(hall->level);
    ASSERT_NE(level, nullptr);
    const world::Footprint& box = hall->boxes.front();
    const float centreX = (box.minX + box.maxX) * 0.5F;
    const float centreZ = (box.minZ + box.maxZ) * 0.5F;

    // Yaw 0 looks north (-Z), so one chair a metre ahead and one a metre behind.
    std::vector<DynamicInstance> chairs;
    DynamicInstance ahead;
    ahead.id = cnahouse::util::Intern("CHAIR_AHEAD");
    ahead.cell = hall->id;
    ahead.bounds = BoundingSphere(Vector3(centreX, level->ffl + 0.45F, centreZ - 1.5F), 0.4F);
    chairs.push_back(ahead);
    DynamicInstance behind = ahead;
    behind.id = cnahouse::util::Intern("CHAIR_BEHIND");
    behind.bounds = BoundingSphere(Vector3(centreX, level->ffl + 0.45F, centreZ + 1.5F), 0.4F);
    chairs.push_back(behind);

    InstanceCuller culler;
    culler.Cull(system.Visible(), chairs);

    ASSERT_EQ(culler.Instances().size(), 1U) << "one of the two chairs should have been culled";
    EXPECT_EQ(culler.Instances()[0], 0U) << "the chair BEHIND the camera was the one kept";
    EXPECT_EQ(culler.Statistics().culledByCone, 1);
    EXPECT_EQ(culler.Statistics().culledByCell, 0) << "both chairs are in the room the camera is in";
}

TEST(InstanceCullingTests, AnInstanceInACellNobodyCanSeeCostsOneComparison)
{
    IdRegistry::ResetForTesting();
    if (!WorldIsDeployed())
    {
        GTEST_SKIP() << "no deployed world";
    }
    const world::WorldData data = LoadWorld();
    VisibilitySystem system(data);
    system.SetCamera(Standing(data, "L0_HALL", 0.0F));
    system.Update(Frame(1));

    std::vector<DynamicInstance> chairs;
    DynamicInstance nowhere;
    nowhere.id = cnahouse::util::Intern("CHAIR_NOWHERE");
    nowhere.cell = cnahouse::util::Intern("NOT_A_CELL");
    nowhere.bounds = BoundingSphere(Vector3(0.0F, 1.0F, -2.0F), 0.4F);
    chairs.push_back(nowhere);

    InstanceCuller culler;
    culler.Cull(system.Visible(), chairs);
    EXPECT_TRUE(culler.Instances().empty()) << "an instance in a cell that does not exist was drawn";
    EXPECT_EQ(culler.Statistics().culledByCell, 1);

    // ...and nothing visible at all is nothing drawn, whatever the instances say.
    culler.Cull({}, chairs);
    EXPECT_TRUE(culler.Instances().empty());
    culler.Cull(system.Visible(), {});
    EXPECT_TRUE(culler.Instances().empty());
    EXPECT_EQ(culler.Statistics().tested, 0);
}
