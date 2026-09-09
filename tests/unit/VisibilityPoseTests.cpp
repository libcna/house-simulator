// SPDX-License-Identifier: MIT
//
// `HOUSE-00685`. §25.8's twenty-four named poses, checked for being a usable FIXTURE: that each
// one stands where it says it does, in the cell it names, and that the cells it expects to see are
// cells §12 has.
//
// `HOUSE-00686` is what asserts the visible sets themselves. This is the check that has to pass
// first, because a pose whose feet are in a wall produces a visible set too -- and it is somebody
// else's.
#include <algorithm>
#include <filesystem>
#include <set>
#include <string>

#include <gtest/gtest.h>

#include "cnahouse/player/PlayerController.hpp"
#include "cnahouse/world/WorldData.hpp"
#include "cnahouse/world/WorldLoader.hpp"

#include "unit/VisibilityPoses.hpp"

namespace
{
    namespace world = cnahouse::world;
    using cnahouse::testsupport::kVisibilityPoses;
    using cnahouse::testsupport::VisibilityPose;
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

} // namespace

TEST(VisibilityPoseTests, ThereAreTwentyFourOfThemAndEachIsNamedOnce)
{
    EXPECT_EQ(kVisibilityPoses.size(), 24U) << "§25.8 asks for twenty-four";
    std::set<std::string_view> names;
    for (const VisibilityPose& pose : kVisibilityPoses)
    {
        EXPECT_FALSE(pose.name.empty());
        EXPECT_TRUE(names.insert(pose.name).second) << pose.name << " is named twice";
    }
    // Both door states are represented: §65.6's start and the arrangement no player will make.
    const auto open = std::count_if(kVisibilityPoses.begin(),
                                    kVisibilityPoses.end(),
                                    [](const VisibilityPose& pose) { return pose.doorsOpen; });
    EXPECT_GE(open, 4) << "no pose has every door open, so the budget case is not covered";
    EXPECT_LE(open, 20) << "no pose is in §65.6's starting state";
}

TEST(VisibilityPoseTests, EveryPoseStandsInTheCellItNames)
{
    // The check that makes the rest mean anything: §16.4 looks a cell up by the FEET, so a pose
    // 20 mm over a boundary is a pose about the next room, and its expected set would be that
    // room's -- which is a test that passes while describing the wrong thing.
    if (!ContentIsBuilt())
    {
        GTEST_SKIP() << "no deployed world; run tools/ci/build_content.py --only world";
    }
    cnahouse::util::IdRegistry::ResetForTesting();
    const world::WorldData data = LoadWorld();

    for (const VisibilityPose& pose : kVisibilityPoses)
    {
        const cnahouse::util::Id id = cnahouse::util::Intern(std::string(pose.cell));
        const world::Cell* cell = data.FindCell(id);
        ASSERT_NE(cell, nullptr) << pose.name << " names " << pose.cell << ", which §12 has not";

        const bool inside =
            std::any_of(cell->boxes.begin(),
                        cell->boxes.end(),
                        [&pose](const world::Footprint& box) { return box.Contains(pose.x, pose.z); });
        EXPECT_TRUE(inside) << pose.name << " stands at (" << pose.x << ", " << pose.z
                            << "), which is outside " << pose.cell;

        // And on its floor, not in the storey above or below it: §12's levels are 2.9 m apart and
        // a pose one storey out is a pose in a different house.
        const world::Level* level = data.FindLevel(cell->level);
        ASSERT_NE(level, nullptr);
        EXPECT_NEAR(pose.y, level->ffl, 0.01F)
            << pose.name << " is not on " << cnahouse::util::IdRegistry::NameOf(cell->level) << "'s floor";
    }
}

TEST(VisibilityPoseTests, EveryCellAPoseExpectsIsACellAndIncludesItsOwn)
{
    if (!ContentIsBuilt())
    {
        GTEST_SKIP() << "no deployed world";
    }
    cnahouse::util::IdRegistry::ResetForTesting();
    const world::WorldData data = LoadWorld();

    std::size_t total = 0;
    for (const VisibilityPose& pose : kVisibilityPoses)
    {
        std::set<std::string_view> expected;
        for (const std::string_view name : pose.visible)
        {
            if (name.empty())
            {
                continue;
            }
            ++total;
            EXPECT_TRUE(expected.insert(name).second) << pose.name << " expects " << name << " twice";
            EXPECT_NE(data.FindCell(cnahouse::util::Intern(std::string(name))), nullptr)
                << pose.name << " expects " << name << ", which §12 has not";
        }
        // The camera's own cell is always visible -- it is the walk's root -- so a set that leaves
        // it out is an expectation nobody could satisfy.
        EXPECT_TRUE(expected.contains(pose.cell))
            << pose.name << " does not expect to see " << pose.cell << ", which it is standing in";
        // In id order, so a reader can compare two poses without sorting them first.
        std::vector<std::string_view> listed;
        for (const std::string_view name : pose.visible)
        {
            if (!name.empty())
            {
                listed.push_back(name);
            }
        }
        EXPECT_TRUE(std::is_sorted(listed.begin(), listed.end()))
            << pose.name << "'s expected set is not in id order";
    }
    std::printf("  24 poses expect %zu cell sighting(s) between them\n", total);
    EXPECT_GT(total, 60U) << "the poses barely see anything, so they cannot catch over-culling";
}

TEST(VisibilityPoseTests, ThePosesCoverTheHouseAndNotOneCornerOfIt)
{
    // A fixture of twenty-four poses in one room would pass every assertion and prove nothing.
    // §12 has five levels and an outside; the set has to reach all of them.
    if (!ContentIsBuilt())
    {
        GTEST_SKIP() << "no deployed world";
    }
    cnahouse::util::IdRegistry::ResetForTesting();
    const world::WorldData data = LoadWorld();

    std::set<std::string_view> levels;
    int outdoors = 0;
    for (const VisibilityPose& pose : kVisibilityPoses)
    {
        const world::Cell* cell = data.FindCell(cnahouse::util::Intern(std::string(pose.cell)));
        ASSERT_NE(cell, nullptr);
        levels.insert(cnahouse::util::IdRegistry::NameOf(cell->level));
        outdoors += cell->kind == world::CellKind::Exterior ? 1 : 0;
    }
    std::printf("  standing on %zu of §12's levels, %d of them outdoors\n", levels.size(), outdoors);
    EXPECT_GE(levels.size(), 5U) << "§12 has B1, L0, L1, L2 and L3 and the poses miss one";
    EXPECT_GE(outdoors, 3) << "§25.2's outdoor-to-indoor rule needs poses that are outdoors";
}
