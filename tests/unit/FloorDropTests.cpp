// SPDX-License-Identifier: MIT
//
// `HOUSE-00614`, §49.5's third guarantee: **the player cannot fall through any floor.** Two
// thousand seeded drops, one after another, all over the house.
//
// What a drop can prove that a walk cannot is the ARRIVAL: a body that has fallen 3 m is moving
// at 7.7 m/s and covers 64 mm in one fixed step, which is most of the way through a 0.15 m slab.
// Tunnelling is a function of speed, and this is the fastest anything in this house moves.
//
// The drop height is the room's, not a number: §70.5 gives interior rooms 2.45-3.05 m of clear
// height and §43.1's body is 1.80 m tall, so three metres of air only exists outdoors, in the
// stair wells and in the garage. Every drop is as high as its cell allows, up to 3 m, and the
// test says how many got the full three.
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
#include "cnahouse/physics/Ground.hpp"
#include "cnahouse/physics/Terrain.hpp"
#include "cnahouse/player/PlayerController.hpp"
#include "cnahouse/util/Rng.hpp"
#include "cnahouse/world/WorldData.hpp"
#include "cnahouse/world/WorldLoader.hpp"

namespace
{
    using cnahouse::physics::BroadPhase;
    using cnahouse::physics::Capsule;
    using cnahouse::physics::CollisionCell;
    using cnahouse::physics::CollisionLoader;
    using cnahouse::physics::CollisionWorld;
    using cnahouse::physics::GroundProbe;
    using cnahouse::physics::GroundProbeResult;
    using cnahouse::physics::kHardLandingDrop;
    using cnahouse::physics::Landing;
    using cnahouse::physics::OverlapCell;
    using cnahouse::player::InputState;
    using cnahouse::player::kPlayerHalfHeight;
    using cnahouse::player::kPlayerRadius;
    using cnahouse::player::PlayerState;
    using cnahouse::player::PlayerStep;
    using cnahouse::player::PlayerStepReport;
    using cnahouse::util::IdRegistry;
    using cnahouse::util::Rng;
    using Microsoft::Xna::Framework::Vector3;

    namespace world = cnahouse::world;

    constexpr float kDt = 1.0F / 120.0F;
    /// The seed is the ticket, so a failure is reproducible by running the test again.
    constexpr std::uint64_t kSeed = 614u;
    constexpr int kDrops = 2000;
    /// The tallest drop §49.5 asks for. 0.78 s of falling, 7.7 m/s at the bottom.
    constexpr float kTallest = 3.0F;
    /// Long enough for the tallest drop and half again: 3 m takes 94 steps.
    constexpr int kSteps = 140;
    constexpr float kRise = kPlayerHalfHeight + kPlayerRadius;

    std::string_view Name(cnahouse::util::Id id)
    {
        return IdRegistry::NameOf(id);
    }

    /// The highest thing under (@p x, @p z) that is below @p below, read straight out of the
    /// collision data.
    ///
    /// **Deliberately not a sweep.** What this test asks is whether the FALL stopped where the
    /// world says the ground is, and a witness that used the same `SweepCell` the fall uses would
    /// agree with it about a bug in it. An OBB's top face and §11.5's height field are the two
    /// things a body can land on, and both are plain arithmetic.
    float
    HighestSurfaceUnder(const CollisionWorld& world, const CollisionCell& cell, float x, float z, float below)
    {
        float best = -1e9F;
        const cnahouse::physics::TerrainSample sample = cnahouse::physics::TerrainAt(world.terrain, x, z);
        if (world.terrain.present && sample.over && sample.height <= below)
        {
            best = sample.height;
        }
        for (const std::uint32_t shape : cell.shapes)
        {
            if (shape >= world.obbs.size())
            {
                continue; // a mesh: a stair ramp or the rafters, and neither is a floor to drop onto
            }
            const cnahouse::physics::CollisionObb& obb = world.obbs[shape];
            if (obb.yaw != 0.0F)
            {
                continue; // a yawed prop proxy; its top is not an axis-aligned rectangle
            }
            if (obb.kind != cnahouse::physics::CollisionKind::Floor &&
                obb.kind != cnahouse::physics::CollisionKind::Exterior)
            {
                continue;
            }
            const float top = obb.centre.Y + obb.halfExtents.Y;
            if (top > below || top <= best)
            {
                continue;
            }
            // The capsule lands on a face it is over by more than its own radius; a body on the
            // very lip of a slab is a different question (`HOUSE-00617`'s).
            if (x < obb.centre.X - obb.halfExtents.X + kPlayerRadius ||
                x > obb.centre.X + obb.halfExtents.X - kPlayerRadius ||
                z < obb.centre.Z - obb.halfExtents.Z + kPlayerRadius ||
                z > obb.centre.Z + obb.halfExtents.Z - kPlayerRadius)
            {
                continue;
            }
            best = top;
        }
        return best;
    }

} // namespace

TEST(FloorDropTests, TwoThousandDropsAllLandOnTheFloorTheyWereDroppedOnto)
{
    IdRegistry::ResetForTesting();
    const std::string directory = "content/world";
    const std::string collisionPath = "content/world/collision.bin";
    if (!std::filesystem::exists(directory + "/layout.cells.json") || !std::filesystem::exists(collisionPath))
    {
        GTEST_SKIP() << "no deployed world; run tools/ci/build_content.py --only world";
    }

    world::WorldData::Contents contents;
    ASSERT_TRUE(world::WorldLoader::LoadLevels(directory, contents));
    ASSERT_TRUE(world::WorldLoader::LoadCells(directory, contents));
    auto built = world::WorldData::Create(std::move(contents));
    ASSERT_TRUE(built) << built.Error().ToString();
    const world::WorldData& data = built.Value();

    const std::unique_ptr<System::IO::FileStream> stream(
        new System::IO::FileStream(collisionPath, System::IO::FileMode::Open, System::IO::FileAccess::Read));
    const auto loaded = CollisionLoader::Read(*stream, collisionPath);
    ASSERT_TRUE(loaded) << loaded.Error().Message();
    const CollisionWorld& statics = loaded.Value();

    std::vector<const world::Cell*> droppable;
    for (const world::Cell& cell : data.Cells())
    {
        const CollisionCell* collision = statics.Cell(Name(cell.id));
        if (collision != nullptr && !collision->shapes.empty() && !cell.boxes.empty())
        {
            droppable.push_back(&cell);
        }
    }
    ASSERT_FALSE(droppable.empty());

    Rng rng(kSeed);
    BroadPhase broad;

    int dropped = 0;
    int skipped = 0;
    int skipNoGround = 0;
    int skipNoRoom = 0;
    int skipInside = 0;
    int skipBlocked = 0;
    int fullHeight = 0;
    int hard = 0;
    int soft = 0;
    int steps = 0;
    float tallest = 0.0F;
    float fastest = 0.0F;
    float worstMiss = 0.0F;
    std::string worstMissCell;
    std::vector<std::string> throughTheFloor;
    std::vector<std::string> neverLanded;
    std::vector<std::string> wrongLanding;

    for (int drop = 0; drop < kDrops; ++drop)
    {
        const world::Cell& cell = *droppable[static_cast<std::size_t>(drop) % droppable.size()];
        const CollisionCell* collision = statics.Cell(Name(cell.id));
        const world::Level* level = data.FindLevel(cell.level);
        ASSERT_NE(level, nullptr) << Name(cell.id);

        // Somewhere inside the cell, a capsule's width clear of its boundary so the drop is onto
        // the floor and not into the wall's own box.
        const world::Footprint& box = cell.boxes[static_cast<std::size_t>(
            rng.NextInt(0, static_cast<std::int32_t>(cell.boxes.size()) - 1))];
        const float insetX = std::min(kPlayerRadius, (box.maxX - box.minX) * 0.25F);
        const float insetZ = std::min(kPlayerRadius, (box.maxZ - box.minZ) * 0.25F);
        const float x = rng.NextFloat(box.minX + insetX, box.maxX - insetX);
        const float z = rng.NextFloat(box.minZ + insetZ, box.maxZ - insetZ);

        // What is under that spot, read out of the collision data rather than swept for. Under
        // the level's ceiling, so a slab overhead is not mistaken for the floor.
        const float ceilingY =
            level->ceiling.has_value() ? level->ceiling.value() : level->ffl + kTallest + 2.0F;
        const float groundY = HighestSurfaceUnder(statics, *collision, x, z, ceilingY);
        if (groundY < -1e8F)
        {
            ++skipped;
            ++skipNoGround; // no floor slab and no terrain under this spot: a well, a void, a lip
            continue;
        }

        // As high as the cell allows, up to 3 m. §70.5's rooms are 2.45-3.05 m in the clear and
        // §43.1's body is 1.80 m tall, so indoors this is usually under a metre -- which is the
        // honest answer to "drop the player 3 m in a house with 2.70 m ceilings".
        const float room = std::max(0.0F, (ceilingY - groundY) - 2.0F * kRise - 0.05F);
        const float height = std::min(kTallest, cell.kind == world::CellKind::Exterior ? kTallest : room);
        if (height < 0.10F)
        {
            ++skipped;
            ++skipNoRoom;
            continue;
        }

        PlayerState state;
        state.position = Vector3(x, groundY + kRise + height, z);
        state.cellId = collision->id;
        state.fall.onGround = false;
        state.fall.fellFrom = state.position.Y;
        state.onGround = false;
        if (OverlapCell(statics, *collision, broad, state.Body()).overlapped)
        {
            ++skipped;
            ++skipInside;
            continue;
        }

        // Is the way down CLEAR? A canopy, a beam or a parapet between the body and the floor
        // makes this a drop onto that thing and not onto the floor -- and a body dropped into one
        // at terminal speed sinks through it at 15 mm a step, which is a different bug and
        // `HOUSE-00617`'s. The sweep is used to SET UP the drop here, never to judge it.
        const cnahouse::physics::CellSweepHit ahead = cnahouse::physics::SweepCell(
            statics, *collision, broad, state.Body(), Vector3(0.0F, -(height + 0.05F), 0.0F));
        if (ahead.hit && ahead.time < 0.90F)
        {
            ++skipped;
            ++skipBlocked;
            continue;
        }

        ++dropped;
        const float startY = state.position.Y;
        tallest = std::max(tallest, height);
        if (height >= kTallest - 1e-3F)
        {
            ++fullHeight;
        }

        Landing landing = Landing::None;
        float landingDrop = 0.0F;
        float impact = 0.0F;
        for (int i = 0; i < kSteps && landing == Landing::None; ++i)
        {
            impact = std::max(impact, state.fall.speed);
            const PlayerStepReport report = PlayerStep(statics, *collision, broad, state, InputState{}, kDt);
            ++steps;
            landing = report.landing;
            landingDrop = report.landingDrop;
        }

        if (landing == Landing::None)
        {
            neverLanded.push_back(std::string(Name(cell.id)) + " from " + std::to_string(height) + " m");
            continue;
        }
        fastest = std::max(fastest, impact);

        // It landed on something, and it descended about as far as it was dropped -- not a
        // storey more. Measured as a DESCENT rather than against the probed height because a
        // capsule resting on a slope touches it uphill of its centre: on §11.5's 20° squares
        // that is 0.11 m of perfectly correct difference (`HOUSE-00553` measured it).
        const float descent = startY - state.position.Y;
        const float miss = std::fabs(descent - height);
        if (miss > worstMiss)
        {
            worstMiss = miss;
            worstMissCell = Name(cell.id);
        }
        const GroundProbeResult resting = GroundProbe(statics, *collision, broad, state.Body(), 0.10F);
        if (descent > height + 0.15F || !resting.onGround)
        {
            throughTheFloor.push_back(std::string(Name(cell.id)) + ": dropped " + std::to_string(height) +
                                      " m, descended " + std::to_string(descent) +
                                      (resting.onGround ? "" : ", resting on nothing"));
        }

        // §43.1's 2.4 m: the landing's own classification, checked against the height it was
        // actually dropped from. `HOUSE-00552` owns the rule; this is the house-wide sample of it.
        const bool shouldBeHard = landingDrop > kHardLandingDrop;
        if ((landing == Landing::Hard) != shouldBeHard)
        {
            wrongLanding.push_back(std::string(Name(cell.id)) + ": fell " + std::to_string(landingDrop) +
                                   " m and called it " + (landing == Landing::Hard ? "hard" : "soft"));
        }
        (landing == Landing::Hard ? hard : soft)++;
    }

    std::printf("  %d drop(s) over %zu cell(s), %d skipped, %d step(s); tallest %.2f m (%d at the full "
                "3 m), fastest arrival %.2f m/s; %d hard, %d soft; worst landing miss %.4f m (%s)\n",
                dropped,
                droppable.size(),
                skipped,
                steps,
                static_cast<double>(tallest),
                fullHeight,
                static_cast<double>(fastest),
                hard,
                soft,
                static_cast<double>(worstMiss),
                worstMissCell.c_str());

    std::printf("  skips: %d with nothing under them, %d with no room to fall, %d starting inside something, "
                "%d with something in the way\n",
                skipNoGround,
                skipNoRoom,
                skipInside,
                skipBlocked);
    EXPECT_GT(dropped, kDrops / 2) << "most drops never happened, so this proves little";
    EXPECT_GT(hard, 0) << "not one drop was hard enough to be a hard landing, so §43.1's 2.4 m is untested";
    EXPECT_TRUE(neverLanded.empty()) << neverLanded.size() << " drop(s) never landed; first: "
                                     << (neverLanded.empty() ? std::string() : neverLanded.front());
    EXPECT_TRUE(throughTheFloor.empty())
        << throughTheFloor.size() << " drop(s) ended below the floor they were dropped onto; first: "
        << (throughTheFloor.empty() ? std::string() : throughTheFloor.front());
    EXPECT_TRUE(wrongLanding.empty())
        << wrongLanding.size() << " landing(s) disagreed with §43.1's 2.4 m; first: "
        << (wrongLanding.empty() ? std::string() : wrongLanding.front());
}
