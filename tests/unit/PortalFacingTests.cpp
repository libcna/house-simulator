// SPDX-License-Identifier: MIT
//
// `HOUSE-00666`: §25.2's `if p.plane faces away from the camera: continue`.
#include <gtest/gtest.h>

#include "cnahouse/util/Ids.hpp"
#include "cnahouse/visibility/PortalFacing.hpp"

namespace
{
    using cnahouse::util::IdRegistry;
    using cnahouse::util::Intern;
    using Microsoft::Xna::Framework::Vector3;
    namespace visibility = cnahouse::visibility;
    namespace world = cnahouse::world;

    /// Two rooms either side of `x = 2`, joined by a doorway in that plane.
    world::WorldData::Contents TwoRooms()
    {
        world::WorldData::Contents contents;
        world::Level ground;
        ground.id = Intern("L0");
        ground.ffl = 0.60F;
        ground.ceiling = 3.30F;
        contents.levels.push_back(ground);

        const auto room = [](const char* id, float minX, float maxX)
        {
            world::Cell cell;
            cell.id = Intern(id);
            cell.level = Intern("L0");
            cell.kind = world::CellKind::Room;
            cell.boxes.push_back(world::Footprint{minX, maxX, 0.0F, 4.0F});
            return cell;
        };
        contents.cells.push_back(room("WEST", -2.0F, 2.0F));
        contents.cells.push_back(room("EAST", 2.0F, 6.0F));

        world::Portal door;
        door.id = Intern("P_WEST__EAST");
        door.cellA = Intern("WEST");
        door.cellB = Intern("EAST");
        door.axis = world::PlaneAxis::X;
        door.planeValue = 2.0F;
        door.minU = 1.5F;
        door.maxU = 2.4F;
        door.minV = 0.60F;
        door.maxV = 2.65F;
        door.kind = world::PortalKind::CasedOpening;
        contents.portals.push_back(door);
        return contents;
    }

    TEST(PortalFacingTests, TheSignedDistanceIsSignedAlongThePortalsOwnAxis)
    {
        world::Portal onX;
        onX.axis = world::PlaneAxis::X;
        onX.planeValue = 2.0F;
        EXPECT_FLOAT_EQ(visibility::SignedDistanceToPlane(onX, Vector3{0.0F, 9.0F, 9.0F}), -2.0F);
        EXPECT_FLOAT_EQ(visibility::SignedDistanceToPlane(onX, Vector3{5.0F, 9.0F, 9.0F}), 3.0F);

        world::Portal onY;
        onY.axis = world::PlaneAxis::Y;
        onY.planeValue = 3.65F;
        EXPECT_FLOAT_EQ(visibility::SignedDistanceToPlane(onY, Vector3{9.0F, 0.65F, 9.0F}), -3.0F)
            << "a stair well is measured in Y, not in X";

        world::Portal onZ;
        onZ.axis = world::PlaneAxis::Z;
        onZ.planeValue = -14.30F;
        EXPECT_FLOAT_EQ(visibility::SignedDistanceToPlane(onZ, Vector3{9.0F, 9.0F, -12.30F}), 2.0F);
    }

    TEST(PortalFacingTests, APortalIsCrossedFromTheSideItsCellIsOn)
    {
        IdRegistry::ResetForTesting();
        auto built = world::WorldData::Create(TwoRooms());
        ASSERT_TRUE(built);
        const world::Portal& door = built.Value().Portals().front();

        // Standing in the west room, looking east: the doorway faces towards.
        EXPECT_FALSE(
            visibility::PlaneFacesAway(door, built.Value(), Intern("WEST"), Vector3{0.0F, 1.6F, 2.0F}));
        // ...and the same portal expanded from the east room, with the camera still in the west,
        // faces away: from there you are looking at that doorway from behind.
        EXPECT_TRUE(
            visibility::PlaneFacesAway(door, built.Value(), Intern("EAST"), Vector3{0.0F, 1.6F, 2.0F}));
        // The mirror image, so the test is about sides and not about which cell is named first.
        EXPECT_FALSE(
            visibility::PlaneFacesAway(door, built.Value(), Intern("EAST"), Vector3{5.0F, 1.6F, 2.0F}));
        EXPECT_TRUE(
            visibility::PlaneFacesAway(door, built.Value(), Intern("WEST"), Vector3{5.0F, 1.6F, 2.0F}));
        IdRegistry::ResetForTesting();
    }

    TEST(PortalFacingTests, StandingInTheDoorwayFacesTowards)
    {
        IdRegistry::ResetForTesting();
        auto built = world::WorldData::Create(TwoRooms());
        ASSERT_TRUE(built);
        const world::Portal& door = built.Value().Portals().front();
        // An eye exactly in the plane has nothing to be behind, and the conservative answer is the
        // one that keeps a room visible rather than the one that blinks it out as you walk through.
        EXPECT_FALSE(
            visibility::PlaneFacesAway(door, built.Value(), Intern("WEST"), Vector3{2.0F, 1.6F, 2.0F}));
        EXPECT_FALSE(
            visibility::PlaneFacesAway(door, built.Value(), Intern("EAST"), Vector3{2.0F, 1.6F, 2.0F}));
        IdRegistry::ResetForTesting();
    }

    TEST(PortalFacingTests, AnUnknownCellFacesTowardsRatherThanDroppingARoom)
    {
        IdRegistry::ResetForTesting();
        auto built = world::WorldData::Create(TwoRooms());
        ASSERT_TRUE(built);
        const world::Portal& door = built.Value().Portals().front();
        EXPECT_FALSE(
            visibility::PlaneFacesAway(door, built.Value(), Intern("NOWHERE"), Vector3{5.0F, 1.6F, 2.0F}))
            << "a portal whose cell is missing is rule 6's finding; the walk must not quietly "
               "lose a room over it";
        IdRegistry::ResetForTesting();
    }
} // namespace
