// SPDX-License-Identifier: MIT
#pragma once

namespace cnahouse::environment
{

    /// @brief §36.3's four seasons, in the order its `primary` index counts them.
    enum class Season : int
    {
        Spring = 0,
        Summer = 1,
        Autumn = 2,
        Winter = 3,
        Count = 4,
    };

    /// @brief Where the year is, as a phase rather than as a season (`HOUSE-01534`).
    ///
    /// **Season is a continuous phase, never an enum**, and §36.3 says why in one line: under
    /// §35.2b's compressed year *"a boundary is crossed every 91 real minutes, so a matrix that
    /// switched at an instant would be visible as a glitch"*. Every seasonal quantity -- the
    /// weather transition matrix, the temperature curve, vegetation colour, foliage density,
    /// snow-cover probability, the ambience bed -- is the `blend`-weighted mix of the two
    /// neighbouring seasons' values, *"never a switch"*.
    struct SeasonPhase
    {
        /// @brief 0 .. 1, with **0 at the vernal equinox**, which is where a new game starts.
        float yearFraction = 0.0F;
        /// @brief The season this is mostly, as a `Season` cast to `int`.
        int primary = 0;
        /// @brief The neighbour being blended towards or away from.
        int secondary = 0;
        /// @brief How far towards `secondary`, 0 .. 0.5.
        ///
        /// **It never exceeds 0.5, and that is the whole of what makes the mix continuous.** At a
        /// boundary the two seasons are half and half, and the reading either side of that instant
        /// is the same mix under two different labels: the last moment of autumn is
        /// `primary = Autumn, secondary = Winter, blend = 0.5`, and the first moment of winter is
        /// `primary = Winter, secondary = Autumn, blend = 0.5`. A blend that ramped to 1.0 would
        /// put a season's own end at 100 % of its successor and then flip to 100 % of its
        /// predecessor one instant later, which is the glitch §36.3 is written to avoid.
        float blend = 0.0F;
    };

    /// @brief The outer fraction of a season over which the blend ramps, at each end.
    ///
    /// §36.3: *"`blend` is 0 through the middle of a season and ramps over the outer 20 % at each
    /// end, so the last fifth of autumn is already partly winter."*
    inline constexpr float kSeasonBlendFraction = 0.2F;

    /// @brief The day of the year the phase counts from: 20 March, the vernal equinox.
    ///
    /// A fixed day rather than a solved one. The true equinox moves by up to eighteen hours across
    /// the leap cycle, which is 0.2 % of the year and below anything §36.3 blends over; solving it
    /// belongs to §35.3's sun model, where the answer is wanted to the minute for a different
    /// reason. Counting from 1, so 20 March in a common year.
    inline constexpr int kVernalEquinoxDayOfYear = 79;

    /// @brief §36.3's phase at @p yearFraction, which is wrapped into `[0, 1)` first.
    ///
    /// Wrapped rather than clamped: a year fraction is an angle, and a caller that has just
    /// stepped past 1.0 means the start of the next year and not the end of this one.
    [[nodiscard]] SeasonPhase SeasonAt(double yearFraction) noexcept;

    /// @brief The `blend`-weighted mix of two per-season values, which is what §36.3 asks callers
    ///        to do with a phase and the reason `primary` alone is never enough.
    [[nodiscard]] float MixBySeason(const SeasonPhase& phase, const float (&perSeason)[4]) noexcept;

} // namespace cnahouse::environment
