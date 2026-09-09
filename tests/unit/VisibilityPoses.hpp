// SPDX-License-Identifier: MIT
#pragma once

#include <array>
#include <string_view>

/// @file
/// §25.8's *"set of 24 named camera poses with expected visible-cell sets"* (`HOUSE-00685`).
///
/// **A fixture, not a test.** `HOUSE-00686` asserts the sets exactly, `HOUSE-00689` the golden
/// list with every door shut, `HOUSE-00690` the budget with every door open, and `HOUSE-00693` the
/// stair well; all four read this one list, because twenty-four poses authored four times are four
/// lists that drift.
///
/// **Feet, not eyes.** A pose is somewhere a body can stand -- §43.1's capsule on §12's floor --
/// and the eye is 1.68 m above it. Authoring eye positions would let a pose sit inside a wall or
/// in the air and still look plausible.
///
/// **Both door states are here, and they are different poses.** §65.6 starts every door shut, and
/// a house with every door open is the arrangement no player will ever make but every budget must
/// survive; the visible set is a different set in each, so a pose names which one it is.
namespace cnahouse::testsupport
{

    struct VisibilityPose
    {
        /// @brief The name §25.8 refers to it by. Stable: a failure names this.
        std::string_view name;
        /// @brief §16's cell the feet are in. Asserted, because a pose that moved into the next
        ///        room would still produce a visible set and it would be the wrong one's.
        std::string_view cell;
        /// @brief Feet in metres, and §14's yaw in degrees (0 north, positive east).
        float x;
        float y;
        float z;
        float yawDegrees;
        /// @brief Whether every leafed portal in the house is open. False is §65.6's start.
        bool doorsOpen;
        /// @brief The cells §25's walk must reach, EXACTLY. In id order.
        std::array<std::string_view, 16> visible;
        /// @brief One of §70.4's *"12 budget poses"* (`HOUSE-00690`).
        ///
        /// The twelve a budget is measured at: the spine of each storey, the open plan that sees
        /// furthest, the stair well that sees three of them, and one outdoors looking back at the
        /// house. Not a sample of the house -- a sample of its WORST corners, because a budget met
        /// in a cupboard is not met. Last in the struct so that a pose which is not one of them
        /// says nothing at all.
        bool budget = false;
    };

    /// @brief §25.8's twenty-four.
    ///
    /// Chosen to cover what a portal system can get wrong rather than to look around the house:
    /// a room with one doorway and a room with five, both ends of the stair well, a landing that
    /// sees three storeys, a basement with nothing but its own walls, the garage across a shut
    /// fire door, and four outdoor poses looking back in through glazing.
    inline constexpr std::array<VisibilityPose, 24> kVisibilityPoses{{
        // one door, shut: a room that sees nothing but itself, which is what a cinema is for
        {"b1-cinema", "B1_CINEMA", 5.45f, -2.30f, -25.05f, 270.0f, false, {"B1_CINEMA"}},
        // the basement spine with a door to every room off it, all shut
        {"b1-hall", "B1_HALL", 0.00f, -2.30f, -20.70f, 0.0f, false, {"B1_HALL"}, true},
        // its two windows onto the west front yard are behind the camera at this heading
        {"b1-gym", "B1_GYM", -5.20f, -2.30f, -16.30f, 90.0f, false, {"B1_GYM"}},
        // §12.1's front door is shut and its sidelights are glass: the porch and the road through them
        {"l0-foyer",
         "L0_FOYER",
         0.00f,
         0.60f,
         -16.30f,
         180.0f,
         false,
         {"EXT_FRONTYARD_E", "EXT_FRONTYARD_W", "EXT_ROAD", "L0_FOYER", "L0_PORCH"},
         true},
        // the same pose with the front door open: the walk and EXT_WORLD arrive, which the sidelights'
        // narrower cones could not reach
        {"l0-foyer-open",
         "L0_FOYER",
         0.00f,
         0.60f,
         -16.30f,
         180.0f,
         true,
         {"EXT_FRONTYARD_E", "EXT_FRONTYARD_W", "EXT_ROAD", "EXT_WALK", "EXT_WORLD", "L0_FOYER", "L0_PORCH"}},
        // north through the kitchen's cased opening and on through the open plan to the sunroom's glazing
        {"l0-hall",
         "L0_HALL",
         0.00f,
         0.60f,
         -20.65f,
         0.0f,
         false,
         {"EXT_TERRACE", "L0_FAMILY", "L0_HALL", "L0_KITCHEN", "L0_SUNROOM"},
         true},
        // the same pose with every door open -- including the fridge's, which §54 makes a cell with a portal
        // like any other
        {"l0-hall-open",
         "L0_HALL",
         0.00f,
         0.60f,
         -20.65f,
         0.0f,
         true,
         {"CELL_FRIDGE_INTERIOR",
          "EXT_BACKYARD",
          "EXT_TERRACE",
          "L0_DINING",
          "L0_FAMILY",
          "L0_HALL",
          "L0_KITCHEN",
          "L0_SUNROOM"}},
        // the open plan east, and three yards through the sunroom's three glazed sides
        {"l0-kitchen",
         "L0_KITCHEN",
         -3.00f,
         0.60f,
         -25.05f,
         90.0f,
         false,
         {"EXT_BACKYARD",
          "EXT_ORCHARD",
          "EXT_SIDEYARD_E",
          "L0_FAMILY",
          "L0_HALL",
          "L0_KITCHEN",
          "L0_SUNROOM"},
         true},
        // doors shut and its windows on the wall behind: a room with nothing but itself in view
        {"l0-living", "L0_LIVING", -5.20f, 0.60f, -17.25f, 90.0f, false, {"L0_LIVING"}, true},
        // east across the hall with every door open, as far as the cloakroom's
        {"l0-dining",
         "L0_DINING",
         -5.20f,
         0.60f,
         -21.60f,
         90.0f,
         true,
         {"EXT_SIDEYARD_E", "L0_DINING", "L0_FAMILY", "L0_HALL", "L0_KITCHEN", "L0_LIVING", "L0_WC1"}},
        // the longest interior sightline in the house: west through the open plan into the pantry and the
        // butler's, and into both cold containers
        {"l0-family",
         "L0_FAMILY",
         5.45f,
         0.60f,
         -24.55f,
         270.0f,
         true,
         {"CELL_FREEZER_INTERIOR",
          "CELL_FRIDGE_INTERIOR",
          "EXT_BACKYARD",
          "EXT_SIDEYARD_W",
          "L0_BUTLERS",
          "L0_DINING",
          "L0_FAMILY",
          "L0_HALL",
          "L0_KITCHEN",
          "L0_PANTRY",
          "L0_SUNROOM"},
         true},
        // ten cells with every door SHUT, which is what an open plan is: south down the whole ground floor
        // through cased openings alone
        {"l0-sunroom",
         "L0_SUNROOM",
         -2.00f,
         0.60f,
         -29.60f,
         180.0f,
         false,
         {"L0_BUTLERS",
          "L0_DINING",
          "L0_FAMILY",
          "L0_FOYER",
          "L0_HALL",
          "L0_KITCHEN",
          "L0_LIVING",
          "L0_OFFICE",
          "L0_STAIR_MAIN",
          "L0_SUNROOM"},
         true},
        // §25.8's own example, and `HOUSE-00693`'s: THREE storeys through the stair well with
        // every door shut. At the FOOT of the flight looking up it -- the cell's centre is
        // mid-flight, which is inside the stairs, and `HOUSE-00688` found that out by rendering a
        // picture of the underside of a tread.
        {"l0-stair-main",
         "L0_STAIR_MAIN",
         3.55f,
         0.60f,
         -14.60f,
         0.0f,
         false,
         {"B1_STAIR", "L0_FOYER", "L0_HALL", "L0_MUDROOM", "L0_STAIR_MAIN", "L1_LANDING", "L1_STAIR_MAIN"},
         true},
        // the fire door to the mudroom and the sectional door are both shut
        {"l0-garage", "L0_GARAGE", 12.90f, 0.60f, -17.50f, 270.0f, false, {"L0_GARAGE"}},
        // the balcony through two glazed windows; the cased openings to the hall and the stair are behind the
        // camera
        {"l1-landing",
         "L1_LANDING",
         0.00f,
         3.65f,
         -16.30f,
         180.0f,
         false,
         {"L1_BALCONY_FRONT", "L1_LANDING"},
         true},
        // north into the master bedroom and out through its slider, with every door open
        {"l1-hall",
         "L1_HALL",
         0.00f,
         3.65f,
         -20.15f,
         0.0f,
         true,
         {"L1_BALCONY_REAR", "L1_HALL", "L1_MASTER_BED"},
         true},
        // its slider is glass, so the balcony is in view with the door shut
        {"l1-master-bed",
         "L1_MASTER_BED",
         -2.40f,
         3.65f,
         -24.55f,
         90.0f,
         false,
         {"L1_BALCONY_REAR", "L1_MASTER_BED"}},
        // two storeys of balcony through glazing: the Juliet in front and the first floor's below it
        {"l2-landing",
         "L2_LANDING",
         0.00f,
         6.55f,
         -16.30f,
         180.0f,
         false,
         {"L1_BALCONY_FRONT", "L2_BALCONY_JULIET", "L2_LANDING"},
         true},
        // the library's door is shut and its windows face the other way
        {"l2-library", "L2_LIBRARY", -5.20f, 6.55f, -16.30f, 90.0f, false, {"L2_LIBRARY"}},
        // the attic and the eaves store it opens onto
        {"l3-room", "L3_ROOM", -0.55f, 9.30f, -20.50f, 0.0f, false, {"L3_ROOM", "L3_STORE_N"}},
        // §25.2's outdoor-to-indoor rule: ONE room through the sidelights, and not the hall behind it
        {"l0-porch", "L0_PORCH", 0.00f, 0.60f, -12.95f, 0.0f, false, {"L0_FOYER", "L0_PORCH"}},
        // looking north away from the house: four yards and no interior at all
        {"ext-terrace",
         "EXT_TERRACE",
         0.00f,
         0.60f,
         -34.05f,
         0.0f,
         false,
         {"EXT_BACKYARD", "EXT_NORTHSTRIP", "EXT_ORCHARD", "EXT_TERRACE"},
         true},
        // back at the house: exactly one room through the sunroom's glazing, which is the depth-1 rule
        {"ext-backyard",
         "EXT_BACKYARD",
         -9.85f,
         0.60f,
         -29.75f,
         45.0f,
         false,
         {"EXT_BACKYARD", "EXT_NORTHSTRIP", "EXT_ORCHARD", "EXT_SIDEYARD_E", "L0_SUNROOM"}},
        // the sectional door open: the garage and nothing behind its fire door
        {"ext-driveway",
         "EXT_DRIVEWAY",
         13.20f,
         0.60f,
         -6.65f,
         0.0f,
         true,
         {"EXT_DRIVEWAY", "EXT_SIDEYARD_E", "L0_GARAGE"}},
    }};

} // namespace cnahouse::testsupport
