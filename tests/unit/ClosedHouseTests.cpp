// SPDX-License-Identifier: MIT
//
// `HOUSE-00689`. §70.3: *"With every door closed, the visible set from `L0_FOYER` is exactly
// { ... } -- a golden list"*, and §16's graph fragmenting as `report_graph.py` predicts.
//
// **Two claims about the same house, and they check each other.** The graph one is about WALKING:
// with every door shut, the only edges are the openings that have no leaf, and the house falls
// apart into 59 pieces. The visible-set one is about SEEING, which is a different graph -- glass
// passes light and a person -- so the two numbers must not agree, and the test says so rather than
// leaving a reader to assume one implies the other.
#include <algorithm>
#include <cstdio>
#include <deque>
#include <filesystem>
#include <set>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "cnahouse/player/FirstPersonCamera.hpp"
#include "cnahouse/visibility/VisibilitySystem.hpp"
#include "cnahouse/world/WorldData.hpp"
#include "cnahouse/world/WorldLoader.hpp"

namespace
{
    namespace world = cnahouse::world;
    using cnahouse::app::FrameContext;
    using cnahouse::player::FirstPersonCamera;
    using cnahouse::player::kPlayerEyeHeight;
    using cnahouse::player::PlayerState;
    using cnahouse::util::Id;
    using cnahouse::util::IdRegistry;
    using cnahouse::visibility::CameraView;
    using cnahouse::visibility::ClipFrustum;
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

    /// `report_graph.py`'s shut graph: *"only the always-open portals are edges -- a cased opening
    /// and a stair well"*. A window is never a way through for a person in either graph.
    bool WalkableWithEveryDoorShut(const world::Portal& portal)
    {
        return portal.kind == world::PortalKind::CasedOpening || portal.kind == world::PortalKind::StairWell;
    }

} // namespace

TEST(ClosedHouseTests, WithEveryDoorShutTheHouseFallsIntoFiftyNinePieces)
{
    if (!ContentIsBuilt())
    {
        GTEST_SKIP() << "no deployed world; run tools/ci/build_content.py --only world";
    }
    IdRegistry::ResetForTesting();
    const world::WorldData data = LoadWorld();

    // Computed here rather than read out of `docs/graph-report.md`: the tool measures the LAYOUT
    // and this measures what the runtime loaded, and the two agreeing is the point.
    std::set<Id> unvisited;
    for (const world::Cell& cell : data.Cells())
    {
        unvisited.insert(cell.id);
    }
    std::size_t components = 0;
    std::size_t largest = 0;
    while (!unvisited.empty())
    {
        ++components;
        std::deque<Id> queue{*unvisited.begin()};
        unvisited.erase(unvisited.begin());
        std::size_t size = 0;
        while (!queue.empty())
        {
            const Id cell = queue.front();
            queue.pop_front();
            ++size;
            for (const std::uint32_t index : data.PortalsOf(cell))
            {
                const world::Portal& portal = data.Portals()[index];
                if (!WalkableWithEveryDoorShut(portal))
                {
                    continue;
                }
                const Id other = portal.cellA == cell ? portal.cellB : portal.cellA;
                if (unvisited.erase(other) != 0)
                {
                    queue.push_back(other);
                }
            }
        }
        largest = std::max(largest, size);
    }
    std::printf("  with every door shut the house is %zu component(s), the largest %zu cell(s)\n",
                components,
                largest);
    // `docs/graph-report.md`'s two numbers, which `report_graph.py` measures from
    // `assets-src/world` and this measures from what the runtime loaded.
    EXPECT_EQ(components, 59U) << "§16's graph no longer fragments the way report_graph.py says";
    EXPECT_EQ(largest, 19U);
}

TEST(ClosedHouseTests, TheGoldenListFromTheFoyerLookingIn)
{
    // §70.3's golden list. From §12.1's foyer looking NORTH into the house with every door shut:
    // the hall through its cased opening, the stair well and both storeys above it, and the porch
    // and the road through the front door's sidelights BEHIND the camera -- which is why they are
    // not here.
    if (!ContentIsBuilt())
    {
        GTEST_SKIP() << "no deployed world";
    }
    IdRegistry::ResetForTesting();
    const world::WorldData data = LoadWorld();
    VisibilitySystem system(data);
    for (const world::Portal& portal : data.Portals())
    {
        system.SetAperture(portal.id, 0.0F);
    }

    PlayerState state;
    state.position = Vector3(0.00F, 0.60F + state.Rise(), -16.30F);
    state.yaw = 0.0F;
    FirstPersonCamera camera;
    camera.SetAspect(16.0F / 9.0F);
    camera.Update(state, kPlayerEyeHeight, 0.0F);

    CameraView view;
    view.cell = cnahouse::util::Intern("L0_FOYER");
    view.eye = camera.Pose().eye;
    view.viewProjection = camera.View() * camera.Projection();
    view.frustum = ClipFrustum(camera.Frustum());
    view.nearPlane = camera.Frustum().getNearProperty();
    view.farPlane = camera.Frustum().getFarProperty();
    system.SetCamera(view);
    FrameContext frame;
    frame.frameIndex = 1;
    system.Update(frame);

    std::set<std::string> reached;
    for (const auto& cell : system.Visible())
    {
        reached.insert(std::string(IdRegistry::NameOf(cell.cell)));
    }
    std::string joined;
    for (const std::string& name : reached)
    {
        joined += (joined.empty() ? "" : " ") + name;
    }
    std::printf("  from L0_FOYER looking north with every door shut: %s\n", joined.c_str());

    // MEASURED, and it is not §70.3's illustrative list -- which ends in an ellipsis and was
    // written before the geometry existed. Every cell §70.3 names and this does not is one the
    // heading cannot see: `L0_STAIR_MAIN` and the two storeys above it are through a cased opening
    // due EAST of the camera (`P_L0_FOYER__L0_STAIR`, plane x = 2.2, centred on the eye's own z),
    // and §44's lens is 102.4 degrees wide, so 90 degrees off the axis is outside it;
    // `L0_LIVING` is west behind a shut double door; `L0_PORCH`, `EXT_ROAD` and the front yards
    // are through the sidelights BEHIND the camera. What is here is the hall through its cased
    // opening and the open plan beyond it.
    const std::set<std::string> golden{"L0_FOYER", "L0_HALL", "L0_KITCHEN", "L0_SUNROOM"};
    EXPECT_EQ(reached, golden);
}
