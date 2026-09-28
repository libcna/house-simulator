// SPDX-License-Identifier: MIT
//
// `HOUSE-00667`: §25.2's depth table, and the asymmetry that is the whole reason it exists.
#include <gtest/gtest.h>

#include "cnahouse/util/Ids.hpp"
#include "cnahouse/visibility/PortalDepth.hpp"

namespace
{
    using cnahouse::util::IdRegistry;
    using cnahouse::util::Intern;
    using cnahouse::visibility::CameraSide;
    using cnahouse::visibility::MaxDepthFor;
    namespace world = cnahouse::world;

    /// A room, a second room, and the outdoors.
    world::WorldData::Contents House()
    {
        world::WorldData::Contents contents;
        world::Level ground;
        ground.id = Intern("L0");
        ground.ffl = 0.60F;
        ground.ceiling = 3.30F;
        contents.levels.push_back(ground);

        const auto cell = [](const char* id, world::CellKind kind, float minX, float maxX)
        {
            world::Cell made;
            made.id = Intern(id);
            made.level = Intern("L0");
            made.kind = kind;
            made.boxes.push_back(world::Footprint{minX, maxX, 0.0F, 4.0F});
            return made;
        };
        contents.cells.push_back(cell("ROOM", world::CellKind::Room, -2.0F, 2.0F));
        contents.cells.push_back(cell("HALL", world::CellKind::Room, 2.0F, 6.0F));
        contents.cells.push_back(cell("EXT_YARD", world::CellKind::Exterior, -6.0F, -2.0F));
        return contents;
    }

    world::Portal Between(const char* a, const char* b, world::PortalKind kind, world::PortalOpacity opacity)
    {
        world::Portal portal;
        portal.id = Intern("P");
        portal.cellA = Intern(a);
        portal.cellB = Intern(b);
        portal.axis = world::PlaneAxis::X;
        portal.planeValue = 2.0F;
        portal.kind = kind;
        portal.opacity = opacity;
        return portal;
    }

    TEST(PortalDepthTests, TheTableIsTheOneTheArchitectureNames)
    {
        IdRegistry::ResetForTesting();
        auto built = world::WorldData::Create(House());
        ASSERT_TRUE(built);
        const world::WorldData& house = built.Value();

        const world::Portal door =
            Between("ROOM", "HALL", world::PortalKind::Door, world::PortalOpacity::OpaqueWhenClosed);
        EXPECT_EQ(MaxDepthFor(door, house, CameraSide::Interior), 6);
        EXPECT_EQ(MaxDepthFor(door, house, CameraSide::Exterior), 6)
            << "an authored-open entrance must not truncate the sightline visible through it";

        const world::Portal opening =
            Between("ROOM", "HALL", world::PortalKind::CasedOpening, world::PortalOpacity::Open);
        EXPECT_EQ(MaxDepthFor(opening, house, CameraSide::Interior), 6);

        const world::Portal well =
            Between("ROOM", "HALL", world::PortalKind::StairWell, world::PortalOpacity::Open);
        EXPECT_EQ(MaxDepthFor(well, house, CameraSide::Interior), 6)
            << "a stair well is on the same row as a door";

        const world::Portal garage =
            Between("ROOM", "HALL", world::PortalKind::GarageDoor, world::PortalOpacity::OpaqueWhenClosed);
        EXPECT_EQ(MaxDepthFor(garage, house, CameraSide::Interior), 4);
        EXPECT_EQ(MaxDepthFor(garage, house, CameraSide::Exterior), 2);

        IdRegistry::ResetForTesting();
    }

    TEST(PortalDepthTests, AWindowOntoTheGardenIsAsymmetric)
    {
        IdRegistry::ResetForTesting();
        auto built = world::WorldData::Create(House());
        ASSERT_TRUE(built);
        const world::WorldData& house = built.Value();

        const world::Portal window =
            Between("ROOM", "EXT_YARD", world::PortalKind::Window, world::PortalOpacity::Glass);
        EXPECT_EQ(MaxDepthFor(window, house, CameraSide::Interior), 3)
            << "from inside, the garden is where the neighbourhood and the sky are";
        EXPECT_EQ(MaxDepthFor(window, house, CameraSide::Exterior), 1)
            << "§25.2: standing in the garden you see ONE room through a window, not that room "
               "plus everything behind its open door";
        EXPECT_LT(MaxDepthFor(window, house, CameraSide::Exterior),
                  MaxDepthFor(window, house, CameraSide::Interior))
            << "the asymmetry is the point of the table";
    }

    TEST(PortalDepthTests, GlassIsDecidedByWhatIsBehindItAndNotByItsKind)
    {
        IdRegistry::ResetForTesting();
        auto built = world::WorldData::Create(House());
        ASSERT_TRUE(built);
        const world::WorldData& house = built.Value();

        // A clear slider must keep the sightline behind its room, shut or open. Frosted glazing
        // remains shallow while shut, but an open leaf exposes an ordinary doorway.
        const world::Portal slider =
            Between("ROOM", "EXT_YARD", world::PortalKind::Slider, world::PortalOpacity::Glass);
        EXPECT_EQ(MaxDepthFor(slider, house, CameraSide::Exterior), 6);
        EXPECT_EQ(MaxDepthFor(slider, house, CameraSide::Exterior, true), 6);
        EXPECT_EQ(MaxDepthFor(slider, house, CameraSide::Interior, true), 6);
        const world::Portal window =
            Between("ROOM", "EXT_YARD", world::PortalKind::Window, world::PortalOpacity::Glass);
        EXPECT_EQ(MaxDepthFor(window, house, CameraSide::Exterior, true), 1);
        const world::Portal frosted =
            Between("ROOM", "EXT_YARD", world::PortalKind::Slider, world::PortalOpacity::Translucent);
        EXPECT_EQ(MaxDepthFor(frosted, house, CameraSide::Exterior), 1);
        EXPECT_EQ(MaxDepthFor(frosted, house, CameraSide::Exterior, true), 6);

        // ...and an INTERIOR glazed door is not a window onto outside: borrowed light between two
        // rooms is a door, and the chain through it is a door's.
        const world::Portal borrowed =
            Between("ROOM", "HALL", world::PortalKind::Window, world::PortalOpacity::Glass);
        EXPECT_EQ(MaxDepthFor(borrowed, house, CameraSide::Interior), 6);
        IdRegistry::ResetForTesting();
    }
} // namespace
