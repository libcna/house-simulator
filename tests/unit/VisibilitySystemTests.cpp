// SPDX-License-Identifier: MIT
//
// `HOUSE-00670`. §25's step 2 as a SYSTEM: the camera's cell in, the visible set out, once a frame.
//
// The walk itself is `HOUSE-00668`'s and is tested there. What is tested here is the part that
// only exists because this is a system: that it runs at §7.5's `Visibility` stage, that everything
// downstream reads ONE answer per frame, that a door moving changes it without the camera moving,
// and that a camera standing in the garden is treated as being outside.
#include <algorithm>
#include <cstdio>
#include <filesystem>
#include <string>

#include <gtest/gtest.h>

#include "cnahouse/player/FirstPersonCamera.hpp"
#include "cnahouse/visibility/VisibilitySystem.hpp"
#include "cnahouse/world/WorldData.hpp"
#include "cnahouse/world/WorldLoader.hpp"

namespace
{
    using cnahouse::app::FrameContext;
    using cnahouse::app::UpdateStage;
    using cnahouse::player::FirstPersonCamera;
    using cnahouse::player::kPlayerEyeHeight;
    using cnahouse::player::PlayerState;
    using cnahouse::util::IdRegistry;
    using cnahouse::visibility::CameraView;
    using cnahouse::visibility::ClipFrustum;
    using cnahouse::visibility::VisibilitySystem;
    using Microsoft::Xna::Framework::Vector3;

    namespace world = cnahouse::world;

    bool WorldIsDeployed()
    {
        return std::filesystem::exists("content/world/layout.cells.json");
    }

    world::WorldData Load()
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

    FrameContext Frame(std::uint64_t index)
    {
        FrameContext frame;
        frame.frameIndex = index;
        frame.deltaSeconds = 1.0F / 60.0F;
        return frame;
    }

} // namespace

TEST(VisibilitySystemTests, ItRunsAtSectionSevensVisibilityStage)
{
    IdRegistry::ResetForTesting();
    if (!WorldIsDeployed())
    {
        GTEST_SKIP() << "no deployed world";
    }
    const world::WorldData data = Load();
    const VisibilitySystem system(data);

    EXPECT_EQ(system.Stage(), UpdateStage::Visibility);
    EXPECT_EQ(system.Name(), "visibility");
    EXPECT_EQ(system.Frame(), 0U);
    EXPECT_TRUE(system.Visible().empty()) << "a system that has never updated already has an answer";
}

TEST(VisibilitySystemTests, TheSetIsThisFramesAndIsComputedOnce)
{
    // §25.1's pipeline runs chunk culling, instance culling, lighting, audio and residency off ONE
    // set. The frame index published beside it is how a consumer can tell it is reading this
    // frame's answer rather than the last one's -- which is what a stage ordered before
    // `Visibility` gets.
    IdRegistry::ResetForTesting();
    if (!WorldIsDeployed())
    {
        GTEST_SKIP() << "no deployed world";
    }
    const world::WorldData data = Load();
    VisibilitySystem system(data);
    system.SetCamera(Standing(data, "L0_HALL", 0.0F));

    system.Update(Frame(1));
    EXPECT_EQ(system.Frame(), 1U);
    const std::size_t first = system.Visible().size();
    EXPECT_GT(first, 0U);

    // Reading it again is the same answer, and reading it does not recompute it.
    EXPECT_EQ(system.Visible().size(), first);
    EXPECT_EQ(system.Frame(), 1U);

    system.Update(Frame(2));
    EXPECT_EQ(system.Frame(), 2U);
    EXPECT_EQ(system.Visible().size(), first) << "the same camera gave a different answer next frame";
}

TEST(VisibilitySystemTests, TheNeighbourStaysVisibleInsideTheRenderNearPlaneAtAnOpenThreshold)
{
    IdRegistry::ResetForTesting();
    if (!WorldIsDeployed())
    {
        GTEST_SKIP() << "no deployed world";
    }
    const world::WorldData data = Load();
    VisibilitySystem system(data);
    std::uint64_t frame = 1;
    // The running controller can still report the previous cell while its eye lies on or a
    // few millimetres across the shared plane. The old sampled positions skipped that exact
    // frame, which is where the real GPU sequence still showed a full-screen sky flash.
    for (const bool fromFoyer : {true, false})
    {
        for (const float x : {2.08F, 2.12F, 2.18F, 2.199F, 2.20F, 2.201F, 2.22F, 2.28F, 2.32F})
        {
            if ((fromFoyer && x > 2.22F) || (!fromFoyer && x < 2.18F))
            {
                continue; // cell-tracker hand-off, not a camera assigned a room 12 cm away
            }
            PlayerState state;
            state.position = Vector3(x, 0.60F + state.Rise(), -15.00F);
            state.yaw = (fromFoyer ? 90.0F : 270.0F) * 3.14159265F / 180.0F;
            FirstPersonCamera camera;
            camera.SetAspect(16.0F / 9.0F);
            camera.Update(state, kPlayerEyeHeight, 0.0F);

            CameraView view;
            view.cell = cnahouse::util::Intern(fromFoyer ? "L0_FOYER" : "L0_STAIR_MAIN");
            view.eye = camera.Pose().eye;
            view.viewProjection = camera.View() * camera.Projection();
            view.frustum = ClipFrustum(camera.Frustum());
            view.nearPlane = camera.Frustum().getNearProperty();
            view.farPlane = camera.Frustum().getFarProperty();
            system.SetCamera(view);
            system.Update(Frame(frame++));

            const char* neighbour = fromFoyer ? "L0_STAIR_MAIN" : "L0_FOYER";
            const auto& stats = system.Stats();
            EXPECT_TRUE(system.IsVisible(cnahouse::util::Intern(neighbour)))
                << "eye x=" << x << " from " << (fromFoyer ? "foyer" : "stair") << "; portals tested/crossed "
                << stats.portalsTested << "/" << stats.portalsCrossed << ", facing " << stats.skippedFacing
                << ", clipped " << stats.skippedClipped << ", area " << stats.skippedArea;
            EXPECT_LE(system.Visible().size(), cnahouse::visibility::kMaxVisibleCells);
        }
    }
}

TEST(VisibilitySystemTests, OpeningADoorChangesTheSetWithoutMovingTheCamera)
{
    // §25.3, and the reason the aperture lives here: with the kitchen door shut, *"the kitchen's 5
    // chunks, 140 props, 4 lights and 6 further portals all vanish from consideration in one
    // test"*. The camera does not move; the door does.
    IdRegistry::ResetForTesting();
    if (!WorldIsDeployed())
    {
        GTEST_SKIP() << "no deployed world";
    }
    const world::WorldData data = Load();
    VisibilitySystem system(data);
    system.SetCamera(Standing(data, "L0_HALL", 0.0F));
    system.Update(Frame(1));
    const std::size_t shut = system.Visible().size();

    int opened = 0;
    for (const world::Portal& portal : data.Portals())
    {
        opened += system.SetAperture(portal.id, 1.0F) ? 1 : 0;
    }
    EXPECT_GT(opened, 0) << "not one portal's latch changed, so §25.3's hysteresis is not being used";

    system.Update(Frame(2));
    EXPECT_GT(system.Visible().size(), shut) << "opening every door in the house changed nothing";

    // ...and the aperture is remembered, not just applied.
    for (const world::Portal& portal : data.Portals())
    {
        if (portal.opacity == world::PortalOpacity::OpaqueWhenClosed)
        {
            EXPECT_FLOAT_EQ(system.Aperture(portal.id), 1.0F) << IdRegistry::NameOf(portal.id);
            break;
        }
    }
    // A portal that is not in this world moves nothing and says so.
    EXPECT_FALSE(system.SetAperture(cnahouse::util::Intern("P_NOT_A_PORTAL"), 1.0F));
    EXPECT_FLOAT_EQ(system.Aperture(cnahouse::util::Intern("P_NOT_A_PORTAL")), 0.0F);
}

TEST(VisibilitySystemTests, ACameraInTheGardenIsTreatedAsBeingOutside)
{
    // §25.2's asymmetry: *"standing in the garden you should see one room through a window, not
    // that room plus everything behind its open door"*. Which row of the depth table applies is
    // decided from the camera's own cell, and `EXT_*` cells are `CellKind::Exterior` (§15.3) --
    // so this is the one thing the system decides that the traversal cannot.
    IdRegistry::ResetForTesting();
    if (!WorldIsDeployed())
    {
        GTEST_SKIP() << "no deployed world";
    }
    const world::WorldData data = Load();
    VisibilitySystem system(data);
    for (const world::Portal& portal : data.Portals())
    {
        system.SetAperture(portal.id, 1.0F);
    }

    // Inside, in the deepest-reaching room the house has, and outside in the garden.
    system.SetCamera(Standing(data, "L0_HALL", 0.0F));
    system.Update(Frame(1));
    const int insideDepth = system.Stats().maxDepth;
    const std::size_t insideCells = system.Visible().size();

    system.SetCamera(Standing(data, "EXT_BACKYARD", 180.0F));
    system.Update(Frame(2));
    const int outsideDepth = system.Stats().maxDepth;
    const std::size_t outsideCells = system.Visible().size();

    std::printf("  from L0_HALL: %zu cells to depth %d; from EXT_BACKYARD: %zu cells to depth %d\n",
                insideCells,
                insideDepth,
                outsideCells,
                outsideDepth);

    // §25.2 caps a window from outside at depth 1; this backyard pose enters through glazing and
    // therefore cannot carry the chain farther into the house.
    EXPECT_LE(outsideDepth, 1) << "the exterior glazing row of §25.2's depth table was not applied";

    // ...over every exterior cell in §12's plot and eight headings each, because which rooms one
    // pose happens to see through one window is a fact about where it points. An entrance door may
    // carry a visible chain to depth 6, while glazing remains capped at 1; at least one pose has to
    // be stopped BY a cap, or the sweep is only saying the garden is dull.
    int worstOutside = 0;
    int cappedPoses = 0;
    int poses = 0;
    for (const world::Cell& cell : data.Cells())
    {
        if (cell.kind != world::CellKind::Exterior)
        {
            continue;
        }
        for (int step = 0; step < 8; ++step)
        {
            system.SetCamera(Standing(data, IdRegistry::NameOf(cell.id), static_cast<float>(step) * 45.0F));
            system.Update(Frame(static_cast<std::uint64_t>(10 + poses)));
            worstOutside = std::max(worstOutside, system.Stats().maxDepth);
            cappedPoses += system.Stats().skippedDepth > 0 ? 1 : 0;
            ++poses;
        }
    }
    std::printf("  %d exterior poses: deepest %d, %d of them stopped by §25.2's depth cap\n",
                poses,
                worstOutside,
                cappedPoses);
    EXPECT_LE(worstOutside, 6) << "a camera outdoors walked deeper than §25.2's exterior rows allow";
    EXPECT_GT(cappedPoses, 0) << "the cap never stopped an outdoor walk, so this sweep proves little";
    // The chosen indoor spine reaches several rooms, while the backyard glazing admits only its
    // immediately visible room. Dedicated WindowDepthTests cover this asymmetry house-wide.
    EXPECT_GT(insideDepth, outsideDepth);
    EXPECT_GT(insideCells, outsideCells);
}

TEST(VisibilitySystemTests, AnOpenGarageIsVisibleFromAnObliqueRoadSightline)
{
    IdRegistry::ResetForTesting();
    if (!WorldIsDeployed())
    {
        GTEST_SKIP() << "no deployed world";
    }
    const auto data = Load();
    VisibilitySystem system(data);
    for (const auto& portal : data.Portals())
    {
        system.SetAperture(portal.id, 1.0F);
    }
    PlayerState state;
    state.position = Vector3(0.0F, state.Rise(), 3.0F);
    state.yaw = 39.0F * 3.14159265F / 180.0F;
    FirstPersonCamera camera;
    camera.SetAspect(16.0F / 9.0F);
    camera.Update(state, kPlayerEyeHeight, 0.0F);
    CameraView view;
    view.cell = cnahouse::util::Intern("EXT_ROAD");
    view.eye = camera.Pose().eye;
    view.viewProjection = camera.View() * camera.Projection();
    view.frustum = ClipFrustum(camera.Frustum());
    view.nearPlane = camera.Frustum().getNearProperty();
    view.farPlane = camera.Frustum().getFarProperty();
    system.SetCamera(view);
    system.Update(Frame(1));
    const auto garage = cnahouse::util::Intern("L0_GARAGE");
    EXPECT_TRUE(
        std::ranges::any_of(system.Visible(), [garage](const auto& cell) { return cell.cell == garage; }));
    for (const auto& portal : data.Portals())
    {
        if (portal.kind == world::PortalKind::GarageDoor)
        {
            system.SetAperture(portal.id, 0.0F);
        }
    }
    system.Update(Frame(2));
    EXPECT_FALSE(
        std::ranges::any_of(system.Visible(), [garage](const auto& cell) { return cell.cell == garage; }));
}

TEST(VisibilitySystemTests, AClearGlazedBalconyDoorKeepsTheInteriorSightlineComplete)
{
    IdRegistry::ResetForTesting();
    if (!WorldIsDeployed())
    {
        GTEST_SKIP() << "no deployed world";
    }
    const auto data = Load();
    VisibilitySystem system(data);
    for (const auto& portal : data.Portals())
    {
        system.SetAperture(portal.id, 1.0F);
    }
    PlayerState state;
    state.position = Vector3(-0.55F, 6.55F + state.Rise(), -14.10F);
    FirstPersonCamera camera;
    camera.SetAspect(16.0F / 9.0F);
    camera.Update(state, kPlayerEyeHeight, 0.0F);
    CameraView view;
    view.cell = cnahouse::util::Intern("L2_BALCONY_JULIET");
    view.eye = camera.Pose().eye;
    view.viewProjection = camera.View() * camera.Projection();
    view.frustum = ClipFrustum(camera.Frustum());
    view.nearPlane = camera.Frustum().getNearProperty();
    view.farPlane = camera.Frustum().getFarProperty();
    system.SetCamera(view);
    system.Update(Frame(1));
    const auto landing = cnahouse::util::Intern("L2_LANDING");
    const auto hall = cnahouse::util::Intern("L2_HALL");
    EXPECT_TRUE(system.IsVisible(landing));
    EXPECT_TRUE(system.IsVisible(hall));
    for (const auto& portal : data.Portals())
    {
        system.SetAperture(portal.id, 0.0F);
    }
    system.Update(Frame(2));
    EXPECT_TRUE(system.IsVisible(landing));
    EXPECT_TRUE(system.IsVisible(hall)) << "closed clear glazing must not turn the hallway into sky";
}

TEST(VisibilitySystemTests, AFrameBeforeAnyoneSaysWhereTheCameraIsIsEmptyRatherThanWrong)
{
    IdRegistry::ResetForTesting();
    if (!WorldIsDeployed())
    {
        GTEST_SKIP() << "no deployed world";
    }
    const world::WorldData data = Load();
    VisibilitySystem system(data);

    system.Update(Frame(7));
    EXPECT_TRUE(system.Visible().empty());
    // The frame still moves, so a consumer comparing `Frame()` against its own can tell the
    // difference between "not computed yet" and "computed and empty".
    EXPECT_EQ(system.Frame(), 7U);
}
