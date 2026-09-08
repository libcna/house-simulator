// SPDX-License-Identifier: MIT
//
// `HOUSE-00552`. §43.1's gravity -- *"9.81 m/s², used only for stepping down and the short fall
// onto the terrace"* -- with §47.2's `land_soft` / `land_hard` decided by §43.1's *"a fall > 2.4 m
// plays a heavy landing sound; there is no damage"*.
//
// There is no jump in this game and nothing to fall off that is more than a storey high, which is
// why the whole of falling is one function and not an integrator. What it still has to get right
// is the arithmetic (a fall of a known height takes a known number of 1/120 s steps and ends at a
// known speed), the threshold (either side of 2.4 m), and the one guarantee that matters: a body
// cannot fall through a floor.
#include <cmath>
#include <memory>
#include <string>

#include <gtest/gtest.h>

#include "System/IO/FileAccess.hpp"
#include "System/IO/FileMode.hpp"
#include "System/IO/FileStream.hpp"

#include "cnahouse/physics/BroadPhase.hpp"
#include "cnahouse/physics/CollisionLoader.hpp"
#include "cnahouse/physics/Fall.hpp"

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
    using cnahouse::physics::FallState;
    using cnahouse::physics::FallStep;
    using cnahouse::physics::kGravity;
    using cnahouse::physics::kHardLandingDrop;
    using cnahouse::physics::kTerminalFallSpeed;
    using cnahouse::physics::Landing;
    using cnahouse::physics::OverlapCell;
    using cnahouse::physics::Sphere;
    using Microsoft::Xna::Framework::BoundingBox;
    using Microsoft::Xna::Framework::Vector3;

    constexpr float kBodyRadius = 0.30F;
    constexpr float kBodyHalfHeight = 0.60F;
    constexpr float kStand = kBodyHalfHeight + kBodyRadius;
    /// §49.3's fixed step.
    constexpr float kDt = 1.0F / 120.0F;

    CollisionObb Box(const Vector3& centre, const Vector3& halfExtents, CollisionKind kind)
    {
        CollisionObb obb;
        obb.centre = centre;
        obb.halfExtents = halfExtents;
        obb.kind = kind;
        return obb;
    }

    CollisionWorld OneCell(std::vector<CollisionObb> obbs, std::vector<CollisionMesh> meshes = {})
    {
        CollisionWorld world;
        world.gridCell = 1.0F;
        world.surfaces = {"plaster"};
        world.obbs = std::move(obbs);
        world.meshes = std::move(meshes);

        CollisionCell cell;
        cell.id = "L0_TEST";
        cell.bounds = BoundingBox(Vector3(-8.0F, -16.0F, -8.0F), Vector3(8.0F, 16.0F, 8.0F));
        cell.nx = 16u;
        cell.nz = 16u;
        cell.originX = -8.0F;
        cell.originZ = -8.0F;
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

    /// A floor whose top is at y = 0, and nothing else.
    CollisionWorld Floor()
    {
        return OneCell({Box(Vector3(0.0F, -0.25F, 0.0F), Vector3(6.0F, 0.25F, 6.0F), CollisionKind::Floor)});
    }

    /// Drops a body from @p height above the floor until it lands, and reports the step it landed
    /// on. Steps are §49.3's fixed 1/120 s, which is the only size this is ever run at.
    FallStep DropFrom(const CollisionWorld& world, float height, int* steps = nullptr)
    {
        BroadPhase broad;
        Capsule body{Vector3(0.0F, height + kStand, 0.0F), kBodyHalfHeight, kBodyRadius};
        FallState state;
        state.onGround = false;
        state.fellFrom = body.centre.Y;
        FallStep step;
        int taken = 0;
        for (; taken < 2000; ++taken)
        {
            step = Fall(world, world.cells[0], broad, body, state, kDt);
            body.centre = step.position;
            state = step.state;
            if (state.onGround)
            {
                break;
            }
        }
        if (steps != nullptr)
        {
            *steps = taken + 1;
        }
        return step;
    }

} // namespace

TEST(FallTests, TheNumbersAreTheOnesTheDesignStates)
{
    EXPECT_FLOAT_EQ(kGravity, 9.81F);
    EXPECT_FLOAT_EQ(kTerminalFallSpeed, 12.0F);
    EXPECT_FLOAT_EQ(kHardLandingDrop, 2.4F);
}

TEST(FallTests, ABodyOnTheGroundDoesNotAccumulateASpeedItCannotSpend)
{
    // Integrating gravity into a body that is already resting on something builds a downward speed
    // out of nothing, and hands it to whichever step first walks off a kerb -- which then lands
    // hard from 0.15 m. A body standing still stays exactly still.
    const CollisionWorld world = Floor();
    BroadPhase broad;
    const Capsule body{Vector3(0.0F, kStand + 0.005F, 0.0F), kBodyHalfHeight, kBodyRadius};
    FallState state; // onGround by default
    for (int i = 0; i < 240; ++i)
    {
        const FallStep step = Fall(world, world.cells[0], broad, body, state, kDt);
        state = step.state;
        EXPECT_FLOAT_EQ(step.position.Y, body.centre.Y);
        EXPECT_FLOAT_EQ(state.speed, 0.0F);
        EXPECT_EQ(step.landing, Landing::None);
    }
}

TEST(FallTests, TheFirstStepOfAFallIsGravityTimesTheStepSquared)
{
    // Semi-implicit Euler: the speed is advanced first and the body moved at the NEW speed. So one
    // step from rest is g·dt of speed and g·dt² of distance -- 81.75 mm/s and 0.68 mm -- and not
    // the zero distance that moving-then-accelerating would give.
    const CollisionWorld world = OneCell({}); // nothing to land on
    BroadPhase broad;
    // Near the origin on purpose: the claim is a 0.68 MILLIMETRE step, and a float at y = 500
    // cannot tell one of those from the next.
    const Capsule body{Vector3(0.0F, 1.0F, 0.0F), kBodyHalfHeight, kBodyRadius};
    FallState state;
    state.onGround = false;
    state.fellFrom = body.centre.Y;

    const FallStep first = Fall(world, world.cells[0], broad, body, state, kDt);
    EXPECT_NEAR(first.state.speed, kGravity * kDt, 1e-6F);
    EXPECT_NEAR(body.centre.Y - first.position.Y, kGravity * kDt * kDt, 1e-7F);
    EXPECT_FALSE(first.state.onGround);
    EXPECT_EQ(first.landing, Landing::None);
}

TEST(FallTests, TheSpeedStopsAtTerminalAndTheStepStopsAtTenCentimetres)
{
    // 12 m/s is reached after 1.22 s -- 147 steps -- and never exceeded. That bound is worth more
    // than the realism: it caps what ONE fixed step can move a body to 0.10 m, and a step that
    // cannot move a body 0.10 m cannot put it through a 0.15 m floor slab whatever else goes
    // wrong.
    const CollisionWorld world = OneCell({});
    BroadPhase broad;
    Capsule body{Vector3(0.0F, 40.0F, 0.0F), kBodyHalfHeight, kBodyRadius};
    FallState state;
    state.onGround = false;
    state.fellFrom = body.centre.Y;

    float worstStep = 0.0F;
    for (int i = 0; i < 400; ++i)
    {
        const FallStep step = Fall(world, world.cells[0], broad, body, state, kDt);
        worstStep = std::max(worstStep, body.centre.Y - step.position.Y);
        body.centre = step.position;
        state = step.state;
        EXPECT_LE(state.speed, kTerminalFallSpeed);
    }
    EXPECT_FLOAT_EQ(state.speed, kTerminalFallSpeed);
    EXPECT_NEAR(worstStep, kTerminalFallSpeed * kDt, 1e-4F);
    EXPECT_LT(worstStep, 0.1001F);
}

TEST(FallTests, AShortFallLandsSoftAndOnTheFloor)
{
    // 0.50 m: `√(2h/g)` is 0.319 s, so 39 of §49.3's steps. The body ends standing on the floor,
    // not in it and not hovering over it.
    const CollisionWorld world = Floor();
    int steps = 0;
    const FallStep step = DropFrom(world, 0.50F, &steps);

    EXPECT_EQ(step.landing, Landing::Soft);
    EXPECT_TRUE(step.state.onGround);
    EXPECT_FLOAT_EQ(step.state.speed, 0.0F);
    EXPECT_NEAR(step.drop, 0.50F, 0.01F);
    EXPECT_NEAR(step.position.Y, kStand, 1e-3F);
    EXPECT_GT(step.position.Y, kStand) << "it must not end the step touching the floor";
    EXPECT_NEAR(static_cast<float>(steps), std::sqrt(2.0F * 0.50F / kGravity) / kDt, 2.0F);
}

TEST(FallTests, AStoreyIsAHardLanding)
{
    // 3.05 m, which is §12's ground-floor storey height and the tallest thing in the house to fall
    // off. Well past §43.1's 2.4 m, so `land_hard`.
    const CollisionWorld world = Floor();
    const FallStep step = DropFrom(world, 3.05F);
    EXPECT_EQ(step.landing, Landing::Hard);
    EXPECT_NEAR(step.drop, 3.05F, 0.02F);
    EXPECT_NEAR(step.position.Y, kStand, 1e-3F);
}

TEST(FallTests, TwoPointFourMetresIsWhereSoftBecomesHard)
{
    // Either side of §43.1's threshold by five centimetres. A fall converges on the floor from
    // ABOVE -- the last step overshoots and is cut short by the sweep -- so the measured drop is
    // the authored height to within a millimetre or two, which is what makes a 50 mm margin a
    // wide one.
    const CollisionWorld world = Floor();
    EXPECT_EQ(DropFrom(world, kHardLandingDrop - 0.05F).landing, Landing::Soft);
    EXPECT_EQ(DropFrom(world, kHardLandingDrop + 0.05F).landing, Landing::Hard);
}

TEST(FallTests, TheDropIsMeasuredFromWhereTheFallBeganAndNotFromTheStep)
{
    // A two-metre fall is a hundred and twenty steps of about fifteen millimetres. Measuring the
    // drop from the top of the LAST step would call every fall in the house soft.
    const CollisionWorld world = Floor();
    const FallStep step = DropFrom(world, 2.0F);
    EXPECT_NEAR(step.drop, 2.0F, 0.01F);
    EXPECT_GT(step.drop, 0.1F) << "the drop was measured over one step, not over the fall";
}

TEST(FallTests, ASlopeTooSteepToLandOnDoesNotEndTheFall)
{
    // A 60° bank. The body stops going through it -- nothing falls through geometry -- but it has
    // not LANDED: §43.1 says that is not a surface to stand on, so the fall continues and the
    // horizontal part of the step sheds the body sideways next tick.
    const float radians = 60.0F * 3.14159265F / 180.0F;
    const float rise = 6.0F * std::tan(radians);
    CollisionMesh bank;
    bank.vertices = {Vector3(-6.0F, rise, -4.0F),
                     Vector3(-6.0F, rise, 4.0F),
                     Vector3(6.0F, -rise, 4.0F),
                     Vector3(6.0F, -rise, -4.0F)};
    bank.indices = {0u, 1u, 2u, 0u, 2u, 3u};
    bank.surface = 0u;
    bank.kind = CollisionKind::Floor;
    const CollisionWorld world = OneCell({}, {bank});

    BroadPhase broad;
    Capsule body{Vector3(0.0F, 3.0F, 0.0F), kBodyHalfHeight, kBodyRadius};
    FallState state;
    state.onGround = false;
    state.fellFrom = body.centre.Y;
    bool slid = false;
    for (int i = 0; i < 400 && !state.onGround; ++i)
    {
        const FallStep step = Fall(world, world.cells[0], broad, body, state, kDt);
        slid = slid || step.slid;
        body.centre = step.position;
        state = step.state;
        EXPECT_EQ(step.landing, Landing::None);
    }
    EXPECT_TRUE(slid) << "the body never met the bank at all";
    EXPECT_FALSE(state.onGround) << "a 60 degree bank is not a place to land";
}

TEST(FallTests, NoBodyFallsThroughAnyFloorInTheRealHouse)
{
    // §49.5's promise -- *"cannot fall through any floor"* -- in the form this task can keep.
    // `HOUSE-00614` does the 2 000 randomised drops; this drops one body from just under the
    // ceiling of every cell in the real house and requires it to arrive on that cell's floor, at
    // rest, not inside anything, and never below the cell it started in.
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
    std::size_t dropped = 0;
    std::size_t hard = 0;
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
        // The floor is found by dropping a 50 mm probe, not read off `bounds.Min.Y` -- which is
        // the bottom of the floor SLAB, 0.35 m lower in the basement (`HOUSE-00547`) -- and the
        // body is then held over it at the greatest of a few heights that is actually clear. A
        // body placed just under a ceiling is usually INSIDE the ceiling slab.
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

        Capsule body{Vector3(midX, 0.0F, midZ), kBodyHalfHeight, kBodyRadius};
        float above = 0.0F;
        for (const float candidate : {1.50F, 1.00F, 0.50F, 0.25F})
        {
            body.centre = Vector3(midX, floorY + kStand + candidate, midZ);
            if (!OverlapCell(*world, cell, broad, body).overlapped)
            {
                above = candidate;
                break;
            }
        }
        if (above == 0.0F)
        {
            continue; // no room over this cell's floor to hold a body up in
        }
        FallState state;
        state.onGround = false;
        state.fellFrom = body.centre.Y;

        FallStep step;
        int taken = 0;
        for (; taken < 600 && !state.onGround; ++taken)
        {
            step = Fall(*world, cell, broad, body, state, kDt);
            body.centre = step.position;
            state = step.state;
            ASSERT_GE(body.centre.Y, cell.bounds.Min.Y - 0.5F)
                << cell.id << ": the body left the bottom of its own cell";
        }
        if (!state.onGround)
        {
            continue; // an exterior cell open to the sky below, or a shaft; not this test's claim
        }
        ++dropped;
        hard += step.landing == Landing::Hard ? 1u : 0u;
        EXPECT_NE(step.landing, Landing::None) << cell.id << ": it stopped without landing";
        EXPECT_GT(step.drop, 0.0F) << cell.id;
        EXPECT_FALSE(OverlapCell(*world, cell, broad, body).overlapped)
            << cell.id << ": it landed INSIDE the floor";
    }
    ASSERT_GT(dropped, 40u) << "only " << dropped << " cells were dropped into";
    // Nothing here is dropped further than 1.50 m, which is under §43.1's 2.4 m, so every one of
    // these must be soft. A hard one would mean the drop is being measured from somewhere other
    // than where the fall started.
    EXPECT_EQ(hard, 0u) << hard << " of " << dropped << " drops of at most 1.50 m landed hard";
}
