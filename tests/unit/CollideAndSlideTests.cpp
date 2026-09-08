// SPDX-License-Identifier: MIT
//
// `HOUSE-00550`. §49.3 step 2 -- sweep, move to just short of what was hit, slide the rest of the
// step into the contact plane, three times -- plus §43.1's 46° slope limit, which that pseudocode
// does not mention and cannot do without: a projection has no opinion about which surfaces are
// floors, so sliding along the face of a 70° bank sends a body up it.
//
// The constructed half puts the contact where its answer can be written down: a wall met head-on,
// a wall met at 45°, an inside corner, a 30° ramp and a 60° one placed so the first contact is on
// a known face at a known distance. The other half is the real house, where a step in every
// direction from the middle of every cell has to end somewhere a body can be.
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
    using cnahouse::physics::CollideAndSlide;
    using cnahouse::physics::CollisionCell;
    using cnahouse::physics::CollisionKind;
    using cnahouse::physics::CollisionLoader;
    using cnahouse::physics::CollisionMesh;
    using cnahouse::physics::CollisionObb;
    using cnahouse::physics::CollisionWorld;
    using cnahouse::physics::Depenetrate;
    using cnahouse::physics::Depenetration;
    using cnahouse::physics::IsWalkable;
    using cnahouse::physics::kContactBackoff;
    using cnahouse::physics::kDepenetrationIterations;
    using cnahouse::physics::kDepenetrationStep;
    using cnahouse::physics::kSlideIterations;
    using cnahouse::physics::kSlopeLimitDegrees;
    using cnahouse::physics::OverlapCell;
    using cnahouse::physics::SlideResult;
    using cnahouse::physics::Sphere;
    using Microsoft::Xna::Framework::BoundingBox;
    using Microsoft::Xna::Framework::Vector3;

    /// §43.1's body: radius 0.30 m, standing 1.80 m, so a half-height of 0.60.
    constexpr float kBodyRadius = 0.30F;
    constexpr float kBodyHalfHeight = 0.60F;

    CollisionObb Wall(const Vector3& centre, const Vector3& halfExtents, float yaw = 0.0F)
    {
        CollisionObb obb;
        obb.centre = centre;
        obb.halfExtents = halfExtents;
        obb.yaw = yaw;
        obb.kind = CollisionKind::Wall;
        return obb;
    }

    /// A ramp through the origin rising along +x at @p degrees, 12 m long and 8 m wide, so that a
    /// body near the middle of it meets a FACE and never an edge. Its outward normal is
    /// `(-sin, cos, 0)` -- up and back down the slope.
    CollisionMesh Ramp(float degrees)
    {
        const float radians = degrees * 3.14159265F / 180.0F;
        const float rise = 6.0F * std::tan(radians);
        CollisionMesh mesh;
        mesh.vertices = {Vector3(-6.0F, -rise, -4.0F),
                         Vector3(-6.0F, -rise, 4.0F),
                         Vector3(6.0F, rise, 4.0F),
                         Vector3(6.0F, rise, -4.0F)};
        mesh.indices = {0u, 1u, 2u, 0u, 2u, 3u};
        mesh.surface = 0u;
        mesh.kind = CollisionKind::Floor;
        return mesh;
    }

    /// One side of a converging passage: a vertical quad 3 m tall running from x = @p nearX at
    /// z = -2 to x = @p farX at z = +2.
    CollisionMesh Vee(float nearX, float farX)
    {
        CollisionMesh mesh;
        mesh.vertices = {Vector3(nearX, 0.0F, -2.0F),
                         Vector3(nearX, 3.0F, -2.0F),
                         Vector3(farX, 3.0F, 2.0F),
                         Vector3(farX, 0.0F, 2.0F)};
        mesh.indices = {0u, 1u, 2u, 0u, 2u, 3u};
        mesh.surface = 0u;
        mesh.kind = CollisionKind::Wall;
        return mesh;
    }

    /// One 16 m cell over a 1 m grid, every shape in every bucket. Conservative, so no case here
    /// can pass or fail on which bucket a shape happened to land in.
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

    /// A body standing at @p x, @p z with its feet on y = 0.
    Capsule Body(float x, float z, float feet = 0.0F)
    {
        return Capsule{Vector3(x, feet + kBodyHalfHeight + kBodyRadius, z), kBodyHalfHeight, kBodyRadius};
    }

    float Length(const Vector3& v)
    {
        return std::sqrt(v.X * v.X + v.Y * v.Y + v.Z * v.Z);
    }

} // namespace

// ---------------------------------------------------------------------------------------------
// The slope limit itself
// ---------------------------------------------------------------------------------------------

TEST(CollideAndSlideTests, FortySixDegreesIsWhereAFloorStopsBeingOne)
{
    EXPECT_FLOAT_EQ(kSlopeLimitDegrees, 46.0F);
    EXPECT_EQ(kSlideIterations, 3);

    EXPECT_TRUE(IsWalkable(Vector3(0.0F, 1.0F, 0.0F))) << "a level floor";

    // A surface tilted by t has an outward normal tilted by t, so `normal.Y` IS the cosine of the
    // slope and the test needs no trigonometry per contact. Half a degree either side of 46:
    for (const float degrees : {0.0F, 30.0F, 45.0F, 45.5F})
    {
        const float radians = degrees * 3.14159265F / 180.0F;
        EXPECT_TRUE(IsWalkable(Vector3(-std::sin(radians), std::cos(radians), 0.0F)))
            << degrees << " degrees";
    }
    for (const float degrees : {46.5F, 60.0F, 89.0F, 90.0F})
    {
        const float radians = degrees * 3.14159265F / 180.0F;
        EXPECT_FALSE(IsWalkable(Vector3(-std::sin(radians), std::cos(radians), 0.0F)))
            << degrees << " degrees";
    }

    // A ceiling is not a floor however flat it is, and that falls out of the same comparison
    // rather than needing a rule of its own: its normal points down.
    EXPECT_FALSE(IsWalkable(Vector3(0.0F, -1.0F, 0.0F)));
}

// ---------------------------------------------------------------------------------------------
// Walls
// ---------------------------------------------------------------------------------------------

TEST(CollideAndSlideTests, AClearStepIsTakenWhole)
{
    const CollisionWorld world = OneCell({Wall(Vector3(4.0F, 1.0F, 0.0F), Vector3(0.1F, 1.5F, 4.0F))});
    BroadPhase broad;
    const SlideResult step =
        CollideAndSlide(world, world.cells[0], broad, Body(0.0F, 0.0F), Vector3(0.5F, 0.0F, 0.0F));
    EXPECT_NEAR(step.position.X, 0.5F, 1e-6F);
    EXPECT_EQ(step.iterations, 1);
    EXPECT_EQ(step.contacts, 0);
    EXPECT_FALSE(step.blocked);
    EXPECT_NEAR(Length(step.motion), 0.0F, 1e-6F);
}

TEST(CollideAndSlideTests, AStepTheSizeOfARealTickIsTakenAndNotRoundedAway)
{
    // §43.2's normal walk is 1.35 m/s and §49.3's step is 1/120 s, so a real frame asks this
    // function to move a body ELEVEN MILLIMETRES. Every case in this file that uses a half-metre
    // step is a hundred times that, and a "motion this small is nothing" threshold set anywhere
    // above a tick's worth would freeze the player while passing all of them.
    const float tick = 1.35F / 120.0F;
    const CollisionWorld world = OneCell({Wall(Vector3(1.1F, 1.0F, 0.0F), Vector3(0.1F, 1.5F, 6.0F))});
    BroadPhase broad;

    const SlideResult open =
        CollideAndSlide(world, world.cells[0], broad, Body(0.0F, 0.0F), Vector3(tick, 0.0F, 0.0F));
    EXPECT_NEAR(open.position.X, tick, 1e-7F);
    EXPECT_EQ(open.contacts, 0);

    // And a tick-sized step ALONG a wall it is a hair away from still slides its whole length,
    // which is what walking down a corridor is made of.
    const SlideResult grazing =
        CollideAndSlide(world, world.cells[0], broad, Body(0.699F, 0.0F), Vector3(tick, 0.0F, tick));
    EXPECT_NEAR(grazing.position.Z, tick, 1e-5F);
    EXPECT_LT(grazing.position.X, 0.70F);
}

TEST(CollideAndSlideTests, AWallMetHeadOnStopsTheBodyAThousandthShort)
{
    // The wall's west face is at x = 1.0 and the body's radius is 0.30, so the centre stops at
    // 0.70 of the 1 m step -- less §49.3's 0.999, which leaves the body a millimetre of daylight
    // rather than resting exactly on the surface. That gap is the whole reason for the factor: a
    // body that ends the step ON a wall starts the next one INSIDE it, and cannot then walk along
    // it.
    const CollisionWorld world = OneCell({Wall(Vector3(1.1F, 1.0F, 0.0F), Vector3(0.1F, 1.5F, 4.0F))});
    BroadPhase broad;
    const SlideResult step =
        CollideAndSlide(world, world.cells[0], broad, Body(0.0F, 0.0F), Vector3(1.0F, 0.0F, 0.0F));

    EXPECT_NEAR(step.position.X, 0.70F * kContactBackoff, 1e-5F);
    EXPECT_LT(step.position.X, 0.70F) << "the body must not end the step touching the wall";
    EXPECT_EQ(step.contacts, 1);
    EXPECT_FALSE(step.lastWalkable) << "a vertical wall is not a floor";
    EXPECT_EQ(step.steepContacts, 1);

    // Head-on, the whole step goes INTO the wall, so the slide leaves nothing of it. That is not a
    // blocked body -- there was no motion the iterations failed to spend.
    EXPECT_NEAR(Length(step.motion), 0.0F, 1e-6F);
    EXPECT_FALSE(step.blocked);
    // One sweep. The second pass of the loop finds nothing left to move and stops before
    // spending a sweep on it, which is why `iterations` counts sweeps and not passes.
    EXPECT_EQ(step.iterations, 1);
}

TEST(CollideAndSlideTests, AWallMetAtAnAngleKeepsTheWholeOfTheAlongwardStep)
{
    // The point of sliding. A body walking north-east into a north-south wall does not stop dead
    // and does not lose the northward half of its step; it ends up as far north as it would have
    // got in open ground.
    const CollisionWorld world = OneCell({Wall(Vector3(1.1F, 1.0F, 0.0F), Vector3(0.1F, 1.5F, 6.0F))});
    BroadPhase broad;
    const SlideResult step =
        CollideAndSlide(world, world.cells[0], broad, Body(0.0F, 0.0F), Vector3(1.0F, 0.0F, 1.0F));

    EXPECT_NEAR(step.position.X, 0.70F * kContactBackoff, 1e-5F);
    EXPECT_NEAR(step.position.Z, 1.0F, 1e-4F) << "the alongward metre is not paid for by the wall";
    EXPECT_EQ(step.contacts, 1);
    EXPECT_FALSE(step.blocked);
}

TEST(CollideAndSlideTests, AnInsideCornerStopsInsideBothWalls)
{
    // Two walls at right angles. One slide is not enough -- the first takes the body along the
    // west wall and straight into the north one -- which is why §49.3 asks for three.
    const CollisionWorld world = OneCell({
        Wall(Vector3(1.1F, 1.0F, 0.0F), Vector3(0.1F, 1.5F, 6.0F)),
        Wall(Vector3(0.0F, 1.0F, 1.1F), Vector3(6.0F, 1.5F, 0.1F)),
    });
    BroadPhase broad;
    const SlideResult step =
        CollideAndSlide(world, world.cells[0], broad, Body(0.0F, 0.0F), Vector3(1.0F, 0.0F, 1.0F));

    EXPECT_EQ(step.contacts, 2);
    EXPECT_LT(step.position.X, 0.70F);
    EXPECT_LT(step.position.Z, 0.70F);
    EXPECT_GT(step.position.X, 0.69F);
    EXPECT_GT(step.position.Z, 0.69F);
    EXPECT_FALSE(step.blocked);

    // ...and the body is not inside either of them, which is the claim the two numbers above are
    // only evidence for.
    Capsule ended = Body(0.0F, 0.0F);
    ended.centre = step.position;
    EXPECT_FALSE(OverlapCell(world, world.cells[0], broad, ended).overlapped);
}

TEST(CollideAndSlideTests, ThreeIterationsAreAllThereAreAndRunningOutIsReported)
{
    // A gap that narrows from 0.72 m to 0.50 m over four metres, walked straight down the middle
    // by a 0.60 m body. It fits at the near end and does not at the far one, so every slide sends
    // it into the opposite side, and the loop is a bounded three -- not "until it stops moving",
    // which in a wedge is never.
    //
    // What is LEFT of the step is neither silently spent nor silently dropped: it comes back in
    // `motion` with `blocked` set, and §49.3's later steps decide what to do about it.
    const CollisionWorld world = OneCell({}, {Vee(-0.36F, -0.25F), Vee(0.36F, 0.25F)});
    BroadPhase broad;
    const SlideResult step =
        CollideAndSlide(world, world.cells[0], broad, Body(0.0F, -2.0F), Vector3(0.0F, 0.0F, 2.7F));

    EXPECT_EQ(step.iterations, kSlideIterations);
    EXPECT_EQ(step.contacts, kSlideIterations);
    EXPECT_TRUE(step.blocked);
    EXPECT_GT(Length(step.motion), 0.0F);

    // Blocked is not stuck-in-a-wall. Whatever the loop gave up on, the body it gave up on is
    // still somewhere §49.3's step 5 can finish.
    Capsule ended = Body(0.0F, -2.0F);
    ended.centre = step.position;
    const CellOverlap overlap = OverlapCell(world, world.cells[0], broad, ended);
    if (overlap.overlapped)
    {
        EXPECT_TRUE(Depenetrate(world, world.cells[0], broad, ended).resolved);
    }
}

TEST(CollideAndSlideTests, ABodyThatStartsInsideStandsStillAndIsPutBackByStepFive)
{
    // 0.05 m into the wall already -- which this step has no business making worse, and no way to
    // make better. `SweepCell` answers with `time` 0 and `startedInside`, and "already touching"
    // is not "hit immediately after moving": there is no fraction of the step to walk before the
    // contact, so the body does not move at all. It spends its three iterations finding that out,
    // reports `blocked` with the step unspent, and §49.3's step 5 is what gets it out.
    //
    // The kind-looking alternative -- let a start overlap the motion is moving OUT of pass, so
    // that a body resting on a floor is not stopped by the floor -- walked a body 0.97 m into the
    // attic stair ramp when it was tried. A triangle's prism is 1.2 m thick for §43.1's body;
    // "inside" it is a volume, not a surface, and the way out of a volume is not a normal a motion
    // can be compared against.
    const CollisionWorld world = OneCell({Wall(Vector3(1.1F, 1.0F, 0.0F), Vector3(0.1F, 1.5F, 4.0F))});
    BroadPhase broad;
    const Capsule start = Body(0.75F, 0.0F); // face at 1.00, radius 0.30: 0.05 m in
    ASSERT_TRUE(OverlapCell(world, world.cells[0], broad, start).overlapped);

    const SlideResult step = CollideAndSlide(world, world.cells[0], broad, start, Vector3(1.0F, 0.0F, 1.0F));
    EXPECT_NEAR(step.position.X, start.centre.X, 1e-6F) << "an overlap must not be deepened";
    EXPECT_NEAR(step.position.Z, start.centre.Z, 1e-6F);
    // One iteration, not three: a body inside something learns that on the first sweep and the
    // other two would ask the same question and get the same answer (`HOUSE-00615`). What matters
    // is unchanged -- it did not move, and it says it was blocked.
    EXPECT_EQ(step.iterations, 1);
    EXPECT_TRUE(step.blocked);

    // And the 0.05 m it started with is still 0.05 m, which four pushes of 0.02 m undo. Standing
    // still for one 1/120 s tick is what it costs to be certain a body never crosses a wall.
    Capsule ended = start;
    ended.centre = step.position;
    const Depenetration out = Depenetrate(world, world.cells[0], broad, ended);
    EXPECT_TRUE(out.resolved);
    EXPECT_NEAR(out.deepest, 0.05F, 1e-4F);
}

// ---------------------------------------------------------------------------------------------
// Slopes
// ---------------------------------------------------------------------------------------------

TEST(CollideAndSlideTests, AThirtyDegreeRampIsClimbed)
{
    // The body is placed 0.80 m off the ramp's rounded surface and walks 2 m into it. It meets the
    // face after 1.00 m -- (0.80 - 0.30) / sin 30 -- and what is left of the step is turned along
    // the slope, keeping its climb, because 30° is inside §43.1's 46°.
    const float radians = 30.0F * 3.14159265F / 180.0F;
    const CollisionWorld world = OneCell({}, {Ramp(30.0F)});
    BroadPhase broad;
    const Capsule start{
        Vector3(0.0F, kBodyHalfHeight + 0.80F / std::cos(radians), 0.0F), kBodyHalfHeight, kBodyRadius};
    const SlideResult step = CollideAndSlide(world, world.cells[0], broad, start, Vector3(2.0F, 0.0F, 0.0F));

    ASSERT_EQ(step.contacts, 1);
    EXPECT_TRUE(step.lastWalkable);
    EXPECT_EQ(step.steepContacts, 0);
    EXPECT_NEAR(step.lastNormal.Y, std::cos(radians), 1e-3F);
    EXPECT_GT(step.position.Y - start.centre.Y, 0.40F) << "a walkable slope is walked UP";
    EXPECT_NEAR(step.position.Y - start.centre.Y, 0.4334F, 2e-3F);
}

TEST(CollideAndSlideTests, ASixtyDegreeBankIsNotAStaircase)
{
    // The same arrangement at 60°. The projection alone would give this body 0.62 m of climb --
    // more than the 30° ramp, because a steeper plane turns more of the step upward -- which is
    // exactly backwards. §43.1's slope limit takes the climb out and leaves the body pressed
    // against the foot of the bank.
    const CollisionWorld world = OneCell({}, {Ramp(60.0F)});
    BroadPhase broad;
    const float radians = 60.0F * 3.14159265F / 180.0F;
    const Capsule start{
        Vector3(0.0F, kBodyHalfHeight + 0.80F / std::cos(radians), 0.0F), kBodyHalfHeight, kBodyRadius};
    const SlideResult step = CollideAndSlide(world, world.cells[0], broad, start, Vector3(2.0F, 0.0F, 0.0F));

    ASSERT_GE(step.contacts, 1);
    EXPECT_FALSE(step.lastWalkable);
    EXPECT_GE(step.steepContacts, 1);
    EXPECT_NEAR(step.position.Y, start.centre.Y, 1e-4F) << "a 60 degree bank was climbed";
    EXPECT_GT(step.position.X, start.centre.X) << "...but the body still gets to its foot";
}

// ---------------------------------------------------------------------------------------------
// The real house
// ---------------------------------------------------------------------------------------------

TEST(CollideAndSlideTests, EveryStepInTheRealHouseEndsSomewhereABodyCanBe)
{
    // §49.5's promise again, from the other end: a step that ends inside static geometry is the
    // thing this loop exists to prevent. A 0.5 m step -- forty of §49.3's 1/120 s ticks at a fast
    // walk, so far more than one frame ever asks for -- in eight directions from the middle of
    // every cell of the real house.
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
    std::size_t stepped = 0;
    std::size_t touched = 0;
    std::size_t blocked = 0;
    std::size_t landedInside = 0;
    for (const auto& cell : world->cells)
    {
        if (cell.shapes.empty() || cell.nx == 0u || cell.nz == 0u)
        {
            continue;
        }
        const float midX = cell.originX + static_cast<float>(cell.nx) * 0.5F;
        const float midZ = cell.originZ + static_cast<float>(cell.nz) * 0.5F;
        const float height = cell.bounds.Max.Y - cell.bounds.Min.Y;
        const float stand = kBodyHalfHeight + kBodyRadius;
        if (height < 2.0F * stand)
        {
            continue;
        }
        // The floor is found by dropping, not assumed from `bounds.Min.Y`, which is the bottom of
        // the floor SLAB -- 0.35 m lower in the basement (`HOUSE-00547`).
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
        if (floorY + 2.0F * stand > cell.bounds.Max.Y)
        {
            continue;
        }
        Capsule body{Vector3(midX, floorY + stand + 0.005F, midZ), kBodyHalfHeight, kBodyRadius};
        if (OverlapCell(*world, cell, broad, body).overlapped)
        {
            continue; // a stair: `HOUSE-00547` counts these and §49.3's StepUp is what handles them
        }

        for (int i = 0; i < 8; ++i)
        {
            const float angle = static_cast<float>(i) * 6.2831853F / 8.0F;
            const Vector3 motion(std::cos(angle) * 0.5F, 0.0F, std::sin(angle) * 0.5F);
            const SlideResult step = CollideAndSlide(*world, cell, broad, body, motion);
            ++stepped;
            touched += step.contacts > 0 ? 1u : 0u;
            blocked += step.blocked ? 1u : 0u;

            EXPECT_LE(step.iterations, kSlideIterations) << cell.id;
            // A slide can only ever shorten a step. A body that comes out of one having gone
            // FURTHER than it asked to is being flung by its own collision response.
            const Vector3 moved(step.position.X - body.centre.X,
                                step.position.Y - body.centre.Y,
                                step.position.Z - body.centre.Z);
            EXPECT_LE(Length(moved), 0.5F + 1e-4F) << cell.id << ": the step grew";

            Capsule ended = body;
            ended.centre = step.position;
            const CellOverlap overlap = OverlapCell(*world, cell, broad, ended);
            if (!overlap.overlapped)
            {
                continue;
            }
            // If it did end up touching something, §49.3's step 5 has to be able to fix it inside
            // its four pushes. A body the slide leaves 0.08 m inside a wall is a body that would
            // stay there.
            ++landedInside;
            const Depenetration out = Depenetrate(*world, cell, broad, ended);
            EXPECT_TRUE(out.resolved)
                << cell.id << ": a slid step left the body " << overlap.depth << " m inside shape "
                << overlap.shape << ", which four pushes could not undo";
            EXPECT_LE(Length(out.offset),
                      static_cast<float>(kDepenetrationIterations) * kDepenetrationStep + 1e-4F)
                << cell.id;
        }
    }
    ASSERT_GT(stepped, 400u) << "only " << stepped << " steps were taken";
    // A 0.5 m step from the middle of a room mostly meets nothing; a house where every step hit
    // something would mean the sweep is finding geometry that is not there.
    EXPECT_LT(touched, stepped / 2) << touched << " of " << stepped << " steps hit something";
    EXPECT_GT(touched, 0u) << "not one step in the whole house met a wall";
    EXPECT_LT(blocked, stepped / 20) << blocked << " of " << stepped << " steps ran out of slides";
}
