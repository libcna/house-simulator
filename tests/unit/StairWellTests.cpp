// SPDX-License-Identifier: MIT
//
// `HOUSE-00693`. §70.3: *"the stair well makes three floors visible from the foyer, and the
// basement/attic doors cut their levels off entirely."*
//
// The stair well is the one opening in §12 that joins two STOREYS, and §25 treats it as an
// ordinary portal -- which is the whole reason it needs a test: a system that special-cased floors
// would either stop at one and lose the view up the stair, or not stop at all and draw the house.
#include <algorithm>
#include <cstdio>
#include <set>
#include <string>

#include <gtest/gtest.h>

#include "cnahouse/player/FirstPersonCamera.hpp"
#include "cnahouse/visibility/VisibilitySystem.hpp"

#include "unit/DoorStand.hpp"
#include "unit/VisibilityPoses.hpp"

namespace
{
    namespace world = cnahouse::world;
    using cnahouse::app::FrameContext;
    using cnahouse::player::FirstPersonCamera;
    using cnahouse::player::kPlayerEyeHeight;
    using cnahouse::player::PlayerState;
    using cnahouse::testsupport::ContentIsBuilt;
    using cnahouse::testsupport::kVisibilityPoses;
    using cnahouse::testsupport::LoadWorld;
    using cnahouse::testsupport::VisibilityPose;
    using cnahouse::util::Id;
    using cnahouse::util::IdRegistry;
    using cnahouse::visibility::CameraView;
    using cnahouse::visibility::ClipFrustum;
    using cnahouse::visibility::VisibilitySystem;
    using cnahouse::visibility::VisibleCell;
    using Microsoft::Xna::Framework::Vector3;

    const VisibilityPose& StairPose()
    {
        const auto found =
            std::find_if(kVisibilityPoses.begin(),
                         kVisibilityPoses.end(),
                         [](const VisibilityPose& pose) { return pose.name == "l0-stair-main"; });
        return *found;
    }

    /// The walk from @p pose with every door shut except @p open.
    std::set<std::string>
    SeenWith(const world::WorldData& data, VisibilitySystem& system, const VisibilityPose& pose, Id open)
    {
        for (const world::Portal& portal : data.Portals())
        {
            system.SetAperture(portal.id, portal.id == open ? 1.0F : 0.0F);
        }
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
        system.SetCamera(view);
        FrameContext frame;
        frame.frameIndex = 1;
        system.Update(frame);

        std::set<std::string> names;
        for (const VisibleCell& cell : system.Visible())
        {
            names.insert(std::string(IdRegistry::NameOf(cell.cell)));
        }
        return names;
    }

    std::set<std::string> LevelsOf(const world::WorldData& data, const std::set<std::string>& cells)
    {
        std::set<std::string> levels;
        for (const std::string& name : cells)
        {
            const world::Cell* cell = data.FindCell(cnahouse::util::Intern(name));
            if (cell != nullptr)
            {
                levels.insert(std::string(IdRegistry::NameOf(cell->level)));
            }
        }
        return levels;
    }

} // namespace

TEST(StairWellTests, ThreeStoreysFromTheFootOfTheStairWithEveryDoorShut)
{
    if (!ContentIsBuilt())
    {
        GTEST_SKIP() << "no deployed world; run tools/ci/build_content.py --only world";
    }
    IdRegistry::ResetForTesting();
    const world::WorldData data = LoadWorld();
    VisibilitySystem system(data);

    const std::set<std::string> seen = SeenWith(data, system, StairPose(), Id{});
    const std::set<std::string> levels = LevelsOf(data, seen);
    std::string joined;
    for (const std::string& level : levels)
    {
        joined += (joined.empty() ? "" : " ") + level;
    }
    std::printf("  from the foot of the main stair with every door shut: %zu cell(s) on %zu "
                "storey(s) -- %s\n",
                seen.size(),
                levels.size(),
                joined.c_str());

    // §12's basement, ground floor and first floor, through two stair wells and one cased opening.
    EXPECT_TRUE(levels.contains("B1")) << "the basement is not in view down the stair well";
    EXPECT_TRUE(levels.contains("L0"));
    EXPECT_TRUE(levels.contains("L1")) << "the first floor is not in view up the stair well";
    EXPECT_GE(levels.size(), 3U);
    // ...and no further: §12 has five storeys and a stair well is not a hole through all of them.
    EXPECT_FALSE(levels.contains("L3")) << "the attic is visible from the ground floor";
}

TEST(StairWellTests, TheBasementDoorCutsTheBasementOffAndOpeningItGivesItBack)
{
    // §70.3's second claim, and the finding it carries: the level BOUNDARY is a stair well with no
    // leaf -- `P_L0_STAIR__B1_STAIR`, always open -- so `B1_STAIR` itself is visible from the
    // ground floor whatever anybody does. What cuts the BASEMENT off is the door one step in from
    // it, `P_B1_STAIR__B1_HALL`, and that is the door §70.3 means.
    if (!ContentIsBuilt())
    {
        GTEST_SKIP() << "no deployed world";
    }
    IdRegistry::ResetForTesting();
    const world::WorldData data = LoadWorld();
    VisibilitySystem system(data);

    const Id door = cnahouse::util::Intern("P_B1_STAIR__B1_HALL");
    ASSERT_NE(data.FindPortal(door), nullptr) << "§12 no longer has a door at the foot of the stair";

    // From the ground floor: with the door shut, the basement is exactly its stair -- the cell the
    // always-open well opens into -- and nothing else of it.
    const std::set<std::string> fromAbove = SeenWith(data, system, StairPose(), Id{});
    std::set<std::string> basementShut;
    for (const std::string& name : fromAbove)
    {
        if (name.starts_with("B1_"))
        {
            basementShut.insert(name);
        }
    }
    EXPECT_EQ(basementShut, (std::set<std::string>{"B1_STAIR"}))
        << "the basement's door is shut and more than its stair is visible from the ground floor";

    // And opening it gives the basement back -- asked from where the door can be SEEN. From the
    // ground floor the reduced cone down the well does not reach the door at the foot of it, so
    // opening it changes nothing there; that is the reduction working, not the door failing.
    VisibilityPose atTheDoor{};
    const world::Cell* foot = data.FindCell(cnahouse::util::Intern("B1_STAIR"));
    ASSERT_NE(foot, nullptr);
    const cnahouse::testsupport::Stand stand =
        cnahouse::testsupport::StandBefore(data, *data.FindPortal(door), *foot);
    ASSERT_TRUE(stand.valid) << "there is nowhere to stand in front of the basement door";
    atTheDoor.name = "b1-stair-door";
    atTheDoor.cell = "B1_STAIR";
    atTheDoor.x = stand.feet.X;
    atTheDoor.y = stand.feet.Y;
    atTheDoor.z = stand.feet.Z;
    atTheDoor.yawDegrees = stand.yawDegrees;

    const std::set<std::string> shut = SeenWith(data, system, atTheDoor, Id{});
    const std::set<std::string> open = SeenWith(data, system, atTheDoor, door);
    std::printf("  the basement: %zu cell(s) of it from the ground floor with its door shut; at "
                "the door itself %zu shut and %zu open\n",
                basementShut.size(),
                shut.size(),
                open.size());
    EXPECT_FALSE(shut.contains("B1_HALL"));
    EXPECT_TRUE(open.contains("B1_HALL")) << "opening the basement door gave nothing back";
    EXPECT_GT(open.size(), shut.size());
}

TEST(StairWellTests, TheAtticDoorCutsTheAtticOff)
{
    // The same shape at the other end of the house, and here the door IS on the level boundary's
    // own approach: `P_L2_STAIR__L2_STAIR_ATTIC`. Shut, nothing above the second floor is in view
    // at all -- not the attic stair, not the attic.
    if (!ContentIsBuilt())
    {
        GTEST_SKIP() << "no deployed world";
    }
    IdRegistry::ResetForTesting();
    const world::WorldData data = LoadWorld();
    VisibilitySystem system(data);

    const Id door = cnahouse::util::Intern("P_L2_STAIR__L2_STAIR_ATTIC");
    ASSERT_NE(data.FindPortal(door), nullptr);

    // Standing at the head of the second-floor stair, looking at the attic door.
    const world::Cell* stair = data.FindCell(cnahouse::util::Intern("L2_STAIR_MAIN"));
    ASSERT_NE(stair, nullptr);
    const cnahouse::testsupport::Stand stand =
        cnahouse::testsupport::StandBefore(data, *data.FindPortal(door), *stair);
    ASSERT_TRUE(stand.valid) << "there is nowhere to stand in front of the attic door";

    VisibilityPose pose{};
    pose.name = "l2-attic-door";
    pose.cell = "L2_STAIR_MAIN";
    pose.x = stand.feet.X;
    pose.y = stand.feet.Y;
    pose.z = stand.feet.Z;
    pose.yawDegrees = stand.yawDegrees;

    const std::set<std::string> shut = SeenWith(data, system, pose, Id{});
    const std::set<std::string> open = SeenWith(data, system, pose, door);

    const auto above = [](const std::set<std::string>& cells)
    {
        std::size_t count = 0;
        for (const std::string& name : cells)
        {
            count += (name.starts_with("L3_") || name == "L2_STAIR_ATTIC") ? 1U : 0U;
        }
        return count;
    };
    std::printf("  the attic from the second floor: %zu cell(s) with its door shut, %zu with it "
                "open\n",
                above(shut),
                above(open));
    EXPECT_EQ(above(shut), 0U) << "the attic door is shut and the attic is visible anyway";
    EXPECT_GT(above(open), 0U) << "opening the attic door gave nothing back";
}
