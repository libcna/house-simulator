// SPDX-License-Identifier: MIT
//
// `HOUSE-00775`, §10.4's third containment layer: **the accessible road ends at x = ±35, and what
// ends it is something you can see.**
//
// §10.4 lists five layers in priority order and calls the fifth -- the playable-volume box -- a
// safety net whose counter is the point rather than the wall. A body walked to the end of the road
// and stopped by that box has found a gap in the first four; a body stopped by a hedge has not.
// So this walks the road, both ways and across it, with the real `PlayerStep`, and asks two
// questions of each walk: where did it stop, and was it §10.3's box that stopped it.
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <memory>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "System/IO/FileAccess.hpp"
#include "System/IO/FileMode.hpp"
#include "System/IO/FileStream.hpp"

#include "cnahouse/physics/BroadPhase.hpp"
#include "cnahouse/physics/CollisionLoader.hpp"
#include "cnahouse/physics/Sweep.hpp"
#include "cnahouse/player/BoundaryGuard.hpp"
#include "cnahouse/player/CellTracker.hpp"
#include "cnahouse/player/PlayerController.hpp"
#include "cnahouse/world/SpatialIndex.hpp"
#include "cnahouse/world/WorldData.hpp"
#include "cnahouse/world/WorldLoader.hpp"

namespace
{
    using cnahouse::physics::BroadPhase;
    using cnahouse::physics::CollisionCell;
    using cnahouse::physics::CollisionLoader;
    using cnahouse::physics::CollisionWorld;
    using cnahouse::player::BoundaryGuard;
    using cnahouse::player::CellTracker;
    using cnahouse::player::InputState;
    using cnahouse::player::kPlayerHalfHeight;
    using cnahouse::player::kPlayerRadius;
    using cnahouse::player::PlayerState;
    using cnahouse::player::PlayerStep;
    using cnahouse::util::IdRegistry;
    using Microsoft::Xna::Framework::Vector3;

    namespace world = cnahouse::world;

    constexpr float kDt = 1.0F / 120.0F;
    /// Long enough to walk 40 m at §43.2's fast walk with time to lean on whatever is there.
    constexpr int kSteps = 120 * 30;
    constexpr float kRise = kPlayerHalfHeight + kPlayerRadius;

    std::string_view Name(cnahouse::util::Id id)
    {
        return IdRegistry::NameOf(id);
    }

    struct Walk
    {
        float reached = 0.0F;
        std::uint64_t escapes = 0u;
        std::string lastCell;
    };

} // namespace

TEST(RoadEndTests, TheRoadEndsInSomethingYouCanSee)
{
    IdRegistry::ResetForTesting();
    const std::string directory = "content/world";
    const std::string collisionPath = directory + "/collision.bin";
    if (!std::filesystem::exists(directory + "/layout.cells.json") || !std::filesystem::exists(collisionPath))
    {
        GTEST_SKIP() << "no deployed world; run tools/ci/build_content.py --only world";
    }

    world::WorldData::Contents contents;
    ASSERT_TRUE(world::WorldLoader::LoadLevels(directory, contents));
    ASSERT_TRUE(world::WorldLoader::LoadCells(directory, contents));
    ASSERT_TRUE(world::WorldLoader::LoadPortals(directory, contents));
    auto built = world::WorldData::Create(std::move(contents));
    ASSERT_TRUE(built) << built.Error().ToString();
    const world::WorldData& data = built.Value();
    const world::SpatialIndex index = world::SpatialIndex::Build(data);

    const std::unique_ptr<System::IO::FileStream> stream(
        new System::IO::FileStream(collisionPath, System::IO::FileMode::Open, System::IO::FileAccess::Read));
    const auto loaded = CollisionLoader::Read(*stream, collisionPath);
    ASSERT_TRUE(loaded) << loaded.Error().Message();
    const CollisionWorld& statics = loaded.Value();

    // The sidewalk outside our own pedestrian gate: the road corridor's near edge, on the ground.
    const auto ground = [&statics](float x, float z)
    { return cnahouse::physics::TerrainAt(statics.terrain, x, z).height; };

    const auto walkTo = [&](float yaw, const char* what, const auto& along)
    {
        BroadPhase broad;
        CellTracker tracker;
        BoundaryGuard guard;
        PlayerState state;
        state.position = Vector3(0.0F, ground(0.0F, 2.4F) + kRise + 0.01F, 2.4F);
        state.yaw = yaw;
        state.fastWalk = true;
        tracker.Forget();
        tracker.Update(data, index, state.position);

        InputState input;
        input.move.Y = 1.0F;
        Walk walk;
        for (int step = 0; step < kSteps; ++step)
        {
            const CollisionCell* cell = statics.Cell(Name(tracker.Current()));
            if (cell == nullptr)
            {
                break;
            }
            state.cellId = cell->id;
            PlayerStep(statics, *cell, broad, state, input, kDt);
            if (guard.Contain(state.position))
            {
                ++walk.escapes;
            }
            tracker.Update(data, index, state.position);
            walk.reached = std::max(walk.reached, along(state.position));
        }
        walk.lastCell = std::string(Name(tracker.Current()));
        walk.escapes = guard.Escapes();
        std::printf("  %-6s reached %.2f m and stopped at (%.2f, %.2f, %.2f) in %s, "
                    "%llu escape(s)\n",
                    what,
                    static_cast<double>(walk.reached),
                    static_cast<double>(state.position.X),
                    static_cast<double>(state.position.Y),
                    static_cast<double>(state.position.Z),
                    walk.lastCell.c_str(),
                    static_cast<unsigned long long>(walk.escapes));
        return walk;
    };

    // §14: yaw 0 looks north (-Z), a quarter turn east (+X), three quarters west.
    const auto byX = [](const Vector3& at) { return std::fabs(at.X); };
    const auto byZ = [](const Vector3& at) { return at.Z; };
    const Walk east = walkTo(1.5707963F, "east", byX);
    const Walk west = walkTo(-1.5707963F, "west", byX);
    const Walk across = walkTo(3.1415927F, "across", byZ);

    // §10.3's corridor is x -35…+35 and z 0…+11.5. The barriers stand at its edges, so a body
    // leaning on one stops a capsule's radius short of it and never reaches the box at ±40 / +12.
    EXPECT_LT(east.reached, 35.0F) << "walked past the east end of the road";
    EXPECT_GT(east.reached, 25.0F) << "stopped long before the end: something is in the road";
    EXPECT_LT(west.reached, 35.0F) << "walked past the west end of the road";
    EXPECT_GT(west.reached, 25.0F) << "stopped long before the end: something is in the road";
    EXPECT_LT(across.reached, 12.0F) << "walked through the far hedge";
    EXPECT_GT(across.reached, 9.0F) << "stopped before crossing the road";

    // The point of the whole thing: §10.4's fifth layer never had to do anything.
    EXPECT_EQ(east.escapes, 0u) << "the east end of the road is an invisible wall";
    EXPECT_EQ(west.escapes, 0u) << "the west end of the road is an invisible wall";
    EXPECT_EQ(across.escapes, 0u) << "the far side of the road is an invisible wall";
}
