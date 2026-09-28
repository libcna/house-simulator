// SPDX-License-Identifier: MIT
//
// `HOUSE-00631`. §69's `F2`: cell, position, yaw/pitch, ground surface, level, held item, target.
//
// An overlay is only worth having if what it says is true, and "looks right on screen" is not a
// test. This one is a presenter -- it takes a snapshot and returns lines -- so every claim it makes
// can be asserted here: that a bearing is §14's bearing, that a body in the air is not told it is
// standing on a floor, and that the two lines phase 14 will fill are already in their places.
#include <numbers>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "cnahouse/debug/WorldOverlay.hpp"

namespace
{
    using cnahouse::debug::WorldOverlay;
    using cnahouse::debug::WorldSnapshot;
    using cnahouse::physics::CollisionKind;
    using cnahouse::world::SpatialIndex;
    using Microsoft::Xna::Framework::Vector3;

    constexpr float kPi = std::numbers::pi_v<float>;

    WorldSnapshot InTheFoyer()
    {
        WorldSnapshot snapshot;
        snapshot.cell = "L0_FOYER";
        snapshot.cellFoundBy = SpatialIndex::Step::Incremental;
        snapshot.level = "L0";
        snapshot.position = Vector3(1.234F, 0.600F, -18.720F);
        snapshot.yaw = 0.0F;
        snapshot.pitch = 0.0F;
        snapshot.speed = 1.35F;
        snapshot.surface = "hardwood";
        snapshot.ground = CollisionKind::Floor;
        snapshot.groundGap = 0.021F;
        return snapshot;
    }

    /// The line beginning with @p prefix, or "" if the overlay does not have one.
    std::string LineStartingWith(const std::vector<std::string>& lines, std::string_view prefix)
    {
        for (const std::string& line : lines)
        {
            if (line.rfind(prefix, 0) == 0)
            {
                return line;
            }
        }
        return {};
    }

} // namespace

TEST(WorldOverlayTests, EveryLineTheDesignAsksForIsThere)
{
    // §69's `F2` row: *"player cell, position, yaw/pitch, ground surface, current level, held item,
    // target interactable and its full state"*. Seven things, and the two that phase 14 will fill
    // are lines NOW rather than lines later: an overlay that grows as features land is one whose
    // layout moves under a reader who has learnt where to look.
    const WorldOverlay overlay;
    const std::vector<std::string> lines = overlay.Lines(InTheFoyer());

    EXPECT_EQ(lines.size(), 7U);
    for (const char* prefix : {"cell", "pos", "look", "move", "ground", "held", "target"})
    {
        EXPECT_FALSE(LineStartingWith(lines, prefix).empty()) << "no line for " << prefix;
    }

    EXPECT_NE(LineStartingWith(lines, "cell").find("L0_FOYER"), std::string::npos);
    EXPECT_NE(LineStartingWith(lines, "cell").find("level L0"), std::string::npos);
    EXPECT_NE(LineStartingWith(lines, "ground").find("hardwood"), std::string::npos);
    EXPECT_NE(LineStartingWith(lines, "ground").find("floor"), std::string::npos);
    // In MILLIMETRES, and labelled as such: §49.3's probe reaches 0.45 m and the gaps worth
    // looking at are the ones a body is resting on, which are two or three of them.
    EXPECT_NE(LineStartingWith(lines, "ground").find("gap 21 mm"), std::string::npos)
        << LineStartingWith(lines, "ground");
    EXPECT_EQ(LineStartingWith(lines, "held"), "held     -") << "nothing is held until phase 14";
    EXPECT_EQ(LineStartingWith(lines, "target"), "target   -");
}

TEST(WorldOverlayTests, TheBearingIsTheOneSectionFourteenDefines)
{
    // §14: yaw 0 looks north (-Z) and positive turns east. That is a compass bearing already, so
    // the overlay's job is to say so in degrees instead of making its reader do trigonometry.
    struct Case
    {
        float yaw;
        float bearing;
        const char* point;
    };

    const Case cases[] = {
        {0.0F, 0.0F, "N"},
        {kPi * 0.25F, 45.0F, "NE"},
        {kPi * 0.5F, 90.0F, "E"},
        {kPi * 0.75F, 135.0F, "SE"},
        {kPi, 180.0F, "S"},
        {-kPi * 0.5F, 270.0F, "W"},
        {-kPi * 0.75F, 225.0F, "SW"},
        {kPi * 1.75F, 315.0F, "NW"},
    };
    for (const Case& one : cases)
    {
        EXPECT_NEAR(WorldOverlay::BearingDegrees(one.yaw), one.bearing, 1e-3F) << one.point;
        EXPECT_EQ(WorldOverlay::Compass(one.yaw), one.point);
    }

    // Wrapped, in both directions and more than once round: a yaw that has been turning for an
    // hour is still a bearing.
    EXPECT_NEAR(WorldOverlay::BearingDegrees(6.0F * kPi), 0.0F, 1e-3F);
    EXPECT_NEAR(WorldOverlay::BearingDegrees(-6.0F * kPi + kPi * 0.5F), 90.0F, 1e-2F);
    EXPECT_EQ(WorldOverlay::Compass(-8.0F * kPi), "N");

    // Each point covers 45°, so it is the NEAREST one and not the one just passed: 20° is still
    // north and 25° is already north-east.
    EXPECT_EQ(WorldOverlay::Compass(20.0F * kPi / 180.0F), "N");
    EXPECT_EQ(WorldOverlay::Compass(25.0F * kPi / 180.0F), "NE");
    EXPECT_EQ(WorldOverlay::Compass(359.0F * kPi / 180.0F), "N") << "the wrap must not read as NW";
}

TEST(WorldOverlayTests, ThePositionCanBeTypedBackIntoTeleport)
{
    // Millimetres, with a sign always shown. A metre-precision position cannot be typed back into
    // §69's `teleport`, which is most of what a position on a debug overlay is for.
    WorldSnapshot snapshot = InTheFoyer();
    snapshot.position = Vector3(-12.3456F, 0.6F, 18.0F);

    const WorldOverlay overlay;
    EXPECT_EQ(LineStartingWith(overlay.Lines(snapshot), "pos"), "pos      -12.346 +0.600 +18.000");
}

TEST(WorldOverlayTests, ABodyInTheAirIsNotToldItIsStandingOnAFloor)
{
    // The failure this line exists to make visible: `floor '-' gap 0 mm` is an ANSWER, and a body
    // with nothing under it has not got one.
    WorldSnapshot snapshot = InTheFoyer();
    snapshot.onGround = false;
    snapshot.surface.clear();
    snapshot.groundGap = 0.0F;

    const WorldOverlay overlay;
    const std::vector<std::string> lines = overlay.Lines(snapshot);
    EXPECT_EQ(LineStartingWith(lines, "ground"), "ground   nothing under the body");
    EXPECT_NE(LineStartingWith(lines, "move").find("AIRBORNE"), std::string::npos);
}

TEST(WorldOverlayTests, TheMoveLineSaysWhichOfSectionFortyThreesModesIsOn)
{
    WorldSnapshot snapshot = InTheFoyer();
    const WorldOverlay overlay;
    EXPECT_NE(LineStartingWith(overlay.Lines(snapshot), "move").find("walk"), std::string::npos);
    EXPECT_EQ(LineStartingWith(overlay.Lines(snapshot), "move").find("crouched"), std::string::npos);

    snapshot.fastWalk = true;
    snapshot.speed = 2.05F;
    EXPECT_NE(LineStartingWith(overlay.Lines(snapshot), "move").find("run"), std::string::npos);
    EXPECT_NE(LineStartingWith(overlay.Lines(snapshot), "move").find("2.05"), std::string::npos);

    snapshot.crouched = true;
    EXPECT_NE(LineStartingWith(overlay.Lines(snapshot), "move").find("crouched"), std::string::npos);
}

TEST(WorldOverlayTests, WhichOfTheFourStepsFoundTheCellIsOnTheLine)
{
    // "grid" on every frame means §16.4's incremental test is failing and the 5 cm hysteresis is
    // doing nothing -- which is invisible in a cell id that happens to be right.
    WorldSnapshot snapshot = InTheFoyer();
    const WorldOverlay overlay;
    EXPECT_NE(LineStartingWith(overlay.Lines(snapshot), "cell").find("incremental"), std::string::npos);

    snapshot.cellFoundBy = SpatialIndex::Step::Neighbour;
    EXPECT_NE(LineStartingWith(overlay.Lines(snapshot), "cell").find("neighbour"), std::string::npos);
    snapshot.cellFoundBy = SpatialIndex::Step::Grid;
    EXPECT_NE(LineStartingWith(overlay.Lines(snapshot), "cell").find("grid"), std::string::npos);

    // And §16.4's fourth answer is "no cell contains the point", which is a world-data bug and is
    // shouted rather than mentioned.
    snapshot.cellFoundBy = SpatialIndex::Step::None;
    snapshot.cell.clear();
    const std::string line = LineStartingWith(overlay.Lines(snapshot), "cell");
    EXPECT_NE(line.find("NOT FOUND"), std::string::npos) << line;
    EXPECT_NE(line.find("-"), std::string::npos);
}

TEST(WorldOverlayTests, TheHeldItemAndTheTargetShowWhenThereIsOne)
{
    // Phase 14 fills these. The lines work now, so that when it does there is nothing to write
    // here but the value.
    WorldSnapshot snapshot = InTheFoyer();
    snapshot.heldItem = "ITEM_TORCH";
    snapshot.target = "D_L0_FOYER_HALL";
    snapshot.targetState = "closed, unlocked";

    const WorldOverlay overlay;
    const std::vector<std::string> lines = overlay.Lines(snapshot);
    EXPECT_EQ(LineStartingWith(lines, "held"), "held     ITEM_TORCH");
    EXPECT_EQ(LineStartingWith(lines, "target"), "target   D_L0_FOYER_HALL  closed, unlocked");
    EXPECT_EQ(lines.size(), 7U) << "the layout moved when the values arrived";
}

TEST(WorldOverlayTests, TheOverlayStartsHiddenAndToggles)
{
    WorldOverlay overlay;
    EXPECT_FALSE(overlay.Visible());
    overlay.Toggle();
    EXPECT_TRUE(overlay.Visible());
    overlay.Toggle();
    EXPECT_FALSE(overlay.Visible());
    overlay.SetVisible(true);
    EXPECT_TRUE(overlay.Visible());

    // Hidden or not, the lines are the same: `Draw` decides whether to show them, so a test and a
    // screenshot never disagree about what the overlay would have said.
    EXPECT_EQ(overlay.Lines(InTheFoyer()).size(), 7U);
}
