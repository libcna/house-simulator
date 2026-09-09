// SPDX-License-Identifier: MIT
//
// `HOUSE-00568`: **a body standing in a hole is in both rooms, and both rooms' collision has to
// say the same thing about it.**
//
// §49.2 partitions static collision per cell and §49.3 sweeps against ONE of them -- whichever
// §16.4's lookup answers with. That is right for everything a wall separates: a wall is in the
// lists on both sides of itself, and nothing behind one can be touched. It is wrong at a hole. The
// main stair's flight begins 0.20 m east of `L0_FOYER`'s cased opening and was in
// `L0_STAIR_MAIN`'s list alone, so a body walking east through that opening met NOTHING until the
// cell tracker changed its mind -- by which time it was 0.16 m inside the staircase and being
// pushed back out. `HOUSE-00618`'s twenty-minute bot walked into it 3 times in 144 000 steps.
//
// The invariant this test states is the one that fix has to hold: **within the band where either
// cell could be the tracker's answer -- §16.4's hysteresis either side of the plane -- the deepest
// overlap does not depend on which of the two lists you ask.**
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <memory>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "System/IO/FileAccess.hpp"
#include "System/IO/FileMode.hpp"
#include "System/IO/FileStream.hpp"

#include "cnahouse/physics/BroadPhase.hpp"
#include "cnahouse/physics/CollisionLoader.hpp"
#include "cnahouse/physics/Sweep.hpp"
#include "cnahouse/player/PlayerController.hpp"
#include "cnahouse/world/SpatialIndex.hpp"
#include "cnahouse/world/WorldData.hpp"
#include "cnahouse/world/WorldLoader.hpp"

namespace
{
    using cnahouse::physics::BroadPhase;
    using cnahouse::physics::Capsule;
    using cnahouse::physics::CellOverlap;
    using cnahouse::physics::CollisionCell;
    using cnahouse::physics::CollisionLoader;
    using cnahouse::physics::CollisionWorld;
    using cnahouse::physics::OverlapCell;
    using cnahouse::player::kPlayerHalfHeight;
    using cnahouse::player::kPlayerRadius;
    using cnahouse::util::IdRegistry;
    using Microsoft::Xna::Framework::Vector3;

    namespace world = cnahouse::world;

    /// The two lists have to agree to this, which is §49.3's own contact tolerance: below it a
    /// body is touching a surface rather than inside one.
    constexpr float kAgree = 1.0e-4F;
    /// A sill this far above the floor is a window, not a way through: no body stands in it, and
    /// the wall under it is in both lists already. 0.25 m is §43.1's step, the height a body can
    /// walk over without noticing.
    constexpr float kStepOver = 0.25F;
    /// Where the capsule's middle is, above the feet.
    constexpr float kRise = kPlayerHalfHeight + kPlayerRadius;

    std::string_view Name(cnahouse::util::Id id)
    {
        return IdRegistry::NameOf(id);
    }

    /// A cell's floor: its own override where it has one, else its level's finished floor level.
    float FloorOf(const world::WorldData& data, cnahouse::util::Id id)
    {
        const world::Cell* cell = data.FindCell(id);
        if (cell == nullptr)
        {
            return 0.0F;
        }
        if (cell->yOverride)
        {
            return cell->yOverride->floorY;
        }
        const world::Level* level = data.FindLevel(cell->level);
        return level == nullptr ? 0.0F : level->ffl;
    }

    std::string Where(const Vector3& point)
    {
        char text[96];
        std::snprintf(text,
                      sizeof(text),
                      "(%.3f, %.3f, %.3f)",
                      static_cast<double>(point.X),
                      static_cast<double>(point.Y),
                      static_cast<double>(point.Z));
        return text;
    }

} // namespace

TEST(OpeningReachTests, BothSidesOfEveryHoleAgreeAboutWhatIsBehindIt)
{
    IdRegistry::ResetForTesting();
    const std::string directory = "content/world";
    const std::string collisionPath = directory + "/collision.bin";
    if (!std::filesystem::exists(directory + "/layout.cells.json") || !std::filesystem::exists(collisionPath))
    {
        GTEST_SKIP() << "no deployed world; run tools/ci/build_content.py --only world";
    }

    world::WorldData::Contents contents;
    ASSERT_TRUE(world::WorldLoader::LoadLevels(directory, contents));
    ASSERT_TRUE(world::WorldLoader::LoadCells(directory, contents));
    ASSERT_TRUE(world::WorldLoader::LoadPortals(directory, contents));
    auto built = world::WorldData::Create(std::move(contents));
    ASSERT_TRUE(built) << built.Error().ToString();
    const world::WorldData& data = built.Value();

    const std::unique_ptr<System::IO::FileStream> stream(
        new System::IO::FileStream(collisionPath, System::IO::FileMode::Open, System::IO::FileAccess::Read));
    const auto loaded = CollisionLoader::Read(*stream, collisionPath);
    ASSERT_TRUE(loaded) << loaded.Error().Message();
    const CollisionWorld& statics = loaded.Value();

    BroadPhase broad;
    std::vector<std::string> disagreements;
    float worst = 0.0F;
    std::string worstWhere;
    int holes = 0;
    int samples = 0;

    for (const world::Portal& portal : data.Portals())
    {
        // A `y` portal is a hole in a SLAB, and a body does not stand in one: it falls through it,
        // and the cell it lands in is the one that then answers for it. The flights that reach
        // through a stair well are in the lists of both cells they connect already.
        if (portal.axis == world::PlaneAxis::Y)
        {
            continue;
        }
        const CollisionCell* left = statics.Cell(Name(portal.cellA));
        const CollisionCell* right = statics.Cell(Name(portal.cellB));
        if (left == nullptr || right == nullptr)
        {
            continue;
        }
        // The higher of the two floors: a body in the hole is standing on one of them, and the
        // higher is the one it stands on. A step, a threshold and a porch are all this difference.
        const float floor = std::max(FloorOf(data, portal.cellA), FloorOf(data, portal.cellB));
        if (portal.minV > floor + kStepOver)
        {
            continue;
        }
        ++holes;

        for (float u = portal.minU + 0.05F; u <= portal.maxU - 0.05F + 1.0e-6F; u += 0.10F)
        {
            // §16.4's hysteresis is the whole band: a body whose centre is further past the plane
            // than this has been handed to the other cell, and asking THIS cell about it is asking
            // a question the game never asks.
            for (const float across :
                 {-world::SpatialIndex::kHysteresis, 0.0F, world::SpatialIndex::kHysteresis})
            {
                const float value = portal.planeValue + across;
                const Vector3 centre = portal.axis == world::PlaneAxis::X ? Vector3(value, floor + kRise, u)
                                                                          : Vector3(u, floor + kRise, value);
                const Capsule body{centre, kPlayerHalfHeight, kPlayerRadius};
                const CellOverlap here = OverlapCell(statics, *left, broad, body);
                const CellOverlap there = OverlapCell(statics, *right, broad, body);
                ++samples;
                const float gap = std::fabs(here.depth - there.depth);
                if (gap > worst)
                {
                    worst = gap;
                    worstWhere = std::string(Name(portal.id)) + " at " + Where(centre);
                }
                if (gap > kAgree)
                {
                    disagreements.push_back(
                        std::string(Name(portal.id)) + " at " + Where(centre) + ": " +
                        std::string(Name(portal.cellA)) + " says " + std::to_string(here.depth) + " m, " +
                        std::string(Name(portal.cellB)) + " says " + std::to_string(there.depth) + " m");
                }
            }
        }
    }

    std::printf("  %d hole(s) a body can stand in, %d pose(s); worst disagreement %.6f m%s%s\n",
                holes,
                samples,
                static_cast<double>(worst),
                worstWhere.empty() ? "" : " at ",
                worstWhere.c_str());

    EXPECT_GT(holes, 90) << "the house has more holes than that; the loop skipped something";
    EXPECT_TRUE(disagreements.empty())
        << disagreements.size() << " pose(s) where the two cells disagree; first: "
        << (disagreements.empty() ? std::string() : disagreements.front());
}

TEST(OpeningReachTests, TheFoyerKnowsTheStaircaseBehindItsOwnOpening)
{
    // The regression, named. `HOUSE-00618`'s bot walked east out of `L0_FOYER` through
    // `P_L0_FOYER__L0_STAIR` and was 0.151 m inside the main stair's first run before anything
    // stopped it, because the flight was in `L0_STAIR_MAIN`'s list alone. The pose below is the
    // one it was in on step 118 848.
    IdRegistry::ResetForTesting();
    const std::string collisionPath = "content/world/collision.bin";
    if (!std::filesystem::exists(collisionPath))
    {
        GTEST_SKIP() << "no deployed world; run tools/ci/build_content.py --only world";
    }
    const std::unique_ptr<System::IO::FileStream> stream(
        new System::IO::FileStream(collisionPath, System::IO::FileMode::Open, System::IO::FileAccess::Read));
    const auto loaded = CollisionLoader::Read(*stream, collisionPath);
    ASSERT_TRUE(loaded) << loaded.Error().Message();
    const CollisionWorld& statics = loaded.Value();

    const CollisionCell* foyer = statics.Cell("L0_FOYER");
    const CollisionCell* stair = statics.Cell("L0_STAIR_MAIN");
    ASSERT_NE(foyer, nullptr);
    ASSERT_NE(stair, nullptr);

    BroadPhase broad;
    const Capsule body{Vector3(2.251F, 1.496F, -16.259F), kPlayerHalfHeight, kPlayerRadius};
    const CellOverlap inFoyer = OverlapCell(statics, *foyer, broad, body);
    const CellOverlap inStair = OverlapCell(statics, *stair, broad, body);

    std::printf("  the bot's own pose: L0_FOYER says %.4f m, L0_STAIR_MAIN says %.4f m\n",
                static_cast<double>(inFoyer.depth),
                static_cast<double>(inStair.depth));
    EXPECT_GT(inStair.depth, 0.10F) << "the pose is meant to be inside the flight";
    EXPECT_NEAR(inFoyer.depth, inStair.depth, kAgree)
        << "the foyer does not know about the staircase 0.20 m past its own opening";
}
