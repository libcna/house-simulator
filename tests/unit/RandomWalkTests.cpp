// SPDX-License-Identifier: MIT
//
// `HOUSE-00618`, §49.5's sixth guarantee: **a twenty-minute seeded random walk never trips the
// boundary counter and never leaves the named cells.**
//
// Twenty minutes is 144 000 fixed steps of §49.3. What a soak of that length finds that a hundred
// aimed pushes cannot is the RARE state: the one doorway approached at the one angle that wedges
// a body, the one lip that launches it, the one gap between two cells' boxes it can settle in. It
// is the only test in this phase that does not know what it is looking for.
//
// Two things are asserted, and they are §10's last two containment layers. The body is always in
// a NAMED cell -- §16.4's lookup answers with a room, never with `EXT_WORLD` -- and §10.3's
// playable volume is never crossed, which `HOUSE-00564`'s counter is what says.
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <memory>
#include <span>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "System/IO/FileAccess.hpp"
#include "System/IO/FileMode.hpp"
#include "System/IO/FileStream.hpp"

#include "cnahouse/physics/BroadPhase.hpp"
#include "cnahouse/physics/CollisionLoader.hpp"
#include "cnahouse/physics/Sweep.hpp"
#include "cnahouse/player/BoundaryGuard.hpp"
#include "cnahouse/player/CellTracker.hpp"
#include "cnahouse/player/PlayerController.hpp"
#include "cnahouse/util/Rng.hpp"
#include "cnahouse/world/SpatialIndex.hpp"
#include "cnahouse/world/WorldData.hpp"
#include "cnahouse/world/WorldLoader.hpp"

namespace
{
    using cnahouse::physics::BroadPhase;
    using cnahouse::physics::CollisionCell;
    using cnahouse::physics::CollisionLoader;
    using cnahouse::physics::CollisionWorld;
    using cnahouse::physics::OverlapCell;
    using cnahouse::player::BoundaryGuard;
    using cnahouse::player::CellTracker;
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
    /// Twenty minutes at §49.3's fixed step.
    constexpr int kSteps = 20 * 60 * 120;
    /// The seed is the ticket, so a failure is reproducible by running the test again.
    constexpr std::uint64_t kSeed = 618u;
    constexpr float kRise = kPlayerHalfHeight + kPlayerRadius;
    /// How often the bot picks a new direction: every 0.25 to 2 s, which is what makes it a WALK
    /// rather than a body spinning on the spot or one that only ever meets a wall head-on.
    constexpr int kMinHold = 30;
    constexpr int kMaxHold = 240;
    /// §49.3's tolerance, written out: see `HOUSE-00617`.
    constexpr float kInside = 1.0e-4F;

    std::string_view Name(cnahouse::util::Id id)
    {
        return IdRegistry::NameOf(id);
    }

    /// Is the cell an interior room? Outdoors the ground is §11.5's height field and has nothing
    /// to do with the level's FFL.
    bool indoors(const world::WorldData& data, cnahouse::util::Id id)
    {
        const world::Cell* cell = data.FindCell(id);
        return cell != nullptr && cell->kind != world::CellKind::Exterior;
    }

    /// The finished floor level of whatever cell @p id names, or a number nothing is below.
    float levelFloor(const world::WorldData& data, cnahouse::util::Id id)
    {
        const world::Cell* cell = data.FindCell(id);
        if (cell == nullptr)
        {
            return -1e9F;
        }
        const world::Level* level = data.FindLevel(cell->level);
        return level == nullptr ? -1e9F : level->ffl;
    }

    /// A capsule can have its feet below the current level while its centre is still in that
    /// level's stair cell: this is the valid transition through the exact authored rectangle of a
    /// downward stair-well portal, including the short airborne step between ramp contacts. A drop
    /// anywhere outside that declared opening remains a failure.
    bool OnDownwardStair(const world::WorldData& data, cnahouse::util::Id cellId, const PlayerState& state)
    {
        const world::Cell* cell = data.FindCell(cellId);
        const world::Level* level = cell == nullptr ? nullptr : data.FindLevel(cell->level);
        if (level == nullptr)
        {
            return false;
        }
        for (const std::uint32_t index : data.PortalsOf(cellId))
        {
            const world::Portal& portal = data.Portals()[index];
            if (portal.kind == world::PortalKind::StairWell && portal.axis == world::PlaneAxis::Y &&
                portal.planeValue <= level->ffl + 1.0e-4F && state.position.X >= portal.minU &&
                state.position.X <= portal.maxU && state.position.Z >= portal.minV &&
                state.position.Z <= portal.maxV)
            {
                return true;
            }
        }
        return false;
    }

    /// Where a portal is, in world space (§15.4's `u`/`v` convention).
    Vector3 PortalCentre(const world::Portal& portal)
    {
        const float u = (portal.minU + portal.maxU) * 0.5F;
        const float v = (portal.minV + portal.maxV) * 0.5F;
        switch (portal.axis)
        {
            case world::PlaneAxis::X:
                return Vector3(portal.planeValue, v, u);
            case world::PlaneAxis::Z:
                return Vector3(u, v, portal.planeValue);
            default:
                return Vector3(u, portal.planeValue, v);
        }
    }

} // namespace

TEST(RandomWalkTests, TwentyMinutesOfWanderingStaysInTheHouse)
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
    ASSERT_TRUE(world::WorldLoader::LoadPortals(directory, contents));
    auto built = world::WorldData::Create(std::move(contents));
    ASSERT_TRUE(built) << built.Error().ToString();
    const world::WorldData& data = built.Value();
    const world::SpatialIndex index = world::SpatialIndex::Build(data);

    const std::unique_ptr<System::IO::FileStream> stream(
        new System::IO::FileStream(collisionPath, System::IO::FileMode::Open, System::IO::FileAccess::Read));
    const auto loaded = CollisionLoader::Read(*stream, collisionPath);
    ASSERT_TRUE(loaded) << loaded.Error().Message();
    const CollisionWorld& statics = loaded.Value();

    // Where a player starts a new house: §13's entrance hall.
    const world::Cell* start = data.FindCell(cnahouse::util::Intern("L0_HALL"));
    ASSERT_NE(start, nullptr);
    const world::Level* level = data.FindLevel(start->level);
    ASSERT_NE(level, nullptr);
    const world::Footprint& box = start->boxes.front();

    PlayerState state;
    state.position =
        Vector3((box.minX + box.maxX) * 0.5F, level->ffl + kRise + 0.002F, (box.minZ + box.maxZ) * 0.5F);

    Rng rng(kSeed);
    BroadPhase broad;
    CellTracker tracker;
    BoundaryGuard guard;
    tracker.Forget();
    tracker.Update(data, index, state.position);
    ASSERT_TRUE(tracker.Current().IsValid()) << "the bot did not even start in a named cell";

    std::vector<std::string> lostCells;
    std::vector<std::string> wedged;
    std::vector<std::string> visited;
    float deepest = 0.0F;
    int hold = 0;
    bool fell = false;
    int fellAt = -1;
    int firstWedge = -1;
    bool steering = false;
    Vector3 aim;
    int changes = 0;
    int blocked = 0;
    int steppedUp = 0;
    int landings = 0;
    float travelled = 0.0F;

    InputState input;
    input.move.Y = 1.0F;

    for (int step = 0; step < kSteps; ++step)
    {
        if (hold <= 0)
        {
            // A destination rather than a heading, three times in four: one of the current cell's
            // own doorways, picked at random. A pure heading-walk in a house with doors spends
            // twenty minutes in the room it started in -- measured: 9 cells of 96 -- and the
            // places worth soaking are the doorways, which a body has to aim at to find. The
            // fourth is a bare heading, so the bot still walks into walls and corners on purpose.
            const std::span<const std::uint32_t> doors = data.PortalsOf(tracker.Current());
            if (!doors.empty() && rng.NextInt(0, 3) != 0)
            {
                const world::Portal& portal = data.Portals()[doors[static_cast<std::size_t>(
                    rng.NextInt(0, static_cast<std::int32_t>(doors.size()) - 1))]];
                aim = PortalCentre(portal);
                steering = true;
            }
            else
            {
                state.yaw = rng.NextFloat(-3.1415927F, 3.1415927F);
                steering = false;
            }
            // §43.2's walk mode is a TOGGLE, and a bot that never uses it never tests the fast
            // walk's 2.05 m/s against a doorway.
            state.fastWalk = rng.NextInt(0, 3) == 0;
            hold = rng.NextInt(kMinHold, kMaxHold);
        }
        --hold;
        if (steering)
        {
            // §14: yaw 0 looks north (-Z) and positive turns east.
            state.yaw = std::atan2(aim.X - state.position.X, state.position.Z - aim.Z);
        }

        const cnahouse::util::Id before = tracker.Current();
        const CollisionCell* cell = statics.Cell(Name(before));
        if (cell == nullptr)
        {
            lostCells.push_back(std::string(Name(before)) + " has no collision geometry (step " +
                                std::to_string(step) + ")");
            break;
        }
        state.cellId = cell->id;

        const Vector3 was = state.position;
        const PlayerStepReport report = PlayerStep(statics, *cell, broad, state, input, kDt);
        blocked += report.blocked ? 1 : 0;
        steppedUp += report.steppedUp ? 1 : 0;
        landings += report.landing != cnahouse::physics::Landing::None ? 1 : 0;
        travelled += std::sqrt((state.position.X - was.X) * (state.position.X - was.X) +
                               (state.position.Z - was.Z) * (state.position.Z - was.Z));

        // §10.3's playable volume, §10's fifth and last containment layer.
        if (guard.Contain(state.position))
        {
            // Recorded rather than asserted here so the walk carries on and the count at the end
            // is the whole story: one escape and one hundred are different failures.
            lostCells.push_back("crossed §10.3's boundary at (" + std::to_string(state.position.X) + ", " +
                                std::to_string(state.position.Y) + ", " + std::to_string(state.position.Z) +
                                ") on step " + std::to_string(step));
        }

        if (tracker.Update(data, index, state.position))
        {
            ++changes;
            const std::string name(Name(tracker.Current()));
            if (std::find(visited.begin(), visited.end(), name) == visited.end())
            {
                visited.push_back(name);
            }
        }
        if (!tracker.Current().IsValid())
        {
            lostCells.push_back("no cell at (" + std::to_string(state.position.X) + ", " +
                                std::to_string(state.position.Y) + ", " + std::to_string(state.position.Z) +
                                ") on step " + std::to_string(step) + ", last named " +
                                std::string(Name(before)));
            break;
        }

        // Sampled rather than asked every step: `HOUSE-00617` owns this invariant and asks it
        // 41 000 times, and asking it here as well doubles the cost of the longest test in the
        // suite to repeat a thing that is already known.
        // Did it fall into the stair well? §12.3 gives a flight a 0.95 m balustrade and the shell
        // draws the parapets it does draw, but the collision has no guard round a floor's hole --
        // `build_collision.py` says so in as many words and calls it a gap against this phase. So
        // a body that walks over the edge of the well goes down it, which is what the house says
        // and not what a house does.
        // Below the floor of an INDOOR room by more than a step: it has gone down a hole. Not
        // outdoors, where §11.5's ground is the terrain and the front walk is 0.60 m below L0's
        // FFL by construction -- an earlier version of this line counted that as a fall and let
        // every wedge after it through, which is a test that passes for the wrong reason.
        if (!fell && indoors(data, tracker.Current()) && !OnDownwardStair(data, tracker.Current(), state) &&
            state.position.Y - state.Rise() < levelFloor(data, tracker.Current()) - 0.60F)
        {
            fell = true;
            fellAt = step;
            std::printf("  fell at (%.2f, %.2f, %.2f) out of %s\n",
                        static_cast<double>(state.position.X),
                        static_cast<double>(state.position.Y),
                        static_cast<double>(state.position.Z),
                        std::string(Name(tracker.Current())).c_str());
        }

        if (step % 8 == 0)
        {
            const float depth = OverlapCell(statics, *cell, broad, state.Body()).depth;
            deepest = std::max(deepest, depth);
            if (depth > kInside)
            {
                if (wedged.empty())
                {
                    firstWedge = step;
                    const cnahouse::physics::CellOverlap in =
                        OverlapCell(statics, *cell, broad, state.Body());
                    std::printf("  FIRST WEDGE step %d in %s at (%.3f, %.3f, %.3f) depth %.3f shape %u "
                                "n (%.2f,%.2f,%.2f) onGround %d fall %.2f\n",
                                step,
                                std::string(Name(before)).c_str(),
                                static_cast<double>(state.position.X),
                                static_cast<double>(state.position.Y),
                                static_cast<double>(state.position.Z),
                                static_cast<double>(in.depth),
                                in.shape,
                                static_cast<double>(in.normal.X),
                                static_cast<double>(in.normal.Y),
                                static_cast<double>(in.normal.Z),
                                state.onGround ? 1 : 0,
                                static_cast<double>(state.fall.speed));
                }
                wedged.push_back("inside by " + std::to_string(depth) + " m in " + std::string(Name(before)) +
                                 " on step " + std::to_string(step));
            }
        }
    }

    std::printf("  %d step(s) = %.1f minutes; %.0f m walked, %d cell change(s) over %zu named "
                "cell(s); %d blocked, %d step-up(s), %d landing(s); deepest contact %.6f m; "
                "%llu boundary escape(s)\n",
                kSteps,
                static_cast<double>(kSteps) * static_cast<double>(kDt) / 60.0,
                static_cast<double>(travelled),
                changes,
                visited.size(),
                blocked,
                steppedUp,
                landings,
                static_cast<double>(deepest),
                static_cast<unsigned long long>(guard.Escapes()));

    EXPECT_GT(travelled, 500.0F) << "the bot barely moved, so twenty minutes of it proves nothing";
    EXPECT_GT(visited.size(), 5u) << "the bot never left the room it started in";
    EXPECT_EQ(guard.Escapes(), 0u) << "§10.3's boundary was crossed";
    EXPECT_TRUE(lostCells.empty()) << lostCells.size() << " time(s) outside the named cells; first: "
                                   << (lostCells.empty() ? std::string() : lostCells.front());
    // Never inside anything, and never below the floor of a room. Both were false when this test
    // was written -- the bot walked in off the front lawn through a 1.30 m hole in the house's
    // front wall at the main stair, and spent its last four minutes wedged under the ground floor
    // -- and `HOUSE-00567` is what closed them.
    if (fell)
    {
        std::printf("  went below an indoor floor on step %d (%.1f minutes in); first wedge on "
                    "step %d\n",
                    fellAt,
                    static_cast<double>(fellAt) * static_cast<double>(kDt) / 60.0,
                    firstWedge);
    }
    EXPECT_FALSE(fell) << "the bot ended up below the floor of a room it was in";
    EXPECT_TRUE(wedged.empty()) << wedged.size() << " sample(s) found the body inside geometry; first: "
                                << (wedged.empty() ? std::string() : wedged.front());
}
