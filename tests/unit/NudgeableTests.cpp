// SPDX-License-Identifier: MIT
//
// `HOUSE-00565`. §49.4's twelve nudgeable props: *"a sphere or box with gravity, a support-plane
// resolve, linear+angular damping, and a push impulse from the player capsule. They never stack
// and never sleep-fail because they are always resolved against static geometry only."*
//
// Four behaviours and two promises, and the promises are the interesting half: "never stack" is
// what makes a prop's world static and therefore cheap, and "never sleep-fail" is the jitter a
// resting body gets when gravity puts a millimetre of speed into it every step and the resolve
// takes it out again.
#include <cmath>
#include <vector>

#include <gtest/gtest.h>

#include "cnahouse/physics/BroadPhase.hpp"
#include "cnahouse/physics/Nudgeable.hpp"

namespace
{
    using cnahouse::physics::BroadPhase;
    using cnahouse::physics::Capsule;
    using cnahouse::physics::CollisionCell;
    using cnahouse::physics::CollisionKind;
    using cnahouse::physics::CollisionObb;
    using cnahouse::physics::CollisionWorld;
    using cnahouse::physics::kMaxPushSpeed;
    using cnahouse::physics::kPropGravity;
    using cnahouse::physics::kPropSleepSpeed;
    using cnahouse::physics::NudgeableProp;
    using cnahouse::physics::NudgeReport;
    using cnahouse::physics::NudgeStep;
    using cnahouse::physics::PushProp;
    using Microsoft::Xna::Framework::BoundingBox;
    using Microsoft::Xna::Framework::Vector3;

    constexpr float kDt = 1.0F / 120.0F;

    CollisionObb Box(const Vector3& centre, const Vector3& half, CollisionKind kind)
    {
        CollisionObb obb;
        obb.centre = centre;
        obb.halfExtents = half;
        obb.kind = kind;
        return obb;
    }

    /// A room 12 m square with a floor at y = 0, on a grid coarse enough that every bucket holds
    /// everything: the broad phase has its own tests.
    CollisionWorld Room(std::vector<CollisionObb> extra = {})
    {
        CollisionWorld world;
        world.gridCell = 1.0F;
        world.surfaces = {"tile"};
        world.obbs = {Box(Vector3(0.0F, -0.25F, 0.0F), Vector3(6.0F, 0.25F, 6.0F), CollisionKind::Floor)};
        for (const CollisionObb& obb : extra)
        {
            world.obbs.push_back(obb);
        }

        CollisionCell cell;
        cell.id = "L0_ROOM";
        cell.bounds = BoundingBox(Vector3(-6.0F, -1.0F, -6.0F), Vector3(6.0F, 4.0F, 6.0F));
        cell.nx = 12u;
        cell.nz = 12u;
        cell.originX = -6.0F;
        cell.originZ = -6.0F;
        cell.buckets.assign(144u, {});
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

    /// §49.4's football: 0.11 m and 0.43 kg, the FIFA size 5 numbers.
    NudgeableProp Football(float x, float y, float z)
    {
        NudgeableProp ball;
        ball.id = cnahouse::util::Intern("PROP_FOOTBALL");
        ball.position = Vector3(x, y, z);
        ball.radius = 0.11F;
        ball.halfHeight = 0.0F;
        ball.mass = 0.43F;
        return ball;
    }

    /// ...and a waste bin: 0.15 m across, 0.30 m tall, 1.2 kg empty.
    NudgeableProp Bin(float x, float y, float z)
    {
        NudgeableProp bin;
        bin.id = cnahouse::util::Intern("PROP_BIN");
        bin.position = Vector3(x, y, z);
        bin.radius = 0.15F;
        bin.halfHeight = 0.075F;
        bin.mass = 1.2F;
        return bin;
    }

    float Speed(const NudgeableProp& prop)
    {
        return std::sqrt(prop.velocity.X * prop.velocity.X + prop.velocity.Z * prop.velocity.Z);
    }

} // namespace

TEST(NudgeableTests, ADroppedPropFallsAtTheHousesGravityAndLandsOnTheFloor)
{
    const CollisionWorld world = Room();
    BroadPhase broad;
    NudgeableProp ball = Football(0.0F, 2.0F, 0.0F);

    // One step of §49.3's semi-implicit Euler is g·dt², the same first step the player's fall
    // takes: one house, one gravity.
    const NudgeReport first = NudgeStep(world, world.cells[0], broad, ball, kDt);
    EXPECT_NEAR(2.0F - ball.position.Y, kPropGravity * kDt * kDt, 1e-6F);
    EXPECT_FALSE(first.supported);

    bool landed = false;
    for (int i = 0; i < 240 && !landed; ++i)
    {
        landed = NudgeStep(world, world.cells[0], broad, ball, kDt).supported;
    }
    ASSERT_TRUE(landed) << "the ball fell through the floor";
    // Resting ON the floor: its lowest point is the floor's face, give or take the thousandth of
    // a step §49.3 lands everything a hair clear of.
    EXPECT_NEAR(ball.position.Y - ball.radius, 0.0F, 0.005F);
    EXPECT_LE(ball.position.Y - ball.radius, 0.001F + 0.005F);
}

TEST(NudgeableTests, APropOnTheFloorGoesToSleepAndStaysExactlyWhereItIs)
{
    // §49.4's "never sleep-fail". Gravity puts 0.08 m/s into a resting body every step and the
    // support plane takes it out again; without a sleep state the prop's position wanders by a
    // hundredth of a millimetre a step for ever, which is a prop that never stops moving in a
    // house whose whole point is that it is still.
    const CollisionWorld world = Room();
    BroadPhase broad;
    NudgeableProp bin = Bin(1.0F, 0.30F, -1.0F);

    for (int i = 0; i < 240; ++i)
    {
        NudgeStep(world, world.cells[0], broad, bin, kDt);
    }
    ASSERT_TRUE(bin.resting) << "it never went to sleep";
    const Vector3 asleep = bin.position;

    for (int i = 0; i < 1200; ++i) // ten seconds
    {
        const NudgeReport report = NudgeStep(world, world.cells[0], broad, bin, kDt);
        EXPECT_TRUE(report.resting);
        EXPECT_FLOAT_EQ(report.travelled, 0.0F);
    }
    EXPECT_FLOAT_EQ(bin.position.X, asleep.X);
    EXPECT_FLOAT_EQ(bin.position.Y, asleep.Y);
    EXPECT_FLOAT_EQ(bin.position.Z, asleep.Z);
    EXPECT_FLOAT_EQ(Speed(bin), 0.0F);
}

TEST(NudgeableTests, TheSameShoveMovesTheBallAndBarelyMovesTheCrate)
{
    // §49.4 says the player NUDGES these, and dividing the impulse by the mass is what makes a
    // football and a 12 kg crate feel like different objects rather than one prop with two names.
    const CollisionWorld world = Room();
    BroadPhase broad;

    NudgeableProp ball = Football(0.5F, 0.11F, 0.0F);
    NudgeableProp crate = Football(0.5F, 0.11F, 0.0F);
    crate.id = cnahouse::util::Intern("PROP_CRATE");
    crate.mass = 12.0F;

    // A body walking east at §43.2's 1.35 m/s, its shoulder against both.
    const Capsule player{Vector3(0.20F, 0.90F, 0.0F), 0.60F, 0.30F};
    const Vector3 walking(1.35F, 0.0F, 0.0F);
    ASSERT_TRUE(PushProp(ball, player, walking));
    ASSERT_TRUE(PushProp(crate, player, walking));

    EXPECT_GT(ball.velocity.X, 0.0F) << "the ball was pushed backwards";
    EXPECT_GT(Speed(ball), 4.0F * Speed(crate)) << "mass did not divide the push";
    EXPECT_LE(Speed(ball), kMaxPushSpeed) << "a nudge became a kick";
    EXPECT_FALSE(ball.resting);

    // ...and a body walking AWAY from a prop it is touching does not tow it.
    NudgeableProp untouched = Football(0.5F, 0.11F, 0.0F);
    ASSERT_TRUE(PushProp(untouched, player, Vector3(-1.35F, 0.0F, 0.0F)));
    EXPECT_FLOAT_EQ(Speed(untouched), 0.0F);
}

TEST(NudgeableTests, AShovedBallRollsToAStopRatherThanForEver)
{
    // Linear damping, and on the ground it is friction. The distance is the number worth pinning:
    // a ball shoved at walking pace crosses a kitchen and stops, it does not cross the house.
    const CollisionWorld world = Room();
    BroadPhase broad;
    NudgeableProp ball = Football(0.0F, 0.11F, 0.0F);
    const Capsule player{Vector3(-0.31F, 0.90F, 0.0F), 0.60F, 0.30F};
    ASSERT_TRUE(PushProp(ball, player, Vector3(1.35F, 0.0F, 0.0F)));
    const float started = ball.position.X;

    int steps = 0;
    while (!ball.resting && steps < 1200)
    {
        NudgeStep(world, world.cells[0], broad, ball, kDt);
        ++steps;
    }
    ASSERT_TRUE(ball.resting) << "it never stopped";
    const float rolled = ball.position.X - started;
    EXPECT_GT(rolled, 0.5F) << "a shove at walking pace barely moved it";
    EXPECT_LT(rolled, 3.0F) << "a shove at walking pace crossed the house";
    std::printf("  a 1.35 m/s shove rolls the ball %.2f m in %.2f s\n",
                static_cast<double>(rolled),
                static_cast<double>(steps) * static_cast<double>(kDt));
}

TEST(NudgeableTests, APropStopsAtAWallAndDoesNotKeepPressingIntoIt)
{
    const CollisionWorld world =
        Room({Box(Vector3(2.0F, 0.5F, 0.0F), Vector3(0.1F, 0.5F, 4.0F), CollisionKind::Wall)});
    BroadPhase broad;
    NudgeableProp ball = Football(1.0F, 0.11F, 0.0F);
    ball.velocity = Vector3(2.0F, 0.0F, 0.0F);

    for (int i = 0; i < 240; ++i)
    {
        NudgeStep(world, world.cells[0], broad, ball, kDt);
    }
    // Against the wall's face at x = 1.9, one radius clear of it, and no longer trying.
    EXPECT_LT(ball.position.X, 1.9F);
    EXPECT_NEAR(ball.position.X + ball.radius, 1.9F, 0.02F);
    EXPECT_TRUE(ball.resting) << "it is still driving into the wall";
    EXPECT_FLOAT_EQ(ball.velocity.X, 0.0F);
}

TEST(NudgeableTests, ThePropsDoNotStack)
{
    // §49.4's own sentence: *"always resolved against static geometry only"*. Two props in the
    // same place ignore each other -- one falls THROUGH the other to the floor -- and that is the
    // property that makes the list cheap and stops a tower of bins from ever existing.
    const CollisionWorld world = Room();
    BroadPhase broad;
    NudgeableProp lower = Bin(0.0F, 0.30F, 0.0F);
    NudgeableProp upper = Bin(0.0F, 0.80F, 0.0F);

    for (int i = 0; i < 240; ++i)
    {
        NudgeStep(world, world.cells[0], broad, lower, kDt);
        NudgeStep(world, world.cells[0], broad, upper, kDt);
    }
    EXPECT_TRUE(lower.resting);
    EXPECT_TRUE(upper.resting);
    // Both on the FLOOR, in the same place, because neither is in the other's world.
    EXPECT_NEAR(upper.position.Y, lower.position.Y, 0.005F);
    EXPECT_NEAR(upper.position.Y - lower.halfHeight - lower.radius, 0.0F, 0.01F);
}

TEST(NudgeableTests, AnOffCentreShoveSpinsItAndTheSpinDampsOut)
{
    const CollisionWorld world = Room();
    BroadPhase broad;
    NudgeableProp bin = Bin(0.0F, 0.30F, 0.0F);
    // A body walking east whose middle passes to the SOUTH of the bin's: the contact is off
    // centre, so the shove turns it as well as moving it.
    const Capsule player{Vector3(-0.40F, 0.90F, 0.20F), 0.60F, 0.30F};
    ASSERT_TRUE(PushProp(bin, player, Vector3(1.35F, 0.0F, 0.0F)));
    EXPECT_NE(bin.spin, 0.0F) << "an off-centre shove did not turn it";
    const float spun = std::fabs(bin.spin);

    for (int i = 0; i < 120; ++i)
    {
        NudgeStep(world, world.cells[0], broad, bin, kDt);
    }
    EXPECT_LT(std::fabs(bin.spin), spun) << "the spin never damps";

    // ...and a shove straight through the middle does not turn it at all.
    NudgeableProp square = Bin(0.0F, 0.30F, 0.0F);
    const Capsule head{Vector3(-0.40F, 0.90F, 0.0F), 0.60F, 0.30F};
    ASSERT_TRUE(PushProp(square, head, Vector3(1.35F, 0.0F, 0.0F)));
    EXPECT_NEAR(square.spin, 0.0F, 1e-6F);
}

TEST(NudgeableTests, TheAnswerIsTheSameAtThirtyAndAtOneFortyFourFrames)
{
    // §49.3's determinism clause reaches these too: the damping is `exp(-k·dt)` and not
    // `1 - k·dt`, so a prop shoved at 30 FPS and one shoved at 144 stop in the same place. The
    // cheap form is wrong by 8 % of the distance over a second, which is a ball that stops on a
    // different floorboard depending on the frame rate.
    const CollisionWorld world = Room();
    const auto roll = [&](float dt, int steps)
    {
        BroadPhase broad;
        NudgeableProp ball = Football(0.0F, 0.11F, 0.0F);
        ball.velocity = Vector3(1.5F, 0.0F, 0.0F);
        for (int i = 0; i < steps; ++i)
        {
            NudgeStep(world, world.cells[0], broad, ball, dt);
        }
        return ball.position.X;
    };

    // One second, at three step sizes. The FIXED step is 1/120 and these are what a caller with a
    // different accumulator would pass; the answers agree to a millimetre.
    const float at120 = roll(1.0F / 120.0F, 120);
    const float at30 = roll(1.0F / 30.0F, 30);
    const float at144 = roll(1.0F / 144.0F, 144);
    std::printf("  one second of rolling: %.5f m at 30 fps, %.5f at 120, %.5f at 144\n",
                static_cast<double>(at30),
                static_cast<double>(at120),
                static_cast<double>(at144));
    // Tight on purpose. The exact form has NO step-size dependence at all, so the spread is float
    // noise and the contact back-off; the cheap `x += v·dt` is out by k·dt/2 of the distance --
    // 2.3 % at 30 fps, 13 mm over this second -- and a tolerance that swallowed that would be a
    // test that does not notice the thing it is for.
    EXPECT_NEAR(at30, at120, 0.003F);
    EXPECT_NEAR(at144, at120, 0.001F);
}
