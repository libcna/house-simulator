// SPDX-License-Identifier: MIT
//
// `HOUSE-00555`. §49.3's fixed step, composed: input becomes a desired velocity, the velocity
// becomes a slide, the slide is assisted over a kerb, gravity runs for a body over nothing, the
// ground is probed, and whatever is left is pushed out. Every one of those pieces has its own
// tests; what is tested here is that they add up to a body that walks.
//
// §43.2's two numbers are the ones to hold on to: 1.35 m/s and 9.0 m/s² are one decision written
// twice, because *"reaches full speed in ~0.15 s"* is 1.35 / 9.0.
#include <cmath>
#include <memory>
#include <string>

#include <gtest/gtest.h>

#include "System/IO/FileAccess.hpp"
#include "System/IO/FileMode.hpp"
#include "System/IO/FileStream.hpp"

#include "cnahouse/physics/BroadPhase.hpp"
#include "cnahouse/physics/CollisionLoader.hpp"
#include "cnahouse/player/PlayerController.hpp"

namespace
{
    using cnahouse::physics::BroadPhase;
    using cnahouse::physics::Capsule;
    using cnahouse::physics::CollisionCell;
    using cnahouse::physics::CollisionKind;
    using cnahouse::physics::CollisionLoader;
    using cnahouse::physics::CollisionMesh;
    using cnahouse::physics::CollisionObb;
    using cnahouse::physics::CollisionWorld;
    using cnahouse::physics::Landing;
    using cnahouse::physics::OverlapCell;
    using cnahouse::physics::Sphere;
    using cnahouse::player::InputState;
    using cnahouse::player::kAcceleration;
    using cnahouse::player::kDeceleration;
    using cnahouse::player::kPlayerEyeHeight;
    using cnahouse::player::kPlayerHalfHeight;
    using cnahouse::player::kPlayerRadius;
    using cnahouse::player::kWalkSpeed;
    using cnahouse::player::PlayerState;
    using cnahouse::player::PlayerStep;
    using cnahouse::player::PlayerStepReport;
    using Microsoft::Xna::Framework::BoundingBox;
    using Microsoft::Xna::Framework::Vector3;

    constexpr float kDt = 1.0F / 120.0F; // §49.3's fixed step
    constexpr float kStand = kPlayerHalfHeight + kPlayerRadius;

    CollisionObb
    Box(const Vector3& centre, const Vector3& half, CollisionKind kind, std::uint16_t surface = 0u)
    {
        CollisionObb obb;
        obb.centre = centre;
        obb.halfExtents = half;
        obb.kind = kind;
        obb.surface = surface;
        return obb;
    }

    CollisionObb Slab(float top, float x0, float x1, CollisionKind kind = CollisionKind::Floor)
    {
        return Box(
            Vector3((x0 + x1) * 0.5F, top - 0.25F, 0.0F), Vector3((x1 - x0) * 0.5F, 0.25F, 12.0F), kind);
    }

    CollisionWorld OneCell(std::vector<CollisionObb> obbs, std::vector<CollisionMesh> meshes = {})
    {
        CollisionWorld world;
        world.gridCell = 1.0F;
        world.surfaces = {"tile", "wood"};
        world.obbs = std::move(obbs);
        world.meshes = std::move(meshes);

        CollisionCell cell;
        cell.id = "L0_WALK";
        cell.bounds = BoundingBox(Vector3(-16.0F, -16.0F, -16.0F), Vector3(16.0F, 16.0F, 16.0F));
        cell.nx = 32u;
        cell.nz = 32u;
        cell.originX = -16.0F;
        cell.originZ = -16.0F;
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

    /// A body standing at (@p x, @p z) with 2 mm of daylight under it, which is where §49.3
    /// itself leaves one.
    PlayerState Standing(float x, float z, float feet = 0.0F)
    {
        PlayerState state;
        state.position = Vector3(x, feet + kStand + 0.002F, z);
        state.cellId = "L0_WALK";
        return state;
    }

    InputState Forward()
    {
        InputState input;
        input.move.Y = 1.0F;
        return input;
    }

    float Speed(const PlayerState& state)
    {
        return std::sqrt(state.velocity.X * state.velocity.X + state.velocity.Z * state.velocity.Z);
    }

} // namespace

TEST(PlayerControllerTests, TheTwoNumbersInTheSpeedTableAreOneDecision)
{
    // §43.2 gives 1.35 m/s and 9.0 m/s² and then says "reaches full speed in ~0.15 s". That is
    // not a third number: it is the first two divided. If somebody tunes either, this says so.
    EXPECT_FLOAT_EQ(kWalkSpeed, 1.35F);
    EXPECT_FLOAT_EQ(kAcceleration, 9.0F);
    EXPECT_NEAR(kWalkSpeed / kAcceleration, 0.15F, 1e-6F);
    // ...and stopping is quicker than starting, which is what keeps it from feeling floaty.
    EXPECT_GT(kDeceleration, kAcceleration);
    EXPECT_NEAR(kWalkSpeed / kDeceleration, 0.1038F, 1e-3F);
}

TEST(PlayerControllerTests, AWalkReachesFullSpeedInTheTimeTheTableSays)
{
    const CollisionWorld world = OneCell({Slab(0.0F, -20.0F, 20.0F)});
    BroadPhase broad;
    PlayerState state = Standing(0.0F, 0.0F);

    // 18 steps is 0.15 s.
    for (int i = 0; i < 18; ++i)
    {
        PlayerStep(world, world.cells[0], broad, state, Forward(), kDt);
    }
    EXPECT_NEAR(Speed(state), kWalkSpeed, 1e-3F);

    // Half way there, it is half way: the ramp is linear, not eased.
    PlayerState half = Standing(0.0F, 0.0F);
    for (int i = 0; i < 9; ++i)
    {
        PlayerStep(world, world.cells[0], broad, half, Forward(), kDt);
    }
    EXPECT_NEAR(Speed(half), kWalkSpeed * 0.5F, 2e-2F);

    // And it does not run away past the top.
    for (int i = 0; i < 240; ++i)
    {
        PlayerStep(world, world.cells[0], broad, state, Forward(), kDt);
    }
    EXPECT_NEAR(Speed(state), kWalkSpeed, 1e-4F);
}

TEST(PlayerControllerTests, LettingGoStopsTheBodyAtTheOtherRate)
{
    const CollisionWorld world = OneCell({Slab(0.0F, -20.0F, 20.0F)});
    BroadPhase broad;
    PlayerState state = Standing(0.0F, 0.0F);
    for (int i = 0; i < 60; ++i)
    {
        PlayerStep(world, world.cells[0], broad, state, Forward(), kDt);
    }
    ASSERT_NEAR(Speed(state), kWalkSpeed, 1e-3F);

    const InputState idle;
    for (int i = 0; i < 13; ++i) // 0.108 s, just past 1.35 / 13.0
    {
        PlayerStep(world, world.cells[0], broad, state, idle, kDt);
    }
    EXPECT_NEAR(Speed(state), 0.0F, 1e-4F);
}

TEST(PlayerControllerTests, ForwardIsNorthAndPositiveYawTurnsEast)
{
    // §14, and the sign that `HOUSE-00473` had to fix once already in the free-fly camera. Two
    // different answers to "which way is forward" show up as the player walking sideways.
    const CollisionWorld world = OneCell({Slab(0.0F, -20.0F, 20.0F)});
    BroadPhase broad;

    PlayerState north = Standing(0.0F, 0.0F);
    for (int i = 0; i < 120; ++i)
    {
        PlayerStep(world, world.cells[0], broad, north, Forward(), kDt);
    }
    EXPECT_LT(north.position.Z, -0.5F) << "yaw 0 did not walk north";
    EXPECT_NEAR(north.position.X, 0.0F, 1e-4F);

    PlayerState east = Standing(0.0F, 0.0F);
    east.yaw = 1.5707963F;
    for (int i = 0; i < 120; ++i)
    {
        PlayerStep(world, world.cells[0], broad, east, Forward(), kDt);
    }
    EXPECT_GT(east.position.X, 0.5F) << "positive yaw did not turn east";
    EXPECT_NEAR(east.position.Z, 0.0F, 1e-4F);

    // Strafing right from yaw 0 goes east too, and the two agree on how far in a second.
    PlayerState strafe = Standing(0.0F, 0.0F);
    InputState right;
    right.move.X = 1.0F;
    for (int i = 0; i < 120; ++i)
    {
        PlayerStep(world, world.cells[0], broad, strafe, right, kDt);
    }
    EXPECT_NEAR(strafe.position.X, east.position.X, 1e-3F);
}

TEST(PlayerControllerTests, AWallStopsTheBodyAndDoesNotStoreUpSpeedBehindIt)
{
    // Held against a wall for two seconds. The velocity must not keep accumulating into it: a
    // body that stores 240 steps of acceleration shoots away the moment the wall ends.
    const CollisionWorld world = OneCell({
        Slab(0.0F, -20.0F, 20.0F),
        Box(Vector3(0.0F, 1.0F, -2.0F), Vector3(6.0F, 1.5F, 0.1F), CollisionKind::Wall),
    });
    BroadPhase broad;
    PlayerState state = Standing(0.0F, 0.0F);
    for (int i = 0; i < 240; ++i)
    {
        PlayerStep(world, world.cells[0], broad, state, Forward(), kDt);
    }
    // The wall's south face is at z = -1.9 and the body's radius is 0.30.
    EXPECT_GT(state.position.Z, -1.61F) << "it went through the wall";
    EXPECT_NEAR(Speed(state), 0.0F, 1e-3F) << "it kept accelerating into the wall";

    // Resting against the wall, not inside it: a body leaning on a wall IS touching it, and the
    // depenetration deliberately leaves it there rather than bouncing it off (`kContactTolerance`).
    Capsule ended = state.Body();
    EXPECT_LE(OverlapCell(world, world.cells[0], broad, ended).depth, cnahouse::physics::kContactTolerance);
}

TEST(PlayerControllerTests, AKerbIsWalkedOverAndADropIsFallenDown)
{
    // Two storeys of §49.3 in one walk: the step assist takes the body up a 0.15 m kerb, and
    // §43.1's gravity takes it down the 1.20 m drop on the far side, landing soft.
    const CollisionWorld world = OneCell({
        Slab(0.0F, -20.0F, 2.0F),
        Slab(0.15F, 2.0F, 4.0F),
        Slab(-1.20F, 4.0F, 20.0F),
    });
    BroadPhase broad;
    PlayerState state = Standing(0.0F, 0.0F);
    state.yaw = 1.5707963F; // east

    bool climbed = false;
    bool landedSoft = false;
    for (int i = 0; i < 900 && state.position.X < 8.0F; ++i)
    {
        const PlayerStepReport step = PlayerStep(world, world.cells[0], broad, state, Forward(), kDt);
        climbed = climbed || step.steppedUp;
        landedSoft = landedSoft || step.landing == Landing::Soft;
        EXPECT_TRUE(step.depenetrated) << "step " << i;
        EXPECT_NE(step.landing, Landing::Hard) << "a 1.2 m drop is not a hard landing";
    }
    EXPECT_TRUE(climbed) << "the 0.15 m kerb was not stepped over";
    EXPECT_TRUE(landedSoft) << "the 1.2 m drop never ended in a landing";
    EXPECT_GT(state.position.X, 8.0F);
    EXPECT_NEAR(state.position.Y, -1.20F + kStand, 0.01F) << "it did not settle on the low floor";
    EXPECT_TRUE(state.onGround);
}

TEST(PlayerControllerTests, TheProbeKeepsTheBodyTellingTheTruthAboutTheGround)
{
    // What §60, §31 and `HOUSE-00559` read: the kind of thing underfoot, its surface, and the
    // cell. Walking from a tiled floor onto a wooden stair changes all three reports.
    const CollisionWorld world = OneCell({
        Box(Vector3(-2.0F, -0.25F, 0.0F), Vector3(2.0F, 0.25F, 12.0F), CollisionKind::Floor, 0u),
        Box(Vector3(2.0F, -0.25F, 0.0F), Vector3(2.0F, 0.25F, 12.0F), CollisionKind::Stair, 1u),
    });
    BroadPhase broad;
    PlayerState state = Standing(-2.0F, 0.0F);
    state.yaw = 1.5707963F;

    PlayerStep(world, world.cells[0], broad, state, InputState{}, kDt);
    EXPECT_EQ(state.groundKind, CollisionKind::Floor);
    EXPECT_EQ(world.SurfaceName(state.surface), "tile");
    EXPECT_EQ(state.cellId, "L0_WALK");

    for (int i = 0; i < 600 && state.position.X < 2.0F; ++i)
    {
        PlayerStep(world, world.cells[0], broad, state, Forward(), kDt);
    }
    ASSERT_GT(state.position.X, 2.0F);
    EXPECT_EQ(state.groundKind, CollisionKind::Stair) << "§60 would not know it is on a stair";
    EXPECT_EQ(world.SurfaceName(state.surface), "wood");
}

TEST(PlayerControllerTests, TheEyeIsWhereFortyThreePointOnePutsIt)
{
    PlayerState state = Standing(1.0F, 2.0F);
    const float feet = state.position.Y - kStand;
    EXPECT_NEAR(state.Eye().Y - feet, kPlayerEyeHeight, 1e-5F);
    EXPECT_FLOAT_EQ(state.Eye().X, state.position.X);
    EXPECT_FLOAT_EQ(state.Eye().Z, state.position.Z);
}

TEST(PlayerControllerTests, TheSameInputTwiceGivesTheSameWalkToTheBit)
{
    // §49.3's determinism clause in miniature -- `HOUSE-00566` does the 30/60/144 FPS replay.
    // Nothing here may depend on anything but the state it was handed.
    const CollisionWorld world = OneCell({
        Slab(0.0F, -20.0F, 20.0F),
        Box(Vector3(3.0F, 1.0F, 0.0F), Vector3(0.1F, 1.5F, 2.0F), CollisionKind::Wall),
    });
    BroadPhase first;
    BroadPhase second;
    PlayerState a = Standing(0.0F, 0.0F);
    PlayerState b = Standing(0.0F, 0.0F);
    a.yaw = b.yaw = 0.7F;
    for (int i = 0; i < 600; ++i)
    {
        const InputState input = (i / 40) % 2 == 0 ? Forward() : InputState{};
        PlayerStep(world, world.cells[0], first, a, input, kDt);
        PlayerStep(world, world.cells[0], second, b, input, kDt);
    }
    EXPECT_FLOAT_EQ(a.position.X, b.position.X);
    EXPECT_FLOAT_EQ(a.position.Y, b.position.Y);
    EXPECT_FLOAT_EQ(a.position.Z, b.position.Z);
    EXPECT_FLOAT_EQ(a.velocity.X, b.velocity.X);
    EXPECT_FLOAT_EQ(a.velocity.Z, b.velocity.Z);
}

TEST(PlayerControllerTests, ASecondOfWalkingInEveryCellOfTheRealHouseEndsSomewhereLegal)
{
    // The whole step, over the real thing: eight directions, 120 fixed steps each, from the middle
    // of every cell. §49.5's promise is that a body never ends a frame inside static geometry, and
    // this is the composed loop being held to it.
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

    BroadPhase broad;
    std::size_t walked = 0;
    std::size_t steps = 0;
    std::size_t inside = 0;
    for (const auto& cell : world->cells)
    {
        if (cell.shapes.empty() || cell.nx == 0u || cell.nz == 0u)
        {
            continue;
        }
        const float midX = cell.originX + static_cast<float>(cell.nx) * 0.5F;
        const float midZ = cell.originZ + static_cast<float>(cell.nz) * 0.5F;
        const float height = cell.bounds.Max.Y - cell.bounds.Min.Y;
        if (height < 2.0F * kStand)
        {
            continue;
        }
        const float pebble = 0.05F;
        const Capsule falling =
            Sphere(Vector3(midX, (cell.bounds.Min.Y + cell.bounds.Max.Y) * 0.5F, midZ), pebble);
        if (OverlapCell(*world, cell, broad, falling).overlapped)
        {
            continue;
        }
        const auto landed = SweepCell(*world, cell, broad, falling, Vector3(0.0F, -height, 0.0F));
        if (!landed.hit)
        {
            continue;
        }
        const float floorY = falling.centre.Y - height * landed.time - pebble;
        if (floorY + 2.0F * kStand > cell.bounds.Max.Y)
        {
            continue;
        }
        PlayerState start;
        start.position = Vector3(midX, floorY + kStand + 0.002F, midZ);
        if (OverlapCell(*world, cell, broad, start.Body()).overlapped)
        {
            continue; // a stair tread under its middle; `HOUSE-00547` counts these
        }
        ++walked;

        for (int i = 0; i < 8; ++i)
        {
            PlayerState state = start;
            state.yaw = static_cast<float>(i) * 6.2831853F / 8.0F;
            for (int t = 0; t < 120; ++t)
            {
                const PlayerStepReport step = PlayerStep(*world, cell, broad, state, Forward(), kDt);
                ++steps;
                EXPECT_LE(Speed(state), kWalkSpeed + 1e-3F) << cell.id;
                EXPECT_TRUE(std::isfinite(state.position.X) && std::isfinite(state.position.Y) &&
                            std::isfinite(state.position.Z))
                    << cell.id;
                if (!step.depenetrated)
                {
                    ++inside;
                }
            }
            // Whatever the second of walking did, the body is not INSIDE anything at the end.
            // Touching is allowed and expected -- most of these walks end against a wall, and a
            // body resting on a wall is touching it (`kContactTolerance`).
            EXPECT_LE(OverlapCell(*world, cell, broad, state.Body()).depth,
                      cnahouse::physics::kContactTolerance)
                << cell.id << " heading " << i;
        }
    }
    ASSERT_GT(walked, 40u) << "only " << walked << " cells were walked";
    ASSERT_GT(steps, 40000u);
    EXPECT_EQ(inside, 0u) << inside << " of " << steps << " steps ended inside geometry";
}
