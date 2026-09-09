// SPDX-License-Identifier: MIT
//
// `HOUSE-00681`. §25.8's `F3`: *"cells: visible 9 / 95   traversals 23   portals tested 61
// frusta 12   maxdepth 4"*, and the visible list.
//
// A presenter is testable exactly because it measures nothing: every number on it came from
// somewhere else, so what can be got wrong here is the PRESENTATION -- a counter that is not shown,
// a zero that means "not measured", a list that silently stops. All three are checked.
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "cnahouse/debug/VisibilityOverlay.hpp"

namespace
{
    using cnahouse::debug::VisibilityOverlay;
    using cnahouse::debug::VisibilitySnapshot;
    using cnahouse::debug::VisibleCellLine;
    using Microsoft::Xna::Framework::Vector3;

    bool Mentions(const std::vector<std::string>& lines, std::string_view text)
    {
        for (const std::string& line : lines)
        {
            if (line.find(text) != std::string::npos)
            {
                return true;
            }
        }
        return false;
    }

    /// The one line that mentions @p text, so a row's own fields can be checked rather than the
    /// whole overlay being searched for a number that might belong to any of it.
    std::string LineWith(const std::vector<std::string>& lines, std::string_view text)
    {
        for (const std::string& line : lines)
        {
            if (line.find(text) != std::string::npos)
            {
                return line;
            }
        }
        return {};
    }

    VisibilitySnapshot AFrame()
    {
        VisibilitySnapshot snapshot;
        snapshot.cell = "L0_KITCHEN";
        snapshot.eye = Vector3(-3.42F, 1.28F, -25.06F);
        snapshot.yaw = 2.0594F; // 118 degrees
        snapshot.cellsInWorld = 96;
        snapshot.traversal.portalsTested = 61;
        snapshot.traversal.portalsCrossed = 9;
        snapshot.traversal.skippedClosed = 3;
        snapshot.traversal.skippedFacing = 34;
        snapshot.traversal.skippedDepth = 4;
        snapshot.traversal.skippedClipped = 29;
        snapshot.traversal.skippedArea = 1;
        snapshot.traversal.skippedContained = 2;
        snapshot.traversal.maxDepth = 4;
        snapshot.chunksDrawn = 30;
        snapshot.chunksTested = 34;
        snapshot.drawCalls = 418;
        snapshot.stateChanges = 12;
        snapshot.visible.push_back(VisibleCellLine{"L0_KITCHEN", 0, 1, 0, false});
        snapshot.visible.push_back(VisibleCellLine{"L0_DINING", 1, 2, 0, false});
        snapshot.visible.push_back(VisibleCellLine{"L0_BATH", 2, 1, 0, true});
        return snapshot;
    }

} // namespace

TEST(VisibilityOverlayTests, ItSaysEverySection25Point8NumberAndNamesEveryCell)
{
    const VisibilityOverlay overlay;
    const std::vector<std::string> lines = overlay.Lines(AFrame());

    // §25.8's first line: how many of how many, how deep, how many cones.
    EXPECT_TRUE(Mentions(lines, "visible 3 / 96")) << lines[0];
    EXPECT_TRUE(Mentions(lines, "maxdepth 4")) << lines[0];

    // The portal line, and the whole of it: six reasons a portal was not crossed, each with its
    // own number. A total would say a chain stopped and not why, which is the one thing a person
    // opens this overlay to find out.
    EXPECT_TRUE(Mentions(lines, "tested 61"));
    EXPECT_TRUE(Mentions(lines, "crossed 9"));
    EXPECT_TRUE(Mentions(lines, "closed 3"));
    EXPECT_TRUE(Mentions(lines, "facing 34"));
    EXPECT_TRUE(Mentions(lines, "deep 4"));
    EXPECT_TRUE(Mentions(lines, "clipped 29"));
    EXPECT_TRUE(Mentions(lines, "small 1"));
    EXPECT_TRUE(Mentions(lines, "covered 2"));

    EXPECT_TRUE(Mentions(lines, "chunks 30 / 34"));
    // §71.2's budgets are printed beside the numbers rather than left to the reader's memory.
    EXPECT_TRUE(Mentions(lines, "418 / 620"));
    EXPECT_TRUE(Mentions(lines, "12 / 90"));

    // §14's bearing, in degrees: 118, not 2.0594 radians.
    EXPECT_TRUE(Mentions(lines, "L0_KITCHEN"));
    EXPECT_TRUE(Mentions(lines, "118deg")) << "the camera's yaw is printed in radians";
    EXPECT_TRUE(Mentions(lines, "-3.42"));

    // Every visible cell, with its depth and its cone count.
    EXPECT_TRUE(Mentions(lines, "L0_DINING"));
    EXPECT_TRUE(Mentions(lines, "L0_BATH"));
    EXPECT_TRUE(Mentions(lines, "2 cones"));
    EXPECT_TRUE(Mentions(lines, "1 cone "));
    EXPECT_TRUE(Mentions(lines, "diffuse")) << "§26.4's frosted-glass flag is not shown";

    // Each row's OWN depth, on its own line: the camera's cell at 0, a room through one doorway at
    // 1, one behind that at 2. Searching the whole overlay for "d1" would find it in any of them.
    // The camera's own cell appears twice -- on the camera line and as the walk's root -- so the
    // root is found by its depth rather than by its name.
    EXPECT_NE(LineWith(lines, "d0").find("L0_KITCHEN"), std::string::npos) << LineWith(lines, "d0");
    EXPECT_NE(LineWith(lines, "L0_DINING").find("d1"), std::string::npos) << LineWith(lines, "L0_DINING");
    EXPECT_NE(LineWith(lines, "L0_BATH").find("d2"), std::string::npos) << LineWith(lines, "L0_BATH");
    // And the cone counts belong to their own rows too.
    EXPECT_NE(LineWith(lines, "L0_DINING").find("2 cones"), std::string::npos);
    EXPECT_NE(LineWith(lines, "L0_BATH").find("1 cone"), std::string::npos);
    EXPECT_NE(LineWith(lines, "L0_BATH").find("diffuse"), std::string::npos);
    EXPECT_EQ(LineWith(lines, "L0_DINING").find("diffuse"), std::string::npos)
        << "a clear room was marked diffuse";
}

TEST(VisibilityOverlayTests, ACountThatWasNeverMeasuredIsNotZero)
{
    // Zero is an answer -- "nothing was drawn" -- and a system that did not run has not given one.
    // The instance and exterior culls do not run in the walk scene yet, and an overlay that
    // reported them as 0 would be reporting an empty garden rather than an absent measurement.
    VisibilitySnapshot snapshot = AFrame();
    snapshot.instancesDrawn = -1;
    snapshot.instancesTested = -1;
    snapshot.exteriorDrawn = -1;
    snapshot.exteriorTested = -1;
    snapshot.exteriorNodes = -1;

    const VisibilityOverlay overlay;
    const std::vector<std::string> lines = overlay.Lines(snapshot);
    EXPECT_TRUE(Mentions(lines, "instances - / -"));
    EXPECT_TRUE(Mentions(lines, "exterior - / -"));

    // And a real zero prints as a zero.
    snapshot.instancesDrawn = 0;
    snapshot.instancesTested = 180;
    const std::vector<std::string> measured = overlay.Lines(snapshot);
    EXPECT_TRUE(Mentions(measured, "instances 0 / 180"));
}

TEST(VisibilityOverlayTests, TheListStopsAndSaysSoRatherThanRunningOffTheScreen)
{
    VisibilitySnapshot snapshot = AFrame();
    snapshot.visible.clear();
    // §25's own cap is thirty cells, which is more than fits beside everything else on screen.
    for (int i = 0; i < 30; ++i)
    {
        snapshot.visible.push_back(VisibleCellLine{"CELL_" + std::to_string(i), 1, 1, 0, false});
    }

    const VisibilityOverlay overlay;
    const std::vector<std::string> lines = overlay.Lines(snapshot);
    EXPECT_TRUE(Mentions(lines, "CELL_0"));
    EXPECT_TRUE(Mentions(lines, "CELL_11"));
    EXPECT_FALSE(Mentions(lines, "CELL_12")) << "the list ran past its own cap";
    EXPECT_TRUE(Mentions(lines, "and 18 more"));
    EXPECT_TRUE(Mentions(lines, "visible 30 / 96")) << "the header must still count them all";
}

TEST(VisibilityOverlayTests, ADroppedConeOrCellIsSaidLoudly)
{
    // `frustaDropped` means a room is seen through more than four openings at once; `cellsDropped`
    // means §71.2's hard stop threw a VISIBLE cell away, which is the loudest thing this system
    // can report. Neither is shown at all when it is zero, so seeing it means something.
    VisibilitySnapshot snapshot = AFrame();
    const VisibilityOverlay overlay;
    EXPECT_FALSE(Mentions(overlay.Lines(snapshot), "dropped"));
    EXPECT_FALSE(Mentions(overlay.Lines(snapshot), "DROPPED"));

    snapshot.traversal.frustaDropped = 2;
    snapshot.traversal.cellsDropped = 3;
    const std::vector<std::string> lines = overlay.Lines(snapshot);
    EXPECT_TRUE(Mentions(lines, "(2 dropped)"));
    EXPECT_TRUE(Mentions(lines, "CELLS DROPPED 3"));

    // And a cone dropped at one cell is named on that cell's own row, not only in the header.
    snapshot.visible[1].conesDropped = 1;
    EXPECT_TRUE(Mentions(overlay.Lines(snapshot), "(+1 dropped)"));

    // `queueDropped` is a different event with a nearly identical name and it is shown apart from
    // both (`HOUSE-00695`): the cap above is §25.2's fifth cone into ONE room, which is by design;
    // this is the work queue running out of room, after which a room may never have been reached
    // at all. The overlay has to make them tellable apart, because the response differs.
    snapshot.traversal.queueDropped = 4;
    const std::vector<std::string> overflowed = overlay.Lines(snapshot);
    EXPECT_TRUE(Mentions(overflowed, "QUEUE DROPPED 4"));
    EXPECT_TRUE(Mentions(overflowed, "CELLS DROPPED 3")) << "and the other two are still there";
    EXPECT_TRUE(Mentions(overflowed, "(2 dropped)"));
}

TEST(VisibilityOverlayTests, AWalkThatReachedNothingSaysSoRatherThanShowingNothing)
{
    VisibilitySnapshot snapshot = AFrame();
    snapshot.visible.clear();
    const VisibilityOverlay overlay;
    const std::vector<std::string> lines = overlay.Lines(snapshot);
    EXPECT_TRUE(Mentions(lines, "visible 0 / 96"));
    EXPECT_TRUE(Mentions(lines, "nothing visible"))
        << "an empty list must not look like an overlay that was never updated";
}

TEST(VisibilityOverlayTests, ItSaysWhetherTheFrameWasActuallyCulled)
{
    // The line that stops this overlay being a lie. The walk runs every frame; the draw list is
    // built from residency until `HOUSE-00684`. An overlay reporting a visible set that nothing
    // acted on has to say which of the two it is describing.
    VisibilitySnapshot snapshot = AFrame();
    snapshot.cullingApplied = false;
    const VisibilityOverlay overlay;
    EXPECT_TRUE(Mentions(overlay.Lines(snapshot), "NOT APPLIED"));

    snapshot.cullingApplied = true;
    EXPECT_TRUE(Mentions(overlay.Lines(snapshot), "culling ON"));
    EXPECT_FALSE(Mentions(overlay.Lines(snapshot), "NOT APPLIED"));
}

TEST(VisibilityOverlayTests, AFrozenWalkSaysSoBeforeAnythingElseItSays)
{
    // §25.8's `F5`. While the walk is frozen every number on the overlay describes an OLD frame,
    // and the eye it was computed from is not where the picture is being drawn from. A reader who
    // has forgotten they pressed the key will believe all of it, so the overlay says so in
    // capitals and gives the detached camera's position beside it.
    VisibilitySnapshot snapshot = AFrame();
    const VisibilityOverlay overlay;
    EXPECT_FALSE(Mentions(overlay.Lines(snapshot), "FROZEN"));

    snapshot.frozen = true;
    snapshot.walkFrame = 417;
    snapshot.inspectionEye = Vector3(12.50F, 9.00F, -40.25F);
    const std::vector<std::string> lines = overlay.Lines(snapshot);
    EXPECT_TRUE(Mentions(lines, "FROZEN"));
    // WHICH frame it caught, because a frozen overlay that kept the current frame number would be
    // indistinguishable from one that never froze.
    EXPECT_TRUE(Mentions(lines, "frame 417"));
    EXPECT_TRUE(Mentions(lines, "+12.50"));
    EXPECT_TRUE(Mentions(lines, "-40.25"));
    // ...and the camera line still says where the WALK was done from, which is the other half of
    // what a reader needs: the two positions together are what the freeze is for.
    EXPECT_TRUE(Mentions(lines, "-3.42"));
    EXPECT_TRUE(Mentions(lines, "L0_KITCHEN"));
}

TEST(VisibilityOverlayTests, ItIsHiddenUntilItIsToggled)
{
    VisibilityOverlay overlay;
    EXPECT_FALSE(overlay.Visible());
    overlay.Toggle();
    EXPECT_TRUE(overlay.Visible());
    overlay.Toggle();
    EXPECT_FALSE(overlay.Visible());
    overlay.SetVisible(true);
    EXPECT_TRUE(overlay.Visible());
    // `Lines` does not consult it: a hidden overlay still knows what it would say, which is what
    // lets a test assert the content without a font or a device.
    EXPECT_FALSE(overlay.Lines(AFrame()).empty());
}
