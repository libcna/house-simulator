// SPDX-License-Identifier: MIT
//
// `HOUSE-00543`, and the task asks for forty analytic cases. Analytic means the expected time and
// normal are worked out on paper from the numbers in the case, not read off a run of the code —
// a table of "what it currently does" is a change detector, not a test, and it passes just as
// happily when the sweep is wrong.
//
// **Forty-six of them across twenty-three tests**: six faces entered square on, sixteen directions
// round the box, three for the segment, two each for the fraction and the zero motion, and one
// apiece for the rest. Two of the first draft's cases were wrong and the code was right — a cube
// turned 45 degrees presents an edge to +x and not a face, and a 1.4 m/s walk covers 11.67 mm in a
// 1/120 s step and cannot cross a 20 mm gap. Both are recorded where they failed, because a case
// that had to be corrected is worth more than one that passed first time.
//
// The geometry under all of it: a capsule against a box is a point against their Minkowski sum,
// and for an upright capsule and a yawed box that sum is a box of half-extents
// `(ex, ey + halfHeight, ez)` rounded by `radius`. So every expected number below is a distance to
// that rounded box divided by the length of the motion.
#include <cmath>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "cnahouse/physics/Sweep.hpp"

namespace
{
    using cnahouse::physics::Capsule;
    using cnahouse::physics::CollisionKind;
    using cnahouse::physics::CollisionObb;
    using cnahouse::physics::SweepHit;
    using Microsoft::Xna::Framework::Vector3;

    /// A 2 x 2 x 2 box at the origin unless a case says otherwise.
    CollisionObb UnitBox(float yaw = 0.0f)
    {
        CollisionObb obb;
        obb.centre = Vector3(0.0f, 0.0f, 0.0f);
        obb.halfExtents = Vector3(1.0f, 1.0f, 1.0f);
        obb.yaw = yaw;
        obb.kind = CollisionKind::Wall;
        return obb;
    }

    /// A sphere of radius r: the capsule with no segment, which is the easiest case to reason about.
    Capsule Sphere(float x, float y, float z, float r)
    {
        Capsule capsule;
        capsule.centre = Vector3(x, y, z);
        capsule.halfHeight = 0.0f;
        capsule.radius = r;
        return capsule;
    }

    Capsule Body(float x, float y, float z, float halfHeight, float r)
    {
        Capsule capsule;
        capsule.centre = Vector3(x, y, z);
        capsule.halfHeight = halfHeight;
        capsule.radius = r;
        return capsule;
    }

    void ExpectNormal(const SweepHit& hit, float x, float y, float z, const char* what)
    {
        ASSERT_TRUE(hit.hit) << what;
        EXPECT_NEAR(hit.normal.X, x, 1e-4f) << what;
        EXPECT_NEAR(hit.normal.Y, y, 1e-4f) << what;
        EXPECT_NEAR(hit.normal.Z, z, 1e-4f) << what;
        const float length = std::sqrt(hit.normal.X * hit.normal.X + hit.normal.Y * hit.normal.Y +
                                       hit.normal.Z * hit.normal.Z);
        EXPECT_NEAR(length, 1.0f, 1e-4f) << what << ": the normal is not a unit vector";
    }

} // namespace

// ---- faces: the six of them, each entered straight on -------------------------------------------

TEST(SweepTests, CapsuleObbHitsEachOfTheSixFacesSquareOn)
{
    // A 0.25 sphere starting 2 m out along each axis and moving 2 m in. The rounded box's face is
    // at 1 + 0.25 = 1.25, so it travels 0.75 of 2 m = 0.375.
    const CollisionObb box = UnitBox();

    const struct
    {
        Vector3 from;
        Vector3 motion;
        Vector3 normal;
        const char* name;
    } cases[] = {
        {Vector3(2.0f, 0.0f, 0.0f), Vector3(-2.0f, 0.0f, 0.0f), Vector3(1.0f, 0.0f, 0.0f), "+x"},
        {Vector3(-2.0f, 0.0f, 0.0f), Vector3(2.0f, 0.0f, 0.0f), Vector3(-1.0f, 0.0f, 0.0f), "-x"},
        {Vector3(0.0f, 2.0f, 0.0f), Vector3(0.0f, -2.0f, 0.0f), Vector3(0.0f, 1.0f, 0.0f), "+y"},
        {Vector3(0.0f, -2.0f, 0.0f), Vector3(0.0f, 2.0f, 0.0f), Vector3(0.0f, -1.0f, 0.0f), "-y"},
        {Vector3(0.0f, 0.0f, 2.0f), Vector3(0.0f, 0.0f, -2.0f), Vector3(0.0f, 0.0f, 1.0f), "+z"},
        {Vector3(0.0f, 0.0f, -2.0f), Vector3(0.0f, 0.0f, 2.0f), Vector3(0.0f, 0.0f, -1.0f), "-z"},
    };

    for (const auto& one : cases)
    {
        const SweepHit hit =
            SweepCapsuleObb(Sphere(one.from.X, one.from.Y, one.from.Z, 0.25f), one.motion, box);
        ASSERT_TRUE(hit.hit) << one.name;
        EXPECT_NEAR(hit.time, 0.375f, 1e-4f) << one.name;
        EXPECT_FALSE(hit.startedInside) << one.name;
        ExpectNormal(hit, one.normal.X, one.normal.Y, one.normal.Z, one.name);
    }
}

TEST(SweepTests, CapsuleObbMissesWhenTheMotionStopsShort)
{
    // 2 m out, 0.25 radius, box face at 1: the gap is 0.75. A motion of 0.7 cannot cross it.
    const SweepHit hit =
        SweepCapsuleObb(Sphere(2.0f, 0.0f, 0.0f, 0.25f), Vector3(-0.7f, 0.0f, 0.0f), UnitBox());
    EXPECT_FALSE(hit.hit);
    EXPECT_FLOAT_EQ(hit.time, 1.0f) << "a miss must leave the whole motion available";
}

TEST(SweepTests, CapsuleObbHitsExactlyAtTheEndOfTheMotion)
{
    // The same gap of 0.75 with a motion of exactly 0.75: it touches at t = 1.
    const SweepHit hit =
        SweepCapsuleObb(Sphere(2.0f, 0.0f, 0.0f, 0.25f), Vector3(-0.75f, 0.0f, 0.0f), UnitBox());
    ASSERT_TRUE(hit.hit);
    EXPECT_NEAR(hit.time, 1.0f, 1e-4f);
}

TEST(SweepTests, CapsuleObbMissesWhenItPassesBeside)
{
    // Travelling along -x at z = 2, which is 1 + 0.25 = 1.25 clear of the rounded box: no contact
    // anywhere along the path.
    const SweepHit hit =
        SweepCapsuleObb(Sphere(3.0f, 0.0f, 2.0f, 0.25f), Vector3(-6.0f, 0.0f, 0.0f), UnitBox());
    EXPECT_FALSE(hit.hit);
}

TEST(SweepTests, CapsuleObbMissesWhenItMovesAway)
{
    const SweepHit hit =
        SweepCapsuleObb(Sphere(2.0f, 0.0f, 0.0f, 0.25f), Vector3(3.0f, 0.0f, 0.0f), UnitBox());
    EXPECT_FALSE(hit.hit);
}

// ---- the segment: a capsule is taller than a sphere ---------------------------------------------

TEST(SweepTests, TheSegmentExtendsTheBoxInYAndNowhereElse)
{
    // A capsule of half-height 0.9 and radius 0.25 centred at y = 2.0 clears a box of half-extent
    // 1 by 2.0 - (1 + 0.9 + 0.25) = -0.15: it is already touching. Raise it to y = 2.2 and the gap
    // is 0.05, crossed in 0.05 / 1.0 of a 1 m descent.
    const CollisionObb box = UnitBox();
    const SweepHit touching =
        SweepCapsuleObb(Body(0.0f, 2.0f, 0.0f, 0.9f, 0.25f), Vector3(0.0f, -1.0f, 0.0f), box);
    EXPECT_TRUE(touching.startedInside) << "the segment did not lengthen the shape in y";

    const SweepHit clear =
        SweepCapsuleObb(Body(0.0f, 2.2f, 0.0f, 0.9f, 0.25f), Vector3(0.0f, -1.0f, 0.0f), box);
    ASSERT_TRUE(clear.hit);
    EXPECT_NEAR(clear.time, 0.05f, 1e-4f);
    ExpectNormal(clear, 0.0f, 1.0f, 0.0f, "landing on the top face");

    // ...and sideways the segment changes nothing: the same 0.375 the sphere got.
    const SweepHit sideways =
        SweepCapsuleObb(Body(2.0f, 0.0f, 0.0f, 0.9f, 0.25f), Vector3(-2.0f, 0.0f, 0.0f), box);
    ASSERT_TRUE(sideways.hit);
    EXPECT_NEAR(sideways.time, 0.375f, 1e-4f) << "the half-height leaked into the horizontal extent";
}

TEST(SweepTests, ATallCapsuleClearsAWallItsMiddleWouldNotHave)
{
    // A wall 0.2 thick and 0.5 tall at y = -1.5. A capsule whose bottom is at -0.35 passes over it.
    CollisionObb kerb;
    kerb.centre = Vector3(0.0f, -1.5f, 0.0f);
    kerb.halfExtents = Vector3(0.1f, 0.25f, 5.0f);
    const Capsule body = Body(2.0f, 0.6f, 0.0f, 0.6f, 0.25f);
    EXPECT_FLOAT_EQ(body.Bottom(), -0.25f);
    const SweepHit hit = SweepCapsuleObb(body, Vector3(-4.0f, 0.0f, 0.0f), kerb);
    EXPECT_FALSE(hit.hit) << "a capsule 0.25 m above a kerb was stopped by it";
}

// ---- starting inside ----------------------------------------------------------------------------

TEST(SweepTests, StartingInsideIsReportedAndNotConfusedWithHittingImmediately)
{
    const SweepHit hit =
        SweepCapsuleObb(Sphere(0.5f, 0.0f, 0.0f, 0.25f), Vector3(1.0f, 0.0f, 0.0f), UnitBox());
    ASSERT_TRUE(hit.hit);
    EXPECT_TRUE(hit.startedInside);
    EXPECT_FLOAT_EQ(hit.time, 0.0f);
    // Deep inside, the way out is the nearest face -- +x, 0.5 away, against 1.0 for the others.
    ExpectNormal(hit, 1.0f, 0.0f, 0.0f, "depenetration direction");
}

TEST(SweepTests, TouchingExactlyCountsAsInside)
{
    // Centre 1.25 out with radius 0.25 against a face at 1: touching to the last bit, and moving
    // INTO the face.
    const SweepHit hit =
        SweepCapsuleObb(Sphere(1.25f, 0.0f, 0.0f, 0.25f), Vector3(-1.0f, 0.0f, 0.0f), UnitBox());
    ASSERT_TRUE(hit.hit);
    EXPECT_TRUE(hit.startedInside);
    EXPECT_TRUE(hit.touching) << "a contact of zero depth was reported as penetration";
    ExpectNormal(hit, 1.0f, 0.0f, 0.0f, "touching the +x face");
}

TEST(SweepTests, ABodyRestingOnAFaceCanStillTravelAlongIt)
{
    // The same body, moving ALONG the face rather than into it. `HOUSE-00615`: answering "already
    // touching, time 0" here freezes a body that has come to rest exactly on something -- and a
    // depenetration ends AT contact, so bodies do come to rest exactly on things. The slide then
    // travels nothing, projects the same motion three times and calls itself blocked, and no
    // step-five push will ever move the body, because it is not inside anything.
    const SweepHit along =
        SweepCapsuleObb(Sphere(1.25f, 0.0f, 0.0f, 0.25f), Vector3(0.0f, 0.0f, 2.0f), UnitBox());
    EXPECT_FALSE(along.startedInside) << "a body resting on a wall could not walk along it";
    EXPECT_FALSE(along.hit) << "sliding along the face of a box is not a collision with it";

    // ...and away from it, which is the easy case and would be absurd to stop.
    const SweepHit away =
        SweepCapsuleObb(Sphere(1.25f, 0.0f, 0.0f, 0.25f), Vector3(1.0f, 0.0f, 0.0f), UnitBox());
    EXPECT_FALSE(away.hit);
}

TEST(SweepTests, ABodyINSIDEOneStandsStillWhicheverWayItIsPushed)
{
    // The other half of the same rule, and the reason it is a tolerance and not a sign test: the
    // way out of a VOLUME is not a surface normal. A body 0.5 m inside the box is reported as
    // started-inside for every direction, including one that would leave along a face, because
    // §49.3's step 5 is what gets it out -- and a sweep that let it travel walked a body 0.97 m
    // into the attic stair ramp (`HOUSE-00550`).
    for (const Vector3& motion :
         {Vector3(0.0f, 0.0f, 2.0f), Vector3(1.0f, 0.0f, 0.0f), Vector3(-1.0f, 0.0f, 0.0f)})
    {
        const SweepHit hit = SweepCapsuleObb(Sphere(0.9f, 0.0f, 0.0f, 0.25f), motion, UnitBox());
        EXPECT_TRUE(hit.hit);
        EXPECT_TRUE(hit.startedInside);
        EXPECT_FALSE(hit.touching) << "0.35 m of penetration was called a touch";
        EXPECT_FLOAT_EQ(hit.time, 0.0f);
    }
}

TEST(SweepTests, AZeroMotionAsksWhetherTheyOverlapNow)
{
    const SweepHit inside =
        SweepCapsuleObb(Sphere(0.5f, 0.0f, 0.0f, 0.25f), Vector3(0.0f, 0.0f, 0.0f), UnitBox());
    EXPECT_TRUE(inside.hit);
    EXPECT_TRUE(inside.startedInside);
    const SweepHit outside =
        SweepCapsuleObb(Sphere(3.0f, 0.0f, 0.0f, 0.25f), Vector3(0.0f, 0.0f, 0.0f), UnitBox());
    EXPECT_FALSE(outside.hit);
}

// ---- the rounded edges and corners, which are the point ------------------------------------------

TEST(SweepTests, AnEdgeIsRoundedAndNotSquare)
{
    // Straight at the +x/+z edge along the diagonal. A SQUARE corner would stop the centre at
    // (1.25, 1.25); the round one stops it where the centre is 0.25 from the edge line at (1,1),
    // i.e. at 1 + 0.25/√2 = 1.1768 on each axis.
    //
    // Start at (3, 0, 3) moving (-4, 0, -4). The centre reaches 1.1768 after 3 - 1.1768 = 1.8232
    // on each axis, of 4: t = 0.45581.
    const SweepHit hit =
        SweepCapsuleObb(Sphere(3.0f, 0.0f, 3.0f, 0.25f), Vector3(-4.0f, 0.0f, -4.0f), UnitBox());
    ASSERT_TRUE(hit.hit);
    const float expected = (3.0f - (1.0f + 0.25f / std::sqrt(2.0f))) / 4.0f;
    EXPECT_NEAR(hit.time, expected, 1e-4f)
        << "the edge behaved as a square corner, which stops the body short of the geometry";
    const float diagonal = 1.0f / std::sqrt(2.0f);
    ExpectNormal(hit, diagonal, 0.0f, diagonal, "the edge normal is the diagonal");
}

TEST(SweepTests, ACornerIsASphereAndNotACube)
{
    // Straight at the +x/+y/+z corner. The centre stops 0.25 from the corner point (1,1,1) along
    // the body diagonal: 1 + 0.25/√3 = 1.14434 on each axis. From 3 that is 1.85566 of 4.
    const SweepHit hit =
        SweepCapsuleObb(Sphere(3.0f, 3.0f, 3.0f, 0.25f), Vector3(-4.0f, -4.0f, -4.0f), UnitBox());
    ASSERT_TRUE(hit.hit);
    const float expected = (3.0f - (1.0f + 0.25f / std::sqrt(3.0f))) / 4.0f;
    EXPECT_NEAR(hit.time, expected, 1e-4f);
    const float diagonal = 1.0f / std::sqrt(3.0f);
    ExpectNormal(hit, diagonal, diagonal, diagonal, "the corner normal is the body diagonal");
}

TEST(SweepTests, AGrazeThatMissesTheRoundedEdgeIsAMiss)
{
    // Along -x at z = 1.30, which is 0.30 past the box's z face: the rounded edge reaches only
    // 0.25. A square corner WOULD hit this, so it is the case that tells the two apart.
    const SweepHit hit =
        SweepCapsuleObb(Sphere(3.0f, 0.0f, 1.30f, 0.25f), Vector3(-6.0f, 0.0f, 0.0f), UnitBox());
    EXPECT_FALSE(hit.hit) << "a square corner caught a body that passes 50 mm clear of the round one";
}

TEST(SweepTests, AGrazeThatCatchesTheRoundedEdgeIsAHit)
{
    // The same path at z = 1.10: within the 0.25 rounding, so it clips the edge.
    const SweepHit hit =
        SweepCapsuleObb(Sphere(3.0f, 0.0f, 1.10f, 0.25f), Vector3(-6.0f, 0.0f, 0.0f), UnitBox());
    ASSERT_TRUE(hit.hit);
    // The centre must end 0.25 from the edge line x=1,z=1: dz = 0.10, so dx = √(0.0625-0.01)
    // = 0.22913, and the centre stops at x = 1.22913 after 3 - 1.22913 = 1.77087 of 6.
    const float dx = std::sqrt(0.25f * 0.25f - 0.10f * 0.10f);
    EXPECT_NEAR(hit.time, (3.0f - (1.0f + dx)) / 6.0f, 1e-4f);
    // The normal points from the edge line to the contact: (dx, 0, 0.10) normalised.
    const float length = std::sqrt(dx * dx + 0.01f);
    ExpectNormal(hit, dx / length, 0.0f, 0.10f / length, "the grazing normal");
}

TEST(SweepTests, AStartInTheCornerRegionIsNotAMiss)
{
    // The gap between the rounded box and its axis-aligned bounding box is real space, up to
    // `radius(√3 − 1)` deep at a corner, and a body standing diagonally off a wall corner is in
    // it. The slab test finds no entry there -- the centre is already inside every slab, so it
    // crosses no plane on the way in -- and reading that as "no entry, therefore no hit" walked a
    // body through the corner. `HOUSE-00551` found it, by asking a body standing on a floor
    // whether there was a floor under it.
    //
    // A 0.25 sphere at (1.20, 1.20, 0) against the unit box: 0.283 from the box's corner edge, so
    // outside the rounding by 0.033 -- and inside the outer box, whose faces are at 1.25.
    const float start = 1.20f;
    const float diagonal = std::sqrt(2.0f) * 0.20f;
    ASSERT_GT(diagonal, 0.25f) << "the sphere must start CLEAR of the rounded edge";
    ASSERT_LT(start, 1.0f + 0.25f) << "...and INSIDE the outer box, which is the whole point";

    const SweepHit hit =
        SweepCapsuleObb(Sphere(start, start, 0.0f, 0.25f), Vector3(-0.4f, -0.4f, 0.0f), UnitBox());
    ASSERT_TRUE(hit.hit) << "a body diagonally off a corner walked straight through it";
    // It meets the vertical edge line x = 1, y = 1 when its centre is 0.25 away, so it travels
    // 0.283 - 0.25 = 0.033 of the 0.566 diagonal it asked for.
    EXPECT_NEAR(hit.time, (diagonal - 0.25f) / (std::sqrt(2.0f) * 0.4f), 1e-3f);
    ExpectNormal(hit, 0.70711f, 0.70711f, 0.0f, "the corner-region normal");
}

TEST(SweepTests, AStartInTheCornerRegionMovingAwayIsStillAMiss)
{
    // The same place, going the other way. The fix must not turn "already past it" into a hit.
    const SweepHit hit =
        SweepCapsuleObb(Sphere(1.20f, 1.20f, 0.0f, 0.25f), Vector3(0.4f, 0.4f, 0.0f), UnitBox());
    EXPECT_FALSE(hit.hit);
}

// ---- yaw ----------------------------------------------------------------------------------------

TEST(SweepTests, AYawedBoxIsMetOnItsOwnFace)
{
    // The box turned 90 degrees is the same box, so the answer must not move at all.
    const SweepHit straight =
        SweepCapsuleObb(Sphere(2.0f, 0.0f, 0.0f, 0.25f), Vector3(-2.0f, 0.0f, 0.0f), UnitBox());
    const SweepHit turned =
        SweepCapsuleObb(Sphere(2.0f, 0.0f, 0.0f, 0.25f), Vector3(-2.0f, 0.0f, 0.0f), UnitBox(1.5707963f));
    ASSERT_TRUE(turned.hit);
    EXPECT_NEAR(turned.time, straight.time, 1e-4f);
    ExpectNormal(turned, 1.0f, 0.0f, 0.0f, "a square box turned a quarter is the same box");
}

TEST(SweepTests, AYawedSlabIsMetSquareOnItsLongFace)
{
    // A 4 x 2 x 0.4 slab turned 90 degrees runs along x instead of z. Coming from +x, the body now
    // meets its END at 0.2 rather than its face at 2.
    CollisionObb slab;
    slab.centre = Vector3(0.0f, 0.0f, 0.0f);
    slab.halfExtents = Vector3(2.0f, 1.0f, 0.2f);
    slab.yaw = 1.5707963f;
    const SweepHit hit = SweepCapsuleObb(Sphere(3.0f, 0.0f, 0.0f, 0.25f), Vector3(-6.0f, 0.0f, 0.0f), slab);
    ASSERT_TRUE(hit.hit);
    // The turned slab's half-extent along world x is 0.2, so the face is at 0.2 + 0.25 = 0.45.
    EXPECT_NEAR(hit.time, (3.0f - 0.45f) / 6.0f, 1e-4f);
    ExpectNormal(hit, 1.0f, 0.0f, 0.0f, "the end of the turned slab");
}

TEST(SweepTests, AYawedCubeIsMetOnAVerticalEdgeAndTheNormalIsTheApproach)
{
    // A cube turned 45 degrees presents an EDGE to +x, not a face -- its silhouette from there is
    // a diamond. A body coming along -x meets that vertical edge head on, so the world normal is
    // (1, 0, 0): the edge is a line and the contact normal runs from it to the body.
    //
    // The distance is the edge's own: the corner (1,1) of the unit cube is √2 from the centre, so
    // the centre stops at √2 + 0.25 = 1.66421.
    const SweepHit hit =
        SweepCapsuleObb(Sphere(4.0f, 0.0f, 0.0f, 0.25f), Vector3(-8.0f, 0.0f, 0.0f), UnitBox(0.7853982f));
    ASSERT_TRUE(hit.hit);
    EXPECT_NEAR(hit.time, (4.0f - (std::sqrt(2.0f) + 0.25f)) / 8.0f, 1e-4f)
        << "the diamond silhouette of a turned cube reaches √2, not 1";
    ExpectNormal(hit, 1.0f, 0.0f, 0.0f, "head on into a vertical edge");
}

TEST(SweepTests, AYawedSlabsNormalComesBackInWorldSpace)
{
    // A slab 6 x 2 x 0.2 turned 45 degrees, approached square on along its own normal. The slab's
    // local +z face has world normal (sin45, 0, cos45) -- a normal with BOTH components non-zero,
    // which is what forgetting to rotate out of the box's frame cannot produce.
    CollisionObb slab;
    slab.centre = Vector3(0.0f, 0.0f, 0.0f);
    slab.halfExtents = Vector3(3.0f, 1.0f, 0.1f);
    slab.yaw = 0.7853982f;
    const float half = std::sqrt(0.5f);
    const SweepHit hit = SweepCapsuleObb(
        Sphere(2.0f * half, 0.0f, 2.0f * half, 0.25f), Vector3(-4.0f * half, 0.0f, -4.0f * half), slab);
    ASSERT_TRUE(hit.hit);
    // The face is 0.1 from the centre along that normal and the body stops 0.25 short of it, so
    // the centre travels 2.0 - 0.35 = 1.65 of the 4.0 m the motion is long.
    EXPECT_NEAR(hit.time, (2.0f - 0.35f) / 4.0f, 1e-4f);
    ExpectNormal(hit, half, 0.0f, half, "the turned slab's own face normal, in world space");
}

// ---- the numbers a caller depends on --------------------------------------------------------------

TEST(SweepTests, TheTimeIsAFractionOfTheMotionAndNotADistance)
{
    // The same geometry with twice the motion must halve the time, or a caller multiplying
    // `time * motion` walks twice as far as it meant to.
    const SweepHit once =
        SweepCapsuleObb(Sphere(2.0f, 0.0f, 0.0f, 0.25f), Vector3(-2.0f, 0.0f, 0.0f), UnitBox());
    const SweepHit twice =
        SweepCapsuleObb(Sphere(2.0f, 0.0f, 0.0f, 0.25f), Vector3(-4.0f, 0.0f, 0.0f), UnitBox());
    ASSERT_TRUE(once.hit);
    ASSERT_TRUE(twice.hit);
    EXPECT_NEAR(twice.time * 2.0f, once.time, 1e-4f);
    EXPECT_NEAR(once.time * 2.0f, 0.75f, 1e-4f) << "the distance travelled is not 0.75 m";
}

TEST(SweepTests, TheNormalIsAlwaysUnitAndPointsOutOfTheBox)
{
    // Sixteen directions round the box, each of which must produce a unit normal pointing back the
    // way the body came -- a normal pointing INTO the box makes a slide accelerate into the wall.
    const CollisionObb box = UnitBox();
    for (int i = 0; i < 16; ++i)
    {
        const float angle = static_cast<float>(i) * 6.2831853f / 16.0f;
        const float x = std::cos(angle) * 3.0f;
        const float z = std::sin(angle) * 3.0f;
        const SweepHit hit =
            SweepCapsuleObb(Sphere(x, 0.0f, z, 0.25f), Vector3(-x * 2.0f, 0.0f, -z * 2.0f), box);
        ASSERT_TRUE(hit.hit) << "direction " << i;
        const float length = std::sqrt(hit.normal.X * hit.normal.X + hit.normal.Y * hit.normal.Y +
                                       hit.normal.Z * hit.normal.Z);
        EXPECT_NEAR(length, 1.0f, 1e-4f) << "direction " << i;
        // Pointing back against the motion: the dot with the direction of travel is negative.
        const float dot = hit.normal.X * (-x) + hit.normal.Z * (-z);
        EXPECT_LT(dot, 0.0f) << "direction " << i << ": the normal points the way the body was going";
    }
}

TEST(SweepTests, AFlatBoxIsStillHit)
{
    // A zero-thickness box is what a floor slab degenerates to if a thickness is ever authored as
    // 0. It must still stop a body rather than being missed by a slab test that divides by it.
    CollisionObb flat;
    flat.centre = Vector3(0.0f, 0.0f, 0.0f);
    flat.halfExtents = Vector3(2.0f, 0.0f, 2.0f);
    const SweepHit hit = SweepCapsuleObb(Sphere(0.0f, 2.0f, 0.0f, 0.25f), Vector3(0.0f, -4.0f, 0.0f), flat);
    ASSERT_TRUE(hit.hit);
    EXPECT_NEAR(hit.time, (2.0f - 0.25f) / 4.0f, 1e-4f);
    ExpectNormal(hit, 0.0f, 1.0f, 0.0f, "landing on a zero-thickness slab");
}

TEST(SweepTests, AZeroRadiusCapsuleIsARayAgainstThePlainBox)
{
    const SweepHit hit =
        SweepCapsuleObb(Sphere(2.0f, 0.0f, 0.0f, 0.0f), Vector3(-2.0f, 0.0f, 0.0f), UnitBox());
    ASSERT_TRUE(hit.hit);
    EXPECT_NEAR(hit.time, 0.5f, 1e-4f) << "with no radius the face is at 1, half of a 2 m motion";
    ExpectNormal(hit, 1.0f, 0.0f, 0.0f, "a ray against a face");
}

TEST(SweepTests, TheRealCaseAPlayerWalkingIntoAWall)
{
    // §70.5's capsule -- 0.62 m across, so radius 0.31 -- 1.80 m tall, walking at 1.4 m/s for one
    // 1/120 s physics step into a 0.15 m partition. §49.3's step is 1.4 / 120 = 11.67 mm, so the
    // gap has to be SHORTER than that for the step to reach the wall at all: 8 mm, which is two
    // thirds of a step and lands at t = 0.686. A 20 mm gap is not crossed in one step of a walk,
    // and the first version of this case asked for exactly that and was right to fail.
    CollisionObb partition;
    partition.centre = Vector3(0.0f, 1.5f, 0.0f);
    partition.halfExtents = Vector3(3.0f, 1.5f, 0.075f);
    const float radius = 0.31f;
    const float gap = 0.008f;
    const Capsule player = Body(0.0f, 0.9f, 0.075f + radius + gap, 0.59f, radius);
    const float step = 1.4f / 120.0f;
    const SweepHit hit = SweepCapsuleObb(player, Vector3(0.0f, 0.0f, -step), partition);
    ASSERT_TRUE(hit.hit) << "the player walked through the wall";
    EXPECT_NEAR(hit.time, gap / step, 1e-3f);
    ExpectNormal(hit, 0.0f, 0.0f, 1.0f, "the wall's near face");
    // ...and the distance actually travelled is the gap, to within a tenth of a millimetre.
    EXPECT_NEAR(hit.time * step, gap, 1e-4f);
}
