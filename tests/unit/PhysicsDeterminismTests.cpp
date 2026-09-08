// SPDX-License-Identifier: MIT
//
// `HOUSE-00566`. §49.3's determinism clause, which is the reason the whole step is fixed:
//
//   *"Determinism: fixed step, no floating-point accumulation across frames beyond the
//    accumulator, and no dependence on frame rate. A unit test replays a recorded 60-second input
//    sequence and asserts the final position matches a stored value bit-for-bit at 30, 60 and
//    144 FPS."*
//
// The point is not that three numbers happen to agree. It is that a body walking a house arrives
// at the same place on a machine that renders at 30 and on one that renders at 144 -- because a
// save file, a replay and a scripted tour are all promises that it will.
#include <cmath>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "System/IO/FileAccess.hpp"
#include "System/IO/FileMode.hpp"
#include "System/IO/FileStream.hpp"

#include "cnahouse/app/FrameTimer.hpp"
#include "cnahouse/physics/BroadPhase.hpp"
#include "cnahouse/physics/CollisionLoader.hpp"
#include "cnahouse/player/PlayerController.hpp"

namespace
{
    using cnahouse::app::FrameContext;
    using cnahouse::app::FrameTimer;
    using cnahouse::physics::BroadPhase;
    using cnahouse::physics::Capsule;
    using cnahouse::physics::CollisionCell;
    using cnahouse::physics::CollisionKind;
    using cnahouse::physics::CollisionLoader;
    using cnahouse::physics::CollisionObb;
    using cnahouse::physics::CollisionWorld;
    using cnahouse::physics::OverlapCell;
    using cnahouse::physics::Sphere;
    using cnahouse::player::InputState;
    using cnahouse::player::PlayerState;
    using cnahouse::player::PlayerStep;
    using Microsoft::Xna::Framework::BoundingBox;
    using Microsoft::Xna::Framework::Vector3;

    /// A deterministic input sequence: no clock, no randomness, just a function of the tick. It
    /// turns, walks, stops, strafes and toggles the walk mode -- so a replay exercises the
    /// acceleration ramp, the modifier table and the toggle, not only a straight line.
    InputState ScriptedInput(std::uint64_t tick)
    {
        InputState input;
        const auto phase = static_cast<int>(tick / 90u % 6u);
        switch (phase)
        {
            case 0:
                input.move.Y = 1.0F;
                break;
            case 1:
                input.move.Y = 1.0F;
                input.move.X = 1.0F;
                break;
            case 2:
                break; // stop, and let the deceleration run
            case 3:
                input.move.Y = -1.0F;
                break;
            case 4:
                input.move.X = -1.0F;
                break;
            default:
                input.move.Y = 1.0F;
                break;
        }
        // Toggle the walk mode twice over the run, on an exact tick so every frame rate sees it on
        // the same STEP rather than at the same wall-clock moment.
        input.runPressed = tick == 300u || tick == 1500u;
        input.run = tick >= 300u && tick < 1500u;
        return input;
    }

    /// A yaw that keeps turning, so the run is not one straight line.
    float ScriptedYaw(std::uint64_t tick)
    {
        return static_cast<float>(tick / 90u) * 0.7F;
    }

    CollisionObb Box(const Vector3& centre, const Vector3& half, CollisionKind kind)
    {
        CollisionObb obb;
        obb.centre = centre;
        obb.halfExtents = half;
        obb.kind = kind;
        return obb;
    }

    /// A room with a floor, four walls and a kerb, so the run meets contacts rather than open air.
    CollisionWorld Room()
    {
        CollisionWorld world;
        world.gridCell = 1.0F;
        world.surfaces = {"tile"};
        world.obbs = {
            Box(Vector3(0.0F, -0.25F, 0.0F), Vector3(8.0F, 0.25F, 8.0F), CollisionKind::Floor),
            Box(Vector3(0.0F, 1.5F, -8.0F), Vector3(8.0F, 1.5F, 0.2F), CollisionKind::Wall),
            Box(Vector3(0.0F, 1.5F, 8.0F), Vector3(8.0F, 1.5F, 0.2F), CollisionKind::Wall),
            Box(Vector3(-8.0F, 1.5F, 0.0F), Vector3(0.2F, 1.5F, 8.0F), CollisionKind::Wall),
            Box(Vector3(8.0F, 1.5F, 0.0F), Vector3(0.2F, 1.5F, 8.0F), CollisionKind::Wall),
            Box(Vector3(3.0F, 0.075F, 0.0F), Vector3(1.0F, 0.075F, 8.0F), CollisionKind::Floor),
        };

        CollisionCell cell;
        cell.id = "L0_ROOM";
        cell.bounds = BoundingBox(Vector3(-9.0F, -1.0F, -9.0F), Vector3(9.0F, 4.0F, 9.0F));
        cell.nx = 18u;
        cell.nz = 18u;
        cell.originX = -9.0F;
        cell.originZ = -9.0F;
        cell.buckets.assign(static_cast<std::size_t>(cell.nx) * cell.nz, {});
        for (std::uint32_t i = 0; i < world.ShapeCount(); ++i)
        {
            cell.shapes.push_back(i);
            for (auto& bucket : cell.buckets)
            {
                bucket.push_back(static_cast<std::uint16_t>(i));
            }
        }
        world.cells = {cell};
        return world;
    }

    /// Replays exactly @p ticks recorded steps at @p fps, through the real accumulator.
    ///
    /// **A recorded sequence is counted in STEPS, not in seconds of wall clock.** §49.3 says "a
    /// recorded 60-second input sequence", and 60 seconds of INPUT is 7 200 fixed steps. Sixty
    /// seconds of WALL CLOCK is not the same amount of simulated time at every frame rate: `1/144`
    /// as a float is not exactly a 144th of a second, and 8 640 of them do not add to exactly 60.
    /// Measured, that is 7 200 steps at 30 and 60 FPS and 7 199 at 144 -- eight milliseconds of
    /// simulation, and nothing to do with determinism, which is that the same 7 200 steps give the
    /// same answer however they were grouped into frames.
    ///
    /// The tick counter is what the script is a function of, so the same tick means the same input
    /// on every frame rate -- which is the whole premise of a replay. What differs between the
    /// runs is only how many steps arrive per frame.
    PlayerState Replay(const CollisionWorld& world, float fps, std::uint64_t ticks, int* framesOut = nullptr)
    {
        BroadPhase broad;
        FrameTimer timer;
        PlayerState state;
        state.position = Vector3(0.0F, 0.90F + 0.002F, 0.0F);

        std::uint64_t tick = 0;
        const float frame = 1.0F / fps;
        int frames = 0;
        while (tick < ticks && frames < 200000)
        {
            const FrameContext context = timer.Advance(frame);
            ++frames;
            for (int s = 0; s < context.fixedSteps && tick < ticks; ++s)
            {
                state.yaw = ScriptedYaw(tick);
                PlayerStep(
                    world, world.cells[0], broad, state, ScriptedInput(tick), context.fixedStepSeconds);
                ++tick;
            }
        }
        if (framesOut != nullptr)
        {
            *framesOut = frames;
        }
        return state;
    }

} // namespace

TEST(PhysicsDeterminismTests, TheSameStepsArriveInDifferentSizedHandfuls)
{
    // The premise of everything below: what the frame rate changes is how the fixed steps are
    // GROUPED, not which ones there are. 1 200 steps arrive in about 300 frames at 30 FPS and
    // about 1 440 at 144, and the simulation sees the identical 1 200 either way.
    const CollisionWorld world = Room();
    int frames30 = 0;
    int frames60 = 0;
    int frames144 = 0;
    Replay(world, 30.0F, 1200u, &frames30);
    Replay(world, 60.0F, 1200u, &frames60);
    Replay(world, 144.0F, 1200u, &frames144);

    EXPECT_NEAR(frames30, 300, 2);
    EXPECT_NEAR(frames60, 600, 2);
    EXPECT_NEAR(frames144, 1440, 2);
    EXPECT_LT(frames30, frames144) << "the handfuls were the same size, so this proves nothing";
}

TEST(PhysicsDeterminismTests, SixtySecondsOfWalkingEndsAtTheSamePlaceAtThirtySixtyAndOneFortyFour)
{
    // §49.3's clause: *"a unit test replays a recorded 60-second input sequence and asserts the
    // final position matches a stored value bit-for-bit at 30, 60 and 144 FPS."* Sixty seconds of
    // recorded input is 7 200 fixed steps.
    //
    // Bit-for-bit and not "within a millimetre": the whole reason for a fixed step is that the
    // answer does not depend on the frame rate AT ALL, and a tolerance would hide exactly the kind
    // of drift a save file cannot survive.
    const CollisionWorld world = Room();
    const PlayerState at30 = Replay(world, 30.0F, 7200u);
    const PlayerState at60 = Replay(world, 60.0F, 7200u);
    const PlayerState at144 = Replay(world, 144.0F, 7200u);

    EXPECT_EQ(at30.position.X, at60.position.X);
    EXPECT_EQ(at30.position.Y, at60.position.Y);
    EXPECT_EQ(at30.position.Z, at60.position.Z);
    EXPECT_EQ(at30.position.X, at144.position.X);
    EXPECT_EQ(at30.position.Y, at144.position.Y);
    EXPECT_EQ(at30.position.Z, at144.position.Z);

    // Everything else the step carries, too: a position that agrees while the velocity or the
    // walk mode does not is a replay that diverges on the next frame.
    EXPECT_EQ(at30.velocity.X, at144.velocity.X);
    EXPECT_EQ(at30.velocity.Z, at144.velocity.Z);
    EXPECT_EQ(at30.fastWalk, at144.fastWalk);
    EXPECT_EQ(at30.crouched, at144.crouched);
    EXPECT_EQ(at30.onGround, at144.onGround);
    EXPECT_EQ(at30.groundKind, at144.groundKind);

    // ...and the run actually went somewhere, so the agreement means something.
    EXPECT_GT(std::fabs(at30.position.X) + std::fabs(at30.position.Z), 1.0F)
        << "the body never left where it started";
}

TEST(PhysicsDeterminismTests, AnUnEVENFrameRateChangesNothingEither)
{
    // Real frames are not 1/60 of a second each. A rate that leaves the accumulator part-way
    // through a step on every frame is the case that catches an implementation quietly using the
    // frame's own delta somewhere instead of the fixed one.
    const CollisionWorld world = Room();
    const PlayerState even = Replay(world, 60.0F, 3600u);
    const PlayerState odd = Replay(world, 37.0F, 3600u);

    // 37 FPS is 27.027 ms a frame, which is 3.243 fixed steps: the accumulator carries a different
    // remainder into every single one of them, so three frames in four deliver a different number
    // of steps from their neighbours.
    int frames = 0;
    Replay(world, 37.0F, 3600u, &frames);
    ASSERT_NEAR(frames, 1110, 3) << "37 FPS did not take about 1110 frames to deliver 3600 steps";

    EXPECT_EQ(even.position.X, odd.position.X);
    EXPECT_EQ(even.position.Y, odd.position.Y);
    EXPECT_EQ(even.position.Z, odd.position.Z);
}

TEST(PhysicsDeterminismTests, TheSameReplayRunTwiceIsTheSameRun)
{
    // Nothing in the step may depend on anything but the state it was handed: no clock, no static,
    // no uninitialised memory, no iteration over a container whose order is an address.
    const CollisionWorld world = Room();
    const PlayerState first = Replay(world, 60.0F, 3600u);
    const PlayerState second = Replay(world, 60.0F, 3600u);
    EXPECT_EQ(first.position.X, second.position.X);
    EXPECT_EQ(first.position.Y, second.position.Y);
    EXPECT_EQ(first.position.Z, second.position.Z);
    EXPECT_EQ(first.velocity.X, second.velocity.X);
    EXPECT_EQ(first.velocity.Z, second.velocity.Z);
}

TEST(PhysicsDeterminismTests, SixtySecondsInTheRealHouseAgreesToo)
{
    // The constructed room has six shapes. A real cell has dozens, a broad phase that hands them
    // over in bucket order, and stair ramps made of triangles -- and the guarantee is the same.
    const std::string path = "content/world/collision.bin";
    System::IO::FileStream* probe = nullptr;
    try
    {
        probe = new System::IO::FileStream(path, System::IO::FileMode::Open, System::IO::FileAccess::Read);
    }
    catch (const std::exception&)
    {
        GTEST_SKIP() << "no " << path << "; run tools/ci/build_content.py --only world";
    }
    const std::unique_ptr<System::IO::FileStream> stream(probe);
    const auto world = CollisionLoader::Read(*stream, path);
    ASSERT_TRUE(world) << world.Error().Message();

    // A cell with room to walk about in, found the way every other real-house case finds one.
    const cnahouse::physics::CollisionCell* chosen = nullptr;
    Vector3 start;
    BroadPhase broad;
    for (const auto& cell : world->cells)
    {
        if (cell.shapes.empty() || cell.nx < 6u || cell.nz < 6u)
        {
            continue;
        }
        const float midX = cell.originX + static_cast<float>(cell.nx) * 0.5F;
        const float midZ = cell.originZ + static_cast<float>(cell.nz) * 0.5F;
        const float height = cell.bounds.Max.Y - cell.bounds.Min.Y;
        if (height < 2.4F)
        {
            continue;
        }
        const Capsule pebble =
            Sphere(Vector3(midX, (cell.bounds.Min.Y + cell.bounds.Max.Y) * 0.5F, midZ), 0.05F);
        if (OverlapCell(*world, cell, broad, pebble).overlapped)
        {
            continue;
        }
        const auto landed = SweepCell(*world, cell, broad, pebble, Vector3(0.0F, -height, 0.0F));
        if (!landed.hit)
        {
            continue;
        }
        const float floorY = pebble.centre.Y - height * landed.time - 0.05F;
        Capsule body{Vector3(midX, floorY + 0.902F, midZ), 0.60F, 0.30F};
        if (OverlapCell(*world, cell, broad, body).overlapped)
        {
            continue;
        }
        chosen = &cell;
        start = body.centre;
        break;
    }
    ASSERT_NE(chosen, nullptr) << "no cell in the house had room for a 60 second walk";

    const auto run = [&](float fps)
    {
        BroadPhase phase;
        FrameTimer timer;
        PlayerState state;
        state.position = start;
        std::uint64_t tick = 0;
        int frames = 0;
        while (tick < 7200u && frames < 200000)
        {
            const FrameContext context = timer.Advance(1.0F / fps);
            ++frames;
            for (int s = 0; s < context.fixedSteps && tick < 7200u; ++s)
            {
                state.yaw = ScriptedYaw(tick);
                PlayerStep(*world, *chosen, phase, state, ScriptedInput(tick), context.fixedStepSeconds);
                ++tick;
            }
        }
        return state;
    };

    const PlayerState at30 = run(30.0F);
    const PlayerState at60 = run(60.0F);
    const PlayerState at144 = run(144.0F);
    EXPECT_EQ(at30.position.X, at60.position.X) << chosen->id;
    EXPECT_EQ(at30.position.Y, at60.position.Y) << chosen->id;
    EXPECT_EQ(at30.position.Z, at60.position.Z) << chosen->id;
    EXPECT_EQ(at30.position.X, at144.position.X) << chosen->id;
    EXPECT_EQ(at30.position.Y, at144.position.Y) << chosen->id;
    EXPECT_EQ(at30.position.Z, at144.position.Z) << chosen->id;
}
