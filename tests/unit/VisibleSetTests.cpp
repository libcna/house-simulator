// SPDX-License-Identifier: MIT
//
// `HOUSE-00686`. §25.8: *"a set of 24 named camera poses with expected visible-cell sets, asserted
// exactly."*
//
// **Exactly, and in both directions.** A test that asserted only that the expected cells are
// present would pass a system that culls nothing at all -- the whole house is a superset of every
// answer. One that asserted only that nothing extra is present would pass a system that culls
// everything. §25's two failure modes are over-culling and over-drawing, and set equality is the
// one assertion that sees both.
#include <algorithm>
#include <cstdio>
#include <filesystem>
#include <set>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "cnahouse/player/FirstPersonCamera.hpp"
#include "cnahouse/visibility/VisibilitySystem.hpp"
#include "cnahouse/world/WorldData.hpp"
#include "cnahouse/world/WorldLoader.hpp"

#include "unit/VisibilityPoses.hpp"

namespace
{
    namespace world = cnahouse::world;
    using cnahouse::app::FrameContext;
    using cnahouse::player::FirstPersonCamera;
    using cnahouse::player::kPlayerEyeHeight;
    using cnahouse::player::PlayerState;
    using cnahouse::testsupport::kVisibilityPoses;
    using cnahouse::testsupport::VisibilityPose;
    using cnahouse::util::IdRegistry;
    using cnahouse::visibility::CameraView;
    using cnahouse::visibility::ClipFrustum;
    using cnahouse::visibility::VisibilitySystem;
    using cnahouse::visibility::VisibleCell;
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

    /// One pose's walk, with the house in the door state the pose names.
    void Walk(const world::WorldData& data, const VisibilityPose& pose, VisibilitySystem& system)
    {
        for (const world::Portal& portal : data.Portals())
        {
            system.SetAperture(portal.id, pose.doorsOpen ? 1.0F : 0.0F);
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
    }

    std::set<std::string> Reached(const VisibilitySystem& system)
    {
        std::set<std::string> names;
        for (const VisibleCell& cell : system.Visible())
        {
            names.insert(std::string(IdRegistry::NameOf(cell.cell)));
        }
        return names;
    }

    std::set<std::string> Expected(const VisibilityPose& pose)
    {
        std::set<std::string> names;
        for (const std::string_view name : pose.visible)
        {
            if (!name.empty())
            {
                names.insert(std::string(name));
            }
        }
        return names;
    }

    std::string Join(const std::set<std::string>& names)
    {
        std::string joined;
        for (const std::string& name : names)
        {
            joined += (joined.empty() ? "" : ", ") + name;
        }
        return joined;
    }

} // namespace

TEST(VisibleSetTests, EveryOneOfTheTwentyFourPosesSeesExactlyWhatItSays)
{
    if (!ContentIsBuilt())
    {
        GTEST_SKIP() << "no deployed world; run tools/ci/build_content.py --only world";
    }
    IdRegistry::ResetForTesting();
    const world::WorldData data = LoadWorld();
    VisibilitySystem system(data);

    std::size_t total = 0;
    int deepest = 0;
    for (const VisibilityPose& pose : kVisibilityPoses)
    {
        Walk(data, pose, system);
        deepest = std::max(deepest, system.Stats().maxDepth);
        const std::set<std::string> reached = Reached(system);
        const std::set<std::string> expected = Expected(pose);
        total += reached.size();

        std::set<std::string> missing;
        std::set<std::string> extra;
        std::set_difference(expected.begin(),
                            expected.end(),
                            reached.begin(),
                            reached.end(),
                            std::inserter(missing, missing.end()));
        std::set_difference(reached.begin(),
                            reached.end(),
                            expected.begin(),
                            expected.end(),
                            std::inserter(extra, extra.end()));
        // Named separately, because the two are different bugs: a cell that should be visible and
        // is not is OVER-CULLING -- §25's unforgivable one -- and a cell that should not be and is
        // costs a frame time rather than a picture.
        EXPECT_TRUE(missing.empty()) << pose.name << " did not reach " << Join(missing) << " (over-culling)";
        EXPECT_TRUE(extra.empty()) << pose.name << " also reached " << Join(extra) << " (over-drawing)";
    }
    std::printf("  24 poses reached %zu cell(s) between them, deepest chain %d\n", total, deepest);
    EXPECT_EQ(total, 99U) << "the poses no longer see what `HOUSE-00685` recorded";
    // §25.2's interior cap is six and NOTHING in §12 reaches it: the deepest chain any of the
    // twenty-four produces is four, and the numbers that actually stop a walk here are the
    // back-face test and the area cutoff. Asserted so that a house whose chains got longer -- a
    // door added, a wall removed -- is noticed rather than silently running closer to the cap.
    EXPECT_LE(deepest, 5) << "a chain got longer; §25.2's cap of six is closer than it was";
    EXPECT_GE(deepest, 3) << "no pose chains more than twice, so the depth rules are untested here";
}

TEST(VisibleSetTests, TheCamerasOwnCellIsAlwaysReachedFirstAndAtDepthZero)
{
    if (!ContentIsBuilt())
    {
        GTEST_SKIP() << "no deployed world";
    }
    IdRegistry::ResetForTesting();
    const world::WorldData data = LoadWorld();
    VisibilitySystem system(data);

    for (const VisibilityPose& pose : kVisibilityPoses)
    {
        Walk(data, pose, system);
        ASSERT_FALSE(system.Visible().empty()) << pose.name;
        EXPECT_EQ(IdRegistry::NameOf(system.Visible().front().cell), pose.cell)
            << pose.name << "'s own cell is not the walk's root";
        EXPECT_EQ(system.Visible().front().depth, 0) << pose.name;
    }
}

TEST(VisibleSetTests, ShuttingEveryDoorNeverAddsACell)
{
    // The direction §25.3 promises: a shut door stops vision, so the set with the doors shut is a
    // SUBSET of the set with them open. Checked pose by pose against both states, which is a claim
    // no single pose can make -- and one that catches an aperture wired backwards, where every
    // number still looks reasonable.
    if (!ContentIsBuilt())
    {
        GTEST_SKIP() << "no deployed world";
    }
    IdRegistry::ResetForTesting();
    const world::WorldData data = LoadWorld();
    VisibilitySystem system(data);

    int wider = 0;
    for (const VisibilityPose& pose : kVisibilityPoses)
    {
        VisibilityPose shut = pose;
        shut.doorsOpen = false;
        VisibilityPose open = pose;
        open.doorsOpen = true;

        Walk(data, shut, system);
        const std::set<std::string> withShut = Reached(system);
        Walk(data, open, system);
        const std::set<std::string> withOpen = Reached(system);

        EXPECT_TRUE(std::includes(withOpen.begin(), withOpen.end(), withShut.begin(), withShut.end()))
            << pose.name << " sees " << Join(withShut) << " with the doors shut and " << Join(withOpen)
            << " with them open";
        wider += withOpen.size() > withShut.size() ? 1 : 0;
    }
    std::printf("  opening every door widens the set at %d of 24 poses\n", wider);
    EXPECT_GT(wider, 8) << "opening every door in the house changed almost nothing, so the "
                           "apertures are not reaching the walk";
}

TEST(VisibleSetTests, TheAnswerIsTheSameEveryTimeItIsAsked)
{
    // §25's set feeds chunk culling, lighting, audio and residency in the same frame. Two runs of
    // one pose that disagreed would make every one of those a coin toss -- and the walk allocates
    // nothing in the steady state, so the second run is the one that reuses the buffers.
    if (!ContentIsBuilt())
    {
        GTEST_SKIP() << "no deployed world";
    }
    IdRegistry::ResetForTesting();
    const world::WorldData data = LoadWorld();
    VisibilitySystem system(data);

    for (const VisibilityPose& pose : kVisibilityPoses)
    {
        Walk(data, pose, system);
        std::vector<std::string> first;
        for (const VisibleCell& cell : system.Visible())
        {
            first.push_back(std::string(IdRegistry::NameOf(cell.cell)));
        }
        Walk(data, pose, system);
        std::vector<std::string> second;
        for (const VisibleCell& cell : system.Visible())
        {
            second.push_back(std::string(IdRegistry::NameOf(cell.cell)));
        }
        // The ORDER too, not only the set: `RenderList` sorts by material and then by chunk index,
        // so a visible set that came out in a different order is a frame submitted in a different
        // order -- which a render fixture compares pixel by pixel.
        EXPECT_EQ(first, second) << pose.name << " gave two different answers";
    }
}
