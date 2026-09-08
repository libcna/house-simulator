// SPDX-License-Identifier: MIT
//
// `HOUSE-00544`. The capsule against a triangle: the stair ramps and the rafter envelope are
// triangle meshes (§49.2) and the terrain will be one, so this is what a body meets on a staircase.
//
// Analytic again, and for the same reason as `SweepTests`: every expected number below is a
// distance to the triangle extruded along Y by the capsule's half-height and rounded by its
// radius, worked out from the case rather than read off a run.
#include <cmath>

#include <gtest/gtest.h>

#include "cnahouse/physics/Sweep.hpp"

namespace
{
    using cnahouse::physics::Capsule;
    using cnahouse::physics::SweepHit;
    using Microsoft::Xna::Framework::Vector3;

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

    /// A 4 x 4 triangle in the y = 0 plane: (0,0,0), (4,0,0), (0,0,4).
    void Flat(Vector3& a, Vector3& b, Vector3& c)
    {
        a = Vector3(0.0f, 0.0f, 0.0f);
        b = Vector3(4.0f, 0.0f, 0.0f);
        c = Vector3(0.0f, 0.0f, 4.0f);
    }

    void ExpectNormal(const SweepHit& hit, float x, float y, float z, const char* what)
    {
        ASSERT_TRUE(hit.hit) << what;
        EXPECT_NEAR(hit.normal.X, x, 1e-4f) << what;
        EXPECT_NEAR(hit.normal.Y, y, 1e-4f) << what;
        EXPECT_NEAR(hit.normal.Z, z, 1e-4f) << what;
    }

} // namespace

TEST(SweepTriangleTests, ASphereLandsOnTheFaceOfAFlatTriangle)
{
    // Dropping onto the middle of the triangle from 2 m up with a 0.25 radius: the contact is at
    // y = 0.25, so it falls 1.75 of a 2 m drop.
    Vector3 a, b, c;
    Flat(a, b, c);
    const SweepHit hit =
        SweepCapsuleTriangle(Sphere(1.0f, 2.0f, 1.0f, 0.25f), Vector3(0.0f, -2.0f, 0.0f), a, b, c);
    ASSERT_TRUE(hit.hit);
    EXPECT_NEAR(hit.time, 1.75f / 2.0f, 1e-4f);
    ExpectNormal(hit, 0.0f, 1.0f, 0.0f, "landing on a floor triangle");
    EXPECT_FALSE(hit.startedInside);
}

TEST(SweepTriangleTests, TheWindingDoesNotMatter)
{
    // §14's winding is for drawing. A sweep is stopped by a surface from either side, and a body
    // that fell through a floor because its triangles were wound the other way would be a bug the
    // renderer cannot see.
    Vector3 a, b, c;
    Flat(a, b, c);
    const SweepHit forward =
        SweepCapsuleTriangle(Sphere(1.0f, 2.0f, 1.0f, 0.25f), Vector3(0.0f, -2.0f, 0.0f), a, b, c);
    const SweepHit reversed =
        SweepCapsuleTriangle(Sphere(1.0f, 2.0f, 1.0f, 0.25f), Vector3(0.0f, -2.0f, 0.0f), a, c, b);
    ASSERT_TRUE(reversed.hit);
    EXPECT_NEAR(reversed.time, forward.time, 1e-5f);
    ExpectNormal(reversed, 0.0f, 1.0f, 0.0f, "the normal still points at the body");
}

TEST(SweepTriangleTests, ASphereMissesBesideTheTriangle)
{
    // Dropping outside the hypotenuse: (3, 3) is beyond x + z = 4 by 2 / √2 = 1.414, well past the
    // 0.25 rounding.
    Vector3 a, b, c;
    Flat(a, b, c);
    const SweepHit hit =
        SweepCapsuleTriangle(Sphere(3.0f, 2.0f, 3.0f, 0.25f), Vector3(0.0f, -4.0f, 0.0f), a, b, c);
    EXPECT_FALSE(hit.hit);
}

TEST(SweepTriangleTests, ASphereCatchesTheRoundedEdge)
{
    // Straight down just past the hypotenuse, at (2.1, 2.1): the perpendicular distance past the
    // line x + z = 4 is 0.2 / √2 = 0.14142, inside the 0.25 rounding. The sphere's centre stops
    // where it is 0.25 from the edge line, so it drops to y = √(0.25² − 0.14142²) = 0.20616.
    Vector3 a, b, c;
    Flat(a, b, c);
    const SweepHit hit =
        SweepCapsuleTriangle(Sphere(2.1f, 2.0f, 2.1f, 0.25f), Vector3(0.0f, -4.0f, 0.0f), a, b, c);
    ASSERT_TRUE(hit.hit);
    const float past = 0.2f / std::sqrt(2.0f);
    const float drop = std::sqrt(0.25f * 0.25f - past * past);
    EXPECT_NEAR(hit.time, (2.0f - drop) / 4.0f, 1e-4f) << "the hypotenuse is a rounded edge, not a cliff";
    // The normal runs from the edge line to the centre: up, and outwards along the diagonal.
    EXPECT_GT(hit.normal.Y, 0.0f);
    EXPECT_NEAR(hit.normal.X, hit.normal.Z, 1e-4f) << "the hypotenuse is symmetric in x and z";
}

TEST(SweepTriangleTests, ASphereCatchesTheRoundedVertex)
{
    // Straight down 0.1 outside the corner at (4, 0, 0), along +x: the centre stops at
    // y = √(0.25² − 0.1²) = 0.22913 above it.
    Vector3 a, b, c;
    Flat(a, b, c);
    const SweepHit hit =
        SweepCapsuleTriangle(Sphere(4.1f, 2.0f, 0.0f, 0.25f), Vector3(0.0f, -4.0f, 0.0f), a, b, c);
    ASSERT_TRUE(hit.hit);
    const float drop = std::sqrt(0.25f * 0.25f - 0.1f * 0.1f);
    EXPECT_NEAR(hit.time, (2.0f - drop) / 4.0f, 1e-4f);
    EXPECT_GT(hit.normal.X, 0.0f) << "the normal leans away from the corner";
    EXPECT_GT(hit.normal.Y, 0.0f);
}

TEST(SweepTriangleTests, AnUprightCapsuleMeetsTheFaceWithItsFoot)
{
    // A capsule of half-height 0.8 and radius 0.25 dropped from y = 3 lands when its BOTTOM
    // touches: 3 - (0.8 + 0.25) = 1.95 of a 3 m drop.
    Vector3 a, b, c;
    Flat(a, b, c);
    const Capsule body = Body(1.0f, 3.0f, 1.0f, 0.8f, 0.25f);
    EXPECT_FLOAT_EQ(body.Bottom(), 1.95f);
    const SweepHit hit = SweepCapsuleTriangle(body, Vector3(0.0f, -3.0f, 0.0f), a, b, c);
    ASSERT_TRUE(hit.hit);
    EXPECT_NEAR(hit.time, 1.95f / 3.0f, 1e-4f);
    ExpectNormal(hit, 0.0f, 1.0f, 0.0f, "standing on the triangle");
}

TEST(SweepTriangleTests, TheSegmentDoesNotWidenTheTriangle)
{
    // The same capsule dropped 1 m outside the triangle in x must still miss: extruding along Y
    // lengthens the shape vertically and nowhere else.
    Vector3 a, b, c;
    Flat(a, b, c);
    const SweepHit hit =
        SweepCapsuleTriangle(Body(5.0f, 3.0f, 1.0f, 0.8f, 0.25f), Vector3(0.0f, -3.0f, 0.0f), a, b, c);
    EXPECT_FALSE(hit.hit);
}

TEST(SweepTriangleTests, AStairRampStopsAWalkAtItsSlope)
{
    // A ramp rising 1 in 2 along -z: (0,0,0), (2,0,0), (0,1,-2) and its mirror. A body walking
    // into it along -z meets the sloping face, whose normal is (0, 2, 1)/√5.
    const Vector3 a(0.0f, 0.0f, 0.0f);
    const Vector3 b(2.0f, 0.0f, 0.0f);
    const Vector3 c(0.0f, 1.0f, -2.0f);
    const SweepHit hit =
        SweepCapsuleTriangle(Sphere(0.5f, 0.6f, 1.0f, 0.25f), Vector3(0.0f, 0.0f, -3.0f), a, b, c);
    ASSERT_TRUE(hit.hit);
    const float root5 = std::sqrt(5.0f);
    ExpectNormal(hit, 0.0f, 2.0f / root5, 1.0f / root5, "the ramp's own slope");
    // The plane through the origin with that normal: 2y + z = 0. The centre stops where its
    // distance to the plane is 0.25, i.e. (2*0.6 + z)/√5 = 0.25 -> z = 0.25√5 - 1.2 = -0.64098.
    const float stopZ = 0.25f * root5 - 1.2f;
    EXPECT_NEAR(hit.time, (1.0f - stopZ) / 3.0f, 1e-4f);
}

TEST(SweepTriangleTests, AVerticalTriangleIsStillASurface)
{
    // A wall triangle in the x = 0 plane. Its extrusion along Y is flat -- zero volume -- and a
    // rounded flat polygon is still a body a sweep must be stopped by.
    const Vector3 a(0.0f, 0.0f, 0.0f);
    const Vector3 b(0.0f, 3.0f, 0.0f);
    const Vector3 c(0.0f, 0.0f, 3.0f);
    const SweepHit hit =
        SweepCapsuleTriangle(Body(2.0f, 1.0f, 0.5f, 0.5f, 0.3f), Vector3(-4.0f, 0.0f, 0.0f), a, b, c);
    ASSERT_TRUE(hit.hit) << "a body walked through a wall triangle";
    EXPECT_NEAR(hit.time, (2.0f - 0.3f) / 4.0f, 1e-4f);
    ExpectNormal(hit, 1.0f, 0.0f, 0.0f, "the wall's face");
}

TEST(SweepTriangleTests, ADegenerateTriangleIsAMissAndNotANormalMadeOfNoise)
{
    // Three points in a line, and two in the same place. Neither has a surface, and a body pushed
    // by a normalised cross product of nothing is worse than one that passes through.
    const SweepHit collinear = SweepCapsuleTriangle(Sphere(0.0f, 2.0f, 0.0f, 0.25f),
                                                    Vector3(0.0f, -4.0f, 0.0f),
                                                    Vector3(0.0f, 0.0f, 0.0f),
                                                    Vector3(1.0f, 0.0f, 0.0f),
                                                    Vector3(2.0f, 0.0f, 0.0f));
    EXPECT_FALSE(collinear.hit);
    const SweepHit doubled = SweepCapsuleTriangle(Sphere(0.0f, 2.0f, 0.0f, 0.25f),
                                                  Vector3(0.0f, -4.0f, 0.0f),
                                                  Vector3(0.0f, 0.0f, 0.0f),
                                                  Vector3(0.0f, 0.0f, 0.0f),
                                                  Vector3(1.0f, 0.0f, 1.0f));
    EXPECT_FALSE(doubled.hit);
}

TEST(SweepTriangleTests, StartingOnTheSurfaceIsReportedAsInside)
{
    Vector3 a, b, c;
    Flat(a, b, c);
    const SweepHit hit =
        SweepCapsuleTriangle(Sphere(1.0f, 0.20f, 1.0f, 0.25f), Vector3(0.0f, -1.0f, 0.0f), a, b, c);
    ASSERT_TRUE(hit.hit);
    EXPECT_FLOAT_EQ(hit.time, 0.0f);
    EXPECT_TRUE(hit.startedInside);
}

TEST(SweepTriangleTests, TwoTrianglesOfOneFloorHaveNoSeamToCatchOn)
{
    // The failure this exists to prevent: a body sliding across the diagonal where two triangles
    // of the same flat quad meet. Both must report the same contact height, so a body crossing the
    // seam neither rises nor drops.
    const Vector3 q0(0.0f, 0.0f, 0.0f);
    const Vector3 q1(4.0f, 0.0f, 0.0f);
    const Vector3 q2(4.0f, 0.0f, 4.0f);
    const Vector3 q3(0.0f, 0.0f, 4.0f);
    for (int step = 0; step <= 8; ++step)
    {
        // A line of samples straight across the shared diagonal from (0,4) to (4,0).
        const float t = static_cast<float>(step) / 8.0f;
        const float x = 4.0f * t;
        const float z = 4.0f * (1.0f - t);
        const Capsule body = Sphere(x, 2.0f, z, 0.25f);
        const SweepHit first = SweepCapsuleTriangle(body, Vector3(0.0f, -4.0f, 0.0f), q0, q1, q2);
        const SweepHit second = SweepCapsuleTriangle(body, Vector3(0.0f, -4.0f, 0.0f), q0, q2, q3);
        ASSERT_TRUE(first.hit || second.hit) << "sample " << step << " fell through the floor";
        const float best = std::min(first.hit ? first.time : 2.0f, second.hit ? second.time : 2.0f);
        EXPECT_NEAR(best, 1.75f / 4.0f, 1e-4f)
            << "sample " << step << " landed at a different height on the seam";
    }
}

TEST(SweepTriangleTests, TheTimeIsAFractionOfTheMotion)
{
    Vector3 a, b, c;
    Flat(a, b, c);
    const SweepHit once =
        SweepCapsuleTriangle(Sphere(1.0f, 2.0f, 1.0f, 0.25f), Vector3(0.0f, -2.0f, 0.0f), a, b, c);
    const SweepHit twice =
        SweepCapsuleTriangle(Sphere(1.0f, 2.0f, 1.0f, 0.25f), Vector3(0.0f, -4.0f, 0.0f), a, b, c);
    ASSERT_TRUE(once.hit);
    ASSERT_TRUE(twice.hit);
    EXPECT_NEAR(twice.time * 2.0f, once.time, 1e-4f);
}

TEST(SweepTriangleTests, EveryApproachGivesAUnitNormalThatOpposesTheMotion)
{
    // Twelve directions in the plane just above a flat triangle, each moving through it. Whatever
    // feature is met, the normal must be unit and must push back.
    Vector3 a, b, c;
    Flat(a, b, c);
    for (int i = 0; i < 12; ++i)
    {
        const float angle = static_cast<float>(i) * 6.2831853f / 12.0f;
        const float x = 1.3f + std::cos(angle) * 3.0f;
        const float z = 1.3f + std::sin(angle) * 3.0f;
        const Vector3 motion(1.3f - x, -0.4f, 1.3f - z);
        const SweepHit hit = SweepCapsuleTriangle(Sphere(x, 0.35f, z, 0.3f), motion, a, b, c);
        ASSERT_TRUE(hit.hit) << "direction " << i;
        const float length = std::sqrt(hit.normal.X * hit.normal.X + hit.normal.Y * hit.normal.Y +
                                       hit.normal.Z * hit.normal.Z);
        EXPECT_NEAR(length, 1.0f, 1e-4f) << "direction " << i;
        const float dot = hit.normal.X * motion.X + hit.normal.Y * motion.Y + hit.normal.Z * motion.Z;
        EXPECT_LT(dot, 1e-4f) << "direction " << i << ": the normal points the way the body went";
    }
}
