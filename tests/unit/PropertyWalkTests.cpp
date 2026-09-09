// SPDX-License-Identifier: MIT
//
// `HOUSE-00782`, phase 10's guarantee: **the property is fully walkable and fully bounded.**
//
// Two halves of one question, and a random walk answers neither. §49.5's twenty-minute bot proves
// that nothing goes wrong along the way it happens to go; it cannot say that the orchard is
// reachable, because a seeded walk that never turns down the side yard proves nothing about the
// orchard. So this is a flood fill instead: every place a body can stand on the lot, found by
// standing one there and asking §49.3's own collision, and then joined up by sweeping between
// neighbours the way the controller does.
//
// **Walkable** is then "every open exterior cell has ground the fill reached", and **bounded** is
// "the fill never left §10.3's playable volume" -- not that the guard was never touched, but that
// it never had to be, which is what §10.4 means by calling it a safety net.
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <deque>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "System/IO/FileAccess.hpp"
#include "System/IO/FileMode.hpp"
#include "System/IO/FileStream.hpp"

#include "cnahouse/physics/BroadPhase.hpp"
#include "cnahouse/physics/CollisionLoader.hpp"
#include "cnahouse/physics/Move.hpp"
#include "cnahouse/physics/Sweep.hpp"
#include "cnahouse/physics/Terrain.hpp"
#include "cnahouse/player/BoundaryGuard.hpp"
#include "cnahouse/player/PlayerController.hpp"
#include "cnahouse/util/Json.hpp"
#include "cnahouse/world/SpatialIndex.hpp"
#include "cnahouse/world/WorldData.hpp"
#include "cnahouse/world/WorldLoader.hpp"

namespace
{
    using cnahouse::physics::BroadPhase;
    using cnahouse::physics::Capsule;
    using cnahouse::physics::CollisionCell;
    using cnahouse::physics::CollisionLoader;
    using cnahouse::physics::CollisionWorld;
    using cnahouse::physics::kStepUpHeight;
    using cnahouse::physics::OverlapCell;
    using cnahouse::physics::SweepCell;
    using cnahouse::physics::TerrainAt;
    using cnahouse::player::kPlayerHalfHeight;
    using cnahouse::player::kPlayerRadius;
    using cnahouse::player::PlayableVolume;
    using cnahouse::util::IdRegistry;
    using Microsoft::Xna::Framework::Vector3;

    namespace world = cnahouse::world;

    /// A quarter of a metre. Fine enough to stand a 0.62 m body in §11.1's 0.90 m shed doorway --
    /// at half a metre the only squares in it put the body's shoulder in a jamb, and the shed read
    /// as unreachable -- and coarse enough that the lot is 320 x 256 squares rather than a million.
    constexpr float kStride = 0.25F;
    constexpr float kRise = kPlayerHalfHeight + kPlayerRadius;

    std::string_view Name(cnahouse::util::Id id)
    {
        return IdRegistry::NameOf(id);
    }

} // namespace

TEST(PropertyWalkTests, EveryPartOfTheLotIsReachableAndNoneOfItLeavesTheBox)
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
    const world::SpatialIndex index = world::SpatialIndex::Build(data);

    const std::unique_ptr<System::IO::FileStream> stream(
        new System::IO::FileStream(collisionPath, System::IO::FileMode::Open, System::IO::FileAccess::Read));
    const auto loaded = CollisionLoader::Read(*stream, collisionPath);
    ASSERT_TRUE(loaded) << loaded.Error().Message();
    const CollisionWorld& statics = loaded.Value();
    ASSERT_TRUE(statics.terrain.present);

    // §65.6 starts every door shut and §11.1 calls the rear gate "locked, decorative": a gate is
    // a leaf that opens (§49.4), so the static file carries the HOLE and the leaf is a dynamic
    // obstacle. This walk is the property with its gates shut, which is what "the property is
    // fully walkable and fully bounded" means: what a body can reach without opening anything.
    std::vector<std::array<float, 4>> shut;
    {
        const std::string path = directory + "/layout.exterior.json";
        ASSERT_TRUE(std::filesystem::exists(path)) << path;
        std::ifstream file(path);
        const std::string text((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
        auto document = cnahouse::util::JsonDocument::Parse(text, "layout.exterior.json");
        ASSERT_TRUE(document) << document.Error().ToString();
        auto gates = document->Root().RequireArray("gates");
        ASSERT_TRUE(gates) << gates.Error().ToString();
        auto rows = gates->Elements();
        ASSERT_TRUE(rows) << rows.Error().ToString();
        for (const cnahouse::util::JsonValue& gate : *rows)
        {
            auto opening = gate.RequireObject("opening");
            ASSERT_TRUE(opening) << opening.Error().ToString();
            auto x = opening->RequireArray("x");
            auto z = opening->RequireArray("z");
            ASSERT_TRUE(x && z);
            auto xs = x->Elements();
            auto zs = z->Elements();
            ASSERT_TRUE(xs && zs);
            ASSERT_EQ(xs->size(), 2u);
            ASSERT_EQ(zs->size(), 2u);
            shut.push_back({(*xs)[0].AsFloat().Value(),
                            (*xs)[1].AsFloat().Value(),
                            (*zs)[0].AsFloat().Value(),
                            (*zs)[1].AsFloat().Value()});
        }
        ASSERT_EQ(shut.size(), 3u) << "§11.2 gives the property three gates";
    }
    const auto throughAShutGate = [&shut](const Vector3& from, const Vector3& to)
    {
        for (const auto& gate : shut)
        {
            const bool a = from.X >= gate[0] - 0.4F && from.X <= gate[1] + 0.4F && from.Z >= gate[2] - 0.6F &&
                           from.Z <= gate[3] + 0.6F;
            const bool b = to.X >= gate[0] - 0.4F && to.X <= gate[1] + 0.4F && to.Z >= gate[2] - 0.6F &&
                           to.Z <= gate[3] + 0.6F;
            if (a || b)
            {
                return true;
            }
        }
        return false;
    };

    const PlayableVolume volume;
    const auto columns = static_cast<int>((volume.maxX - volume.minX) / kStride) + 1;
    const auto rows = static_cast<int>((volume.maxZ - volume.minZ) / kStride) + 1;
    const auto at = [&](int i, int j)
    {
        return Vector3(volume.minX + static_cast<float>(i) * kStride,
                       0.0F,
                       volume.minZ + static_cast<float>(j) * kStride);
    };

    BroadPhase broad;
    // Where a body standing at each square would be, and which cell answers for it. A square with
    // no cell, no ground or something in it is not somewhere a body can be.
    std::vector<float> feet(static_cast<std::size_t>(columns * rows), 0.0F);
    std::vector<const CollisionCell*> owner(static_cast<std::size_t>(columns * rows), nullptr);
    std::vector<cnahouse::util::Id> cellOf(static_cast<std::size_t>(columns * rows));
    std::size_t standable = 0;
    for (int j = 0; j < rows; ++j)
    {
        for (int i = 0; i < columns; ++i)
        {
            const Vector3 square = at(i, j);
            const float lawn = TerrainAt(statics.terrain, square.X, square.Z).height;
            cnahouse::util::Id id = index.Find(data, Vector3(square.X, lawn + kRise, square.Z));
            const CollisionCell* cell = id.IsValid() ? statics.Cell(Name(id)) : nullptr;
            if (cell == nullptr)
            {
                continue;
            }
            // A 50 mm PROBE finds the ground, and the body is then stood on it with 50 mm of
            // daylight. Neither half is arbitrary: a body-sized drop cannot be used in a doorway,
            // because a 1.80 m capsule dropped from anywhere clear of the floor has its head in
            // the wall over a 2.05 m door and lands on top of it -- which is how §11.1's shed read
            // as a building with no way in -- and a body placed exactly ON the surface is 35 mm
            // inside a sloped one, which is how the porch steps read as unstandable.
            const Capsule pebble = cnahouse::physics::Sphere(Vector3(square.X, lawn + 1.0F, square.Z), 0.05F);
            const Vector3 down(0.0F, -2.0F, 0.0F);
            const auto onShapes = SweepCell(statics, *cell, broad, pebble, down);
            const auto onGround = cell->outdoors
                                      ? cnahouse::physics::SweepCapsuleTerrain(statics.terrain, pebble, down)
                                      : cnahouse::physics::SweepHit{};
            float travel = 0.0F;
            if (onShapes.hit && (!onGround.hit || onShapes.time <= onGround.time))
            {
                travel = 2.0F * onShapes.time;
            }
            else if (onGround.hit)
            {
                travel = 2.0F * onGround.time;
            }
            else
            {
                continue; // nothing under this square within a metre either way of the lawn
            }
            const float ground = pebble.centre.Y - travel - 0.05F;
            const Vector3 centre(square.X, ground + kRise + 0.05F, square.Z);
            id = index.Find(data, centre, id);
            const CollisionCell* standingIn = id.IsValid() ? statics.Cell(Name(id)) : nullptr;
            if (standingIn == nullptr)
            {
                continue;
            }
            // The lot and the one building on it, not the house: §49.5's bot owns the indoors.
            const world::Cell* row = data.FindCell(id);
            if (row == nullptr || row->kind != world::CellKind::Exterior)
            {
                continue;
            }
            cell = standingIn;
            const Capsule body{centre, kPlayerHalfHeight, kPlayerRadius};
            const auto inside = OverlapCell(statics, *cell, broad, body);
            if (inside.overlapped)
            {
                continue;
            }
            const std::size_t k = static_cast<std::size_t>(j * columns + i);
            feet[k] = ground;
            owner[k] = cell;
            cellOf[k] = id;
            ++standable;
        }
    }
    ASSERT_GT(standable, 3000u) << "only " << standable << " squares of the lot can be stood on";

    // The front walk, outside our own door: where a player arrives.
    const int startI = static_cast<int>((0.0F - volume.minX) / kStride);
    const int startJ = static_cast<int>((-9.0F - volume.minZ) / kStride);
    ASSERT_NE(owner[static_cast<std::size_t>(startJ * columns + startI)], nullptr)
        << "the front walk is not somewhere a body can stand";

    // ...and now walk. A step between two squares is legal when a swept capsule gets there, which
    // is `MoveWithStepAssist`'s own question minus the assist: the step up is asked for separately
    // so that a kerb is a step and a fence is not.
    std::vector<bool> reached(static_cast<std::size_t>(columns * rows), false);
    std::deque<int> queue;
    const std::size_t start = static_cast<std::size_t>(startJ * columns + startI);
    reached[start] = true;
    queue.push_back(static_cast<int>(start));
    std::size_t walked = 1;
    while (!queue.empty())
    {
        const int here = queue.front();
        queue.pop_front();
        const int i = here % columns;
        const int j = here / columns;
        for (const auto& step : std::array<std::pair<int, int>, 4>{{{1, 0}, {-1, 0}, {0, 1}, {0, -1}}})
        {
            const int ni = i + step.first;
            const int nj = j + step.second;
            if (ni < 0 || nj < 0 || ni >= columns || nj >= rows)
            {
                continue;
            }
            const std::size_t there = static_cast<std::size_t>(nj * columns + ni);
            if (reached[there] || owner[there] == nullptr)
            {
                continue;
            }
            // §43.1's step-up, and nothing more generous. A quarter-metre stride is what makes
            // that enough for the ramps too: §12.4's steepest flight climbs 0.16 m in it and
            // §11.6's terrace and porch steps less, so a ramp is a sequence of legal steps and a
            // 0.30 m plinth round a yard -- which is what an exterior cell's floor SLAB is over
            // ground that has dropped away from it -- is the wall it is to a player.
            if (feet[there] - feet[static_cast<std::size_t>(here)] > kStepUpHeight)
            {
                continue;
            }
            const Vector3 from(at(i, j).X, feet[static_cast<std::size_t>(here)] + kRise, at(i, j).Z);
            const Vector3 to(at(ni, nj).X, feet[there] + kRise, at(ni, nj).Z);
            const Capsule body{from, kPlayerHalfHeight, kPlayerRadius};
            const Vector3 motion(to.X - from.X, to.Y - from.Y, to.Z - from.Z);
            if (throughAShutGate(from, to))
            {
                continue;
            }
            const CollisionCell& walking = *owner[static_cast<std::size_t>(here)];
            const auto met = SweepCell(statics, walking, broad, body, motion);
            // A ramp is not a wall: §49.3's slide follows a surface a body can stand on, which is
            // §43.1's 46 degrees and `IsWalkable`'s own test.
            bool got = !met.hit || cnahouse::physics::IsWalkable(met.normal);
            if (!got)
            {
                // §49.3's step 4: lift by the step-up and try again. A terrace step and a porch
                // step are RISERS -- the body meets the face of one and climbs it -- so a fill
                // that only swept horizontally found the terrace and the porch unreachable, which
                // is the fill missing the assist rather than the house missing its steps.
                Capsule lifted = body;
                lifted.centre = Vector3(from.X, from.Y + kStepUpHeight, from.Z);
                got = !SweepCell(statics, walking, broad, lifted, motion).hit;
            }
            if (!got)
            {
                continue;
            }
            reached[there] = true;
            ++walked;
            queue.push_back(static_cast<int>(there));
        }
    }

    // Which cells the fill got into, and how much of each.
    std::map<std::string, std::size_t> visited;
    float furthestX = 0.0F;
    float furthestZ = -100.0F;
    float furthestNorth = 0.0F;
    for (int j = 0; j < rows; ++j)
    {
        for (int i = 0; i < columns; ++i)
        {
            const std::size_t k = static_cast<std::size_t>(j * columns + i);
            if (!reached[k])
            {
                continue;
            }
            ++visited[std::string(Name(cellOf[k]))];
            furthestX = std::max(furthestX, std::fabs(at(i, j).X));
            furthestZ = std::max(furthestZ, at(i, j).Z);
            furthestNorth = std::min(furthestNorth, at(i, j).Z);
        }
    }

    std::printf("  %zu of %zu standable square(s) reached from the front walk, over %zu cell(s); "
                "furthest |x| %.1f m, z from %.1f to %.1f m\n",
                walked,
                standable,
                visited.size(),
                static_cast<double>(furthestX),
                static_cast<double>(furthestNorth),
                static_cast<double>(furthestZ));

    // §10.3's "Property (fenced)": what the fence encloses, and therefore what "the property is
    // fully walkable" is about. The road corridor is outside it and `RoadEndTests` is what walks
    // that; `EXT_NORTHSTRIP` is the alley behind the rear fence, which §11.1 reaches through a
    // gate it calls "locked, decorative".
    constexpr float kFenceX = 22.5F;
    constexpr float kFenceNorth = -48.0F;
    const auto insideTheFence = [&](const world::Cell& cell)
    {
        for (const world::Footprint& box : cell.boxes)
        {
            if (box.minX < -kFenceX - 0.01F || box.maxX > kFenceX + 0.01F || box.minZ < kFenceNorth - 0.01F ||
                box.maxZ > 0.01F)
            {
                return false;
            }
        }
        return !cell.boxes.empty();
    };

    // WALKABLE: every open exterior cell inside the fence, from the front door, without opening
    // anything. The shed is one of them -- §11.1 calls it enterable and its door is a hole.
    std::vector<std::string> unreachable;
    std::size_t counted = 0;
    for (const world::Cell& cell : data.Cells())
    {
        if (cell.kind != world::CellKind::Exterior || cell.visibilityHint != world::VisibilityHint::Open)
        {
            continue;
        }
        // A balcony is an exterior cell you reach through a bedroom, and this walk starts on the
        // front path: §49.5's bot is what walks the house. The ground storey is found rather than
        // named, the way `terrain_gen` finds it -- the lowest level at or above grade.
        const world::Level* level = data.FindLevel(cell.level);
        if (level == nullptr || level->ffl > 0.6F || !insideTheFence(cell))
        {
            continue;
        }
        ++counted;
        const std::string name(Name(cell.id));
        const auto found = visited.find(name);
        if (found == visited.end() || found->second < 4u)
        {
            unreachable.push_back(name + " (" + std::to_string(found == visited.end() ? 0u : found->second) +
                                  " square(s))");
        }
    }
    EXPECT_GT(counted, 8u) << "only " << counted << " cells were checked; §11.1 has more than that";
    EXPECT_TRUE(unreachable.empty()) << unreachable.size()
                                     << " outdoor cell(s) cannot be walked to from the front door; first: "
                                     << (unreachable.empty() ? std::string() : unreachable.front());
    // The shed by name, because §11.1 calls it enterable and a shed you cannot get into is the
    // defect `HOUSE-00374` found in the layout and `HOUSE-00768` drew the door for.
    EXPECT_GT(visited["EXT_SHED"], 8u) << "the garden shed cannot be walked into";

    // BOUNDED: with its gates shut the property is CLOSED. Not "the boundary counter stayed at
    // zero" -- §10.3's box is 17 m away at its nearest -- but that the fill never got out of the
    // fence at all, which is the thing the counter is a net under.
    EXPECT_LE(furthestX, kFenceX + 0.01F)
        << "the fill reached |x| " << furthestX << ", which is past the property fence";
    EXPECT_LE(furthestZ, 0.01F) << "the fill reached z " << furthestZ << ", which is past the front fence";
    EXPECT_GE(furthestNorth, kFenceNorth - 0.01F)
        << "the fill reached z " << furthestNorth << ", which is past the rear fence";
}
