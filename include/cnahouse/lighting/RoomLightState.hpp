// SPDX-License-Identifier: MIT
#pragma once

#include <cstdint>

#include "Microsoft/Xna/Framework/Vector3.hpp"

#include "cnahouse/util/Ids.hpp"

namespace cnahouse::lighting
{

    /// @brief §30's *"very dark but not black"* floor.
    ///
    /// *"The `ambientFloor` (≈ 0.025 × the room's paint colour)"* — what a windowless room with its
    /// light off still shows, so a closet is a silhouette rather than a hole in the screen.
    /// `HOUSE-01257` is where it reaches the draw; this is the number, declared once.
    inline constexpr float kAmbientFloor = 0.025F;

    /// @brief §28.5: the daylight level at which the KEY light becomes the sun rather than a bulb.
    inline constexpr float kDaylightKeyThreshold = 0.15F;

    /// @brief One cell's lighting, as §28.1's per-frame loop computes it.
    ///
    /// §28.3's whole argument is that a room's lighting SHAPE is baked and its INTENSITY is not, so
    /// the levels are the scalars the baked passes use. `artificialColor` is the lumen-weighted
    /// Planckian hue of the groups that are on; the later renderer combines it with paint and sky
    /// into §28.1's `ambientColor`.
    ///
    /// Everything here is in `[0, 1]` and everything is a **blend rather than a switch**, for the
    /// reason §36.3 gives about seasons and §28.4 gives about doors: a level that stepped would be
    /// seen, because a player watches a room while a door swings.
    struct RoomLightState
    {
        util::Id cell;

        /// @brief The cell's artificial light, 0 with every switch off and 1 with every switch on.
        ///
        /// **Weighted by lumens and not by fixture count**, which is the only combination that
        /// behaves: `L0_KITCHEN`'s four 1 200 lm down-lights and its one 60 lm cabinet strip are
        /// not a fifth of the room each, and a mean over groups would say they were.
        float artificial = 0.0F;

        /// @brief Lumen-weighted colour of the artificial sources that currently contribute.
        ///
        /// Black when every group is off. This is a hue, not radiance: `artificial` remains the
        /// one intensity value, so a consumer must not multiply the lumens into the colour again.
        Microsoft::Xna::Framework::Vector3 artificialColor{0.0F, 0.0F, 0.0F};

        /// @brief §28.4's daylight, `[0, 1]`. `DaylightModel` computes it and `HOUSE-01564` copies
        ///        the result here in world-cell order on every lighting update.
        float daylight = 0.0F;

        /// @brief §28.4's 2-hop flood through open portals. `HOUSE-01265` computes this.
        float borrowed = 0.0F;

        /// @brief What the cell is lit to in all: the floor, plus whichever sources reach it.
        ///
        /// Not a sum — two full sources do not make a room twice as bright as it can be — and not a
        /// max either, since a room with sun AND lights on is brighter than with one of them. §28.1
        /// gives no formula, so this is the smallest one that behaves: a saturating combination
        /// that is exactly `kAmbientFloor` when nothing is lit and exactly 1 when anything is full.
        [[nodiscard]] float Level() const noexcept;

        /// @brief Whether §28.5 would take the KEY light from the sun rather than from a fixture.
        [[nodiscard]] bool DaylightIsKey() const noexcept
        {
            return daylight >= kDaylightKeyThreshold;
        }
    };

    /// @brief One switch group's state. §53: a group is *"what a wall switch controls"*.
    ///
    /// **Stable-id keyed and value-shaped**, because §65's delta save has to write this: a group is
    /// `LG_L0_KITCHEN_MAIN` for the life of the project, and the save records the groups that
    /// differ from `initialstate.json` rather than an array whose meaning is its index.
    struct SwitchGroupState
    {
        util::Id group;
        bool on = false;

        /// @brief §28.1's `fixtureDimmer(g)`, `[0, 1]`. 1 for a group with no dimmer, which is all
        ///        135 of them today; the field is here because the level it multiplies is.
        float dimmer = 1.0F;

        /// @brief The level this group contributes: `switchState · fixtureDimmer`.
        [[nodiscard]] float Level() const noexcept
        {
            return on ? dimmer : 0.0F;
        }
    };

} // namespace cnahouse::lighting
