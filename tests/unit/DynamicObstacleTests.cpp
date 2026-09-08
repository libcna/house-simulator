// SPDX-License-Identifier: MIT
//
// `HOUSE-00554`. §49.4's per-cell dynamic list and its per-frame refresh: door leaves, the garage
// door's five segments, pets and the twelve nudgeable props.
//
// The list is REBUILT every frame rather than updated. A door that swings from one cell into
// another, a pet that walks through a doorway and a prop that is kicked across a room all change
// which cell they belong to, and an incremental list has to be told about every one of those.
#include <string>

#include <gtest/gtest.h>

#include "cnahouse/physics/DynamicObstacles.hpp"
#include "cnahouse/util/Ids.hpp"

namespace
{
    using cnahouse::physics::Capsule;
    using cnahouse::physics::CollisionKind;
    using cnahouse::physics::CollisionObb;
    using cnahouse::physics::DynamicKind;
    using cnahouse::physics::DynamicObstacle;
    using cnahouse::physics::DynamicObstacles;
    using cnahouse::physics::DynamicSweepHit;
    using cnahouse::physics::OverlapDynamic;
    using cnahouse::physics::Sphere;
    using cnahouse::physics::SweepDynamic;
    using cnahouse::util::Intern;
    using Vector3 = Microsoft::Xna::Framework::Vector3;

    /// A door leaf: 0.9 m wide, 2.04 m tall, 40 mm thick, hinged so that `yaw` swings it.
    DynamicObstacle Leaf(std::string_view id, const Vector3& centre, float yaw)
    {
        DynamicObstacle obstacle;
        obstacle.shape.centre = centre;
        obstacle.shape.halfExtents = Vector3(0.45F, 1.02F, 0.02F);
        obstacle.shape.yaw = yaw;
        obstacle.shape.kind = CollisionKind::Prop;
        obstacle.source = Intern(id);
        obstacle.kind = DynamicKind::Door;
        return obstacle;
    }

} // namespace

TEST(DynamicObstacleTests, ACellWithNothingInItHasAnEmptyList)
{
    const DynamicObstacles obstacles;
    EXPECT_TRUE(obstacles.For("L0_HALL").empty());
    EXPECT_EQ(obstacles.Count(), 0u);
}

TEST(DynamicObstacleTests, TheListIsPerCellAndCountsAcrossThem)
{
    DynamicObstacles obstacles;
    obstacles.BeginFrame();
    obstacles.Add("L0_HALL", Leaf("DOOR_HALL", Vector3(0.0F, 1.02F, 0.0F), 0.0F));
    obstacles.Add("L0_HALL", Leaf("DOOR_WC", Vector3(2.0F, 1.02F, 0.0F), 0.0F));
    obstacles.Add("L0_KITCHEN", Leaf("DOOR_KITCHEN", Vector3(5.0F, 1.02F, 0.0F), 0.0F));

    EXPECT_EQ(obstacles.For("L0_HALL").size(), 2u);
    EXPECT_EQ(obstacles.For("L0_KITCHEN").size(), 1u);
    EXPECT_TRUE(obstacles.For("L0_BATH").empty()) << "a cell nothing was added to is not empty";
    EXPECT_EQ(obstacles.Count(), 3u);
}

TEST(DynamicObstacleTests, EachFrameStartsEmptySoNothingCanGoStale)
{
    // The reason for a rebuild. A door that swings from the hall into the kitchen belongs to a
    // different cell than it did last frame, and a list that was only ever added to would have it
    // in both -- so the player would be stopped by a door that is no longer there.
    DynamicObstacles obstacles;
    obstacles.BeginFrame();
    obstacles.Add("L0_HALL", Leaf("DOOR_HALL", Vector3(0.0F, 1.02F, 0.0F), 0.0F));
    ASSERT_EQ(obstacles.For("L0_HALL").size(), 1u);

    obstacles.BeginFrame();
    EXPECT_TRUE(obstacles.For("L0_HALL").empty()) << "last frame's door survived into this one";
    EXPECT_EQ(obstacles.Count(), 0u);

    obstacles.Add("L0_KITCHEN", Leaf("DOOR_HALL", Vector3(0.0F, 1.02F, 0.0F), 0.0F));
    EXPECT_TRUE(obstacles.For("L0_HALL").empty());
    EXPECT_EQ(obstacles.For("L0_KITCHEN").size(), 1u);
}

TEST(DynamicObstacleTests, ADoorLeafStopsTheBodyAndSaysWhichDoorItWas)
{
    // A hit has to name its source: §50's interaction reports "blocked" against a particular door,
    // and a hit that only said "something" could not.
    DynamicObstacles obstacles;
    obstacles.BeginFrame();
    obstacles.Add("L0_HALL", Leaf("DOOR_L0_HALL__WC", Vector3(1.0F, 1.02F, 0.0F), 0.0F));

    const Capsule body{Vector3(1.0F, 0.90F, -1.0F), 0.60F, 0.30F};
    const DynamicSweepHit hit = SweepDynamic(obstacles, "L0_HALL", body, Vector3(0.0F, 0.0F, 1.0F));
    ASSERT_TRUE(hit.hit);
    // The leaf's south face is at z = -0.02 and the body's radius is 0.30, so it stops at -0.32,
    // which is 0.68 of the metre it asked for.
    EXPECT_NEAR(hit.time, 0.68F, 1e-3F);
    EXPECT_NEAR(hit.normal.Z, -1.0F, 1e-4F);
    EXPECT_EQ(hit.source, Intern("DOOR_L0_HALL__WC"));
    EXPECT_EQ(hit.kind, DynamicKind::Door);
    EXPECT_EQ(hit.obstacle, 0u);
}

TEST(DynamicObstacleTests, TheEARLIESTOfSeveralIsWhatStopsTheBody)
{
    // The same rule `SweepCell` follows: a body between a door and a pet is stopped by whichever
    // it reaches first, not by whichever the list happens to hold first.
    DynamicObstacles obstacles;
    obstacles.BeginFrame();
    obstacles.Add("L0_HALL", Leaf("DOOR_FAR", Vector3(0.0F, 1.02F, 3.0F), 0.0F));
    DynamicObstacle pet;
    pet.shape.centre = Vector3(0.0F, 0.25F, 1.5F);
    pet.shape.halfExtents = Vector3(0.15F, 0.25F, 0.35F);
    pet.shape.kind = CollisionKind::Prop;
    pet.source = Intern("PET_CAT");
    pet.kind = DynamicKind::Pet;
    obstacles.Add("L0_HALL", pet);

    const Capsule body{Vector3(0.0F, 0.90F, 0.0F), 0.60F, 0.30F};
    const DynamicSweepHit hit = SweepDynamic(obstacles, "L0_HALL", body, Vector3(0.0F, 0.0F, 4.0F));
    ASSERT_TRUE(hit.hit);
    EXPECT_EQ(hit.kind, DynamicKind::Pet) << "it walked through the cat to reach the door";
    EXPECT_EQ(hit.source, Intern("PET_CAT"));
}

TEST(DynamicObstacleTests, ASwingingDoorOverlapsThePlayerAndSaysHowDeeply)
{
    // §49.4: *"A door swinging into the player pushes them (the door's motion is authoritative;
    // the player is depenetrated)."* The push-out needs a depth and a direction, and the deepest
    // is the one §49.3 pushes along.
    DynamicObstacles obstacles;
    obstacles.BeginFrame();
    // A leaf that has swung so its face is 0.25 m from the body's centre: 50 mm of overlap.
    obstacles.Add("L0_HALL", Leaf("DOOR_SWUNG", Vector3(0.0F, 1.02F, 0.27F), 0.0F));

    const Capsule body{Vector3(0.0F, 0.90F, 0.0F), 0.60F, 0.30F};
    const DynamicSweepHit overlap = OverlapDynamic(obstacles, "L0_HALL", body);
    ASSERT_TRUE(overlap.hit);
    EXPECT_TRUE(overlap.startedInside);
    EXPECT_FLOAT_EQ(overlap.time, 0.0F);
    EXPECT_NEAR(overlap.normal.Z, -1.0F, 1e-4F) << "the push is away from the door";
    EXPECT_EQ(overlap.source, Intern("DOOR_SWUNG"));

    // Nothing overlapping is not a hit, however close.
    obstacles.BeginFrame();
    obstacles.Add("L0_HALL", Leaf("DOOR_CLEAR", Vector3(0.0F, 1.02F, 0.35F), 0.0F));
    EXPECT_FALSE(OverlapDynamic(obstacles, "L0_HALL", body).hit);
}

TEST(DynamicObstacleTests, TheDEEPESTOverlapWinsBecauseThatIsWhereThePushGoes)
{
    // A player pressed into a corner by a closing door has two overlaps, and §49.3 pushes along
    // the deeper one. Taking the first would spend an iteration undoing the shallower.
    DynamicObstacles obstacles;
    obstacles.BeginFrame();
    obstacles.Add("L0_HALL", Leaf("DOOR_SHALLOW", Vector3(0.0F, 1.02F, 0.31F), 0.0F));
    obstacles.Add("L0_HALL", Leaf("DOOR_DEEP", Vector3(0.0F, 1.02F, 0.22F), 0.0F));

    const Capsule body{Vector3(0.0F, 0.90F, 0.0F), 0.60F, 0.30F};
    const DynamicSweepHit overlap = OverlapDynamic(obstacles, "L0_HALL", body);
    ASSERT_TRUE(overlap.hit);
    EXPECT_EQ(overlap.source, Intern("DOOR_DEEP")) << "it reported the shallower of the two";
}

TEST(DynamicObstacleTests, TheGarageDoorIsFiveSegmentsAndTheyMoveTogether)
{
    // §49.4 gives the garage door five segment OBBs. What that costs the list is five entries in
    // one cell, and what it buys is a door that can be half open -- the segments at different
    // heights, the body stopped by whichever it meets.
    DynamicObstacles obstacles;
    obstacles.BeginFrame();
    for (int i = 0; i < 5; ++i)
    {
        DynamicObstacle segment;
        segment.shape.centre = Vector3(0.0F, 0.30F + static_cast<float>(i) * 0.60F, 2.0F);
        segment.shape.halfExtents = Vector3(2.4F, 0.30F, 0.03F);
        segment.shape.kind = CollisionKind::Prop;
        segment.source = Intern("GARAGE_DOOR");
        segment.kind = DynamicKind::GarageSegment;
        obstacles.Add("L0_GARAGE", segment);
    }
    EXPECT_EQ(obstacles.For("L0_GARAGE").size(), 5u);

    // A body at standing height meets the segment at its own height, not the bottom one.
    const Capsule body{Vector3(0.0F, 0.90F, 0.0F), 0.60F, 0.30F};
    const DynamicSweepHit hit = SweepDynamic(obstacles, "L0_GARAGE", body, Vector3(0.0F, 0.0F, 4.0F));
    ASSERT_TRUE(hit.hit);
    EXPECT_EQ(hit.kind, DynamicKind::GarageSegment);
    EXPECT_EQ(hit.source, Intern("GARAGE_DOOR")) << "every segment names the door it belongs to";

    // Raise the bottom three out of the way -- a half-open door -- and a crawling body gets under.
    obstacles.BeginFrame();
    for (int i = 3; i < 5; ++i)
    {
        DynamicObstacle segment;
        segment.shape.centre = Vector3(0.0F, 1.80F + static_cast<float>(i) * 0.60F, 2.0F);
        segment.shape.halfExtents = Vector3(2.4F, 0.30F, 0.03F);
        segment.source = Intern("GARAGE_DOOR");
        segment.kind = DynamicKind::GarageSegment;
        obstacles.Add("L0_GARAGE", segment);
    }
    const Capsule crawling = Sphere(Vector3(0.0F, 0.30F, 0.0F), 0.30F);
    EXPECT_FALSE(SweepDynamic(obstacles, "L0_GARAGE", crawling, Vector3(0.0F, 0.0F, 4.0F)).hit);
}

TEST(DynamicObstacleTests, ASweepInACellWithNoObstaclesHitsNothing)
{
    DynamicObstacles obstacles;
    obstacles.BeginFrame();
    obstacles.Add("L0_HALL", Leaf("DOOR_HALL", Vector3(0.0F, 1.02F, 1.0F), 0.0F));
    const Capsule body{Vector3(0.0F, 0.90F, 0.0F), 0.60F, 0.30F};
    EXPECT_FALSE(SweepDynamic(obstacles, "L0_KITCHEN", body, Vector3(0.0F, 0.0F, 4.0F)).hit)
        << "the hall's door stopped a body in the kitchen";
    EXPECT_FALSE(OverlapDynamic(obstacles, "L0_KITCHEN", body).hit);
}
