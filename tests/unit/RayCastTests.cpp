// SPDX-License-Identifier: MIT
//
// `HOUSE-00546`. §49.1 names `RayCast` as one of the five calls the whole physics interface is,
// and §50.1 is what asks for it: *"if occluded by static geometry between eye and hit: continue
// -- one short raycast"*. A light switch behind a wall must not be reachable through the wall.
//
// Three surfaces to hit and one thing they must agree on: the normal faces back along the ray.
// A wall is opaque from both sides and a triangle's winding is for drawing (§14), so which way a
// surface faces is the ray's question rather than the geometry's.
#include <cmath>
#include <memory>
#include <string>

#include <gtest/gtest.h>

#include "System/IO/FileAccess.hpp"
#include "System/IO/FileMode.hpp"
#include "System/IO/FileStream.hpp"

#include "cnahouse/physics/BroadPhase.hpp"
#include "cnahouse/physics/CollisionLoader.hpp"
#include "cnahouse/physics/RayCast.hpp"

namespace
{
    using cnahouse::physics::BroadPhase;
    using cnahouse::physics::CollisionCell;
    using cnahouse::physics::CollisionKind;
    using cnahouse::physics::CollisionLoader;
    using cnahouse::physics::CollisionMesh;
    using cnahouse::physics::CollisionObb;
    using cnahouse::physics::CollisionTerrain;
    using cnahouse::physics::CollisionWorld;
    using cnahouse::physics::RayCastCell;
    using cnahouse::physics::RayCastObb;
    using cnahouse::physics::RayCastTerrain;
    using cnahouse::physics::RayCastTriangle;
    using cnahouse::physics::RayHit;
    using Microsoft::Xna::Framework::BoundingBox;
    using Microsoft::Xna::Framework::Vector3;

    /// The unit box at the origin: half-extents 1, no yaw.
    CollisionObb UnitBox(float yaw = 0.0f)
    {
        CollisionObb obb;
        obb.halfExtents = Vector3(1.0f, 1.0f, 1.0f);
        obb.yaw = yaw;
        obb.kind = CollisionKind::Wall;
        return obb;
    }

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

    CollisionTerrain Field(std::uint32_t samplesX, std::uint32_t samplesZ, std::vector<float> heights)
    {
        CollisionTerrain terrain;
        terrain.present = true;
        terrain.samplesX = samplesX;
        terrain.samplesZ = samplesZ;
        terrain.step = 1.0f;
        terrain.heights = std::move(heights);
        terrain.materials = {0u};
        terrain.materialIndex.assign(static_cast<std::size_t>(samplesX) * samplesZ, 0u);
        return terrain;
    }

    CollisionWorld OneCell(std::vector<CollisionObb> obbs,
                           std::vector<CollisionMesh> meshes = {},
                           CollisionTerrain terrain = {})
    {
        CollisionWorld world;
        world.gridCell = 1.0f;
        world.surfaces = {"plaster", "grass"};
        world.obbs = std::move(obbs);
        world.meshes = std::move(meshes);
        world.terrain = std::move(terrain);

        CollisionCell cell;
        cell.id = "L0_TEST";
        cell.bounds = BoundingBox(Vector3(-8.0f, -8.0f, -8.0f), Vector3(8.0f, 8.0f, 8.0f));
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

    void ExpectNormal(const RayHit& hit, float x, float y, float z, const char* what)
    {
        EXPECT_NEAR(hit.normal.X, x, 1e-4f) << what;
        EXPECT_NEAR(hit.normal.Y, y, 1e-4f) << what;
        EXPECT_NEAR(hit.normal.Z, z, 1e-4f) << what;
    }

} // namespace

// ---------------------------------------------------------------------------------------------
// Boxes
// ---------------------------------------------------------------------------------------------

TEST(RayCastTests, TheSixFacesAreMetSquareOnAtTheirOwnDistances)
{
    // From 3 m out along each axis: the face is at 1, so the distance is 2 every time, and the
    // normal is that face's own. Six cases and not one, because a sign error in the slab test
    // shows on three of them and not on the other three.
    const CollisionObb box = UnitBox();
    const float d[6][3] = {{1, 0, 0}, {-1, 0, 0}, {0, 1, 0}, {0, -1, 0}, {0, 0, 1}, {0, 0, -1}};
    for (const auto& v : d)
    {
        const Vector3 direction(v[0], v[1], v[2]);
        const Vector3 origin(-v[0] * 3.0f, -v[1] * 3.0f, -v[2] * 3.0f);
        const RayHit hit = RayCastObb(origin, direction, 10.0f, box);
        ASSERT_TRUE(hit.hit) << v[0] << v[1] << v[2];
        EXPECT_NEAR(hit.distance, 2.0f, 1e-4f);
        ExpectNormal(hit, -v[0], -v[1], -v[2], "the face the ray came at");
        EXPECT_NEAR(hit.point.X, origin.X + direction.X * 2.0f, 1e-4f);
        EXPECT_NEAR(hit.point.Y, origin.Y + direction.Y * 2.0f, 1e-4f);
        EXPECT_NEAR(hit.point.Z, origin.Z + direction.Z * 2.0f, 1e-4f);
    }
}

TEST(RayCastTests, ARayThatStopsShortOfTheBoxMissesIt)
{
    // `maxDistance` is the whole of §50.1's question -- "is anything between the eye and a thing
    // 2.6 m away" -- so a wall at 3 m is not an answer to it.
    const CollisionObb box = UnitBox();
    EXPECT_FALSE(RayCastObb(Vector3(-3.0f, 0.0f, 0.0f), Vector3(1, 0, 0), 1.9f, box).hit);
    EXPECT_TRUE(RayCastObb(Vector3(-3.0f, 0.0f, 0.0f), Vector3(1, 0, 0), 2.1f, box).hit);
}

TEST(RayCastTests, ARayBesideTheBoxAndOneMovingAwayBothMiss)
{
    const CollisionObb box = UnitBox();
    EXPECT_FALSE(RayCastObb(Vector3(-3.0f, 1.5f, 0.0f), Vector3(1, 0, 0), 10.0f, box).hit)
        << "it passed 0.5 m over the top";
    EXPECT_FALSE(RayCastObb(Vector3(-3.0f, 0.0f, 0.0f), Vector3(-1, 0, 0), 10.0f, box).hit)
        << "a box behind the ray is not in front of it";
}

TEST(RayCastTests, AnEyeInsideAWallIsOccludedByIt)
{
    // §50.1 asks whether anything is BETWEEN the eye and the thing. A ray starting inside a box
    // is inside it for its whole length, so the honest answer is "yes, at zero". Reporting a miss
    // would let a player pick a switch through the wall they are standing in.
    const CollisionObb box = UnitBox();
    const RayHit hit = RayCastObb(Vector3(0.2f, 0.0f, 0.0f), Vector3(1, 0, 0), 10.0f, box);
    ASSERT_TRUE(hit.hit);
    EXPECT_FLOAT_EQ(hit.distance, 0.0f);
    // The nearest way out is +x, and the normal is turned to face the ray it stopped.
    ExpectNormal(hit, -1.0f, 0.0f, 0.0f, "the normal of a ray starting inside");
}

TEST(RayCastTests, AYawedBoxIsMetOnItsOwnFaceAndTheNormalComesBackInWorldAxes)
{
    // A box turned 90° about +Y. Its local +x face now looks along +z, so a ray along -z meets it
    // at 2 m with a normal of (0, 0, 1) -- and a normal left in the box's frame would say (1,0,0),
    // which is the wall's own axis rather than the world's.
    const CollisionObb box = UnitBox(1.5707963f);
    const RayHit hit = RayCastObb(Vector3(0.0f, 0.0f, 3.0f), Vector3(0, 0, -1), 10.0f, box);
    ASSERT_TRUE(hit.hit);
    EXPECT_NEAR(hit.distance, 2.0f, 1e-4f);
    ExpectNormal(hit, 0.0f, 0.0f, 1.0f, "a yawed box's normal in world axes");
}

TEST(RayCastTests, AYawedSlabIsMetOnItsDiagonalFace)
{
    // Turned 45°, a 1 x 1 slab presents its face at 45°, and the distance is the analytic one:
    // the face's plane is `x + z = √2 / 2 · 1`, so a ray along -x from 3 stops at 3 - 0.7071.
    CollisionObb slab = UnitBox(0.7853982f);
    slab.halfExtents = Vector3(1.0f, 1.0f, 1.0f);
    const RayHit hit = RayCastObb(Vector3(3.0f, 0.0f, 0.0f), Vector3(-1, 0, 0), 10.0f, slab);
    ASSERT_TRUE(hit.hit);
    EXPECT_NEAR(hit.distance, 3.0f - std::sqrt(2.0f), 1e-4f);
    ExpectNormal(hit, 0.70711f, 0.0f, -0.70711f, "the 45 degree face");
}

// ---------------------------------------------------------------------------------------------
// Triangles
// ---------------------------------------------------------------------------------------------

TEST(RayCastTests, ATriangleIsHitFromBothSidesAtTheSameDistance)
{
    // The winding is for drawing (§14) and a wall stops a ray from either side. The normal is the
    // one that faces the ray, so the two answers differ in exactly that and in nothing else.
    const Vector3 a(-1.0f, 0.0f, -1.0f);
    const Vector3 b(1.0f, 0.0f, -1.0f);
    const Vector3 c(0.0f, 0.0f, 1.0f);

    const RayHit above = RayCastTriangle(Vector3(0, 2, 0), Vector3(0, -1, 0), 10.0f, a, b, c);
    ASSERT_TRUE(above.hit);
    EXPECT_NEAR(above.distance, 2.0f, 1e-5f);
    ExpectNormal(above, 0.0f, 1.0f, 0.0f, "hit from above");

    const RayHit below = RayCastTriangle(Vector3(0, -2, 0), Vector3(0, 1, 0), 10.0f, a, b, c);
    ASSERT_TRUE(below.hit);
    EXPECT_NEAR(below.distance, 2.0f, 1e-5f);
    ExpectNormal(below, 0.0f, -1.0f, 0.0f, "hit from below");
}

TEST(RayCastTests, PastTheEdgeOfATriangleIsPastIt)
{
    const Vector3 a(-1.0f, 0.0f, -1.0f);
    const Vector3 b(1.0f, 0.0f, -1.0f);
    const Vector3 c(0.0f, 0.0f, 1.0f);
    // Inside the triangle's bounding box but outside the triangle: past the sloping edge.
    EXPECT_FALSE(RayCastTriangle(Vector3(0.9f, 2.0f, 0.9f), Vector3(0, -1, 0), 10.0f, a, b, c).hit);
    EXPECT_TRUE(RayCastTriangle(Vector3(0.0f, 2.0f, 0.9f), Vector3(0, -1, 0), 10.0f, a, b, c).hit);
}

TEST(RayCastTests, ARayInTheTrianglesOwnPlaneIsNotStoppedByIt)
{
    // Edge-on. There is no crossing to report, and reporting one would make every floor a wall to
    // a ray travelling along it.
    const Vector3 a(-1.0f, 0.0f, -1.0f);
    const Vector3 b(1.0f, 0.0f, -1.0f);
    const Vector3 c(0.0f, 0.0f, 1.0f);
    EXPECT_FALSE(RayCastTriangle(Vector3(-3, 0, 0), Vector3(1, 0, 0), 10.0f, a, b, c).hit);
}

TEST(RayCastTests, ATriangleWithNoAreaIsNotASurface)
{
    EXPECT_FALSE(
        RayCastTriangle(
            Vector3(0, 2, 0), Vector3(0, -1, 0), 10.0f, Vector3(-1, 0, 0), Vector3(0, 0, 0), Vector3(1, 0, 0))
            .hit);
}

// ---------------------------------------------------------------------------------------------
// The ground
// ---------------------------------------------------------------------------------------------

TEST(RayCastTests, ARayDownOntoALawnLandsOnIt)
{
    const CollisionTerrain terrain = Field(4u, 4u, std::vector<float>(16u, 0.35f));
    const RayHit hit = RayCastTerrain(terrain, Vector3(1.5f, 3.0f, 1.5f), Vector3(0, -1, 0), 10.0f);
    ASSERT_TRUE(hit.hit);
    EXPECT_NEAR(hit.distance, 2.65f, 1e-4f);
    EXPECT_NEAR(hit.point.Y, 0.35f, 1e-4f);
    ExpectNormal(hit, 0.0f, 1.0f, 0.0f, "flat ground");
    EXPECT_EQ(hit.shape, RayHit::kTerrain);
}

TEST(RayCastTests, ARayThatNeverReachesTheLotMissesWithoutWalkingIt)
{
    const CollisionTerrain terrain = Field(4u, 4u, std::vector<float>(16u, 0.0f));
    // Parallel to the field and 5 m above it.
    EXPECT_FALSE(RayCastTerrain(terrain, Vector3(-5, 5, 1.5f), Vector3(1, 0, 0), 20.0f).hit);
    // Straight down, but 10 m off the side of the lot.
    EXPECT_FALSE(RayCastTerrain(terrain, Vector3(-10, 5, 1.5f), Vector3(0, -1, 0), 20.0f).hit);
    // Pointing away from it.
    EXPECT_FALSE(RayCastTerrain(terrain, Vector3(1.5f, 5, 1.5f), Vector3(0, 1, 0), 20.0f).hit);
}

TEST(RayCastTests, AGrazingRayClearsTheNEARBankAndLandsOnTheFARONE)
{
    // The case the square-by-square walk exists for. A valley -- down from x = 0 to x = 1, flat to
    // x = 2, up again to x = 3 -- with a ray that enters low over the near bank, passes over the
    // floor, and meets the far one. A search that took whichever square it happened to test first,
    // or that stopped at the first square whose triangles are anywhere near the ray, lands in the
    // wrong place.
    std::vector<float> heights;
    for (int row = 0; row < 4; ++row)
    {
        for (const float h : {1.0f, 0.0f, 0.0f, 1.0f})
        {
            heights.push_back(h);
        }
    }
    const CollisionTerrain terrain = Field(4u, 4u, std::move(heights));

    // From above the west bank, sloping down at 1:4 towards the east one.
    const float dx = 4.0f / std::sqrt(17.0f);
    const float dy = -1.0f / std::sqrt(17.0f);
    const RayHit hit = RayCastTerrain(terrain, Vector3(0.5f, 1.0f, 1.5f), Vector3(dx, dy, 0.0f), 10.0f);
    ASSERT_TRUE(hit.hit);
    // The west bank at x = 0.5 is at 0.5 and the ray starts 0.5 above it, so it clears it; it
    // meets the east bank, which rises from x = 2.
    EXPECT_GT(hit.point.X, 2.0f) << "it stopped on the near bank at x = " << hit.point.X;
    EXPECT_LT(hit.point.X, 3.0f);
    // ...and the point really is ON the ground there: the east bank rises 1 m per metre from 0.
    EXPECT_NEAR(hit.point.Y, hit.point.X - 2.0f, 1e-3f);
    EXPECT_NEAR(hit.distance * dy + 1.0f, hit.point.Y, 1e-3f) << "the distance and the point disagree";
}

TEST(RayCastTests, TheGroundWalkStepsInZAsWellAsInX)
{
    // A ridge running across the ray rather than along it: flat until z = 1, rising to 1 m by
    // z = 2. A walk that only ever stepped in x would stay in the near row for ever and report
    // clear ground where there is a bank.
    std::vector<float> heights;
    for (int row = 0; row < 4; ++row)
    {
        for (int column = 0; column < 4; ++column)
        {
            heights.push_back(row <= 1 ? 0.0f : 1.0f);
        }
    }
    const CollisionTerrain terrain = Field(4u, 4u, std::move(heights));

    // Along +z at half a metre up: the bank rises 1 m over the metre from z = 1, so it crosses
    // y = 0.5 at z = 1.5.
    const RayHit alongZ = RayCastTerrain(terrain, Vector3(1.5f, 0.5f, 0.2f), Vector3(0, 0, 1), 4.0f);
    ASSERT_TRUE(alongZ.hit) << "the ray walked past a bank it went straight into";
    EXPECT_NEAR(alongZ.point.Z, 1.5f, 1e-3f);
    EXPECT_NEAR(alongZ.distance, 1.3f, 1e-3f);

    // And diagonally, where the walk has to alternate between the two axes.
    const float inv = 1.0f / std::sqrt(2.0f);
    const RayHit diagonal = RayCastTerrain(terrain, Vector3(0.2f, 0.5f, 0.2f), Vector3(inv, 0.0f, inv), 6.0f);
    ASSERT_TRUE(diagonal.hit);
    EXPECT_NEAR(diagonal.point.Z, 1.5f, 1e-3f);
    EXPECT_NEAR(diagonal.point.X, diagonal.point.Z, 1e-3f) << "it left the diagonal";
}

TEST(RayCastTests, ARayEnteringTheLotFromOutsideStillFindsIt)
{
    const CollisionTerrain terrain = Field(4u, 4u, std::vector<float>(16u, 0.0f));
    // Starts 5 m west of the field, descending, and must pick it up at the edge.
    const float dx = 5.0f / std::sqrt(26.0f);
    const float dy = -1.0f / std::sqrt(26.0f);
    const RayHit hit = RayCastTerrain(terrain, Vector3(-5.0f, 1.0f, 1.5f), Vector3(dx, dy, 0.0f), 40.0f);
    ASSERT_TRUE(hit.hit);
    EXPECT_NEAR(hit.point.Y, 0.0f, 1e-3f);
    EXPECT_GE(hit.point.X, 0.0f);
    EXPECT_LE(hit.point.X, 3.0f);
}

// ---------------------------------------------------------------------------------------------
// A whole cell
// ---------------------------------------------------------------------------------------------

TEST(RayCastTests, TheNEARESTThingInTheCellIsWhatStopsTheRay)
{
    // Two walls in a line, and the far one listed FIRST. §50.1 asks what is between the eye and a
    // thing; the shape a bucket happened to list first is not an answer to that.
    const CollisionWorld world = OneCell({
        Box(Vector3(4.0f, 0.0f, 0.0f), Vector3(0.1f, 2.0f, 2.0f), CollisionKind::Wall, 0u),
        Box(Vector3(2.0f, 0.0f, 0.0f), Vector3(0.1f, 2.0f, 2.0f), CollisionKind::Wall, 1u),
    });
    BroadPhase broad;
    const RayHit hit = RayCastCell(world, world.cells[0], broad, Vector3(0, 0, 0), Vector3(1, 0, 0), 10.0f);
    ASSERT_TRUE(hit.hit);
    EXPECT_NEAR(hit.distance, 1.9f, 1e-4f);
    EXPECT_EQ(hit.shape, 1u) << "it stopped at the far wall";
    EXPECT_EQ(world.SurfaceName(hit.surface), "grass") << "the surface came from the wrong shape";
}

TEST(RayCastTests, TheOcclusionTestSectionFiftyPointOneAsksFor)
{
    // A switch 2.2 m away with a partition between. The reach is §50.1's 2.6 m sphere.
    const CollisionWorld world =
        OneCell({Box(Vector3(1.0f, 0.0f, 0.0f), Vector3(0.05f, 2.0f, 2.0f), CollisionKind::Wall)});
    BroadPhase broad;
    const Vector3 eye(0.0f, 0.0f, 0.0f);
    const Vector3 forward(1, 0, 0);

    const RayHit blocked = RayCastCell(world, world.cells[0], broad, eye, forward, 2.6f);
    ASSERT_TRUE(blocked.hit);
    EXPECT_LT(blocked.distance, 2.2f) << "the wall is nearer than the switch, so the switch is out";

    // Take the wall away and the same ray reaches past where the switch is.
    const CollisionWorld clear = OneCell({});
    EXPECT_FALSE(RayCastCell(clear, clear.cells[0], broad, eye, forward, 2.6f).hit);
}

TEST(RayCastTests, TheGroundIsPartOfWhatACellRayCanHit)
{
    // §49.2: exterior collision is the height field PLUS the OBBs. A ray down an outdoor cell
    // that ignored the ground would report clear sky under the player's feet.
    const CollisionWorld world = OneCell({}, {}, Field(4u, 4u, std::vector<float>(16u, 0.25f)));
    BroadPhase broad;
    const RayHit hit =
        RayCastCell(world, world.cells[0], broad, Vector3(1.5f, 2.0f, 1.5f), Vector3(0, -1, 0), 10.0f);
    ASSERT_TRUE(hit.hit);
    EXPECT_EQ(hit.shape, RayHit::kTerrain);
    EXPECT_NEAR(hit.distance, 1.75f, 1e-4f);

    // ...and a shape BELOW the ground does not win over it.
    const CollisionWorld buried =
        OneCell({Box(Vector3(1.5f, -1.0f, 1.5f), Vector3(0.5f, 0.1f, 0.5f), CollisionKind::Floor)},
                {},
                Field(4u, 4u, std::vector<float>(16u, 0.25f)));
    const RayHit onto =
        RayCastCell(buried, buried.cells[0], broad, Vector3(1.5f, 2.0f, 1.5f), Vector3(0, -1, 0), 10.0f);
    ASSERT_TRUE(onto.hit);
    EXPECT_EQ(onto.shape, RayHit::kTerrain) << "it went through the ground to the slab beneath";
}

// ---------------------------------------------------------------------------------------------
// The real house
// ---------------------------------------------------------------------------------------------

TEST(RayCastTests, EveryRayInTheRealHouseReportsSomethingCoherent)
{
    // §50.1's own query, over the whole house: a 2.6 m ray from eye height in six directions from
    // the middle of every cell. Whatever each one hits, the answer has to hold together -- the
    // point is ON the ray, at the distance reported, within reach, and the normal is unit and
    // faces back.
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
    std::size_t cast = 0;
    std::size_t blocked = 0;
    const Vector3 directions[6] = {Vector3(1, 0, 0),
                                   Vector3(-1, 0, 0),
                                   Vector3(0, 0, 1),
                                   Vector3(0, 0, -1),
                                   Vector3(0, 1, 0),
                                   Vector3(0, -1, 0)};
    for (const auto& cell : world->cells)
    {
        if (cell.shapes.empty() || cell.nx == 0u || cell.nz == 0u)
        {
            continue;
        }
        const float midX = cell.originX + static_cast<float>(cell.nx) * 0.5f;
        const float midZ = cell.originZ + static_cast<float>(cell.nz) * 0.5f;
        const float eyeY = cell.bounds.Min.Y + 1.68f; // §43.1's eye height
        if (eyeY > cell.bounds.Max.Y)
        {
            continue;
        }
        const Vector3 eye(midX, eyeY, midZ);
        for (const Vector3& direction : directions)
        {
            const RayHit hit = RayCastCell(*world, cell, broad, eye, direction, 2.6f);
            ++cast;
            if (!hit.hit)
            {
                continue;
            }
            ++blocked;
            EXPECT_GE(hit.distance, 0.0f) << cell.id;
            EXPECT_LE(hit.distance, 2.6f + 1e-4f) << cell.id << ": it reached past the ray's end";
            EXPECT_NEAR(hit.point.X, eye.X + direction.X * hit.distance, 1e-3f) << cell.id;
            EXPECT_NEAR(hit.point.Y, eye.Y + direction.Y * hit.distance, 1e-3f) << cell.id;
            EXPECT_NEAR(hit.point.Z, eye.Z + direction.Z * hit.distance, 1e-3f) << cell.id;
            const float length = std::sqrt(hit.normal.X * hit.normal.X + hit.normal.Y * hit.normal.Y +
                                           hit.normal.Z * hit.normal.Z);
            EXPECT_NEAR(length, 1.0f, 1e-3f) << cell.id;
            const float facing =
                hit.normal.X * direction.X + hit.normal.Y * direction.Y + hit.normal.Z * direction.Z;
            EXPECT_LE(facing, 1e-4f) << cell.id << ": the normal faced away from the ray";
            if (hit.shape != RayHit::kTerrain)
            {
                EXPECT_LT(hit.shape, world->ShapeCount()) << cell.id;
            }
        }
    }
    ASSERT_GT(cast, 300u);
    // A room has a floor, a ceiling and walls, so most of these must find something; a house in
    // which they did not would mean the ray is missing geometry it is standing in the middle of.
    EXPECT_GT(blocked, cast / 2) << blocked << " of " << cast << " rays hit anything";
}
