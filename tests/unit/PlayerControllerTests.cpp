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

#include "cnahouse/app/Settings.hpp"
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

    // §43.2's fast walk is 2.05 and the sentence next to it says why: *"a brisk walk, not a run.
    // Above ~2.2 m/s a human transitions to a jog, and the brief is explicit that this is still
    // walking."* That upper bound is the reason for the number, so it is asserted beside it.
    EXPECT_FLOAT_EQ(cnahouse::player::kFastWalkSpeed, 2.05F);
    EXPECT_LT(cnahouse::player::kFastWalkSpeed, 2.2F) << "that is a jog, and the brief says walk";
    EXPECT_GT(cnahouse::player::kFastWalkSpeed, kWalkSpeed);
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

    // Strafing right from yaw 0 goes east too -- and §43.2's strafe modifier means it covers
    // 0.85 of the ground that turning east and walking does, which is the modifier arriving at the
    // body rather than only at the report.
    PlayerState strafe = Standing(0.0F, 0.0F);
    InputState right;
    right.move.X = 1.0F;
    for (int i = 0; i < 120; ++i)
    {
        PlayerStep(world, world.cells[0], broad, strafe, right, kDt);
    }
    EXPECT_GT(strafe.position.X, 0.5F) << "strafing right did not go east";
    EXPECT_NEAR(strafe.position.X / east.position.X, cnahouse::player::kStrafeFactor, 0.02F);
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
    // And it does NOT duck. Crouching does not get a body past a full-height wall, so the retry
    // that ducks under a header must not fire here -- a player who crouches at every wall they
    // walk into is a player who is permanently crouched.
    EXPECT_FALSE(state.crouched) << "it crouched at a wall that goes up to the ceiling";

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

TEST(PlayerControllerTests, BothWalkSpeedsAreMeasuredOverTwentyMetres)
{
    // The acceptance criterion `HOUSE-00556` was written with: 1.35 and 2.05 m/s over a 20 m run,
    // within 1 %. Measured over the RUN and not read off the velocity, because a controller that
    // reported the right speed while moving a different distance would pass the easier check.
    const CollisionWorld world = OneCell({Slab(0.0F, -40.0F, 40.0F)});
    BroadPhase broad;

    for (const bool fast : {false, true})
    {
        PlayerState state = Standing(-15.0F, 0.0F);
        state.yaw = 1.5707963F; // east
        state.fastWalk = fast;
        const float want = fast ? cnahouse::player::kFastWalkSpeed : kWalkSpeed;

        // Up to speed first, so the 0.15 s ramp is not part of what is being measured.
        for (int i = 0; i < 60; ++i)
        {
            PlayerStep(world, world.cells[0], broad, state, Forward(), kDt);
        }
        const float from = state.position.X;
        int ticks = 0;
        while (state.position.X - from < 20.0F && ticks < 4000)
        {
            PlayerStep(world, world.cells[0], broad, state, Forward(), kDt);
            ++ticks;
        }
        ASSERT_LT(ticks, 4000) << "it never covered the 20 m";
        const float measured = (state.position.X - from) / (static_cast<float>(ticks) * kDt);
        EXPECT_NEAR(measured, want, want * 0.01F) << (fast ? "fast" : "normal");
    }
}

TEST(PlayerControllerTests, ShiftTOGGLESTheWalkModeAndDoesNotHoldIt)
{
    // §43.2: *"Shift toggles between normal and fast walk. It is not hold-to-sprint."* Which
    // means the EDGE decides, and the level does not: holding the key down for a second must
    // change the mode once, not 120 times.
    const CollisionWorld world = OneCell({Slab(0.0F, -40.0F, 40.0F)});
    BroadPhase broad;
    PlayerState state = Standing(0.0F, 0.0F);
    EXPECT_FALSE(state.fastWalk) << "the walk is the default, not the run";

    InputState press = Forward();
    press.run = true;
    press.runPressed = true;
    const PlayerStepReport first = PlayerStep(world, world.cells[0], broad, state, press, kDt);
    EXPECT_TRUE(state.fastWalk);
    EXPECT_TRUE(first.walkModeChanged);

    // Held down: the level stays true, the edge does not repeat, and neither does the toggle.
    InputState held = Forward();
    held.run = true;
    for (int i = 0; i < 120; ++i)
    {
        const PlayerStepReport step = PlayerStep(world, world.cells[0], broad, state, held, kDt);
        EXPECT_FALSE(step.walkModeChanged) << "tick " << i;
    }
    EXPECT_TRUE(state.fastWalk) << "holding the key toggled the mode back";

    // Pressed again: back to the walk.
    PlayerStep(world, world.cells[0], broad, state, press, kDt);
    EXPECT_FALSE(state.fastWalk);
}

TEST(PlayerControllerTests, TheWalkModeSurvivesASettingsRoundTrip)
{
    // D-09: it is a preference, so it lives in `Settings` -- which is what makes it survive a
    // save/load and a *Reset House*. A file written before the mode existed says nothing about
    // it, and the migration gives such a player the walk rather than the run.
    cnahouse::app::Settings settings = cnahouse::app::Settings::Defaults();
    EXPECT_FALSE(settings.fastWalk);
    settings.fastWalk = true;
    const auto again = cnahouse::app::Settings::FromJson(settings.ToJson(), "settings.json");
    ASSERT_TRUE(again) << again.Error().Message();
    EXPECT_TRUE(again->fastWalk);
    EXPECT_EQ(again->version, cnahouse::app::Settings::kCurrentVersion);

    const auto old = cnahouse::app::Settings::FromJson(R"({"version": 2})", "settings.json");
    ASSERT_TRUE(old) << old.Error().Message();
    EXPECT_FALSE(old->fastWalk) << "a file from before the mode existed chose the run";
    EXPECT_EQ(old->version, cnahouse::app::Settings::kCurrentVersion);
}

TEST(PlayerControllerTests, EveryNumberInTheModifierTableIsTheOneItSays)
{
    using namespace cnahouse::player;
    EXPECT_FLOAT_EQ(kBackwardsFactor, 0.72F);
    EXPECT_FLOAT_EQ(kStrafeFactor, 0.85F);
    EXPECT_FLOAT_EQ(kStairsFactor, 0.72F);
    EXPECT_FLOAT_EQ(kCrouchFactor, 0.55F);
    EXPECT_FLOAT_EQ(kCarryingFactor, 0.94F);
    EXPECT_FLOAT_EQ(kDeepSnowFactor, 0.80F);
    EXPECT_FLOAT_EQ(kDeepSnowDepth, 0.12F);
}

TEST(PlayerControllerTests, TheThreeDirectionsAreExactAndTheDiagonalIsBetweenThem)
{
    // §43.2 gives forward, backwards and strafe as three numbers. They are the axes of an ellipse
    // the speed is limited by, not factors to multiply: a body backing away diagonally is not
    // travelling at 0.72 x 0.85 = 0.61 of a walk, which is SLOWER than either of the things that
    // mixture is made of.
    const CollisionWorld world = OneCell({Slab(0.0F, -40.0F, 40.0F)});
    BroadPhase broad;

    struct Case
    {
        float x;
        float y;
        float want;
        const char* what;
    };

    const Case cases[] = {
        {0.0F, 1.0F, 1.0F, "straight forward"},
        {0.0F, -1.0F, cnahouse::player::kBackwardsFactor, "straight back"},
        {1.0F, 0.0F, cnahouse::player::kStrafeFactor, "pure strafe"},
        {-1.0F, 0.0F, cnahouse::player::kStrafeFactor, "the other strafe"},
    };
    for (const Case& one : cases)
    {
        PlayerState state = Standing(0.0F, 0.0F);
        InputState input;
        input.move.X = one.x;
        input.move.Y = one.y;
        const PlayerStepReport step = PlayerStep(world, world.cells[0], broad, state, input, kDt);
        EXPECT_NEAR(step.speedFactor, one.want, 1e-4F) << one.what;
    }

    // ...and the diagonal falls BETWEEN the two it is a mixture of.
    PlayerState diagonal = Standing(0.0F, 0.0F);
    InputState back;
    const float inv = 1.0F / std::sqrt(2.0F);
    back.move.X = inv;
    back.move.Y = -inv;
    const PlayerStepReport step = PlayerStep(world, world.cells[0], broad, diagonal, back, kDt);
    EXPECT_NEAR(step.speedFactor, 0.7772F, 1e-3F);
    EXPECT_GT(step.speedFactor, cnahouse::player::kBackwardsFactor);
    EXPECT_LT(step.speedFactor, cnahouse::player::kStrafeFactor);
}

TEST(PlayerControllerTests, TheStateModifiersMultiplyBecauseTheyAreIndependent)
{
    // A stair, a crouch, a carried item and deep snow are four separate facts about the body, and
    // one doing all four is slowed by all four -- unlike the directional three, which are one
    // fact asked in different directions.
    using namespace cnahouse::player;
    // A stair under a ceiling low enough to crouch under: the crouch is AUTOMATIC (§43.1), so a
    // test that set the flag by hand would be overwritten by the head-room the moment it ran.
    const CollisionWorld stairs = OneCell({Slab(0.0F, -40.0F, 40.0F, CollisionKind::Stair)});
    const CollisionWorld world = OneCell({
        Slab(0.0F, -40.0F, 40.0F, CollisionKind::Stair),
        Box(Vector3(0.0F, 1.60F, 0.0F), Vector3(40.0F, 0.10F, 12.0F), CollisionKind::Ceiling),
    });
    BroadPhase broad;

    PlayerState upright = Standing(0.0F, 0.0F);
    PlayerStep(stairs, stairs.cells[0], broad, upright, InputState{}, kDt);
    ASSERT_EQ(upright.groundKind, CollisionKind::Stair);
    ASSERT_FALSE(upright.crouched);
    PlayerStepReport step = PlayerStep(stairs, stairs.cells[0], broad, upright, Forward(), kDt);
    EXPECT_NEAR(step.speedFactor, kStairsFactor, 1e-4F);

    PlayerState state = Standing(0.0F, 0.0F);
    PlayerStep(world, world.cells[0], broad, state, InputState{}, kDt);
    ASSERT_EQ(state.groundKind, CollisionKind::Stair);
    ASSERT_TRUE(state.crouched) << "the low ceiling should have crouched it";
    step = PlayerStep(world, world.cells[0], broad, state, Forward(), kDt);
    EXPECT_NEAR(step.speedFactor, kStairsFactor * kCrouchFactor, 1e-4F);

    state.carrying = true;
    step = PlayerStep(world, world.cells[0], broad, state, Forward(), kDt);
    EXPECT_NEAR(step.speedFactor, kStairsFactor * kCrouchFactor * kCarryingFactor, 1e-4F);

    // §43.2's snow threshold is a threshold, not a ramp: 0.12 m is not deep and 0.13 is.
    state.snowDepth = 0.12F;
    step = PlayerStep(world, world.cells[0], broad, state, Forward(), kDt);
    EXPECT_NEAR(step.speedFactor, kStairsFactor * kCrouchFactor * kCarryingFactor, 1e-4F);
    state.snowDepth = 0.13F;
    step = PlayerStep(world, world.cells[0], broad, state, Forward(), kDt);
    EXPECT_NEAR(step.speedFactor, kStairsFactor * kCrouchFactor * kCarryingFactor * kDeepSnowFactor, 1e-4F);
}

TEST(PlayerControllerTests, WalkingBackwardsReallyIsSlowerOverTheGround)
{
    // The factor has to reach the body and not just the report. Ten metres backwards takes
    // 1 / 0.72 as long as ten metres forwards, measured over the run.
    const CollisionWorld world = OneCell({Slab(0.0F, -40.0F, 40.0F)});
    BroadPhase broad;

    const auto ticksFor = [&](float moveY)
    {
        PlayerState state = Standing(0.0F, 0.0F);
        InputState input;
        input.move.Y = moveY;
        for (int i = 0; i < 60; ++i)
        {
            PlayerStep(world, world.cells[0], broad, state, input, kDt);
        }
        const float from = state.position.Z;
        int ticks = 0;
        while (std::fabs(state.position.Z - from) < 10.0F && ticks < 4000)
        {
            PlayerStep(world, world.cells[0], broad, state, input, kDt);
            ++ticks;
        }
        return ticks;
    };
    const int forwards = ticksFor(1.0F);
    const int backwards = ticksFor(-1.0F);
    ASSERT_LT(forwards, 4000);
    ASSERT_LT(backwards, 4000);
    EXPECT_NEAR(static_cast<float>(backwards) / static_cast<float>(forwards),
                1.0F / cnahouse::player::kBackwardsFactor,
                0.02F);
}

TEST(PlayerControllerTests, TheCrouchedBodyIsTheOneFortyThreePointOneDescribes)
{
    using namespace cnahouse::player;
    // 1.25 m tall with the same 0.30 m radius, and an eye at 1.15 m.
    EXPECT_FLOAT_EQ(kPlayerCrouchHalfHeight, 0.325F);
    EXPECT_FLOAT_EQ((kPlayerCrouchHalfHeight + kPlayerRadius) * 2.0F, 1.25F);
    EXPECT_FLOAT_EQ(kPlayerCrouchEyeHeight, 1.15F);

    // The FEET are what a crouch preserves: the body shrinks towards the floor.
    PlayerState state = Standing(0.0F, 0.0F);
    const float feet = state.Feet().Y;
    state.crouched = true;
    state.position = Vector3(state.position.X, feet + state.Rise(), state.position.Z);
    EXPECT_NEAR(state.Feet().Y, feet, 1e-6F);
    EXPECT_NEAR(state.Eye().Y - feet, kPlayerCrouchEyeHeight, 1e-6F);
    EXPECT_LT(state.Eye().Y, feet + kPlayerEyeHeight) << "crouching did not lower the eye";
}

TEST(PlayerControllerTests, ALowCeilingCrouchesTheBodyAndALowerOneDoesNotStandItUp)
{
    // §43.1's crouch is AUTOMATIC and attic-only: the player never asks for it. A ceiling at
    // 1.50 m has no room for a 1.80 m body and plenty for a 1.25 m one.
    using namespace cnahouse::player;
    const CollisionWorld world = OneCell({
        Slab(0.0F, -20.0F, 20.0F),
        Box(Vector3(0.0F, 1.60F, 0.0F), Vector3(20.0F, 0.10F, 12.0F), CollisionKind::Ceiling),
    });
    BroadPhase broad;
    PlayerState state = Standing(0.0F, 0.0F);
    ASSERT_FALSE(state.crouched);
    const float feet = state.Feet().Y;

    const PlayerStepReport step = PlayerStep(world, world.cells[0], broad, state, InputState{}, kDt);
    EXPECT_TRUE(state.crouched) << "it stayed standing under a 1.50 m ceiling";
    EXPECT_TRUE(step.crouchChanged);
    EXPECT_NEAR(state.Feet().Y, feet, 2e-3F) << "the crouch moved the feet";

    // It stays crouched for as long as the ceiling is there, and reports the change only once.
    for (int i = 0; i < 60; ++i)
    {
        const PlayerStepReport again = PlayerStep(world, world.cells[0], broad, state, InputState{}, kDt);
        EXPECT_FALSE(again.crouchChanged) << "tick " << i;
        EXPECT_TRUE(state.crouched);
    }

    // ...and the modifier table slows it while it is (§43.2).
    const PlayerStepReport walking = PlayerStep(world, world.cells[0], broad, state, Forward(), kDt);
    EXPECT_NEAR(walking.speedFactor, kCrouchFactor, 1e-4F);
}

TEST(PlayerControllerTests, ItStandsUpAgainOnlyWhenThereIsRoom)
{
    // The other half, and the one that hurts if it is wrong: standing up under a rafter puts the
    // head through it. The ceiling here covers only the western half of the room.
    using namespace cnahouse::player;
    const CollisionWorld world = OneCell({
        Slab(0.0F, -20.0F, 20.0F),
        Box(Vector3(-10.0F, 1.60F, 0.0F), Vector3(10.0F, 0.10F, 12.0F), CollisionKind::Ceiling),
    });
    BroadPhase broad;
    PlayerState state = Standing(-5.0F, 0.0F);
    state.yaw = 1.5707963F; // east, out from under the ceiling

    PlayerStep(world, world.cells[0], broad, state, InputState{}, kDt);
    ASSERT_TRUE(state.crouched);

    bool stoodUp = false;
    for (int i = 0; i < 1200 && !stoodUp; ++i)
    {
        const PlayerStepReport step = PlayerStep(world, world.cells[0], broad, state, Forward(), kDt);
        if (!state.crouched)
        {
            stoodUp = true;
            EXPECT_TRUE(step.crouchChanged);
            EXPECT_GT(state.position.X, -0.6F) << "it stood up while still under the ceiling";
        }
        // Whatever it does, it is never inside the ceiling it is under.
        EXPECT_LE(OverlapCell(world, world.cells[0], broad, state.Body()).depth,
                  cnahouse::physics::kContactTolerance)
            << "tick " << i;
    }
    EXPECT_TRUE(stoodUp) << "it never stood up after leaving the low ceiling";
}

TEST(PlayerControllerTests, ASlopingRafterIsWalkedUnderRatherThanWalkedInto)
{
    // The attic case §43.1 is written for. A rafter sloping down towards the eaves does not stop
    // a standing body in FRONT of it -- it stops it from ABOVE -- so the "does the standing body
    // still fit here?" question cannot see it: the body does still fit where it is. The move is
    // retried crouched, once, exactly as the kerb assist lifts and retries.
    const float slope = 0.5F; // 1 m down over 2 m east
    CollisionMesh rafter;
    rafter.vertices = {Vector3(-2.0F, 2.60F, -6.0F),
                       Vector3(-2.0F, 2.60F, 6.0F),
                       Vector3(14.0F, 2.60F - 16.0F * slope, 6.0F),
                       Vector3(14.0F, 2.60F - 16.0F * slope, -6.0F)};
    rafter.indices = {0u, 1u, 2u, 0u, 2u, 3u};
    rafter.surface = 0u;
    rafter.kind = CollisionKind::Ceiling;
    const CollisionWorld world = OneCell({Slab(0.0F, -20.0F, 20.0F)}, {rafter});
    BroadPhase broad;

    PlayerState state = Standing(-1.5F, 0.0F);
    state.yaw = 1.5707963F;
    ASSERT_FALSE(state.crouched);

    for (int i = 0; i < 900; ++i)
    {
        PlayerStep(world, world.cells[0], broad, state, Forward(), kDt);
        EXPECT_LE(OverlapCell(world, world.cells[0], broad, state.Body()).depth,
                  cnahouse::physics::kContactTolerance)
            << "tick " << i << " at x = " << state.position.X;
    }
    EXPECT_TRUE(state.crouched) << "it never ducked under the rafter";
    // The rafter's clear height is `2.60 - 0.5(x + 2)`. A 1.80 m body runs out of room at
    // x = -0.40 and a 1.25 m one at x = +0.70, so ducking is worth 1.10 m of attic -- and the
    // body has to end up past the first of those and not past the second.
    EXPECT_GT(state.position.X, 0.40F) << "it stopped where a STANDING body would have";
    EXPECT_LT(state.position.X, 0.80F) << "it went further than a 1.25 m body fits";
}

TEST(PlayerControllerTests, ALowHEADERIsDuckedUnderRatherThanWalkedInto)
{
    // The case the ceiling RETRY exists for, and a different one from the sloping rafter above.
    // Under a rafter, the ceiling over the body's own feet comes down first, so the "does the
    // standing body still fit HERE?" question sees it coming. A header does not slope: the room
    // is 2.4 m and the opening is 1.4 m, so the body fits perfectly well where it stands and is
    // stopped by something in front of it that is entirely above its waist.
    const CollisionWorld world = OneCell({
        Slab(0.0F, -20.0F, 20.0F),
        Box(Vector3(0.0F, 2.50F, 0.0F), Vector3(20.0F, 0.10F, 12.0F), CollisionKind::Ceiling),
        // The header: a beam from 1.40 m up to the ceiling, across the body's path at x = 2.
        Box(Vector3(2.0F, 1.95F, 0.0F), Vector3(0.15F, 0.55F, 12.0F), CollisionKind::Wall),
    });
    BroadPhase broad;
    PlayerState state = Standing(0.0F, 0.0F);
    state.yaw = 1.5707963F; // east, at the opening

    PlayerStep(world, world.cells[0], broad, state, InputState{}, kDt);
    ASSERT_FALSE(state.crouched) << "2.4 m of room is not a reason to crouch";

    for (int i = 0; i < 900; ++i)
    {
        PlayerStep(world, world.cells[0], broad, state, Forward(), kDt);
        EXPECT_LE(OverlapCell(world, world.cells[0], broad, state.Body()).depth,
                  cnahouse::physics::kContactTolerance)
            << "tick " << i;
    }
    EXPECT_GT(state.position.X, 4.0F) << "it stopped at the header instead of ducking under it";
    // ...and once through, with 2.4 m over its head again, it stands back up.
    EXPECT_FALSE(state.crouched) << "it stayed crouched after clearing the opening";
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
