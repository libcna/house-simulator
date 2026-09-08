// SPDX-License-Identifier: MIT
//
// `HOUSE-00547`. §49.3 step 5: *"Depenetrate: 4 iterations of 0.02 m push-out along the deepest
// overlap normal"*. Three claims, and the numbers in them are the point:
//
//   * `Overlap` measures a DEPTH and a way OUT, for a box and for a triangle, from either side;
//   * `OverlapCell` takes the DEEPEST of what a cell holds, not the first the list happens to
//     hold — pushing out of the shallowest first leaves the body inside the other and has spent an
//     iteration to do it;
//   * `Depenetrate` pushes a FIXED 0.02 m four times and reports honestly when that was not
//     enough. Pushing out by the measured depth would resolve every case in one step, and would
//     teleport a deeply buried body through the wall it is buried in.
//
// Half the cases are constructed, where the depth can be written down to the millimetre. The other
// half is the real house, because a body that ends a frame inside static geometry is what §49.5
// promises never happens and a corner of a real corridor is where it would.
#include <cmath>
#include <memory>
#include <string>

#include <gtest/gtest.h>

#include "System/IO/FileAccess.hpp"
#include "System/IO/FileMode.hpp"
#include "System/IO/FileStream.hpp"

#include "cnahouse/physics/BroadPhase.hpp"
#include "cnahouse/physics/CollisionLoader.hpp"
#include "cnahouse/physics/Sweep.hpp"

namespace
{
    using cnahouse::physics::BroadPhase;
    using cnahouse::physics::Capsule;
    using cnahouse::physics::CellOverlap;
    using cnahouse::physics::CellSweepHit;
    using cnahouse::physics::CollisionCell;
    using cnahouse::physics::CollisionKind;
    using cnahouse::physics::CollisionLoader;
    using cnahouse::physics::CollisionMesh;
    using cnahouse::physics::CollisionObb;
    using cnahouse::physics::CollisionWorld;
    using cnahouse::physics::Depenetrate;
    using cnahouse::physics::Depenetration;
    using cnahouse::physics::kContactTolerance;
    using cnahouse::physics::kDepenetrationIterations;
    using cnahouse::physics::kDepenetrationStep;
    using cnahouse::physics::Overlap;
    using cnahouse::physics::OverlapCapsuleObb;
    using cnahouse::physics::OverlapCapsuleTriangle;
    using cnahouse::physics::OverlapCell;
    using cnahouse::physics::Sphere;
    using Microsoft::Xna::Framework::BoundingBox;
    using Microsoft::Xna::Framework::Vector3;

    /// §43.1's body: a capsule of radius 0.30 m standing 1.80 m tall, so a half-height of 0.60.
    /// (§70.5's 0.62 m is the CLEARANCE a portal must give it, not the body's own width.)
    constexpr float kBodyRadius = 0.30f;
    constexpr float kBodyHalfHeight = 0.60f;

    CollisionObb Wall(const Vector3& centre, const Vector3& halfExtents, float yaw = 0.0f)
    {
        CollisionObb obb;
        obb.centre = centre;
        obb.halfExtents = halfExtents;
        obb.yaw = yaw;
        obb.kind = CollisionKind::Wall;
        return obb;
    }

    /// One cell, one grid bucket, holding every shape given. Small enough that the broad phase
    /// cannot be what makes a case pass or fail.
    CollisionWorld OneCell(std::vector<CollisionObb> obbs, std::vector<CollisionMesh> meshes = {})
    {
        CollisionWorld world;
        world.gridCell = 1.0f;
        world.surfaces = {"plaster"};
        world.obbs = std::move(obbs);
        world.meshes = std::move(meshes);

        CollisionCell cell;
        cell.id = "L0_TEST";
        cell.bounds = BoundingBox(Vector3(-8.0f, -4.0f, -8.0f), Vector3(8.0f, 8.0f, 8.0f));
        // The broad phase's grid is a metre a bucket (§49.2), so 16 x 16 of them cover the whole
        // 16 m cell. Every shape goes in every bucket: a conservative grid is still a correct one,
        // and no case here may pass or fail on which bucket a shape landed in.
        cell.nx = 16u;
        cell.nz = 16u;
        cell.originX = -8.0f;
        cell.originZ = -8.0f;
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

    /// One mesh holding two triangles at different heights, the SHALLOWER one first in index
    /// order. A mesh is one shape and its overlap is the deepest of its triangles; taking the
    /// first that overlaps would make the answer depend on the order they were written in.
    CollisionMesh TwoDecks()
    {
        CollisionMesh mesh;
        mesh.vertices = {Vector3(-2.0f, 0.10f, -2.0f),
                         Vector3(-2.0f, 0.10f, 2.0f),
                         Vector3(2.0f, 0.10f, 2.0f),
                         Vector3(-2.0f, 0.0f, -2.0f),
                         Vector3(-2.0f, 0.0f, 2.0f),
                         Vector3(2.0f, 0.0f, 2.0f)};
        mesh.indices = {0u, 1u, 2u, 3u, 4u, 5u};
        mesh.surface = 0u;
        mesh.kind = CollisionKind::Floor;
        return mesh;
    }

    CollisionMesh Floor(float y)
    {
        // One quad, 4 m x 4 m, centred on the origin. Wound anticlockwise seen from above -- and
        // the overlap must not care, which is what `TheFloorPushesUpFromAboveAndDownFromBelow`
        // exists to say.
        CollisionMesh mesh;
        mesh.vertices = {Vector3(-2.0f, y, -2.0f),
                         Vector3(-2.0f, y, 2.0f),
                         Vector3(2.0f, y, 2.0f),
                         Vector3(2.0f, y, -2.0f)};
        mesh.indices = {0u, 1u, 2u, 0u, 2u, 3u};
        mesh.surface = 0u;
        mesh.kind = CollisionKind::Floor;
        return mesh;
    }

    float Length(const Vector3& v)
    {
        return std::sqrt(v.X * v.X + v.Y * v.Y + v.Z * v.Z);
    }

} // namespace

// ---------------------------------------------------------------------------------------------
// The constants §49.3 states, asserted as literals
// ---------------------------------------------------------------------------------------------

TEST(DepenetrationTests, TheStepAndTheCountAreTheOnesTheDesignStates)
{
    // Written as literals on purpose. The constants are what the code uses and these are what
    // §49.3 says; if somebody "tunes" one, this is the line that asks whether §49.3 was updated.
    EXPECT_FLOAT_EQ(kDepenetrationStep, 0.02f);
    EXPECT_EQ(kDepenetrationIterations, 4);
}

// ---------------------------------------------------------------------------------------------
// Overlap against one box
// ---------------------------------------------------------------------------------------------

TEST(DepenetrationTests, AClearCapsuleOverlapsNothing)
{
    const CollisionObb wall = Wall(Vector3(1.1f, 1.5f, 0.0f), Vector3(0.1f, 1.5f, 2.0f));
    // The face is at x = 1.0 and the sphere's surface reaches 0.72: 0.28 m of air.
    const Overlap overlap = OverlapCapsuleObb(Sphere(Vector3(0.5f, 1.55f, 0.0f), 0.22f), wall);
    EXPECT_FALSE(overlap.overlapped);
    EXPECT_FLOAT_EQ(overlap.depth, 0.0f);
}

TEST(DepenetrationTests, TheBoundaryIsWhereTheSkinMeetsTheFace)
{
    const CollisionObb wall = Wall(Vector3(1.1f, 1.5f, 0.0f), Vector3(0.1f, 1.5f, 2.0f));
    // The face is at x = 1.0 and the sphere's skin reaches centre + 0.22, so the boundary is a
    // centre of 0.78. A millimetre either side of it, and the answer is the one it should be --
    // stated as two cases a millimetre apart rather than as one exact float, because "exactly
    // touching" is not a state a float can be asked about.
    EXPECT_FALSE(OverlapCapsuleObb(Sphere(Vector3(0.7790f, 1.55f, 0.0f), 0.22f), wall).overlapped);
    EXPECT_TRUE(OverlapCapsuleObb(Sphere(Vector3(0.7810f, 1.55f, 0.0f), 0.22f), wall).overlapped);

    // ...and a millimetre in is a millimetre of depth, not a step of one. This is what makes the
    // fixed 0.02 m push-out safe: the depth is measured honestly and only the STEP is fixed.
    const Overlap grazing = OverlapCapsuleObb(Sphere(Vector3(0.7810f, 1.55f, 0.0f), 0.22f), wall);
    EXPECT_NEAR(grazing.depth, 0.001f, 1e-5f);
    EXPECT_NEAR(grazing.normal.X, -1.0f, 1e-5f);
}

TEST(DepenetrationTests, ThePartlyBuriedCapsuleMeasuresTheRemainingSkin)
{
    const CollisionObb wall = Wall(Vector3(1.1f, 1.5f, 0.0f), Vector3(0.1f, 1.5f, 2.0f));
    // Centre at 0.90, face at 1.00: 0.10 m of the 0.22 m radius is left outside, so 0.12 m is in.
    const Overlap overlap = OverlapCapsuleObb(Sphere(Vector3(0.90f, 1.55f, 0.0f), 0.22f), wall);
    ASSERT_TRUE(overlap.overlapped);
    EXPECT_NEAR(overlap.depth, 0.12f, 1e-5f);
    EXPECT_NEAR(overlap.normal.X, -1.0f, 1e-5f);
    EXPECT_NEAR(overlap.normal.Y, 0.0f, 1e-5f);
    EXPECT_NEAR(overlap.normal.Z, 0.0f, 1e-5f);
}

TEST(DepenetrationTests, TheCentreInsideTheBoxLeavesByTheNEARESTFace)
{
    const CollisionObb wall = Wall(Vector3(1.1f, 1.5f, 0.0f), Vector3(0.1f, 1.5f, 2.0f));
    // The centre is INSIDE the 0.2 m thick wall, 0.01 m past its middle. There is no "away from
    // the surface" direction any more -- every direction leaves -- so the shortest way out wins:
    // 0.09 m to the west face, plus the 0.22 m the skin must clear it by.
    const Overlap overlap = OverlapCapsuleObb(Sphere(Vector3(1.09f, 1.55f, 0.0f), 0.22f), wall);
    ASSERT_TRUE(overlap.overlapped);
    EXPECT_NEAR(overlap.depth, 0.31f, 1e-5f);
    EXPECT_NEAR(overlap.normal.X, -1.0f, 1e-5f);
    // ...and NOT out through the 3 m tall face or the 4 m long one, which are both nearer to
    // reach in index order and much further away in metres.
    EXPECT_NEAR(overlap.normal.Y, 0.0f, 1e-5f);
    EXPECT_NEAR(overlap.normal.Z, 0.0f, 1e-5f);
}

TEST(DepenetrationTests, TheWHOLECapsuleOverlapsNotJustItsCentre)
{
    // A kerb 0.15 m tall at the body's feet. The centre is 1.05 m above it and nowhere near, but
    // the capsule reaches down to 0.31 m off the floor and its lower cap is inside the kerb. A
    // point test would call this clear.
    const CollisionObb kerb = Wall(Vector3(0.6f, 0.075f, 0.0f), Vector3(0.3f, 0.075f, 2.0f));
    const Capsule body{Vector3(0.15f, 0.90f, 0.0f), kBodyHalfHeight, kBodyRadius};
    const Overlap overlap = OverlapCapsuleObb(body, kerb);
    ASSERT_TRUE(overlap.overlapped);

    // A SPHERE at the same centre clears the kerb by 0.45 m and touches nothing. That is the
    // whole difference between testing the body and testing a point where its middle happens to
    // be.
    EXPECT_FALSE(OverlapCapsuleObb(Sphere(body.centre, kBodyRadius), kerb).overlapped);

    // The rounded box is the kerb grown by 0.60 m in Y: 0.675 tall, centred at 0.075, so its top
    // is at 0.75. The nearest point of it to the body's centre is its top west EDGE, at (0.30,
    // 0.75) -- 0.15 m west and 0.15 m below the centre, so 0.2121 m away and 0.0879 m of the
    // radius inside. The push is out along that edge, west AND up at 45 degrees, and not squarely
    // west: a body clipping the corner of a kerb is lifted over it, not shoved back off it.
    EXPECT_NEAR(overlap.depth, 0.0879f, 1e-4f);
    EXPECT_NEAR(overlap.normal.X, -0.7071f, 1e-3f);
    EXPECT_NEAR(overlap.normal.Y, 0.7071f, 1e-3f);
    EXPECT_NEAR(overlap.normal.Z, 0.0f, 1e-5f);
}

TEST(DepenetrationTests, TheNormalComesBackInWorldAxesNotTheBoxs)
{
    // The same wall turned a quarter turn about +Y: it now faces along Z, and a body against it
    // must be pushed along Z. A normal left in the box's own frame would push it along X --
    // sideways along the wall, forever.
    const CollisionObb wall = Wall(Vector3(0.0f, 1.5f, 1.1f), Vector3(0.1f, 1.5f, 2.0f), 1.5707963f);
    const Overlap overlap = OverlapCapsuleObb(Sphere(Vector3(0.0f, 1.55f, 0.90f), 0.22f), wall);
    ASSERT_TRUE(overlap.overlapped);
    EXPECT_NEAR(overlap.depth, 0.12f, 1e-4f);
    EXPECT_NEAR(overlap.normal.X, 0.0f, 1e-4f);
    EXPECT_NEAR(overlap.normal.Z, -1.0f, 1e-4f);
}

// ---------------------------------------------------------------------------------------------
// Overlap against one triangle
// ---------------------------------------------------------------------------------------------

TEST(DepenetrationTests, TheFloorPushesUpFromAboveAndDownFromBelow)
{
    // A triangle has a winding, and the winding is for DRAWING (§14). A body under a floor is in
    // the floor as much as a body on top of it, and the two must be pushed opposite ways. Getting
    // this wrong is how a body falls through a floor it is standing on -- the exact defect the
    // sweep had before `HOUSE-00545`.
    const Vector3 a(-2.0f, 0.0f, -2.0f);
    const Vector3 b(-2.0f, 0.0f, 2.0f);
    const Vector3 c(2.0f, 0.0f, 2.0f);

    const Overlap above = OverlapCapsuleTriangle(Sphere(Vector3(-0.5f, 0.15f, 0.5f), 0.22f), a, b, c);
    ASSERT_TRUE(above.overlapped);
    EXPECT_NEAR(above.depth, 0.07f, 1e-5f);
    EXPECT_NEAR(above.normal.Y, 1.0f, 1e-5f);

    const Overlap below = OverlapCapsuleTriangle(Sphere(Vector3(-0.5f, -0.15f, 0.5f), 0.22f), a, b, c);
    ASSERT_TRUE(below.overlapped);
    EXPECT_NEAR(below.depth, 0.07f, 1e-5f);
    EXPECT_NEAR(below.normal.Y, -1.0f, 1e-5f);
}

TEST(DepenetrationTests, PastTheTrianglesEdgeIsPastTheTriangle)
{
    // The same plane, 1 m beyond the triangle's edge. A body is not standing on a floor that is
    // not there, and a plane test rather than a triangle test would say it is.
    const Vector3 a(-2.0f, 0.0f, -2.0f);
    const Vector3 b(-2.0f, 0.0f, 2.0f);
    const Vector3 c(2.0f, 0.0f, 2.0f);
    const Overlap overlap = OverlapCapsuleTriangle(Sphere(Vector3(3.0f, 0.05f, 0.0f), 0.22f), a, b, c);
    EXPECT_FALSE(overlap.overlapped);
}

TEST(DepenetrationTests, ACapsuleStraddlingTheFloorLeavesByTheSIDEItIsMOSTlyOn)
{
    // The body's centre is INSIDE the prism the triangle sweeps -- 0.59 m of capsule above the
    // floor plane and 0.59 m below it -- so there is no "away from the surface" to push along and
    // the way out is decided by which end of the prism is nearer.
    //
    // Centre 0.10 m above the plane: the bottom cap's centre is at -0.50 and has to reach +0.30,
    // so the body rises 0.80 m. That is the depth, exactly, and the direction is up.
    const Vector3 a(-2.0f, 0.0f, -2.0f);
    const Vector3 b(-2.0f, 0.0f, 2.0f);
    const Vector3 c(2.0f, 0.0f, 2.0f);

    const Capsule above{Vector3(-0.5f, 0.10f, 0.5f), kBodyHalfHeight, kBodyRadius};
    const Overlap up = OverlapCapsuleTriangle(above, a, b, c);
    ASSERT_TRUE(up.overlapped);
    EXPECT_NEAR(up.depth, 0.80f, 1e-5f);
    EXPECT_NEAR(up.normal.Y, 1.0f, 1e-4f);

    // And 0.10 m below it, the mirror: a body that is mostly under a floor leaves underneath it.
    // Pushing it up instead would drive 0.69 m of capsule through the slab.
    const Capsule below{Vector3(-0.5f, -0.10f, 0.5f), kBodyHalfHeight, kBodyRadius};
    const Overlap down = OverlapCapsuleTriangle(below, a, b, c);
    ASSERT_TRUE(down.overlapped);
    EXPECT_NEAR(down.depth, 0.80f, 1e-5f);
    EXPECT_NEAR(down.normal.Y, -1.0f, 1e-4f);
}

TEST(DepenetrationTests, ADegenerateTriangleIsNotASurface)
{
    // Three points in a line. There is no normal, and a normal made of noise is worse than a
    // miss: it would push a body in a direction nothing chose.
    const Overlap overlap = OverlapCapsuleTriangle(Sphere(Vector3(0.0f, 0.0f, 0.0f), 0.5f),
                                                   Vector3(-1.0f, 0.0f, 0.0f),
                                                   Vector3(0.0f, 0.0f, 0.0f),
                                                   Vector3(1.0f, 0.0f, 0.0f));
    EXPECT_FALSE(overlap.overlapped);
}

// ---------------------------------------------------------------------------------------------
// The sweep and the overlap are the same question asked twice
// ---------------------------------------------------------------------------------------------

TEST(DepenetrationTests, ASweepThatStartsInsideAgreesWithTheOverlap)
{
    // `SweepCapsuleObb` and `SweepCapsuleTriangle` answer "already touching?" by CALLING this
    // code, so the two cannot drift apart: a sweep that reports `startedInside` and an overlap
    // that reports nothing would be a body that is stuck and cannot be pushed out of anything.
    const CollisionObb wall = Wall(Vector3(1.1f, 1.5f, 0.0f), Vector3(0.1f, 1.5f, 2.0f));
    const Capsule capsule = Sphere(Vector3(0.90f, 1.55f, 0.0f), 0.22f);
    const auto swept = SweepCapsuleObb(capsule, Vector3(0.0f, 0.0f, 1.0f), wall);
    const Overlap overlap = OverlapCapsuleObb(capsule, wall);
    ASSERT_TRUE(swept.startedInside);
    ASSERT_TRUE(overlap.overlapped);
    EXPECT_NEAR(swept.normal.X, overlap.normal.X, 1e-5f);
    EXPECT_NEAR(swept.normal.Y, overlap.normal.Y, 1e-5f);
    EXPECT_NEAR(swept.normal.Z, overlap.normal.Z, 1e-5f);

    const Vector3 a(-2.0f, 0.0f, -2.0f);
    const Vector3 b(-2.0f, 0.0f, 2.0f);
    const Vector3 c(2.0f, 0.0f, 2.0f);
    const Capsule sunk = Sphere(Vector3(-0.5f, 0.15f, 0.5f), 0.22f);
    const auto sweptFloor = SweepCapsuleTriangle(sunk, Vector3(1.0f, 0.0f, 0.0f), a, b, c);
    const Overlap floorOverlap = OverlapCapsuleTriangle(sunk, a, b, c);
    ASSERT_TRUE(sweptFloor.startedInside);
    ASSERT_TRUE(floorOverlap.overlapped);
    EXPECT_NEAR(sweptFloor.normal.Y, floorOverlap.normal.Y, 1e-5f);
}

// ---------------------------------------------------------------------------------------------
// The deepest of a cell, not the first
// ---------------------------------------------------------------------------------------------

TEST(DepenetrationTests, ACornerIsResolvedByTheDEEPESTWallNotTheFirstOne)
{
    // Two walls, and the body is 0.06 m into the west one and 0.16 m into the north one. The west
    // wall is shape 0 and the north wall is shape 1, so "the first that overlaps" would push the
    // body along X -- out of the shallow overlap, still inside the deep one, one iteration spent.
    const CollisionWorld world = OneCell({
        Wall(Vector3(-1.1f, 1.5f, 0.0f), Vector3(0.1f, 1.5f, 4.0f)), // face at x = -1.0
        Wall(Vector3(0.0f, 1.5f, -1.1f), Vector3(4.0f, 1.5f, 0.1f)), // face at z = -1.0
    });
    BroadPhase broad;
    const Capsule body = Sphere(Vector3(-0.84f, 1.55f, -0.94f), 0.22f);

    const Overlap west = OverlapCapsuleObb(body, world.obbs[0]);
    const Overlap north = OverlapCapsuleObb(body, world.obbs[1]);
    ASSERT_TRUE(west.overlapped);
    ASSERT_TRUE(north.overlapped);
    EXPECT_NEAR(west.depth, 0.06f, 1e-5f);
    EXPECT_NEAR(north.depth, 0.16f, 1e-5f);

    const CellOverlap deepest = OverlapCell(world, world.cells[0], broad, body);
    ASSERT_TRUE(deepest.overlapped);
    EXPECT_EQ(deepest.shape, 1u);
    EXPECT_NEAR(deepest.depth, 0.16f, 1e-5f);
    EXPECT_NEAR(deepest.normal.Z, 1.0f, 1e-5f);
    EXPECT_EQ(deepest.tested, 2u);
}

TEST(DepenetrationTests, AMeshsDeepestTRIANGLEIsWhatCounts)
{
    // ONE mesh, two triangles: the upper at y = 0.10 and the lower at y = 0.0, the upper written
    // first. The body's centre is at y = 0.02 -- 0.08 m under the upper deck and 0.02 m above the
    // lower one -- so the deepest is the lower one, at 0.20 m, and the way out is UP.
    const CollisionWorld world = OneCell({}, {TwoDecks()});
    BroadPhase broad;
    const CellOverlap deepest =
        OverlapCell(world, world.cells[0], broad, Sphere(Vector3(0.0f, 0.02f, 0.0f), 0.22f));
    ASSERT_TRUE(deepest.overlapped);
    EXPECT_EQ(deepest.tested, 1u) << "one mesh is one shape";
    EXPECT_NEAR(deepest.depth, 0.20f, 1e-5f);
    // Not 0.14 m and not downwards, which is what the first triangle in the list would have said.
    EXPECT_NEAR(deepest.normal.Y, 1.0f, 1e-5f);
}

TEST(DepenetrationTests, ACellWithNothingNearbyOverlapsNothing)
{
    const CollisionWorld world = OneCell({Wall(Vector3(-1.1f, 1.5f, 0.0f), Vector3(0.1f, 1.5f, 4.0f))});
    BroadPhase broad;
    const CellOverlap overlap =
        OverlapCell(world, world.cells[0], broad, Sphere(Vector3(2.0f, 1.55f, 0.0f), 0.22f));
    EXPECT_FALSE(overlap.overlapped);
    EXPECT_EQ(overlap.shape, CellSweepHit::kNothing);
}

// ---------------------------------------------------------------------------------------------
// Four pushes of two centimetres
// ---------------------------------------------------------------------------------------------

TEST(DepenetrationTests, AClearBodyIsNotMovedAtAll)
{
    const CollisionWorld world = OneCell({Wall(Vector3(-1.1f, 1.5f, 0.0f), Vector3(0.1f, 1.5f, 4.0f))});
    BroadPhase broad;
    const Depenetration out =
        Depenetrate(world, world.cells[0], broad, Sphere(Vector3(2.0f, 1.55f, 0.0f), 0.22f));
    EXPECT_TRUE(out.resolved);
    EXPECT_EQ(out.iterations, 0);
    EXPECT_FLOAT_EQ(out.deepest, 0.0f);
    EXPECT_FLOAT_EQ(Length(out.offset), 0.0f);
}

TEST(DepenetrationTests, AShallowOverlapTakesAsManyTwoCentimetrePushesAsItNeeds)
{
    const CollisionWorld world = OneCell({Wall(Vector3(1.1f, 1.5f, 0.0f), Vector3(0.1f, 1.5f, 4.0f))});
    BroadPhase broad;
    // 0.03 m in. One push leaves 0.01 m, the second clears it: 0.04 m of travel to fix 0.03 m of
    // overlap, which is the price of a fixed step and the reason it is cheap.
    const Depenetration out =
        Depenetrate(world, world.cells[0], broad, Sphere(Vector3(0.81f, 1.55f, 0.0f), 0.22f));
    EXPECT_TRUE(out.resolved);
    EXPECT_EQ(out.iterations, 2);
    EXPECT_NEAR(out.deepest, 0.03f, 1e-5f);
    EXPECT_NEAR(out.offset.X, -2.0f * kDepenetrationStep, 1e-5f);
    EXPECT_NEAR(out.offset.Y, 0.0f, 1e-6f);
    EXPECT_NEAR(out.offset.Z, 0.0f, 1e-6f);
}

TEST(DepenetrationTests, ADeeplyBuriedBodyIsNOTTeleportedOutAndSaysSo)
{
    const CollisionWorld world = OneCell({Wall(Vector3(1.1f, 1.5f, 0.0f), Vector3(0.1f, 1.5f, 4.0f))});
    BroadPhase broad;
    // 0.12 m in, which would need six pushes. Four are allowed, so 0.08 m of it is undone and the
    // body is left 0.04 m inside with `resolved` false -- NOT moved 0.12 m, which from the middle
    // of a 0.10 m wall would have put it in the room beyond.
    const Depenetration out =
        Depenetrate(world, world.cells[0], broad, Sphere(Vector3(0.90f, 1.55f, 0.0f), 0.22f));
    EXPECT_FALSE(out.resolved);
    EXPECT_EQ(out.iterations, kDepenetrationIterations);
    EXPECT_NEAR(out.deepest, 0.12f, 1e-5f);
    EXPECT_NEAR(Length(out.offset), static_cast<float>(kDepenetrationIterations) * kDepenetrationStep, 1e-5f);
    EXPECT_NEAR(out.offset.X, -0.08f, 1e-5f);
}

TEST(DepenetrationTests, ACornerIsLeftWithNeitherWallStillOverlapping)
{
    // The point of pushing along the deepest normal, iteration by iteration rather than once: two
    // walls at right angles, shallow in both, and the body must end clear of BOTH.
    const CollisionWorld world = OneCell({
        Wall(Vector3(-1.1f, 1.5f, 0.0f), Vector3(0.1f, 1.5f, 4.0f)),
        Wall(Vector3(0.0f, 1.5f, -1.1f), Vector3(4.0f, 1.5f, 0.1f)),
    });
    BroadPhase broad;
    const Capsule body = Sphere(Vector3(-0.80f, 1.55f, -0.79f), 0.22f);
    const Depenetration out = Depenetrate(world, world.cells[0], broad, body);
    EXPECT_TRUE(out.resolved);
    EXPECT_GE(out.iterations, 2);
    EXPECT_LE(out.iterations, kDepenetrationIterations);

    Capsule moved = body;
    moved.centre =
        Vector3(body.centre.X + out.offset.X, body.centre.Y + out.offset.Y, body.centre.Z + out.offset.Z);
    // Clear of both, where "clear" means NOT PENETRATING. Resting against a wall is touching it,
    // and a push-out that insisted on daylight would shove a body off every wall it leaned on
    // (`kContactTolerance`, found by `HOUSE-00555`).
    EXPECT_LE(OverlapCapsuleObb(moved, world.obbs[0]).depth, kContactTolerance);
    EXPECT_LE(OverlapCapsuleObb(moved, world.obbs[1]).depth, kContactTolerance);
}

TEST(DepenetrationTests, AFloorPushesTheBodyUpOntoIt)
{
    const CollisionWorld world = OneCell({}, {Floor(0.0f)});
    BroadPhase broad;
    // Standing 0.02 m too low: the lower cap's centre is at 0.28 and needs to be at 0.30.
    // ONE push of 0.02 m is exactly that, and the second is not taken -- what is left after it is
    // contact, not penetration.
    const Capsule body{Vector3(0.0f, 0.88f, 0.0f), kBodyHalfHeight, kBodyRadius};
    const Depenetration out = Depenetrate(world, world.cells[0], broad, body);
    EXPECT_TRUE(out.resolved);
    EXPECT_EQ(out.iterations, 1);
    EXPECT_NEAR(out.deepest, 0.02f, 1e-5f);
    EXPECT_NEAR(out.offset.Y, kDepenetrationStep, 1e-5f);
    EXPECT_NEAR(out.offset.X, 0.0f, 1e-6f);
    EXPECT_NEAR(out.offset.Z, 0.0f, 1e-6f);
}

// ---------------------------------------------------------------------------------------------
// The real house
// ---------------------------------------------------------------------------------------------

TEST(DepenetrationTests, TheRealHouseIsClearWhereABodyStandsAndRecoversWhereItIsPushedIn)
{
    // §49.5 promises a body *"never ends a frame inside static geometry"*. This is the part of that
    // promise this task can keep: drop §70.5's 0.62 m body onto the middle of every cell of the
    // real house, and it lands CLEAR; shove it 0.03 m into whatever is around it, and four pushes
    // of 0.02 m bring it back out.
    //
    // The standing height is found by dropping rather than assumed from the cell's bounds. A
    // cell's `bounds.Min.Y` is the bottom of its floor SLAB, not the surface a body stands on --
    // 0.35 m lower in the basement -- and a test that assumed otherwise would have measured the
    // depenetration of a body buried in its own floor and called it a pass.
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
    std::size_t stood = 0;
    std::size_t crowded = 0;
    std::size_t nudged = 0;
    std::size_t unresolved = 0;
    for (const auto& cell : world->cells)
    {
        if (cell.shapes.empty() || cell.nx == 0u || cell.nz == 0u)
        {
            continue;
        }
        const float midX = cell.originX + static_cast<float>(cell.nx) * 0.5f;
        const float midZ = cell.originZ + static_cast<float>(cell.nz) * 0.5f;
        const float height = cell.bounds.Max.Y - cell.bounds.Min.Y;
        const float stand = kBodyHalfHeight + kBodyRadius;
        if (height < 2.0f * stand)
        {
            continue; // a crawl space or a duct: no room for a person to stand up in it
        }

        // A 50 mm pebble dropped down the middle of the cell finds the floor. It is small on
        // purpose: a body-sized probe started at head height is inside the ceiling slab in most
        // rooms, and what is wanted here is the surface, not a body already in trouble.
        const float pebble = 0.05f;
        const Capsule falling =
            Sphere(Vector3(midX, (cell.bounds.Min.Y + cell.bounds.Max.Y) * 0.5f, midZ), pebble);
        if (OverlapCell(*world, cell, broad, falling).overlapped)
        {
            continue; // solid at mid-height down the middle: a chimney, a stack, a stair
        }
        const CellSweepHit landing = SweepCell(*world, cell, broad, falling, Vector3(0.0f, -height, 0.0f));
        if (!landing.hit)
        {
            continue; // nothing under the middle of this cell to stand on
        }
        const float floorY = falling.centre.Y - height * landing.time - pebble;
        if (floorY + 2.0f * stand > cell.bounds.Max.Y)
        {
            continue; // the floor sits high enough in the cell that a body would not fit above it
        }
        // 5 mm of daylight: the contact itself is the boundary, and the boundary belongs to the
        // overlap. A body is stood ON a floor here, not welded to it.
        Capsule body{Vector3(midX, floorY + stand + 0.005f, midZ), kBodyHalfHeight, kBodyRadius};

        const CellOverlap standing = OverlapCell(*world, cell, broad, body);
        if (standing.overlapped)
        {
            // A staircase is the case this catches: a 50 mm pebble lands on one tread and a
            // 0.62 m body clips the two above it. That is a real place a real body gets to, and
            // §49.3's step 4 -- StepUp -- is what handles it, not step 5. So it is counted and
            // left, and the count is asserted small so it cannot quietly become "most of the
            // house".
            ++crowded;
            continue;
        }
        ++stood;

        // Now shove it 0.03 m into the nearest thing there is, from the outside in, and require
        // the push-out to undo it. The direction is found by sweeping: whatever the sweep stops
        // against is what to bury the body in.
        for (int i = 0; i < 4; ++i)
        {
            const float angle = static_cast<float>(i) * 3.1415927f / 2.0f;
            const float dx = std::cos(angle);
            const float dz = std::sin(angle);
            const CellSweepHit hit =
                SweepCell(*world, cell, broad, body, Vector3(dx * 4.0f, 0.0f, dz * 4.0f));
            if (!hit.hit || hit.startedInside)
            {
                continue;
            }
            Capsule buried = body;
            const float travel = hit.time * 4.0f + 0.03f;
            buried.centre = Vector3(body.centre.X + dx * travel, body.centre.Y, body.centre.Z + dz * travel);
            const CellOverlap in = OverlapCell(*world, cell, broad, buried);
            if (!in.overlapped)
            {
                continue; // the sweep stopped against a corner it only grazed; not a burial
            }
            ++nudged;
            const Depenetration out = Depenetrate(*world, cell, broad, buried);
            EXPECT_GT(out.deepest, 0.0f) << cell.id;
            EXPECT_LE(Length(out.offset),
                      static_cast<float>(kDepenetrationIterations) * kDepenetrationStep + 1e-4f)
                << cell.id << ": a push-out that moves further than four steps is a teleport";
            if (!out.resolved)
            {
                ++unresolved;
            }
        }
    }
    ASSERT_GT(stood, 40u) << "only " << stood << " cells had room to stand in";
    EXPECT_LE(crowded, 4u) << crowded
                           << " cells put a body inside something just by standing it on their "
                              "floor; a stair or two is expected, a house is not";
    ASSERT_GT(nudged, 80u) << "only " << nudged << " burials were arranged";
    // 0.03 m needs two of the four pushes, so every one of these must come out. A failure here is
    // a body that would still be inside a wall at the end of a frame.
    EXPECT_EQ(unresolved, 0u) << unresolved << " of " << nudged << " burials survived four pushes";
}
