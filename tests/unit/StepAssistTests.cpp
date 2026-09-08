// SPDX-License-Identifier: MIT
//
// `HOUSE-00551`. §49.3 step 4 -- *"StepUp: if blocked horizontally and a 0.22 m raised sweep is
// clear, lift and retry once"* -- and §43.1's step-down, which is the same idea pointing the other
// way and twice as far: 0.22 m up, 0.45 m down.
//
// The two numbers are not symmetric by accident. Going UP, 0.22 m is chosen to clear every step
// this house has (a 175 mm riser, a 150 mm kerb, a 20 mm threshold) and to stop well short of
// anything a body should have to climb deliberately. Coming DOWN, a body that leaves a tread
// horizontally is over nothing for a tick and would start a fall on every single stair; 0.45 m
// catches it, and beyond that it really is a drop.
#include <cmath>
#include <memory>
#include <string>

#include <gtest/gtest.h>

#include "System/IO/FileAccess.hpp"
#include "System/IO/FileMode.hpp"
#include "System/IO/FileStream.hpp"

#include "cnahouse/physics/BroadPhase.hpp"
#include "cnahouse/physics/CollisionLoader.hpp"
#include "cnahouse/physics/Move.hpp"

namespace
{
    using cnahouse::physics::BroadPhase;
    using cnahouse::physics::Capsule;
    using cnahouse::physics::CellOverlap;
    using cnahouse::physics::CollisionCell;
    using cnahouse::physics::CollisionKind;
    using cnahouse::physics::CollisionLoader;
    using cnahouse::physics::CollisionMesh;
    using cnahouse::physics::CollisionObb;
    using cnahouse::physics::CollisionWorld;
    using cnahouse::physics::Depenetrate;
    using cnahouse::physics::Depenetration;
    using cnahouse::physics::kStepDownHeight;
    using cnahouse::physics::kStepUpHeight;
    using cnahouse::physics::MoveWithStepAssist;
    using cnahouse::physics::OverlapCell;
    using cnahouse::physics::Sphere;
    using cnahouse::physics::StepAssist;
    using Microsoft::Xna::Framework::BoundingBox;
    using Microsoft::Xna::Framework::Vector3;

    constexpr float kBodyRadius = 0.30F;
    constexpr float kBodyHalfHeight = 0.60F;
    constexpr float kStand = kBodyHalfHeight + kBodyRadius;

    CollisionObb Box(const Vector3& centre, const Vector3& halfExtents, CollisionKind kind)
    {
        CollisionObb obb;
        obb.centre = centre;
        obb.halfExtents = halfExtents;
        obb.kind = kind;
        return obb;
    }

    /// A slab whose TOP is at @p top, 12 m by 12 m, half a metre thick.
    CollisionObb Ground(float top, float centreX = 0.0F, float sizeX = 12.0F)
    {
        return Box(
            Vector3(centreX, top - 0.25F, 0.0F), Vector3(sizeX * 0.5F, 0.25F, 6.0F), CollisionKind::Floor);
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
        cell.bounds = BoundingBox(Vector3(-8.0F, -8.0F, -8.0F), Vector3(8.0F, 8.0F, 8.0F));
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

    /// A body standing on a surface at @p feet -- with five millimetres of daylight under it,
    /// which is where the system itself always leaves one.
    ///
    /// Nothing in §49.3 ever puts a body exactly ON a surface: a slide backs off by `hit.t · 0.999`
    /// and a depenetration pushes until it is strictly clear, both of which end with a positive
    /// gap. A body constructed exactly touching is in a state the game cannot reach, and it
    /// behaves differently -- `startedInside`, which stops the step dead -- so testing from there
    /// would be testing something else.
    constexpr float kRest = 0.005F;

    Capsule Body(float x, float feet)
    {
        return Capsule{Vector3(x, feet + kStand + kRest, 0.0F), kBodyHalfHeight, kBodyRadius};
    }

    /// Ground at y = 0 west of x = 0.5, and a step of height @p rise east of it.
    CollisionWorld Step(float rise)
    {
        return OneCell({
            Ground(0.0F),
            Box(Vector3(2.5F, rise * 0.5F, 0.0F), Vector3(2.0F, rise * 0.5F, 6.0F), CollisionKind::Floor),
        });
    }

} // namespace

TEST(StepAssistTests, TheTwoHeightsAreTheOnesTheDesignStates)
{
    EXPECT_FLOAT_EQ(kStepUpHeight, 0.22F);
    EXPECT_FLOAT_EQ(kStepDownHeight, 0.45F);
}

// ---------------------------------------------------------------------------------------------
// Up
// ---------------------------------------------------------------------------------------------

TEST(StepAssistTests, AKerbIsClimbedWithoutBeingAskedTo)
{
    // A 0.20 m kerb. The plain slide stops dead against its west face; lifting 0.22 m puts the
    // body's feet 0.02 m above the kerb top, the retry crosses it, and the settle gives the lift
    // back all but that 0.02 m. Net: the body is 0.20 m higher and standing ON the kerb.
    const CollisionWorld world = Step(0.20F);
    BroadPhase broad;
    const Capsule start = Body(0.0F, 0.0F);
    const StepAssist step =
        MoveWithStepAssist(world, world.cells[0], broad, start, Vector3(1.0F, 0.0F, 0.0F));

    EXPECT_TRUE(step.steppedUp);
    // The net rise is the kerb less the 5 mm of daylight the body started with, because it ends
    // resting ON the kerb rather than 5 mm over it.
    EXPECT_NEAR(step.rise, 0.20F - kRest, 1e-3F);
    EXPECT_NEAR(step.position.Y, kStand + 0.20F, 1e-3F);
    EXPECT_GT(step.position.X, 0.9F) << "and it got where it was going, not just upward";
    EXPECT_FALSE(step.airborne);

    Capsule ended = start;
    ended.centre = step.position;
    EXPECT_FALSE(OverlapCell(world, world.cells[0], broad, ended).overlapped)
        << "a body standing on a kerb is not inside it";
}

TEST(StepAssistTests, ALedgeTallerThanTheAssistIsAWall)
{
    // 0.30 m. The lift is 0.22, so the raised retry is still standing in front of the same face
    // and gets no further -- there is nothing to accept, and the plain slide stands.
    const CollisionWorld world = Step(0.30F);
    BroadPhase broad;
    const Capsule start = Body(0.0F, 0.0F);
    const StepAssist step =
        MoveWithStepAssist(world, world.cells[0], broad, start, Vector3(1.0F, 0.0F, 0.0F));

    EXPECT_FALSE(step.steppedUp);
    EXPECT_NEAR(step.position.Y, start.centre.Y, 1e-3F);
    EXPECT_LT(step.position.X, 0.5F - kBodyRadius + 1e-3F) << "it stopped at the ledge";
    EXPECT_EQ(step.slide.contacts, 1);
}

TEST(StepAssistTests, TheLimitIsWhereTheDesignPutsIt)
{
    // Either side of 0.22 m, and not right up against it. A ledge exactly as tall as the lift
    // leaves the raised body's feet EXACTLY level with its top, which is a float comparison rather
    // than a design decision; the cases here are a centimetre clear of that on both sides.
    BroadPhase broad;
    for (const float rise : {0.05F, 0.15F, 0.21F})
    {
        const CollisionWorld world = Step(rise);
        const StepAssist step =
            MoveWithStepAssist(world, world.cells[0], broad, Body(0.0F, 0.0F), Vector3(1.0F, 0.0F, 0.0F));
        EXPECT_TRUE(step.steppedUp) << rise << " m was not climbed";
        EXPECT_NEAR(step.rise, rise - kRest, 1e-3F) << rise;
    }
    for (const float rise : {0.24F, 0.35F, 0.80F})
    {
        const CollisionWorld world = Step(rise);
        const StepAssist step =
            MoveWithStepAssist(world, world.cells[0], broad, Body(0.0F, 0.0F), Vector3(1.0F, 0.0F, 0.0F));
        EXPECT_FALSE(step.steppedUp) << rise << " m was climbed";
    }
}

TEST(StepAssistTests, AKerbUnderALowCeilingIsNotClimbed)
{
    // The same 0.20 m kerb with 1.85 m of clear height over it. §43.1's body is 1.80 m tall, so it
    // FITS -- and cannot be lifted 0.22 m to climb anything, because there is 0.05 m of air over
    // its head. A step assist that did not ask would put the player's head through the ceiling.
    CollisionWorld world = Step(0.20F);
    world.obbs.push_back(
        Box(Vector3(0.0F, 1.85F + 0.1F, 0.0F), Vector3(6.0F, 0.1F, 6.0F), CollisionKind::Ceiling));
    world.cells[0].shapes.push_back(static_cast<std::uint32_t>(world.obbs.size() - 1));
    for (auto& bucket : world.cells[0].buckets)
    {
        bucket.push_back(static_cast<std::uint16_t>(world.obbs.size() - 1));
    }
    BroadPhase broad;
    const StepAssist step =
        MoveWithStepAssist(world, world.cells[0], broad, Body(0.0F, 0.0F), Vector3(1.0F, 0.0F, 0.0F));
    EXPECT_FALSE(step.steppedUp);
}

TEST(StepAssistTests, AnUnobstructedStepIsNotDisturbed)
{
    // Nothing in the way: no lift is attempted, nothing is settled, and the body goes exactly
    // where it was sent. The assist has to be invisible when it is not needed.
    const CollisionWorld world = OneCell({Ground(0.0F)});
    BroadPhase broad;
    const Capsule start = Body(0.0F, 0.0F);
    const StepAssist step =
        MoveWithStepAssist(world, world.cells[0], broad, start, Vector3(0.5F, 0.0F, 0.0F));
    EXPECT_FALSE(step.steppedUp);
    EXPECT_FALSE(step.airborne);
    EXPECT_NEAR(step.position.X, 0.5F, 1e-6F);
    // It settles the 5 mm of daylight it started with and NOTHING else. That settle is the
    // mechanism that keeps a body on the ground it walks over, so it is not switched off on flat
    // ground -- it simply has nothing to do there.
    EXPECT_LT(step.drop, 0.01F);
    EXPECT_NEAR(step.position.Y, kStand, 1e-3F);
}

TEST(StepAssistTests, AStepThatWouldLandOnTheSIDEOfAKerbIsRefused)
{
    // A 0.15 m kerb only 0.30 m wide, and a step long enough to overshoot its far edge by 0.25 m.
    // The lift clears the kerb and the raised retry crosses it, so "did it get further?" says yes.
    // The settle then comes down 0.21 m and meets the kerb's top-east rounding at 56° from
    // vertical -- past §43.1's 46°, so it is the SIDE of the kerb and not the top of it. There is
    // nothing there to stand on, and the whole raised attempt is thrown away.
    //
    // Overshoot it by only 0.20 m instead and the same descent meets the same rounding at 42°,
    // inside the limit, and the body IS left resting against the kerb's corner -- which is what a
    // capsule balanced on an edge really does, and is a place it can stand.
    const CollisionWorld world = OneCell({
        Box(Vector3(-2.75F, -0.25F, 0.0F), Vector3(3.25F, 0.25F, 6.0F), CollisionKind::Floor),
        Box(Vector3(0.65F, 0.075F, 0.0F), Vector3(0.15F, 0.075F, 6.0F), CollisionKind::Floor),
    });
    BroadPhase broad;
    const StepAssist over =
        MoveWithStepAssist(world, world.cells[0], broad, Body(0.0F, 0.0F), Vector3(1.05F, 0.0F, 0.0F));
    EXPECT_FALSE(over.steppedUp);
    EXPECT_LE(over.position.Y, kStand + kRest + 1e-4F) << "it was left hanging beside the kerb";

    const StepAssist onto =
        MoveWithStepAssist(world, world.cells[0], broad, Body(0.0F, 0.0F), Vector3(1.00F, 0.0F, 0.0F));
    EXPECT_TRUE(onto.steppedUp);
    EXPECT_GT(onto.position.Y, kStand);
    EXPECT_LT(onto.position.Y, kStand + 0.15F) << "resting on the corner, not standing on the top";
}

// ---------------------------------------------------------------------------------------------
// Down
// ---------------------------------------------------------------------------------------------

TEST(StepAssistTests, WalkingOffATreadSettlesOntoWhatIsBelow)
{
    // The body starts on a 0.30 m platform and walks off the east edge of it. Without the
    // step-down it would be over nothing for a tick and start a fall -- on every tread of every
    // staircase in the house, which reads as a stumble and fires §43.1's landing sound seventeen
    // times on the way down.
    const CollisionWorld world = OneCell({
        Ground(0.0F, 3.0F, 6.0F),
        Box(Vector3(-2.0F, 0.15F, 0.0F), Vector3(2.0F, 0.15F, 6.0F), CollisionKind::Floor),
    });
    BroadPhase broad;
    const Capsule start = Body(-1.0F, 0.30F);
    const StepAssist step =
        MoveWithStepAssist(world, world.cells[0], broad, start, Vector3(1.5F, 0.0F, 0.0F));

    EXPECT_TRUE(step.steppedDown);
    // The 0.30 m platform plus the 5 mm the body was standing clear of it.
    EXPECT_NEAR(step.drop, 0.30F + kRest, 1e-3F);
    EXPECT_NEAR(step.position.Y, kStand, 1e-3F);
    EXPECT_NEAR(step.position.X, 0.5F, 1e-4F);
    EXPECT_FALSE(step.airborne);
}

TEST(StepAssistTests, ADropTallerThanTheAssistIsAFall)
{
    // 0.60 m down. There is nothing within §43.1's 0.45 m, so this is not a step -- the body is
    // left in the air where the move put it and `airborne` says so. §43.1's gravity is what
    // happens next, and `HOUSE-00552` owns it.
    const CollisionWorld world = OneCell({
        Ground(0.0F, 3.0F, 6.0F),
        Box(Vector3(-2.0F, 0.30F, 0.0F), Vector3(2.0F, 0.30F, 6.0F), CollisionKind::Floor),
    });
    BroadPhase broad;
    const Capsule start = Body(-1.0F, 0.60F);
    const StepAssist step =
        MoveWithStepAssist(world, world.cells[0], broad, start, Vector3(1.5F, 0.0F, 0.0F));

    EXPECT_FALSE(step.steppedDown);
    EXPECT_TRUE(step.airborne);
    EXPECT_NEAR(step.position.Y, start.centre.Y, 1e-6F) << "a fall does not start by teleporting";
}

TEST(StepAssistTests, ThereIsNothingToSettleOntoOnTheSideOfABank)
{
    // Over a 60° bank whose surface is 0.20 m below the body's feet -- well inside §43.1's
    // 0.45 m. Something IS down there and the sweep finds it; §43.1 says it is not a floor, so
    // there is nothing to settle ONTO. This is a fall like any other rather than a step onto a
    // slope the body could not have stood on.
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

    // The plane is `sin60·x + cos60·y = 0`, so putting the lower cap centre 0.40 from it leaves
    // 0.10 of perpendicular approach -- 0.20 m of vertical drop -- before the capsule touches.
    const float capY = (0.40F - std::sin(radians)) / std::cos(radians);
    const Capsule start{Vector3(1.0F, capY + kBodyHalfHeight, 0.0F), kBodyHalfHeight, kBodyRadius};
    const StepAssist step =
        MoveWithStepAssist(world, world.cells[0], broad, start, Vector3(0.05F, 0.0F, 0.0F));

    EXPECT_FALSE(step.steppedDown);
    EXPECT_TRUE(step.airborne);
    EXPECT_NEAR(step.position.Y, start.centre.Y, 1e-6F);
}

// ---------------------------------------------------------------------------------------------
// The real house
// ---------------------------------------------------------------------------------------------

TEST(StepAssistTests, WalkingTheRealHouseTickByTick)
{
    // Not one step but a WALK: a full second of §49.3's 1/120 s ticks at §43.2's 1.35 m/s --
    // 1.35 m, far enough to cross a room and reach what is at the far side of it -- in eight
    // directions from the middle of every cell, each tick fed the position the last one produced. That is the
    // only way the assist can be seen doing its job, because a lift-retry-settle happens INSIDE one tick and
    // a body needs several of them to arrive at a stair, climb a tread and arrive at the next.
    //
    // The bounds hold every tick of it: never more than 0.22 m up, never more than 0.45 m down,
    // and never left inside anything that four pushes cannot fix.
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

    const float tick = 1.35F / 120.0F;
    BroadPhase broad;
    std::size_t walked = 0;
    std::size_t ticks = 0;
    std::size_t ups = 0;
    std::size_t climbers = 0;
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
        const auto landing = SweepCell(*world, cell, broad, falling, Vector3(0.0F, -height, 0.0F));
        if (!landing.hit)
        {
            continue;
        }
        const float floorY = falling.centre.Y - height * landing.time - pebble;
        if (floorY + 2.0F * kStand > cell.bounds.Max.Y)
        {
            continue;
        }
        Capsule standing{Vector3(midX, floorY + kStand + kRest, midZ), kBodyHalfHeight, kBodyRadius};
        // A stair cell puts a body dropped down its middle inside a tread or two (`HOUSE-00547`).
        // Those are the cells with something to climb, so they are pushed out rather than skipped
        // -- with as many rounds of §49.3's four pushes as it takes, because getting a body to a
        // legal START is test setup and not the thing being measured.
        for (int round = 0; round < 8; ++round)
        {
            const Depenetration out = Depenetrate(*world, cell, broad, standing);
            standing.centre = Vector3(standing.centre.X + out.offset.X,
                                      standing.centre.Y + out.offset.Y,
                                      standing.centre.Z + out.offset.Z);
            if (out.resolved)
            {
                break;
            }
        }
        if (OverlapCell(*world, cell, broad, standing).overlapped)
        {
            continue;
        }
        ++walked;

        bool climbed = false;
        for (int i = 0; i < 8; ++i)
        {
            const float angle = static_cast<float>(i) * 6.2831853F / 8.0F;
            const Vector3 motion(std::cos(angle) * tick, 0.0F, std::sin(angle) * tick);
            Capsule body = standing;
            for (int t = 0; t < 120; ++t)
            {
                const StepAssist step = MoveWithStepAssist(*world, cell, broad, body, motion);
                ++ticks;
                if (step.steppedUp)
                {
                    ++ups;
                    climbed = true;
                }

                EXPECT_LE(step.rise, kStepUpHeight + 1e-4F) << cell.id;
                EXPECT_GE(step.rise, 0.0F) << cell.id;
                EXPECT_LE(step.drop, kStepDownHeight + 1e-4F) << cell.id;
                EXPECT_GE(step.drop, 0.0F) << cell.id;
                EXPECT_LE(step.position.Y, body.centre.Y + kStepUpHeight + 1e-4F) << cell.id;
                EXPECT_GE(step.position.Y, body.centre.Y - kStepDownHeight - 1e-4F) << cell.id;
                EXPECT_FALSE(step.steppedUp && step.airborne) << cell.id << ": climbed onto nothing";

                body.centre = step.position;
                const CellOverlap overlap = OverlapCell(*world, cell, broad, body);
                if (overlap.overlapped)
                {
                    const Depenetration out = Depenetrate(*world, cell, broad, body);
                    ASSERT_TRUE(out.resolved) << cell.id << ": tick " << t << " left the body "
                                              << overlap.depth << " m inside shape " << overlap.shape;
                    body.centre = Vector3(body.centre.X + out.offset.X,
                                          body.centre.Y + out.offset.Y,
                                          body.centre.Z + out.offset.Z);
                }
            }
        }
        climbers += climbed ? 1u : 0u;
    }
    ASSERT_GT(walked, 40u) << "only " << walked << " cells were walked";
    ASSERT_GT(ticks, 40000u) << "only " << ticks << " ticks were taken";
    // The house has stairs, kerbs and thresholds in it, and a second of walking reaches some.
    // A run in which the assist never fired would prove nothing about the assist.
    EXPECT_GT(ups, 0u) << "the step-up never fired anywhere in the house";
    EXPECT_GT(climbers, 0u) << "not one cell had anything to climb";
}
