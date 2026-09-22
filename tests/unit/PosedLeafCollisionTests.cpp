// SPDX-License-Identifier: MIT
//
// `HOUSE-03223`: the drawn fixed door/gate poses are real static obstacles, not scenery the
// player walks through. The collision writer gives every leaf a stable surface name so this test
// can prove the complete set survived the Python -> binary -> C++ boundary.
#include <algorithm>
#include <cmath>
#include <filesystem>
#include <memory>
#include <set>
#include <string>
#include <string_view>

#include <gtest/gtest.h>

#include "System/IO/FileAccess.hpp"
#include "System/IO/FileMode.hpp"
#include "System/IO/FileStream.hpp"

#include "cnahouse/physics/CollisionLoader.hpp"
#include "cnahouse/physics/Sweep.hpp"
#include "cnahouse/world/WorldData.hpp"
#include "cnahouse/world/WorldLoader.hpp"

namespace
{
    using cnahouse::physics::Capsule;
    using cnahouse::physics::CollisionLoader;
    using cnahouse::physics::CollisionObb;
    using cnahouse::physics::CollisionWorld;
    using cnahouse::physics::SweepCapsuleObb;
    using cnahouse::util::IdRegistry;
    using Microsoft::Xna::Framework::Vector3;
    namespace world = cnahouse::world;

    constexpr std::string_view kDoorPrefix = "door_leaf:";
    constexpr std::string_view kGatePrefix = "gate_leaf:";

    struct Production
    {
        world::WorldData layout;
        CollisionWorld collision;
    };

    Production Load()
    {
        IdRegistry::ResetForTesting();
        const std::string directory = "content/world";
        world::WorldData::Contents contents;
        EXPECT_TRUE(world::WorldLoader::LoadLevels(directory, contents));
        EXPECT_TRUE(world::WorldLoader::LoadCells(directory, contents));
        EXPECT_TRUE(world::WorldLoader::LoadPortals(directory, contents));
        EXPECT_TRUE(world::WorldLoader::LoadOpenings(directory, contents));
        auto built = world::WorldData::Create(std::move(contents));
        EXPECT_TRUE(built) << (built ? std::string() : built.Error().ToString());

        const std::string path = directory + "/collision.bin";
        System::IO::FileStream stream(path, System::IO::FileMode::Open, System::IO::FileAccess::Read);
        auto collision = CollisionLoader::Read(stream, path);
        EXPECT_TRUE(collision) << (collision ? std::string() : collision.Error().Message());
        return Production{std::move(built.Value()), std::move(collision.Value())};
    }

    bool StartsWith(std::string_view text, std::string_view prefix)
    {
        return text.size() >= prefix.size() && text.substr(0, prefix.size()) == prefix;
    }

    const CollisionObb* FindObb(const CollisionWorld& collision, std::string_view surface)
    {
        for (const CollisionObb& obb : collision.obbs)
        {
            if (collision.SurfaceName(obb.surface) == surface)
            {
                return &obb;
            }
        }
        return nullptr;
    }

} // namespace

TEST(PosedLeafCollisionTests, EveryAuthoredWalkthroughLeafHasOneTraceableProxy)
{
    if (!std::filesystem::exists("content/world/collision.bin"))
    {
        GTEST_SKIP() << "no deployed world; run tools/ci/build_content.py --only world";
    }
    const Production production = Load();
    std::set<std::string> actual;
    std::size_t doorObbs = 0u;
    std::size_t gateObbs = 0u;
    for (const CollisionObb& obb : production.collision.obbs)
    {
        const std::string name(production.collision.SurfaceName(obb.surface));
        if (StartsWith(name, kDoorPrefix))
        {
            ++doorObbs;
            EXPECT_TRUE(actual.insert(name).second) << name;
        }
        else if (StartsWith(name, kGatePrefix))
        {
            ++gateObbs;
            EXPECT_TRUE(actual.insert(name).second) << name;
        }
    }
    std::size_t doorMeshes = 0u;
    for (const auto& mesh : production.collision.meshes)
    {
        const std::string name(production.collision.SurfaceName(mesh.surface));
        if (StartsWith(name, kDoorPrefix))
        {
            ++doorMeshes;
            EXPECT_TRUE(actual.insert(name).second) << name;
        }
    }

    std::set<std::string> expected;
    for (const world::Opening& opening : production.layout.Openings())
    {
        if (opening.kind != world::OpeningKind::Door)
        {
            continue;
        }
        const std::string type(IdRegistry::NameOf(opening.type));
        if (type.empty() || type[0] != 'D' || type == "D_APPLIANCE")
        {
            continue;
        }
        const std::string base = "door_leaf:" + std::string(IdRegistry::NameOf(opening.id)) + ":";
        EXPECT_TRUE(expected.insert(base + "0").second);
        if (type == "D_DOUBLE" || type == "D_SLIDER")
        {
            EXPECT_TRUE(expected.insert(base + "1").second);
        }
    }
    for (const char* gate : {"EXT_GATE_PED", "EXT_GATE_DRIVE", "EXT_GATE_REAR"})
    {
        expected.insert(std::string("gate_leaf:") + gate + ":0");
    }

    EXPECT_EQ(actual, expected);
    EXPECT_EQ(doorObbs, 69u);
    EXPECT_EQ(doorMeshes, 1u) << "the pitched sectional garage leaf is the one mesh";
    EXPECT_EQ(gateObbs, 3u);
}

TEST(PosedLeafCollisionTests, HingedSliderAndGarageProxiesRetainTheirPoses)
{
    if (!std::filesystem::exists("content/world/collision.bin"))
    {
        GTEST_SKIP() << "no deployed world; run tools/ci/build_content.py --only world";
    }
    const Production production = Load();

    int hinged = 0;
    for (const CollisionObb& obb : production.collision.obbs)
    {
        const std::string_view name = production.collision.SurfaceName(obb.surface);
        if (!StartsWith(name, kDoorPrefix))
        {
            continue;
        }
        if (std::abs(obb.yaw) > 1.30F)
        {
            ++hinged;
        }
    }
    EXPECT_EQ(hinged, 63) << "all open hinged leaves retain their 85.5/90 degree pose";

    const CollisionObb* fixed = FindObb(production.collision, "door_leaf:DOOR_L0_SUNROOM__EXT_TERRACE:0");
    const CollisionObb* moving = FindObb(production.collision, "door_leaf:DOOR_L0_SUNROOM__EXT_TERRACE:1");
    ASSERT_NE(fixed, nullptr);
    ASSERT_NE(moving, nullptr);
    EXPECT_LT(std::abs(fixed->yaw), 1.0e-6F);
    EXPECT_LT(std::abs(moving->yaw), 1.0e-6F);
    EXPECT_LT(std::abs(fixed->centre.X - moving->centre.X), 0.20F)
        << "the translated moving sash rests over the fixed sash";

    const auto garage = std::find_if(
        production.collision.meshes.begin(),
        production.collision.meshes.end(),
        [&](const auto& mesh)
        { return production.collision.SurfaceName(mesh.surface) == "door_leaf:DOOR_GARAGE_SECTIONAL:0"; });
    ASSERT_NE(garage, production.collision.meshes.end());
    EXPECT_LT(garage->bounds.Max.Y - garage->bounds.Min.Y, 0.06F)
        << "the fully open sectional leaf lies horizontal under the head";
    EXPECT_GT(
        std::max(garage->bounds.Max.X - garage->bounds.Min.X, garage->bounds.Max.Z - garage->bounds.Min.Z),
        4.8F);

    const CollisionObb* pedestrian = FindObb(production.collision, "gate_leaf:EXT_GATE_PED:0");
    const CollisionObb* driveway = FindObb(production.collision, "gate_leaf:EXT_GATE_DRIVE:0");
    const CollisionObb* rear = FindObb(production.collision, "gate_leaf:EXT_GATE_REAR:0");
    ASSERT_NE(pedestrian, nullptr);
    ASSERT_NE(driveway, nullptr);
    ASSERT_NE(rear, nullptr);
    EXPECT_NEAR(std::abs(pedestrian->yaw), 81.0F * 3.14159265F / 180.0F, 1.0e-5F);
    EXPECT_LT(pedestrian->centre.Z, 0.0F) << "the open pedestrian leaf rests inside the property";
    EXPECT_NEAR(driveway->centre.X, 7.2F, 1.0e-4F)
        << "the six-metre drive leaf translates its full width west";
    EXPECT_NEAR(rear->centre.X, 19.0F, 1.0e-4F);
    EXPECT_NEAR(rear->centre.Z, -48.0F, 1.0e-4F);
    EXPECT_LT(std::abs(rear->yaw), 1.0e-6F) << "the inaccessible rear gate is the closed exception";
}

TEST(PosedLeafCollisionTests, AClosedLeafStopsThePlayerCapsule)
{
    if (!std::filesystem::exists("content/world/collision.bin"))
    {
        GTEST_SKIP() << "no deployed world; run tools/ci/build_content.py --only world";
    }
    const Production production = Load();
    const CollisionObb* closed =
        FindObb(production.collision, "door_leaf:DOOR_L2_LANDING__L2_BALCONY_JULIET:0");
    ASSERT_NE(closed, nullptr);
    EXPECT_LT(std::abs(closed->yaw), 1.0e-6F);

    const bool thinX = closed->halfExtents.X < closed->halfExtents.Z;
    const Vector3 normal = thinX ? Vector3(1.0F, 0.0F, 0.0F) : Vector3(0.0F, 0.0F, 1.0F);
    Capsule body{closed->centre + normal, 0.60F, 0.30F};
    const auto hit = SweepCapsuleObb(body, normal * -2.0F, *closed);
    EXPECT_TRUE(hit.hit);
    EXPECT_FALSE(hit.startedInside);
    EXPECT_GT(hit.time, 0.0F);
    EXPECT_LT(hit.time, 1.0F);
}
