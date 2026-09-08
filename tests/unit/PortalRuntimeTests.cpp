// SPDX-License-Identifier: MIT
//
// `HOUSE-00665`. §25.3's two answers, both of which are easy to get wrong in a way nothing sees:
// a leaf does not shrink the doorway, and the open/closed state is latched.
#include <gtest/gtest.h>

#include "cnahouse/util/Ids.hpp"
#include "cnahouse/visibility/PortalRuntime.hpp"

namespace
{
    using cnahouse::util::Intern;
    using cnahouse::visibility::PortalRuntime;
    namespace world = cnahouse::world;

    world::Portal Doorway(world::PlaneAxis axis, world::PortalOpacity opacity, bool withLeaf)
    {
        world::Portal portal;
        portal.id = Intern("P_A__B");
        portal.cellA = Intern("A");
        portal.cellB = Intern("B");
        portal.axis = axis;
        portal.planeValue = 2.0F;
        portal.minU = -0.45F;
        portal.maxU = 0.45F;
        portal.minV = 0.60F;
        portal.maxV = 2.65F;
        portal.kind = withLeaf ? world::PortalKind::Door : world::PortalKind::CasedOpening;
        portal.opacity = opacity;
        if (withLeaf)
        {
            portal.aperture = Intern("DOOR_A");
        }
        return portal;
    }

    TEST(PortalRuntimeTests, TheWorldRectIsTheDoorwayOnEveryAxis)
    {
        const PortalRuntime onX{Doorway(world::PlaneAxis::X, world::PortalOpacity::Open, false)};
        for (const auto& corner : onX.WorldRect())
        {
            EXPECT_FLOAT_EQ(corner.X, 2.0F) << "an X-plane portal is flat in X";
        }
        const PortalRuntime onZ{Doorway(world::PlaneAxis::Z, world::PortalOpacity::Open, false)};
        for (const auto& corner : onZ.WorldRect())
        {
            EXPECT_FLOAT_EQ(corner.Z, 2.0F);
        }
        // A horizontal portal is a hole in a floor: `u` and `v` are X and Z there, not X and Y.
        const PortalRuntime onY{Doorway(world::PlaneAxis::Y, world::PortalOpacity::Open, false)};
        for (const auto& corner : onY.WorldRect())
        {
            EXPECT_FLOAT_EQ(corner.Y, 2.0F) << "a stair well is flat in Y, not standing on edge";
        }
        float lowest = 1e9F;
        float highest = -1e9F;
        for (const auto& corner : onX.WorldRect())
        {
            lowest = std::min(lowest, corner.Y);
            highest = std::max(highest, corner.Y);
        }
        EXPECT_FLOAT_EQ(lowest, 0.60F);
        EXPECT_FLOAT_EQ(highest, 2.65F) << "the doorway is as tall as the data says";
    }

    TEST(PortalRuntimeTests, APortalWithNoLeafIsAlwaysOpen)
    {
        const PortalRuntime hole{Doorway(world::PlaneAxis::X, world::PortalOpacity::Open, false)};
        EXPECT_TRUE(hole.IsOpen());
        EXPECT_TRUE(hole.PassesLight());
        EXPECT_TRUE(hole.PassesBodies());
        EXPECT_FLOAT_EQ(hole.Aperture(), 1.0F);
    }

    TEST(PortalRuntimeTests, ALeafStartsShutAndLatchesOpen)
    {
        PortalRuntime door{Doorway(world::PlaneAxis::X, world::PortalOpacity::OpaqueWhenClosed, true)};
        EXPECT_FALSE(door.IsOpen()) << "§65.6 starts the house with its doors shut";
        EXPECT_FALSE(door.PassesLight());
        EXPECT_FALSE(door.PassesBodies());

        // Between the two thresholds nothing changes. This is the whole point of the latch: a door
        // settling shut sits in this band for a frame or two, and a naive `aperture > 0` would
        // flicker the cell behind it -- its chunks, props, lights and further portals -- on and off.
        EXPECT_FALSE(door.SetAperture(0.06F));
        EXPECT_FALSE(door.IsOpen());
        EXPECT_FALSE(door.SetAperture(0.079F));
        EXPECT_FALSE(door.IsOpen());

        EXPECT_TRUE(door.SetAperture(0.081F)) << "crossing the open threshold is a change";
        EXPECT_TRUE(door.IsOpen());
        EXPECT_TRUE(door.PassesLight());
        EXPECT_TRUE(door.PassesBodies());

        // ...and it stays open on the way back down through the band.
        EXPECT_FALSE(door.SetAperture(0.06F));
        EXPECT_TRUE(door.IsOpen());
        EXPECT_TRUE(door.SetAperture(0.049F));
        EXPECT_FALSE(door.IsOpen());
    }

    TEST(PortalRuntimeTests, TheApertureNeverShrinksTheDoorway)
    {
        // §25.3, the question the brief asks: a door open 10° reveals a thin slice of the room
        // because the LEAF blocks the rest, not because the portal got narrow.
        PortalRuntime door{Doorway(world::PlaneAxis::X, world::PortalOpacity::OpaqueWhenClosed, true)};
        const auto shut = door.WorldRect();
        door.SetAperture(0.10F);
        EXPECT_EQ(door.WorldRect(), shut);
        door.SetAperture(1.0F);
        EXPECT_EQ(door.WorldRect(), shut) << "the doorway is a hole in a wall and does not move";
    }

    TEST(PortalRuntimeTests, ClosedGlassStopsBodiesAndNotSight)
    {
        PortalRuntime pane{Doorway(world::PlaneAxis::Z, world::PortalOpacity::Glass, true)};
        EXPECT_FALSE(pane.IsOpen());
        EXPECT_TRUE(pane.PassesLight()) << "§25.3: a closed glass door never closes its portal for "
                                           "vision";
        EXPECT_FALSE(pane.PassesBodies()) << "...only for movement";

        PortalRuntime solid{Doorway(world::PlaneAxis::Z, world::PortalOpacity::OpaqueWhenClosed, true)};
        EXPECT_FALSE(solid.PassesLight()) << "and an opaque one closes both, which is where the "
                                             "whole win is";
    }

    TEST(PortalRuntimeTests, TheApertureIsClamped)
    {
        PortalRuntime door{Doorway(world::PlaneAxis::X, world::PortalOpacity::OpaqueWhenClosed, true)};
        door.SetAperture(4.0F);
        EXPECT_FLOAT_EQ(door.Aperture(), 1.0F);
        door.SetAperture(-2.0F);
        EXPECT_FLOAT_EQ(door.Aperture(), 0.0F);
    }

    TEST(PortalRuntimeTests, TheThresholdsAreTheOnesTheArchitectureNames)
    {
        // §25.3: "treated as closed below 0.05 and as open above 0.08".
        EXPECT_FLOAT_EQ(PortalRuntime::kClosedBelow, 0.05F);
        EXPECT_FLOAT_EQ(PortalRuntime::kOpenAbove, 0.08F);
        EXPECT_LT(PortalRuntime::kClosedBelow, PortalRuntime::kOpenAbove)
            << "a band, not a threshold: equal values are no hysteresis at all";
    }
} // namespace
