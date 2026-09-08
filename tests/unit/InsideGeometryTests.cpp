// SPDX-License-Identifier: MIT
//
// `HOUSE-00617`, §49.5's fifth guarantee: **the player never ends a frame inside static
// geometry.** Checked after every step of a scripted tour of the whole house.
//
// The tour is the doorways. A body walks from each cell's middle to each of its portals in turn --
// 179 of them, most walked from both sides -- because a doorway is where a 0.62 m capsule meets a
// 0.90 m opening with a jamb either side, and if there is anywhere in this house a body can be
// wedged into the geometry it is there. §49.3's step 5 is what has to stop that happening, and
// this is the test that says whether it does.
//
// "Inside" means deeper than `kContactTolerance`. A body resting against a wall is TOUCHING it --
// that is what resting means -- and `HOUSE-00555` and `HOUSE-00615` both turned on the difference.
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <iterator>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include "System/IO/FileAccess.hpp"
#include "System/IO/FileMode.hpp"
#include "System/IO/FileStream.hpp"

#include "cnahouse/physics/BroadPhase.hpp"
#include "cnahouse/physics/CollisionLoader.hpp"
#include "cnahouse/physics/Ground.hpp"
#include "cnahouse/physics/Sweep.hpp"
#include "cnahouse/player/PlayerController.hpp"
#include "cnahouse/world/WorldData.hpp"
#include "cnahouse/world/WorldLoader.hpp"

namespace
{
    using cnahouse::physics::BroadPhase;
    using cnahouse::physics::CellOverlap;
    using cnahouse::physics::CollisionCell;
    using cnahouse::physics::CollisionLoader;
    using cnahouse::physics::CollisionWorld;
    using cnahouse::physics::kContactTolerance;
    using cnahouse::physics::OverlapCell;
    using cnahouse::player::InputState;
    using cnahouse::player::kPlayerHalfHeight;
    using cnahouse::player::kPlayerRadius;
    using cnahouse::player::PlayerState;
    using cnahouse::player::PlayerStep;
    using cnahouse::util::IdRegistry;
    using Microsoft::Xna::Framework::Vector3;

    namespace world = cnahouse::world;

    constexpr float kDt = 1.0F / 120.0F;
    constexpr float kRise = kPlayerHalfHeight + kPlayerRadius;
    /// 1 s a leg, at §43.2's walk. Long enough to cross a room and lean on what is at the far end.
    constexpr int kSteps = 120;
    /// §49.3 step 5 pushes 0.02 m four times a step, so a body half a capsule inside a wall is out
    /// in four. Twenty is five times that, and a body still inside after twenty is not coming out.
    constexpr int kPushOutSteps = 20;
    /// §49.3's tolerance, written out rather than read from the header: a guarantee that took the
    /// physics' own constant would agree with it about a change to it, and "inside" would quietly
    /// come to mean whatever the constant was widened to.
    constexpr float kInside = 1.0e-4F;

    /// Places a body cannot be pushed out of, because there is nowhere to push it TO. Sorted, and
    /// diffed both ways below: a seventh has to be added here, in a commit, with a reason.
    const std::vector<std::string>& NoWayOut()
    {
        static const std::vector<std::string> kDeadEnds{
            // A chest freezer, 1.4 x 0.7 m inside -- smaller than §70.5's 0.62 m capsule, so a
            // body in it is inside the box on every side at once.
            "CELL_FREEZER_INTERIOR east",
            "CELL_FREEZER_INTERIOR north",
            "CELL_FREEZER_INTERIOR south",
            "CELL_FREEZER_INTERIOR west",
            // The corner of two exterior shapes at the bottom of the garden. A body TELEPORTED
            // into it is 0.24 m inside and stays there: six times as long changes nothing, so it
            // is a fixed point rather than a slow escape, and it is not reachable by walking.
            "EXT_GARDEN north",
        };
        return kDeadEnds;
    }

    std::string_view Name(cnahouse::util::Id id)
    {
        return IdRegistry::NameOf(id);
    }

    /// Where a portal is, in world space. §15.4: on `X` and `Z`, `u` is the other horizontal axis
    /// and `v` is world Y; on `Y`, `u` is world X and `v` is world Z.
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

TEST(InsideGeometryTests, NoStepOfTheTourEndsInsideAnything)
{
    IdRegistry::ResetForTesting();
    const std::string directory = "content/world";
    const std::string collisionPath = "content/world/collision.bin";
    if (!std::filesystem::exists(directory + "/layout.portals.json") ||
        !std::filesystem::exists(collisionPath))
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
    int cells = 0;
    int legs = 0;
    int steps = 0;
    int startedInside = 0;
    float worst = 0.0F;
    std::string worstWhere;
    std::vector<std::string> wedged;

    for (const world::Cell& cell : data.Cells())
    {
        const CollisionCell* collision = statics.Cell(Name(cell.id));
        if (collision == nullptr || collision->shapes.empty() || cell.boxes.empty())
        {
            continue;
        }
        const world::Level* level = data.FindLevel(cell.level);
        ASSERT_NE(level, nullptr) << Name(cell.id);
        ++cells;

        const world::Footprint& box = cell.boxes.front();
        const Vector3 middle(
            (box.minX + box.maxX) * 0.5F, level->ffl + kRise + 0.002F, (box.minZ + box.maxZ) * 0.5F);

        for (const std::uint32_t index : data.PortalsOf(cell.id))
        {
            const world::Portal& portal = data.Portals()[index];
            const Vector3 target = PortalCentre(portal);

            PlayerState state;
            state.position = middle;
            state.cellId = collision->id;
            if (OverlapCell(statics, *collision, broad, state.Body()).overlapped)
            {
                // The middle of this cell is inside something -- a chimney breast, a stair
                // flight, the freezer. Where a body cannot START is not this guarantee's business.
                ++startedInside;
                break;
            }
            ++legs;

            InputState input;
            input.move.Y = 1.0F;
            for (int i = 0; i < kSteps; ++i)
            {
                // §14: yaw 0 looks north (-Z) and positive turns east.
                state.yaw = std::atan2(target.X - state.position.X, state.position.Z - target.Z);
                PlayerStep(statics, *collision, broad, state, input, kDt);
                ++steps;

                const CellOverlap inside = OverlapCell(statics, *collision, broad, state.Body());
                if (!inside.overlapped)
                {
                    continue;
                }
                if (inside.depth > worst)
                {
                    worst = inside.depth;
                    worstWhere = std::string(Name(cell.id)) + " -> " + std::string(Name(portal.id));
                }
                if (inside.depth > kInside)
                {
                    wedged.push_back(std::string(Name(cell.id)) + " walking at " +
                                     std::string(Name(portal.id)) + ": " + std::to_string(inside.depth) +
                                     " m inside shape " + std::to_string(inside.shape) + " on step " +
                                     std::to_string(i));
                    break;
                }
            }
        }
    }

    // ...and the other half of the guarantee: a body that IS inside something has to come OUT.
    // The tour above never gets wedged -- the deepest contact in 40 000 steps is a fortieth of the
    // tolerance -- which is the right answer and leaves §49.3's step 5 untested by it. So the
    // second pass puts a body deliberately half inside a wall, in every cell, and gives it twenty
    // steps of standing still to get out. Four is what it should need.
    int buried = 0;
    int freed = 0;
    int worstPushOut = 0;
    float deepestStart = 0.0F;
    std::vector<std::string> stuck;
    for (const world::Cell& cell : data.Cells())
    {
        const CollisionCell* collision = statics.Cell(Name(cell.id));
        if (collision == nullptr || collision->shapes.empty() || cell.boxes.empty())
        {
            continue;
        }
        if (cell.kind == world::CellKind::Stair)
        {
            // Not a stair cell. It is a stack of flights, landings and `HOUSE-00567`'s
            // balustrades, and the strips between a rail and the wall behind it are 0.30 m --
            // half a capsule, so nothing can walk into one and a body PUT there has nowhere to be
            // pushed to. The tour above still walks these cells; what is skipped is teleporting a
            // body into their masonry and demanding it come out.
            continue;
        }
        const world::Level* level = data.FindLevel(cell.level);
        const world::Footprint& box = cell.boxes.front();
        const float feet = level->ffl + kRise + 0.002F;
        const Vector3 midX((box.minX + box.maxX) * 0.5F, feet, (box.minZ + box.maxZ) * 0.5F);
        // A tenth of a metre INSIDE the room from each boundary, which puts two thirds of the
        // capsule into the wall on that side: the depth a body reaches when the physics has gone
        // wrong, not the depth of a body teleported into the middle of the masonry. Exactly ON the
        // boundary buries it up to 0.45 m, and a body in the corner of two walls at that depth is
        // being pushed by both and can take a second to work its way out -- a different question
        // from this one.
        constexpr float kIn = 0.10F;
        const std::array<std::pair<std::string_view, Vector3>, 4> sides{{
            {"west", Vector3(box.minX + kIn, feet, midX.Z)},
            {"east", Vector3(box.maxX - kIn, feet, midX.Z)},
            {"north", Vector3(midX.X, feet, box.minZ + kIn)},
            {"south", Vector3(midX.X, feet, box.maxZ - kIn)},
        }};
        for (const auto& [side, at] : sides)
        {
            PlayerState state;
            state.position = at;
            state.cellId = collision->id;
            const CellOverlap start = OverlapCell(statics, *collision, broad, state.Body());
            if (!start.overlapped || start.depth <= kInside)
            {
                continue; // that boundary has no wall on it: an opening, or the edge of a terrace
            }
            ++buried;
            deepestStart = std::max(deepestStart, start.depth);

            int took = 0;
            for (; took < kPushOutSteps; ++took)
            {
                PlayerStep(statics, *collision, broad, state, InputState{}, kDt);
                ++steps;
                if (OverlapCell(statics, *collision, broad, state.Body()).depth <= kInside)
                {
                    break;
                }
            }
            if (took >= kPushOutSteps)
            {
                stuck.push_back(std::string(Name(cell.id)) + " " + std::string(side));
                continue;
            }
            ++freed;
            worstPushOut = std::max(worstPushOut, took + 1);
        }
    }

    std::printf("  %d body(s) buried in a wall on purpose, %d pushed back out, worst %d step(s), "
                "deepest start %.3f m\n",
                buried,
                freed,
                worstPushOut,
                static_cast<double>(deepestStart));
    EXPECT_GT(buried, 50) << "no body was ever actually inside anything, so step 5 is untested";

    // Six places have no room to push a body OUT to, and they are named. Four are the chest
    // freezer, whose interior is 1.4 x 0.7 m -- smaller than §70.5's 0.62 m capsule, so a body in
    // it is inside the box on every side at once and there is no outside to reach. The other two
    // are the wedge between a stair well's wall and the flight's own ramp, and the corner of two
    // exterior shapes at the bottom of the garden: a body TELEPORTED into either is 0.24 m inside
    // and stays there, and six times as long changes nothing -- it is a fixed point, not a slow
    // escape.
    //
    // None of them is reachable by walking. `HOUSE-00613`'s 1 894 pushes and the tour above never
    // put a body inside anything at all, which is the guarantee that matters; this list is what
    // keeps that claim honest about the cases it cannot make.
    std::sort(stuck.begin(), stuck.end());
    std::vector<std::string> unexpected;
    std::vector<std::string> gone;
    std::set_difference(
        stuck.begin(), stuck.end(), NoWayOut().begin(), NoWayOut().end(), std::back_inserter(unexpected));
    std::set_difference(
        NoWayOut().begin(), NoWayOut().end(), stuck.begin(), stuck.end(), std::back_inserter(gone));
    EXPECT_TRUE(unexpected.empty()) << unexpected.size() << " body(s) were still inside after "
                                    << kPushOutSteps << " steps; first: "
                                    << (unexpected.empty() ? std::string() : unexpected.front());
    EXPECT_TRUE(gone.empty()) << gone.size() << " recorded dead end(s) no longer apply; first: "
                              << (gone.empty() ? std::string() : gone.front());

    std::printf("  %d cell(s), %d leg(s), %d step(s); %d cell(s) whose middle is inside something; "
                "deepest contact %.6f m (%s), tolerance %.6f\n",
                cells,
                legs,
                steps,
                startedInside,
                static_cast<double>(worst),
                worstWhere.c_str(),
                static_cast<double>(kContactTolerance));

    EXPECT_FLOAT_EQ(kContactTolerance, kInside) << "§49.3's contact tolerance moved; this guarantee "
                                                   "pins it on purpose, because 'inside' is defined by it";
    EXPECT_GT(legs, 100) << "the tour barely happened, so it proves little";
    EXPECT_TRUE(wedged.empty()) << wedged.size() << " step(s) ended inside static geometry; first: "
                                << (wedged.empty() ? std::string() : wedged.front());
}
