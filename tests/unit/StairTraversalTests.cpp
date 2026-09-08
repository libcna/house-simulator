// SPDX-License-Identifier: MIT
//
// `HOUSE-00615`, §49.5's fourth guarantee: **every flight is traversable in both directions, and
// every landing is reachable.** All eight of them -- the main stair's two storeys, the attic
// flight, the basement flight, and the four exterior and garage steps.
//
// A flight is the one place where §43.1's step assist, §43.2's stair speed modifier, the slope
// limit and the ground probe all have to agree at once, and where getting any of them slightly
// wrong stops the player halfway up a staircase. A validation rule can say a flight's geometry is
// plausible; only walking it says a body can climb it.
//
// The path comes out of the collision RAMP itself -- the lowest and highest vertices of each
// wedge, and the centre of each landing box -- because that is the surface the body will actually
// be standing on. The body is steered from waypoint to waypoint the way a player is: yaw at the
// next one, walk forward, and never any vertical help.
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
#include "cnahouse/physics/Move.hpp"
#include "cnahouse/physics/Sweep.hpp"
#include "cnahouse/player/PlayerController.hpp"
#include "cnahouse/world/WorldData.hpp"
#include "cnahouse/world/WorldLoader.hpp"
#include "unit/StairPath.hpp"

namespace
{
    using cnahouse::physics::BroadPhase;
    using cnahouse::physics::CollisionCell;
    using cnahouse::physics::CollisionKind;
    using cnahouse::physics::CollisionLoader;
    using cnahouse::physics::CollisionMesh;
    using cnahouse::physics::CollisionObb;
    using cnahouse::physics::CollisionWorld;
    using cnahouse::physics::kStepDownHeight;
    using cnahouse::physics::Landing;
    using cnahouse::player::InputState;
    using cnahouse::player::kPlayerHalfHeight;
    using cnahouse::player::kPlayerRadius;
    using cnahouse::player::PlayerState;
    using cnahouse::player::PlayerStep;
    using cnahouse::player::PlayerStepReport;
    using cnahouse::tests::Flat;
    using cnahouse::tests::FootStart;
    using cnahouse::tests::PathUp;
    using cnahouse::tests::SegmentsOf;
    using cnahouse::tests::StairSegment;
    using cnahouse::util::IdRegistry;
    using Microsoft::Xna::Framework::Vector3;

    namespace world = cnahouse::world;

    constexpr float kDt = 1.0F / 120.0F;
    constexpr float kRise = kPlayerHalfHeight + kPlayerRadius;
    /// 25 s a climb. The longest flight is 17 risers over 4.76 m of going, and §43.2's stairs
    /// modifier walks it at 1.35 x 0.72 = 0.97 m/s -- about 6 s. The rest is room to be wrong in.
    constexpr int kSteps = 3000;
    /// Close enough to a waypoint to move on to the next. A landing is 1.1 x 2.3 m and a body
    /// 0.60 m from its centre is standing on it; the flights are 1.1 m wide, so this is half a
    /// width. Tighter than this and a body stopped by the balustrade a hand's breadth short of a
    /// waypoint's exact centre reads as one that could not get there.
    constexpr float kArrived = cnahouse::tests::kStairArrived;
    /// No progress for this many steps and the body is stuck, whatever it is doing.
    constexpr int kStuck = 240;

    std::string_view Name(cnahouse::util::Id id)
    {
        return IdRegistry::NameOf(id);
    }

    /// Walks @p state through @p waypoints, steering at each in turn. Returns how many it reached.
    std::size_t Walk(const CollisionWorld& world,
                     const CollisionCell& cell,
                     PlayerState& state,
                     const std::vector<Vector3>& waypoints,
                     int& steps,
                     bool& fellHard)
    {
        BroadPhase broad;
        InputState input;
        input.move.Y = 1.0F;

        std::size_t next = 0;
        int sinceProgress = 0;
        float closest = 1e9F;
        steps = 0;
        fellHard = false;
        while (next < waypoints.size() && steps < kSteps)
        {
            const Vector3& target = waypoints[next];
            // §14: yaw 0 looks north (-Z) and positive turns east, so this is atan2(dx, -dz).
            state.yaw = std::atan2(target.X - state.position.X, state.position.Z - target.Z);
            const PlayerStepReport report = PlayerStep(world, cell, broad, state, input, kDt);
            ++steps;
            fellHard = fellHard || report.landing == Landing::Hard;

            const float distance = Flat(state.position, target);
            if (distance < kArrived)
            {
                ++next;
                sinceProgress = 0;
                closest = 1e9F;
                continue;
            }
            if (distance < closest - 0.01F)
            {
                closest = distance;
                sinceProgress = 0;
            }
            else if (++sinceProgress > kStuck)
            {
                {
                    // What a body that could not get up a flight was standing against. Printed
                    // when that happens and never otherwise: a guarantee that fails should say
                    // WHERE, or the next person starts this investigation from nothing.
                    const cnahouse::physics::CellOverlap in =
                        cnahouse::physics::OverlapCell(world, cell, broad, state.Body());
                    const cnahouse::physics::CellSweepHit ahead = cnahouse::physics::SweepCell(
                        world,
                        cell,
                        broad,
                        state.Body(),
                        Vector3(std::sin(state.yaw) * 0.5F, 0.0F, -std::cos(state.yaw) * 0.5F));
                    std::printf("  STUCK at (%.2f,%.2f,%.2f) overlap %d shape %u depth %.3f; ahead hit %d "
                                "shape %u t %.3f n (%.2f,%.2f,%.2f)\n",
                                static_cast<double>(state.position.X),
                                static_cast<double>(state.position.Y),
                                static_cast<double>(state.position.Z),
                                in.overlapped ? 1 : 0,
                                in.shape,
                                static_cast<double>(in.depth),
                                ahead.hit ? 1 : 0,
                                ahead.shape,
                                static_cast<double>(ahead.time),
                                static_cast<double>(ahead.normal.X),
                                static_cast<double>(ahead.normal.Y),
                                static_cast<double>(ahead.normal.Z));
                }
                break;
            }
        }
        return next;
    }

} // namespace

TEST(StairTraversalTests, EveryFlightIsWalkableUpAndDownAndEveryLandingIsStoodOn)
{
    IdRegistry::ResetForTesting();
    const std::string directory = "content/world";
    const std::string collisionPath = "content/world/collision.bin";
    if (!std::filesystem::exists(directory + "/layout.stairs.json") ||
        !std::filesystem::exists(collisionPath))
    {
        GTEST_SKIP() << "no deployed world; run tools/ci/build_content.py --only world";
    }

    world::WorldData::Contents contents;
    ASSERT_TRUE(world::WorldLoader::LoadLevels(directory, contents));
    ASSERT_TRUE(world::WorldLoader::LoadCells(directory, contents));
    ASSERT_TRUE(world::WorldLoader::LoadStairs(directory, contents));
    auto built = world::WorldData::Create(std::move(contents));
    ASSERT_TRUE(built) << built.Error().ToString();
    const world::WorldData& data = built.Value();

    const std::unique_ptr<System::IO::FileStream> stream(
        new System::IO::FileStream(collisionPath, System::IO::FileMode::Open, System::IO::FileAccess::Read));
    const auto loaded = CollisionLoader::Read(*stream, collisionPath);
    ASSERT_TRUE(loaded) << loaded.Error().Message();
    const CollisionWorld& statics = loaded.Value();

    int flights = 0;
    int landings = 0;
    int steps = 0;
    std::vector<std::string> notClimbed;
    std::vector<std::string> notDescended;
    std::vector<std::string> hardLandings;

    for (const world::StairFlight& flight : data.Stairs())
    {
        const world::Cell* fromCell = data.FindCell(flight.fromCell);
        const world::Cell* toCell = data.FindCell(flight.toCell);
        ASSERT_NE(fromCell, nullptr) << Name(flight.id);
        ASSERT_NE(toCell, nullptr) << Name(flight.id);
        const CollisionCell* collision = statics.Cell(Name(flight.fromCell));
        ASSERT_NE(collision, nullptr) << Name(flight.id) << ": its lower cell has no collision";

        const world::Level* fromLevel = data.FindLevel(fromCell->level);
        ASSERT_NE(fromLevel, nullptr);
        const float footY = flight.fromY.value_or(fromLevel->ffl);
        const std::vector<StairSegment> segments =
            SegmentsOf(statics, *collision, footY, footY + flight.Climb());
        ASSERT_FALSE(segments.empty()) << Name(flight.id) << ": no ramp in the collision to walk on";
        ++flights;

        // Up: `PathUp` builds the waypoints from the ramp itself -- the ends of every run, an L
        // across each landing, four steps along each run and a stride past the top. Every one of
        // those details was learnt by walking into something, and `HOUSE-00632`'s tune pass walks
        // the same path, so there is one of it (`unit/StairPath.hpp`).
        const std::vector<Vector3> up = PathUp(segments);
        // Counted from the SEGMENTS rather than from the path, which is the same number said
        // once: `PathUp` puts one waypoint on each landing, and this is what makes "every landing
        // is reachable" a thing the test can claim to have checked.
        landings += static_cast<int>(std::count_if(
            segments.begin(), segments.end(), [](const StairSegment& one) { return one.landing; }));
        const StairSegment& first = segments.front();
        const Vector3 foot = FootStart(segments);

        // 0.30 m of air under the soles, not 2 mm. A capsule resting on a 32.6° flight touches it
        // UPHILL of its centre by `radius · tan(slope)` -- 0.19 m (`HOUSE-00553` measured the same
        // thing on the terrain) -- so a body placed with its feet on the surface under its centre
        // starts a fifth of a metre INSIDE the ramp, and §49.3's step 5 spends the first second
        // shoving it out sideways. It settles in three steps.
        PlayerState state;
        state.position = Vector3(foot.X, foot.Y + kRise + 0.30F, foot.Z);

        int climbSteps = 0;
        bool fellHard = false;
        const std::size_t reached = Walk(statics, *collision, state, up, climbSteps, fellHard);
        steps += climbSteps;
        // Arrived means BEING THERE -- within the arrival radius of the last waypoint, and within
        // a step-down of its height -- and not a waypoint count. A body that stops on the top
        // tread rather than a stride past it has still climbed the flight, and one that reached
        // every waypoint but is still half way up has not. The count is reported for diagnosis.
        const float top = footY + flight.Climb();
        const float slack = kStepDownHeight;
        // Within the arrival radius of it, horizontally AND vertically. The two tolerances are
        // the same number on purpose: the steepest flight in the house is 35°, so a body 0.60 m
        // from a waypoint along the slope is at most 0.42 m above or below it, and a tighter
        // vertical bound would call that a body that had not got there.
        const auto arrived = [&](const Vector3& at)
        {
            return Flat(state.position, at) < kArrived &&
                   std::fabs(state.position.Y - state.Rise() - at.Y) < kArrived;
        };
        if (!arrived(up.back()) && state.position.Y - state.Rise() < top - slack)
        {
            notClimbed.push_back(std::string(Name(flight.id)) + ": reached " + std::to_string(reached) +
                                 " of " + std::to_string(up.size()) + " waypoint(s), feet at " +
                                 std::to_string(state.position.Y - state.Rise()) + " of " +
                                 std::to_string(top));
        }
        if (fellHard)
        {
            hardLandings.push_back(std::string(Name(flight.id)) + " going up");
        }

        // ...and back down, from wherever the climb ended, which is the honest starting point.
        // The way down is the way up reversed, minus the stride PAST the top: that one exists to
        // get a climbing body off the nosing and onto the upper floor, and a body that is already
        // there and now facing the wall beyond it has nowhere to walk.
        std::vector<Vector3> down(up.rbegin() + 1, up.rend());
        // Down to the flight's own foot, not to the spot the climb started from a stride up it:
        // arriving within `kArrived` of a waypoint 0.24 m up a 32.6° flight leaves the body higher
        // than the flight's bottom by more than the slack allows, and reads as a body that could
        // not get down.
        down.push_back(first.low);
        int descentSteps = 0;
        const std::size_t returned = Walk(statics, *collision, state, down, descentSteps, fellHard);
        steps += descentSteps;
        if (!arrived(down.back()) && state.position.Y - state.Rise() > footY + slack)
        {
            notDescended.push_back(std::string(Name(flight.id)) + ": reached " + std::to_string(returned) +
                                   " of " + std::to_string(down.size()) + " waypoint(s), feet at " +
                                   std::to_string(state.position.Y - state.Rise()) + " of " +
                                   std::to_string(footY));
        }
        if (fellHard)
        {
            hardLandings.push_back(std::string(Name(flight.id)) + " coming down");
        }
    }

    std::printf(
        "  %d flight(s) walked up and down, %d landing(s) stood on, %d step(s)\n", flights, landings, steps);

    EXPECT_EQ(flights, 8) << "§12.4's flight count changed; this guarantee counts them on purpose";
    EXPECT_GT(landings, 0) << "not one landing was walked over, so 'every landing is reachable' is untested";
    EXPECT_TRUE(notClimbed.empty()) << notClimbed.size() << " flight(s) could not be climbed; first: "
                                    << (notClimbed.empty() ? std::string() : notClimbed.front());
    EXPECT_TRUE(notDescended.empty())
        << notDescended.size() << " flight(s) could not be walked back down; first: "
        << (notDescended.empty() ? std::string() : notDescended.front());
    EXPECT_TRUE(hardLandings.empty())
        << hardLandings.size() << " flight(s) dropped the body hard enough for §47.2's heavy landing; "
        << "first: " << (hardLandings.empty() ? std::string() : hardLandings.front());
}
