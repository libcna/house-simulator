// SPDX-License-Identifier: MIT
//
// `HOUSE-00553`. §11.5's ground: *"a height field on a 1.0 m grid ... collision uses the same
// height field (bilinear sample + a triangle test for slopes > 20°)"*.
//
// The two answers are the point of the task. A bilinear patch over four samples is smooth and
// cheap and is what a body walking a lawn should follow -- the alternative is a visible crease
// down every metre of it. It is also NOT the surface the renderer draws, which is two triangles,
// and on a steep square they disagree by up to a quarter of the height difference across it. So
// bilinear where it is flat enough not to matter, and the triangles themselves where it is not.
#include <cmath>
#include <memory>
#include <string>

#include <gtest/gtest.h>

#include "System/IO/FileAccess.hpp"
#include "System/IO/FileMode.hpp"
#include "System/IO/FileStream.hpp"

#include "cnahouse/physics/CollisionLoader.hpp"
#include "cnahouse/physics/Terrain.hpp"

namespace
{
    using cnahouse::physics::Capsule;
    using cnahouse::physics::CollisionLoader;
    using cnahouse::physics::CollisionTerrain;
    using cnahouse::physics::CollisionWorld;
    using cnahouse::physics::kTerrainTriangleSlopeDegrees;
    using cnahouse::physics::Overlap;
    using cnahouse::physics::OverlapCapsuleTerrain;
    using cnahouse::physics::Sphere;
    using cnahouse::physics::SweepCapsuleTerrain;
    using cnahouse::physics::SweepHit;
    using cnahouse::physics::TerrainAt;
    using cnahouse::physics::TerrainSample;
    using Microsoft::Xna::Framework::Vector3;

    constexpr float kBodyRadius = 0.30F;
    constexpr float kBodyHalfHeight = 0.60F;

    /// A field of @p samplesX × @p samplesZ on a 1 m grid at the origin, with the heights given
    /// row-major (z outer). One material, `grass`.
    CollisionTerrain Field(std::uint32_t samplesX, std::uint32_t samplesZ, std::vector<float> heights)
    {
        CollisionTerrain terrain;
        terrain.present = true;
        terrain.samplesX = samplesX;
        terrain.samplesZ = samplesZ;
        terrain.originX = 0.0F;
        terrain.originZ = 0.0F;
        terrain.step = 1.0F;
        terrain.heights = std::move(heights);
        terrain.materials = {0u};
        terrain.materialIndex.assign(static_cast<std::size_t>(samplesX) * samplesZ, 0u);
        return terrain;
    }

    /// A perfectly flat field at y = @p height, 5 x 5 m.
    CollisionTerrain Flat(float height)
    {
        return Field(6u, 6u, std::vector<float>(36u, height));
    }

    /// A field tilted @p degrees about the z axis, rising along +x.
    CollisionTerrain Ramp(float degrees)
    {
        const float slope = std::tan(degrees * 3.14159265F / 180.0F);
        std::vector<float> heights;
        for (std::uint32_t z = 0; z < 6u; ++z)
        {
            for (std::uint32_t x = 0; x < 6u; ++x)
            {
                heights.push_back(static_cast<float>(x) * slope);
            }
        }
        return Field(6u, 6u, std::move(heights));
    }

} // namespace

/// Where a capsule's centre sits when it rests on ground of height @p height and normal @p n.
///
/// **Not `height + halfHeight + radius`.** A capsule on a slope touches it UPHILL of its centre,
/// so the lower cap's centre is `radius` from the plane along the NORMAL, which is `radius / n.Y`
/// above the ground measured vertically. On the 16.7° squares below that is 13 mm, and on a 63°
/// one it is 0.36 m. Whatever reads the ground to stand a body on it -- `HOUSE-00548`'s ground
/// probe next -- needs this and not the flat-ground shortcut.
float RestingCentre(float height, const Vector3& n, float halfHeight, float radius)
{
    return height + radius / n.Y + halfHeight;
}

TEST(TerrainTests, TheCutIsTwentyDegrees)
{
    EXPECT_FLOAT_EQ(kTerrainTriangleSlopeDegrees, 20.0F);
}

TEST(TerrainTests, AFlatLawnIsFlatEverywhereAndItsNormalIsUp)
{
    const CollisionTerrain terrain = Flat(0.35F);
    for (const float x : {0.0F, 0.5F, 2.25F, 5.0F})
    {
        for (const float z : {0.0F, 1.75F, 5.0F})
        {
            const TerrainSample sample = TerrainAt(terrain, x, z);
            EXPECT_TRUE(sample.over) << x << ", " << z;
            EXPECT_FLOAT_EQ(sample.height, 0.35F) << x << ", " << z;
            EXPECT_NEAR(sample.normal.Y, 1.0F, 1e-6F);
            EXPECT_FALSE(sample.steep);
        }
    }
}

TEST(TerrainTests, OffTheLotIsTheEdgeOfItAndSaysSo)
{
    // Clamped, not extrapolated: a lot does not continue for ever, and §64's playable boundary is
    // what stops a body walking off it. `over` is how a caller tells the difference.
    const CollisionTerrain terrain = Ramp(10.0F);
    const TerrainSample inside = TerrainAt(terrain, 5.0F, 2.0F);
    const TerrainSample outside = TerrainAt(terrain, 40.0F, 2.0F);
    EXPECT_TRUE(inside.over);
    EXPECT_FALSE(outside.over);
    EXPECT_FLOAT_EQ(outside.height, inside.height) << "the ground continued uphill for ever";
}

TEST(TerrainTests, TheHeightAtASampleIsThatSampleAndTheGroundIsContinuous)
{
    // Whatever else it does, the surface has to pass through the samples it is made of, and it
    // has to have no step in it -- a body walking a metre must not drop into a crack at every
    // square boundary. Two squares side by side, sampled along the seam between them.
    const CollisionTerrain terrain = Field(3u, 2u, {0.0F, 1.0F, 0.5F, 2.0F, 2.0F, 1.5F});
    EXPECT_NEAR(TerrainAt(terrain, 0.0F, 0.0F).height, 0.0F, 1e-5F);
    EXPECT_NEAR(TerrainAt(terrain, 1.0F, 0.0F).height, 1.0F, 1e-5F);
    EXPECT_NEAR(TerrainAt(terrain, 2.0F, 0.0F).height, 0.5F, 1e-5F);
    EXPECT_NEAR(TerrainAt(terrain, 0.0F, 1.0F).height, 2.0F, 1e-5F);
    EXPECT_NEAR(TerrainAt(terrain, 2.0F, 1.0F).height, 1.5F, 1e-5F);

    float previous = TerrainAt(terrain, 0.0F, 0.5F).height;
    for (float x = 0.01F; x <= 2.0F; x += 0.01F)
    {
        const float height = TerrainAt(terrain, x, 0.5F).height;
        EXPECT_LT(std::fabs(height - previous), 0.05F) << "a step in the ground at x = " << x;
        previous = height;
    }
}

TEST(TerrainTests, TheQueryAndTheColliderAreTheSameSurface)
{
    // §11.5 asked for a bilinear sample and it cannot be one: over these four samples the bilinear
    // patch says 1.25 at the centre and the triangles say 1.00, and the collider sweeps the
    // triangles. A body told 1.25 and placed a millimetre over it would be 0.25 m inside the
    // ground it is drawn on. So the query answers with the surface the collider uses --
    // `HOUSE-00553` corrected §11.5 to say so, having measured a 74 mm gap on this house's own
    // lot, on a square the 20° rule calls gentle.
    // A square that is GENTLE by §11.5's own 20° -- both its triangles are 16.7° -- and where the
    // two answers are still 75 mm apart, which is the size of the worst gap on this house's lot.
    const CollisionTerrain terrain = Field(2u, 2u, {0.0F, 0.0F, 0.0F, 0.30F});
    const TerrainSample middle = TerrainAt(terrain, 0.5F, 0.5F);
    EXPECT_FALSE(middle.steep) << "the case has to be one the 20 degree rule calls gentle";
    const float bilinear = (0.0F + 0.0F + 0.0F + 0.30F) / 4.0F;
    EXPECT_NEAR(middle.height, 0.15F, 1e-5F);
    EXPECT_NEAR(std::fabs(middle.height - bilinear), 0.075F, 1e-5F)
        << "this case stopped telling the two surfaces apart";

    // And the collider agrees with the query: a body RESTING on what `TerrainAt` reports, plus a
    // millimetre, is clear -- everywhere on the square.
    for (float u = 0.05F; u < 1.0F; u += 0.1F)
    {
        for (float v = 0.05F; v < 1.0F; v += 0.1F)
        {
            const TerrainSample ground = TerrainAt(terrain, u, v);
            const Capsule body{
                Vector3(
                    u, RestingCentre(ground.height, ground.normal, kBodyHalfHeight, kBodyRadius) + 0.001F, v),
                kBodyHalfHeight,
                kBodyRadius};
            EXPECT_FALSE(OverlapCapsuleTerrain(terrain, body).overlapped) << u << ", " << v;
        }
    }
}

TEST(TerrainTests, ASteepSquareIsReadAsTheTrianglesItIsDrawnAs)
{
    // The same four corners over one metre: 63°, well past §11.5's 20. At the centre the two
    // answers differ by 0.25 m -- a quarter of the height difference across the square -- which
    // is the difference between standing on the ground and standing in the air above it.
    const CollisionTerrain terrain = Field(2u, 2u, {0.0F, 1.0F, 2.0F, 2.0F});
    const TerrainSample middle = TerrainAt(terrain, 0.5F, 0.5F);
    EXPECT_TRUE(middle.steep);
    // The centre lies on the (0,0)–(1,1) diagonal, so it is on both triangles and both give
    // (h00 + h11) / 2 = 1.0 -- not the bilinear 1.25.
    EXPECT_NEAR(middle.height, 1.0F, 1e-5F);

    // Either side of that diagonal picks a different triangle, and the two are NOT coplanar here.
    // The lower one is `h00 + u + v` and the upper one is `2v`, so mirrored points about the
    // diagonal give 1.0 and 1.5.
    const float lower = TerrainAt(terrain, 0.75F, 0.25F).height; // v < u: the h00-h10-h11 triangle
    const float upper = TerrainAt(terrain, 0.25F, 0.75F).height; // v > u: the h00-h11-h01 triangle
    EXPECT_NEAR(lower, 1.0F, 1e-5F);
    EXPECT_NEAR(upper, 1.5F, 1e-5F);
    EXPECT_NEAR(TerrainAt(terrain, 0.75F, 0.25F).normal.Z, TerrainAt(terrain, 0.25F, 0.75F).normal.Z, 0.4F)
        << "...and they are different planes, so their normals differ too";
    EXPECT_GT(std::fabs(upper - lower), 0.4F) << "the two triangles turned out to be one plane";

    // And the SWEEP lands on the same triangle the query names. (0.25, 0.75) is on the far side
    // of the diagonal -- 0.35 m from it, further than the body's 0.30 m radius, so the body rests
    // on the SECOND triangle and touches the first nowhere. A sweep that only ever tested the
    // first would drop it half a metre.
    const TerrainSample under = TerrainAt(terrain, 0.25F, 0.75F);
    const Capsule falling{Vector3(0.25F, 6.0F, 0.75F), kBodyHalfHeight, kBodyRadius};
    const SweepHit landed = SweepCapsuleTerrain(terrain, falling, Vector3(0.0F, -6.0F, 0.0F));
    ASSERT_TRUE(landed.hit);
    EXPECT_NEAR(falling.centre.Y - 6.0F * landed.time,
                RestingCentre(under.height, under.normal, kBodyHalfHeight, kBodyRadius),
                5e-3F)
        << "it landed on the other triangle of the square";
}

TEST(TerrainTests, TheNormalOfARampIsTheRampsAndTheSlopeCutIsWhereItSays)
{
    // A field tilted by a known angle has a normal tilted by the same angle, whichever side of
    // §11.5's 20° it falls on -- the rule chooses HOW the height is found, not what the ground is.
    for (const float degrees : {5.0F, 15.0F, 19.0F, 25.0F, 45.0F})
    {
        const CollisionTerrain terrain = Ramp(degrees);
        const TerrainSample sample = TerrainAt(terrain, 2.5F, 2.5F);
        const float radians = degrees * 3.14159265F / 180.0F;
        EXPECT_NEAR(sample.normal.Y, std::cos(radians), 1e-4F) << degrees;
        EXPECT_NEAR(sample.normal.X, -std::sin(radians), 1e-4F) << degrees;
        EXPECT_EQ(sample.steep, degrees > kTerrainTriangleSlopeDegrees) << degrees;
        EXPECT_NEAR(sample.height, 2.5F * std::tan(radians), 1e-4F) << degrees;
    }
}

TEST(TerrainTests, TheMaterialIsTheNearestSamplesAndNotAnAverageOfTwo)
{
    // A material is a NAME. Half of grass and half of gravel is neither, and a footstep needs one
    // of them.
    CollisionTerrain terrain = Field(2u, 2u, {0.0F, 0.0F, 0.0F, 0.0F});
    terrain.materials = {7u, 9u};
    terrain.materialIndex = {0u, 1u, 0u, 1u};
    EXPECT_EQ(TerrainAt(terrain, 0.1F, 0.5F).surface, 7u);
    EXPECT_EQ(TerrainAt(terrain, 0.9F, 0.5F).surface, 9u);
    EXPECT_EQ(TerrainAt(terrain, 0.49F, 0.5F).surface, 7u);
    EXPECT_EQ(TerrainAt(terrain, 0.51F, 0.5F).surface, 9u);
}

TEST(TerrainTests, AWorldWithNoGroundAnswersNothing)
{
    const CollisionTerrain none;
    const TerrainSample sample = TerrainAt(none, 0.0F, 0.0F);
    EXPECT_FALSE(sample.over);
    EXPECT_FALSE(SweepCapsuleTerrain(none, Sphere(Vector3(), 0.3F), Vector3(0.0F, -1.0F, 0.0F)).hit);
    EXPECT_FALSE(OverlapCapsuleTerrain(none, Sphere(Vector3(), 0.3F)).overlapped);
}

// ---------------------------------------------------------------------------------------------
// Sweeping and overlapping
// ---------------------------------------------------------------------------------------------

TEST(TerrainTests, ABodyDroppedOnALawnStopsOnIt)
{
    const CollisionTerrain terrain = Flat(0.35F);
    const Capsule body{Vector3(2.5F, 2.0F, 2.5F), kBodyHalfHeight, kBodyRadius};
    const SweepHit hit = SweepCapsuleTerrain(terrain, body, Vector3(0.0F, -2.0F, 0.0F));
    ASSERT_TRUE(hit.hit);
    // The feet are at 2.0 - 0.90 = 1.10 and the lawn is at 0.35, so 0.75 of the 2 m sweep.
    EXPECT_NEAR(hit.time, 0.75F / 2.0F, 1e-4F);
    EXPECT_NEAR(hit.normal.Y, 1.0F, 1e-4F);
}

TEST(TerrainTests, ABodyWalkingOverALawnMeetsNothing)
{
    // The one that matters most in practice: walking ALONG the ground must not report a hit every
    // tick. The body is held a millimetre clear, which is where every other part of §49.3 leaves
    // it, and walks a metre.
    const CollisionTerrain terrain = Flat(0.0F);
    const Capsule body{
        Vector3(1.0F, kBodyHalfHeight + kBodyRadius + 0.001F, 2.5F), kBodyHalfHeight, kBodyRadius};
    EXPECT_FALSE(SweepCapsuleTerrain(terrain, body, Vector3(1.0F, 0.0F, 0.0F)).hit);
    EXPECT_FALSE(OverlapCapsuleTerrain(terrain, body).overlapped);
}

TEST(TerrainTests, ABodySunkIntoTheLawnIsOverlappingItByTheDepthItIsSunk)
{
    const CollisionTerrain terrain = Flat(0.0F);
    // Feet 0.05 m under the grass.
    const Capsule body{
        Vector3(2.5F, kBodyHalfHeight + kBodyRadius - 0.05F, 2.5F), kBodyHalfHeight, kBodyRadius};
    const Overlap overlap = OverlapCapsuleTerrain(terrain, body);
    ASSERT_TRUE(overlap.overlapped);
    EXPECT_NEAR(overlap.depth, 0.05F, 1e-4F);
    EXPECT_NEAR(overlap.normal.Y, 1.0F, 1e-4F);
}

TEST(TerrainTests, ABodyRestsOnTheSQUARENEXTDOORWhenItsSideReachesIt)
{
    // A valley: 45° down from x = 0 to x = 1, flat to x = 2, 45° back up to x = 3. A body 0.60 m
    // wide standing 50 mm from the foot of either bank is over the FLAT square, and its side is
    // over the bank -- so the bank is what holds it up, 74 mm higher than the floor of the valley
    // its middle is above.
    //
    // Three separate ways of getting this wrong land on the same wrong number, which is why this
    // case is worth its length: forgetting the capsule's radius when choosing which squares to
    // test, keeping the LAST square's hit instead of the earliest, and -- for the overlap --
    // keeping the first square that overlaps rather than the deepest.
    // Four rows of the same profile, so the body sits in a MIDDLE square rather than against the
    // edge of the field, and its z is chosen to put it over the FIRST of that square's two
    // triangles -- the half where the order the squares are tested in actually decides the answer.
    std::vector<float> profile;
    for (int row = 0; row < 4; ++row)
    {
        for (const float height : {1.0F, 0.0F, 0.0F, 1.0F})
        {
            profile.push_back(height);
        }
    }
    const CollisionTerrain terrain = Field(4u, 4u, std::move(profile));
    constexpr float kZ = 1.02F; // just inside the square, on the `v <= u` side of its diagonal

    // The floor of the valley: 0.30 under the cap centre. The bank: the cap centre is 0.30 from a
    // plane that is 0.05 m of run away, so (0.05 + y)/√2 = 0.30 and y = 0.3743.
    const float onTheFlat = kBodyRadius;
    const float onTheBank = 0.30F * std::sqrt(2.0F) - 0.05F;
    ASSERT_NEAR(onTheBank, 0.3743F, 1e-4F);

    // Dropped 50 mm east of the west bank's foot. `TerrainAt` says the ground under its middle is
    // 0, and it must NOT sink to there.
    EXPECT_NEAR(TerrainAt(terrain, 1.05F, kZ).height, 0.0F, 1e-5F);
    const Capsule west{Vector3(1.05F, 3.0F, kZ), kBodyHalfHeight, kBodyRadius};
    const SweepHit hit = SweepCapsuleTerrain(terrain, west, Vector3(0.0F, -3.0F, 0.0F));
    ASSERT_TRUE(hit.hit);
    const float capY = west.centre.Y - 3.0F * hit.time - kBodyHalfHeight;
    EXPECT_NEAR(capY, onTheBank, 2e-3F) << "it sank to the floor of the valley beside the bank";
    EXPECT_GT(capY, onTheFlat + 0.05F);

    // And 50 mm west of the EAST bank's foot, sunk 50 mm into the valley floor, the deepest thing
    // it is inside is the bank -- which is tested second, after the flat square it is only 50 mm
    // into.
    const Capsule east{Vector3(1.95F, 0.85F, kZ), kBodyHalfHeight, kBodyRadius};
    const Overlap overlap = OverlapCapsuleTerrain(terrain, east);
    ASSERT_TRUE(overlap.overlapped);
    EXPECT_NEAR(overlap.depth, 0.30F - (0.05F + 0.25F) / std::sqrt(2.0F), 2e-3F);
    EXPECT_GT(overlap.depth, 0.05F + 1e-3F) << "it reported the flat square it is barely in";
    // ...and the way out is away from the bank, up and to the west.
    EXPECT_NEAR(overlap.normal.X, -0.7071F, 1e-2F);
    EXPECT_NEAR(overlap.normal.Y, 0.7071F, 1e-2F);
}

TEST(TerrainTests, ASweepPastTheEdgeOfTheLotHitsNothing)
{
    const CollisionTerrain terrain = Flat(0.0F);
    const Capsule body{Vector3(40.0F, 2.0F, 40.0F), kBodyHalfHeight, kBodyRadius};
    EXPECT_FALSE(SweepCapsuleTerrain(terrain, body, Vector3(0.0F, -4.0F, 0.0F)).hit)
        << "the lawn was found 35 m away from itself";
}

// ---------------------------------------------------------------------------------------------
// The real lot
// ---------------------------------------------------------------------------------------------

TEST(TerrainTests, TheRealLotIsTheGroundTheLayoutDescribes)
{
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
    const CollisionTerrain& terrain = world->terrain;
    ASSERT_TRUE(terrain.present);

    // §10.2 in one sentence: "+0.15 at the front property line falling to -0.35 at the rear
    // fence". The front line is Z 0 and the fence is Z -48.
    // Sampled on the LAWN and not on the front line itself, where the walk and the carriageway
    // are pads at 0.00 and the slope no longer shows.
    EXPECT_NEAR(TerrainAt(terrain, 20.0F, -2.0F).height, 0.15F - 0.5F * 2.0F / 48.0F, 2e-4F);
    EXPECT_NEAR(TerrainAt(terrain, 20.0F, -48.0F).height, -0.35F, 2e-4F);
    // ...and a QUARTER of the way down, which is the point that says which way it falls.
    EXPECT_NEAR(TerrainAt(terrain, 20.0F, -12.0F).height, 0.025F, 2e-4F);

    // §11.6's terrace is a flat pad at +0.45, and it is paved.
    const TerrainSample terrace = TerrainAt(terrain, 0.0F, -34.0F);
    EXPECT_NEAR(terrace.height, 0.45F, 2e-4F);
    EXPECT_EQ(world->SurfaceName(terrace.surface), "bluestone");

    // The whole lot is gentle: the lot moves 0.5 m over 48 and the steepest thing on it is the
    // lip of a pad. Nothing anywhere near §11.5's 20° -- which is exactly why the bilinear path
    // is the one that runs, and why the triangle path needed constructed cases above.
    std::size_t steep = 0;
    std::size_t samples = 0;
    float lowest = 1000.0F;
    float highest = -1000.0F;
    for (float z = terrain.originZ; z <= terrain.MaxZ(); z += 1.0F)
    {
        for (float x = terrain.originX; x <= terrain.MaxX(); x += 1.0F)
        {
            const TerrainSample sample = TerrainAt(terrain, x + 0.5F, z + 0.5F);
            if (!sample.over)
            {
                continue;
            }
            ++samples;
            steep += sample.steep ? 1u : 0u;
            lowest = std::min(lowest, sample.height);
            highest = std::max(highest, sample.height);
            EXPECT_GT(sample.normal.Y, 0.0F) << x << ", " << z << ": the ground faced down";
            const float length =
                std::sqrt(sample.normal.X * sample.normal.X + sample.normal.Y * sample.normal.Y +
                          sample.normal.Z * sample.normal.Z);
            EXPECT_NEAR(length, 1.0F, 1e-4F);
        }
    }
    ASSERT_GT(samples, 4000u);
    EXPECT_NEAR(lowest, -0.35F, 1e-3F);
    EXPECT_NEAR(highest, 0.57F, 1e-2F) << "the porch is the highest ground on the lot";
    // A handful of pad lips are steep; a lot of them would mean a balcony is in the ground again
    // (`HOUSE-00761`).
    EXPECT_LT(steep, samples / 10) << steep << " of " << samples << " squares are over 20 degrees";
}

TEST(TerrainTests, ABodyStandsOnTheRealLotWhereeverItIsPutDown)
{
    const std::string path = "content/world/collision.bin";
    System::IO::FileStream* probe = nullptr;
    try
    {
        probe = new System::IO::FileStream(path, System::IO::FileMode::Open, System::IO::FileAccess::Read);
    }
    catch (const std::exception&)
    {
        GTEST_SKIP() << "no " << path;
    }
    const std::unique_ptr<System::IO::FileStream> stream(probe);
    const auto world = CollisionLoader::Read(*stream, path);
    ASSERT_TRUE(world) << world.Error().Message();
    const CollisionTerrain& terrain = world->terrain;
    ASSERT_TRUE(terrain.present);

    // Dropped from 3 m over every second metre of the lot: it lands, the landing agrees with what
    // `TerrainAt` says the ground is there, and standing a millimetre over that is clear.
    std::size_t dropped = 0;
    std::size_t pressed = 0;
    std::size_t propped = 0;
    for (float z = terrain.originZ + 1.0F; z < terrain.MaxZ(); z += 2.0F)
    {
        for (float x = terrain.originX + 1.0F; x < terrain.MaxX(); x += 2.0F)
        {
            const TerrainSample ground = TerrainAt(terrain, x, z);
            const Capsule body{Vector3(x, ground.height + 3.0F, z), kBodyHalfHeight, kBodyRadius};
            const SweepHit hit = SweepCapsuleTerrain(terrain, body, Vector3(0.0F, -4.0F, 0.0F));
            ASSERT_TRUE(hit.hit) << x << ", " << z;
            ++dropped;
            const float landedY = body.centre.Y - 4.0F * hit.time;
            const float feet = landedY - kBodyHalfHeight - kBodyRadius;
            // It never SINKS: the feet are never below the ground under the body's own centre.
            EXPECT_GE(feet, ground.height - 0.005F) << x << ", " << z;
            // It may land HIGH, though, and that is not a defect either: a body 0.60 m wide
            // dropped beside a 0.60 m terrace lip catches the lip with its side rather than
            // reaching the grass under its middle. Counted, because most of the lot is not a lip.
            if (feet > ground.height + 0.005F)
            {
                ++propped;
            }

            Capsule standing = body;
            standing.centre = Vector3(
                x, RestingCentre(ground.height, ground.normal, kBodyHalfHeight, kBodyRadius) + 0.001F, z);
            const Overlap resting = OverlapCapsuleTerrain(terrain, standing);
            if (resting.overlapped)
            {
                // A body 0.30 m wide cannot stand with its centre 0.30 m from the face of a
                // 0.60 m step, and this lot has terrace and shed-pad edges exactly that size.
                // Being PRESSED against one is real geometry; being buried in a hill is not, and
                // the depth is what says which of the two this is.
                ++pressed;
                EXPECT_LT(resting.depth, kBodyRadius) << x << ", " << z;
            }
        }
    }
    ASSERT_GT(dropped, 1000u);
    // Pad edges are a small minority of the lot; a lot of them would mean the ground is a
    // staircase rather than a garden.
    EXPECT_LT(pressed, dropped / 50) << pressed << " of " << dropped << " resting places are against a step";
    EXPECT_LT(propped, dropped / 20) << propped << " of " << dropped
                                     << " landings caught something higher than the ground under them";
}
