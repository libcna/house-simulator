// SPDX-License-Identifier: MIT
#pragma once

#include <cstdint>

#include "cnahouse/util/Ids.hpp"

namespace cnahouse::rendering
{

    /// @brief §11.4's "real windows with interior-glow cards at night": WHICH of them are lit.
    ///
    /// `HOUSE-00849`. `neighbourhood_gen.py` draws a card behind every window of a LOD0 house --
    /// four on `MODEL_NB_HOUSE_A_*`, four on `_B_*`, two on `_C_*` -- and drawing all of them is
    /// what makes a street read as a showroom. A house at night has some lights on and some off,
    /// and which ones is a fact about the house rather than about the mesh, which is why it lives
    /// here and not in the geometry: the mesh is the same every night and this is not.
    ///
    /// **When** they are lit is a different question and is `HOUSE-00850`'s: §35.3 puts the
    /// exterior lights on a dusk sensor with per-fixture offsets, and §35's clock (`HOUSE-01531`)
    /// is what will supply the hour. Nothing here reads a clock, so nothing here is waiting for
    /// one -- a caller passes the night level it already has.
    ///
    /// **Deterministic and stable across platforms.** The mask comes from `util::Id::Hash` --
    /// FNV-1a, 32-bit, no seed, the same function ids themselves use -- so a house has the same
    /// windows lit on Linux, in a browser, and in a screenshot taken a year apart. A `rand()` here
    /// would make every reference frame of a night scene unreproducible.
    namespace WindowGlow
    {
        /// @brief The most windows one mask can describe. §11.4's houses have two or four.
        inline constexpr std::uint32_t kMaxWindows = 32u;

        /// @brief Which of @p windowCount windows of @p instance are lit, one bit each.
        ///
        /// Bit *n* is window *n* in the order `openings_on` emits them, which is the order the
        /// glow cards are drawn in.
        ///
        /// @param instance the `neighbourhood` row's id -- `NB_HOUSE_N1`, not its asset. Two
        ///        houses sharing a mesh must not share a lighting pattern, and the asset is
        ///        exactly what they do share.
        /// @param windowCount 0 gives 0; more than `kMaxWindows` is clamped to it.
        [[nodiscard]] std::uint32_t LitMask(util::Id instance, std::uint32_t windowCount) noexcept;

        /// @brief Whether window @p index of @p instance is lit.
        [[nodiscard]] bool IsLit(util::Id instance, std::uint32_t windowCount, std::uint32_t index) noexcept;

        /// @brief How many of them are.
        [[nodiscard]] std::uint32_t LitCount(util::Id instance, std::uint32_t windowCount) noexcept;

        /// @brief The emissive level a lit card is drawn at, 0 when it is not lit.
        ///
        /// @param night 0 in full day, 1 in full night -- §35.3's own curve, which the caller
        ///        already has and this does not recompute.
        /// @return `night` scaled by a per-window brightness in `kDimmest`..1, so a street's lit
        ///         windows are not all the same lamp. Curtains, a hallway, a room two doors back:
        ///         they differ, and identical brightness is the second way a row of houses reads
        ///         as copies after identical shapes.
        [[nodiscard]] float
        Level(util::Id instance, std::uint32_t windowCount, std::uint32_t index, float night) noexcept;

        /// @brief The dimmest a lit window is drawn, as a fraction of a fully lit one.
        inline constexpr float kDimmest = 0.45F;
    } // namespace WindowGlow

} // namespace cnahouse::rendering
