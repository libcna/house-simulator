// SPDX-License-Identifier: MIT
//
// `HOUSE-00679`. §25.7: *"Crossing an exterior door is just another portal traversal, so there is
// no special case."* There is none here either -- §25.2's walk reaches the yards through their
// windows and reduces a cone for each. What is new is the join: those cones are what §25.6's
// hierarchy is culled against, and the whole point of doing it that way is that a window shows a
// slice of the garden and not the garden.
//
// Tested against §12's ACTUAL house, because the claim is about a building: eighteen exterior
// cells, sixty-six windows, and rooms with no way outside at all.
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <set>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "cnahouse/player/FirstPersonCamera.hpp"
#include "cnahouse/visibility/ExteriorCulling.hpp"
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
    using cnahouse::visibility::ExteriorBvh;
    using cnahouse::visibility::ExteriorCones;
    using cnahouse::visibility::ExteriorCuller;
    using cnahouse::visibility::ExteriorInstance;
    using cnahouse::visibility::PropCategory;
    using cnahouse::visibility::VisibilitySystem;
    using cnahouse::visibility::VisibleCell;
    using Microsoft::Xna::Framework::BoundingBox;
    using Microsoft::Xna::Framework::Vector3;

    namespace world = cnahouse::world;

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

    struct Pose
    {
        CameraView view;
        ClipFrustum camera;
    };

    Pose Standing(const world::WorldData& data, std::string_view cellName, float yawDegrees)
    {
        Pose pose;
        pose.view.cell = cnahouse::util::Intern(std::string(cellName));
        const world::Cell* cell = data.FindCell(pose.view.cell);
        EXPECT_NE(cell, nullptr) << cellName;
        if (cell == nullptr)
        {
            return pose;
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

        pose.view.eye = camera.Pose().eye;
        pose.view.viewProjection = camera.View() * camera.Projection();
        pose.view.frustum = ClipFrustum(camera.Frustum());
        pose.view.nearPlane = camera.Frustum().getNearProperty();
        pose.view.farPlane = camera.Frustum().getFarProperty();
        pose.camera = pose.view.frustum;
        return pose;
    }

    FrameContext Frame(std::uint64_t index)
    {
        FrameContext frame;
        frame.frameIndex = index;
        return frame;
    }

    /// A garden's worth of things, spread over §10.2's plot: 400 m across, the house at the middle.
    std::vector<ExteriorInstance> AGarden()
    {
        std::vector<ExteriorInstance> instances;
        std::uint32_t state = 20260909u;
        auto next = [&state](float low, float high)
        {
            state = state * 1664525u + 1013904223u;
            const float unit = static_cast<float>(state >> 8u) / static_cast<float>(1u << 24u);
            return low + unit * (high - low);
        };
        constexpr std::array<PropCategory, 4> kKinds{PropCategory::Tree,
                                                     PropCategory::GardenFurniture,
                                                     PropCategory::Fence,
                                                     PropCategory::NeighbourhoodLod0};
        for (int i = 0; i < 2000; ++i)
        {
            ExteriorInstance instance;
            instance.id = cnahouse::util::Intern("GARDEN_" + std::to_string(i));
            instance.category = kKinds[static_cast<std::size_t>(i) % kKinds.size()];
            const float radius = instance.category == PropCategory::Tree ? 3.0F : 0.8F;
            const Vector3 centre(next(-120.0F, 120.0F), next(0.0F, 5.0F), next(-120.0F, 120.0F));
            instance.bounds = BoundingBox(Vector3(centre.X - radius, centre.Y - radius, centre.Z - radius),
                                          Vector3(centre.X + radius, centre.Y + radius, centre.Z + radius));
            instances.push_back(instance);
        }
        return instances;
    }

    std::set<std::uint32_t> AsSet(std::span<const std::uint32_t> indices)
    {
        return std::set<std::uint32_t>(indices.begin(), indices.end());
    }

} // namespace

TEST(ExteriorTraversalTests, AWindowShowsASliceOfTheGardenAndNotTheGarden)
{
    // The claim §25.2's reduction exists for, made where it is checkable: the cone that arrives in
    // the yard came through a window, so what it draws is a strict subset of what the camera's own
    // frustum would -- and a subset, not a different set, because a reduced cone is contained in
    // the frustum it was reduced from.
    IdRegistry::ResetForTesting();
    if (!ContentIsBuilt())
    {
        GTEST_SKIP() << "no deployed world; run tools/ci/build_content.py --only world";
    }
    const world::WorldData data = LoadWorld();
    ExteriorBvh bvh;
    bvh.Build(AGarden());

    VisibilitySystem system(data);
    ExteriorCones cones;
    ExteriorCuller culler;
    ExteriorCuller reference;

    int posesOutside = 0;
    // Rooms with glass onto the outdoors (`layout.portals.json`): the sunroom's slider, the
    // master bedroom's, the basement gym's and workshop's windows.
    for (const char* room : {"L0_SUNROOM", "L1_MASTER_BED", "B1_GYM", "B1_WORKSHOP"})
    {
        for (const float yaw : {0.0F, 90.0F, 180.0F, 270.0F})
        {
            const Pose pose = Standing(data, room, yaw);
            system.SetCamera(pose.view);
            system.Update(Frame(1));
            cones.Collect(data, system.Visible(), pose.camera);
            if (cones.Cones().empty())
            {
                EXPECT_EQ(cones.CellsOutside(), 0) << room << " reached the outdoors but produced no cone";
                continue;
            }
            ++posesOutside;

            culler.Cull(bvh, cones.Cones(), pose.view.eye);
            const std::array<ClipFrustum, 1> whole{pose.camera};
            reference.Cull(bvh, whole, pose.view.eye);

            const std::set<std::uint32_t> throughTheWindow = AsSet(culler.Instances());
            const std::set<std::uint32_t> throughTheWall = AsSet(reference.Instances());
            EXPECT_TRUE(std::includes(throughTheWall.begin(),
                                      throughTheWall.end(),
                                      throughTheWindow.begin(),
                                      throughTheWindow.end()))
                << room << " at " << yaw
                << " degrees drew something outside the camera's own frustum, which a reduced cone "
                   "cannot contain";
        }
    }
    EXPECT_GT(posesOutside, 0) << "not one of the glazed rooms ever reached the outdoors";
    std::printf("  %d of 16 poses in glazed rooms see outside\n", posesOutside);
}

TEST(ExteriorTraversalTests, TheSunroomsSliderIsNarrowerThanTheRoomsOwnView)
{
    IdRegistry::ResetForTesting();
    if (!ContentIsBuilt())
    {
        GTEST_SKIP() << "no deployed world";
    }
    const world::WorldData data = LoadWorld();
    ExteriorBvh bvh;
    bvh.Build(AGarden());

    VisibilitySystem system(data);
    ExteriorCones cones;
    ExteriorCuller culler;
    ExteriorCuller reference;

    // The one pose that has to work, and the numbers it gives, so a regression is a number and not
    // a boolean: the sunroom is glazed on three sides onto the terrace and the back garden.
    bool narrower = false;
    for (const float yaw : {0.0F, 45.0F, 90.0F, 135.0F, 180.0F, 225.0F, 270.0F, 315.0F})
    {
        const Pose pose = Standing(data, "L0_SUNROOM", yaw);
        system.SetCamera(pose.view);
        system.Update(Frame(2));
        cones.Collect(data, system.Visible(), pose.camera);
        if (cones.Cones().empty())
        {
            continue;
        }
        culler.Cull(bvh, cones.Cones(), pose.view.eye);
        const std::array<ClipFrustum, 1> whole{pose.camera};
        reference.Cull(bvh, whole, pose.view.eye);
        std::printf("  L0_SUNROOM facing %3.0f degrees: %d exterior cell(s), %zu cone(s) "
                    "(%d merged), %zu instance(s) through them against %zu through the walls\n",
                    static_cast<double>(yaw),
                    cones.CellsOutside(),
                    cones.Cones().size(),
                    cones.ConesMerged(),
                    culler.Instances().size(),
                    reference.Instances().size());
        if (culler.Instances().size() < reference.Instances().size())
        {
            narrower = true;
        }
        EXPECT_LE(culler.Instances().size(), reference.Instances().size());
    }
    EXPECT_TRUE(narrower) << "every view out of the sunroom drew as much as no walls at all, so "
                             "the reduction bought nothing";
}

TEST(ExteriorTraversalTests, ARoomWithNoWayOutsideDrawsNoGarden)
{
    // The other half, and the one that matters for the budget: §25.6's hierarchy is only walked
    // when the walk actually got outdoors. A cellar with no window contributes no cone at all, and
    // an empty cone list is not the identity frustum (`ExteriorCullingTests` asserts that
    // difference; this is where it happens for real).
    IdRegistry::ResetForTesting();
    if (!ContentIsBuilt())
    {
        GTEST_SKIP() << "no deployed world";
    }
    const world::WorldData data = LoadWorld();
    ExteriorBvh bvh;
    bvh.Build(AGarden());

    VisibilitySystem system(data);
    ExteriorCones cones;
    ExteriorCuller culler;

    int blind = 0;
    for (const char* room : {"B1_CINEMA", "B1_CELLAR", "L0_PANTRY", "L1_BATH2"})
    {
        for (const float yaw : {0.0F, 90.0F, 180.0F, 270.0F})
        {
            const Pose pose = Standing(data, room, yaw);
            system.SetCamera(pose.view);
            system.Update(Frame(3));
            cones.Collect(data, system.Visible(), pose.camera);
            if (cones.CellsOutside() != 0)
            {
                continue; // A door was open onto a yard; that is not this test's case.
            }
            ++blind;
            EXPECT_TRUE(cones.Cones().empty());
            EXPECT_FALSE(cones.Degraded());
            culler.Cull(bvh, cones.Cones(), pose.view.eye);
            EXPECT_TRUE(culler.Instances().empty())
                << room << " drew part of the garden from inside a windowless room";
            EXPECT_EQ(culler.Statistics().nodesVisited, 0) << "the hierarchy was walked for nothing";
        }
    }
    EXPECT_GT(blind, 0) << "every room tried could see outside, so nothing was proved";
    std::printf("  %d of 16 poses in windowless rooms walk the hierarchy not at all\n", blind);
}

TEST(ExteriorTraversalTests, StandingOutsideTheConeIsTheCamerasOwn)
{
    // From a yard the camera's own cell is exterior, so the walk records it with the unreduced
    // frustum -- and the garden in front of the player is drawn without a window in the way.
    IdRegistry::ResetForTesting();
    if (!ContentIsBuilt())
    {
        GTEST_SKIP() << "no deployed world";
    }
    const world::WorldData data = LoadWorld();
    ExteriorBvh bvh;
    bvh.Build(AGarden());

    VisibilitySystem system(data);
    ExteriorCones cones;
    ExteriorCuller culler;
    ExteriorCuller reference;

    const Pose pose = Standing(data, "EXT_BACKYARD", 180.0F);
    system.SetCamera(pose.view);
    system.Update(Frame(4));
    cones.Collect(data, system.Visible(), pose.camera);

    ASSERT_GT(cones.CellsOutside(), 0);
    ASSERT_FALSE(cones.Cones().empty());
    culler.Cull(bvh, cones.Cones(), pose.view.eye);
    const std::array<ClipFrustum, 1> whole{pose.camera};
    reference.Cull(bvh, whole, pose.view.eye);

    std::printf("  from EXT_BACKYARD: %d exterior cell(s), %zu cone(s), %zu instance(s) against "
                "%zu with no cells at all\n",
                cones.CellsOutside(),
                cones.Cones().size(),
                culler.Instances().size(),
                reference.Instances().size());
    // Standing in it, the camera cell's own cone IS the camera frustum, so everything the frustum
    // holds is drawn: the outdoors is not culled by the room the player is standing in.
    EXPECT_EQ(AsSet(culler.Instances()), AsSet(reference.Instances()));
}

TEST(ExteriorTraversalTests, TooManyConesFallBackToTheCameraAndNotToNothing)
{
    // The degradation, forced. Eighteen exterior cells with four cones each is seventy-two walks
    // of the hierarchy; past the cap the collection stands the CAMERA's frustum in, which contains
    // every cone that was thrown away -- so the frame over-draws the garden and never over-culls
    // it, which is the only direction §25 tolerates.
    IdRegistry::ResetForTesting();
    if (!ContentIsBuilt())
    {
        GTEST_SKIP() << "no deployed world";
    }
    const world::WorldData data = LoadWorld();
    ExteriorBvh bvh;
    bvh.Build(AGarden());

    const Pose pose = Standing(data, "EXT_BACKYARD", 180.0F);
    // One cone per exterior cell, each with a rectangle no other contains, so the containment rule
    // merges none of them and the cap is what stops the list.
    std::vector<VisibleCell> visible;
    float at = -1.0F;
    for (const world::Cell& cell : data.Cells())
    {
        if (cell.kind != world::CellKind::Exterior)
        {
            continue;
        }
        VisibleCell entry;
        entry.cell = cell.id;
        entry.frusta[0] = pose.camera;
        entry.rects[0] = cnahouse::visibility::NdcRect{at, at, at + 0.05F, at + 0.05F};
        at += 0.1F;
        entry.frustumCount = 1;
        visible.push_back(entry);
    }
    ASSERT_GT(visible.size(), ExteriorCones::kMaxCones) << "the house has too few exterior cells "
                                                           "to overflow the cap";

    ExteriorCones cones;
    cones.Collect(data, visible, pose.camera);
    EXPECT_TRUE(cones.Degraded());
    ASSERT_EQ(cones.Cones().size(), 1U) << "the fallback is ONE frustum, the camera's";
    // The count stops where the collection does: it returns the moment the cap overflows, so what
    // it reports is the cells it got through and not the cells there were.
    EXPECT_GT(cones.CellsOutside(), 0);
    EXPECT_LT(cones.CellsOutside(), static_cast<int>(visible.size()))
        << "the collection kept walking cells after it had already given up on their cones";

    // And the fallback draws a SUPERSET of what the first eight cones would have.
    ExteriorCuller degraded;
    ExteriorCuller partial;
    degraded.Cull(bvh, cones.Cones(), pose.view.eye);
    const std::vector<VisibleCell> few(visible.begin(),
                                       visible.begin() + static_cast<long>(ExteriorCones::kMaxCones));
    ExteriorCones under;
    under.Collect(data, few, pose.camera);
    EXPECT_FALSE(under.Degraded());
    partial.Cull(bvh, under.Cones(), pose.view.eye);

    const std::set<std::uint32_t> all = AsSet(degraded.Instances());
    const std::set<std::uint32_t> some = AsSet(partial.Instances());
    EXPECT_TRUE(std::includes(all.begin(), all.end(), some.begin(), some.end()))
        << "the fallback culled something the cones it replaced would have drawn";
}

TEST(ExteriorTraversalTests, AConeAlreadyCoveredIsNotWalkedAgain)
{
    // §25.2's containment approximation, reused: a cone whose screen rectangle is inside one
    // already collected can see nothing more, and walking the whole hierarchy again to find that
    // out is the cost this avoids.
    IdRegistry::ResetForTesting();
    if (!ContentIsBuilt())
    {
        GTEST_SKIP() << "no deployed world";
    }
    const world::WorldData data = LoadWorld();
    const Pose pose = Standing(data, "EXT_BACKYARD", 180.0F);

    std::vector<VisibleCell> visible;
    for (const world::Cell& cell : data.Cells())
    {
        if (cell.kind != world::CellKind::Exterior)
        {
            continue;
        }
        VisibleCell entry;
        entry.cell = cell.id;
        entry.frusta[0] = pose.camera;
        entry.frusta[1] = pose.camera;
        // The second is inside the first, so it adds nothing.
        entry.rects[0] = cnahouse::visibility::NdcRect{-1.0F, -1.0F, 1.0F, 1.0F};
        entry.rects[1] = cnahouse::visibility::NdcRect{-0.2F, -0.2F, 0.2F, 0.2F};
        entry.frustumCount = 2;
        visible.push_back(entry);
        break; // One cell is enough to see the rule fire.
    }
    ASSERT_FALSE(visible.empty());

    ExteriorCones cones;
    cones.Collect(data, visible, pose.camera);
    EXPECT_EQ(cones.Cones().size(), 1U) << "the covered cone was collected anyway";
    EXPECT_EQ(cones.ConesMerged(), 1);
    EXPECT_EQ(cones.CellsOutside(), 1);
    EXPECT_FALSE(cones.Degraded());
}

TEST(ExteriorTraversalTests, AnInteriorCellsConesAreNeverCollected)
{
    // The outdoors is what the hierarchy holds. A kitchen's cone would draw the garden through a
    // wall, and it is the CELL's kind that says so -- not the portal's, and not the cone's.
    IdRegistry::ResetForTesting();
    if (!ContentIsBuilt())
    {
        GTEST_SKIP() << "no deployed world";
    }
    const world::WorldData data = LoadWorld();
    const Pose pose = Standing(data, "L0_KITCHEN", 0.0F);

    std::vector<VisibleCell> visible;
    for (const world::Cell& cell : data.Cells())
    {
        if (cell.kind == world::CellKind::Exterior)
        {
            continue;
        }
        VisibleCell entry;
        entry.cell = cell.id;
        entry.frusta[0] = pose.camera;
        entry.rects[0] = cnahouse::visibility::NdcRect{-1.0F, -1.0F, 1.0F, 1.0F};
        entry.frustumCount = 1;
        visible.push_back(entry);
    }
    ASSERT_GT(visible.size(), 20U);

    ExteriorCones cones;
    cones.Collect(data, visible, pose.camera);
    EXPECT_EQ(cones.CellsOutside(), 0);
    EXPECT_TRUE(cones.Cones().empty()) << "an interior cell contributed a cone to the exterior";
    EXPECT_FALSE(cones.Degraded()) << "and it did not overflow the cap doing it";
}
