// SPDX-License-Identifier: MIT
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <memory>
#include <set>
#include <string>

#include <gtest/gtest.h>

#include "System/IO/FileAccess.hpp"
#include "System/IO/FileMode.hpp"
#include "System/IO/FileStream.hpp"

#include "cnahouse/physics/BroadPhase.hpp"
#include "cnahouse/physics/CollisionLoader.hpp"
#include "cnahouse/player/CellTracker.hpp"
#include "cnahouse/player/FilmingTour.hpp"
#include "cnahouse/player/FixedStep.hpp"
#include "cnahouse/util/Json.hpp"
#include "cnahouse/world/SpatialIndex.hpp"
#include "cnahouse/world/WorldLoader.hpp"

TEST(FilmingTourTests, CompleteAuthoredWalkUsesCollisionAndCoversEveryAccessibleCell)
{
    namespace player = cnahouse::player;
    namespace world = cnahouse::world;
    namespace physics = cnahouse::physics;
    using cnahouse::util::IdRegistry;
    using Microsoft::Xna::Framework::Vector3;
    IdRegistry::ResetForTesting();
    const std::string directory = std::string(CNAHOUSE_TEST_CONTENT_ROOT) + "/world";
    world::WorldData::Contents contents;
    ASSERT_TRUE(world::WorldLoader::LoadLevels(directory, contents));
    ASSERT_TRUE(world::WorldLoader::LoadCells(directory, contents));
    ASSERT_TRUE(world::WorldLoader::LoadPortals(directory, contents));
    ASSERT_TRUE(world::WorldLoader::LoadStairs(directory, contents));
    ASSERT_TRUE(world::WorldLoader::LoadInitialState(directory, contents));
    auto data = world::WorldData::Create(std::move(contents));
    ASSERT_TRUE(data);
    const auto spatial = world::SpatialIndex::Build(*data);
    System::IO::FileStream stream(
        directory + "/collision.bin", System::IO::FileMode::Open, System::IO::FileAccess::Read);
    auto collision = physics::CollisionLoader::Read(stream, directory + "/collision.bin");
    ASSERT_TRUE(collision);
    const auto zones = cnahouse::util::JsonDocument::Load("docs/zones.json");
    ASSERT_TRUE(zones);
    std::set<std::string> expected;
    const auto zoneArray = zones->Root().RequireArray("zones");
    ASSERT_TRUE(zoneArray);
    const auto zoneRows = zoneArray->Elements();
    ASSERT_TRUE(zoneRows);
    for (const auto& zone : *zoneRows)
    {
        const auto cells = zone.RequireArray("cells");
        ASSERT_TRUE(cells);
        const auto rows = cells->Elements();
        ASSERT_TRUE(rows);
        for (const auto& row : *rows)
        {
            const auto accessible = row.RequireBool("accessible");
            ASSERT_TRUE(accessible);
            if (*accessible)
            {
                const auto name = row.RequireString("id");
                ASSERT_TRUE(name);
                expected.insert(*name);
            }
        }
    }
    ASSERT_EQ(expected.size(), 90U);
    player::FilmingTour tour;
    ASSERT_TRUE(tour.Load(directory + "/initialstate.json"));
    std::set<std::string> authored;
    for (const auto& point : tour.Route())
    {
        ASSERT_NE(data->FindCell(cnahouse::util::Intern(point.cell)), nullptr);
        if (point.pauseSeconds > 0.0F)
        {
            authored.insert(point.cell);
        }
    }
    ASSERT_EQ(authored, expected);
    player::PlayerState body;
    body.position = data->GetInitialState().player.position + Vector3(0.0F, body.Rise() + 0.02F, 0.0F);
    player::CellTracker tracker;
    tracker.Update(*data, spatial, body.position);
    physics::BroadPhase broad;
    player::LookAngles look;
    std::set<std::string> reached;
    ASSERT_TRUE(tour.Start(body, IdRegistry::NameOf(tracker.Current()), *collision, broad));
    unsigned int steps = 0;
    for (; tour.Active() && steps < 600000U; ++steps)
    {
        const auto* cell = collision->Cell(IdRegistry::NameOf(tracker.Current()));
        ASSERT_NE(cell, nullptr);
        body.cellId = cell->id;
        const auto previousViews = tour.Visited().size();
        const auto input = tour.Step(body, look, player::kFixedStepSeconds);
        if (tour.Visited().size() != previousViews)
        {
            ASSERT_EQ(tour.Visited().back(), IdRegistry::NameOf(tracker.Current()))
                << "an authored view must actually be inside its room";
        }
        ASSERT_LE(std::fabs(input.look.X), 0.00655F);
        ASSERT_FALSE(input.jump);
        ASSERT_FALSE(input.runPressed);
        player::ApplyLook(look, input, true);
        body.yaw = look.yaw;
        const auto old = body.Feet();
        const auto report =
            player::PlayerStep(*collision, *cell, broad, body, input, player::kFixedStepSeconds);
        EXPECT_FALSE(report.walkModeChanged);
        const auto now = body.Feet();
        ASSERT_LT(std::hypot(now.X - old.X, now.Z - old.Z), 0.03F);
        tracker.Update(*data, spatial, body.position);
        reached.insert(std::string(IdRegistry::NameOf(tracker.Current())));
    }
    const auto target = tour.Route()[std::min(tour.Index(), tour.Route().size() - 1U)];
    std::printf("filming walk: %u steps, %zu views, at point %zu/%zu %s; feet %.5f %.5f %.5f; target %.5f "
                "%.5f %.5f\n",
                steps,
                tour.Visited().size(),
                tour.Index(),
                tour.Route().size(),
                target.cell.c_str(),
                static_cast<double>(body.Feet().X),
                static_cast<double>(body.Feet().Y),
                static_cast<double>(body.Feet().Z),
                static_cast<double>(target.feet.X),
                static_cast<double>(target.feet.Y),
                static_cast<double>(target.feet.Z));
    ASSERT_FALSE(tour.Stuck());
    ASSERT_TRUE(tour.Completed());
    EXPECT_EQ(std::set<std::string>(tour.Visited().begin(), tour.Visited().end()), expected);
    for (const auto& cell : expected)
    {
        EXPECT_TRUE(reached.contains(cell)) << cell;
    }
    EXPECT_FALSE(body.fastWalk);
    EXPECT_FALSE(body.noclip);
    EXPECT_LT(Vector3::Distance(body.Feet(), data->GetInitialState().player.position), 0.75F);
    tour.Stop();
    EXPECT_FALSE(tour.Active());
    EXPECT_FALSE(tour.Completed());
}
