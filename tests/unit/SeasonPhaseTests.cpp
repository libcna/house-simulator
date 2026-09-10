// SPDX-License-Identifier: MIT
//
// `HOUSE-01534`. §36.3's season, which is a **phase and never an enum**.
//
// The reason is §35.2b's compressed year: at 24 calendar days per simulated day a season boundary
// is crossed every 91 real minutes, so a transition matrix that switched at an instant would be
// visible as a glitch several times a session. Every seasonal quantity is the `blend`-weighted mix
// of two neighbouring seasons' values, *"never a switch"*, and what is proved here is that the mix
// really is continuous -- including across the instant where the labels change.
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <utility>

#include <gtest/gtest.h>

#include "cnahouse/environment/Season.hpp"
#include "cnahouse/environment/SimClock.hpp"

namespace
{
    using cnahouse::environment::CivilTime;
    using cnahouse::environment::kSeasonBlendFraction;
    using cnahouse::environment::kVernalEquinoxDayOfYear;
    using cnahouse::environment::MixBySeason;
    using cnahouse::environment::Season;
    using cnahouse::environment::SeasonAt;
    using cnahouse::environment::SeasonPhase;
    using cnahouse::environment::SimClock;

    constexpr int kSeasons = static_cast<int>(Season::Count);

    /// One value per season, deliberately far apart, so a mix that took the wrong one is obvious.
    constexpr float kPerSeason[4] = {10.0F, 20.0F, 30.0F, 40.0F};

} // namespace

TEST(SeasonPhaseTests, ZeroIsTheVernalEquinoxAndTheQuartersFollowFromIt)
{
    // §36.3's own comment: "0 = vernal equinox (a new game starts here)", and §35.2b's table:
    // "Starting season: Spring".
    EXPECT_EQ(SeasonAt(0.0).primary, static_cast<int>(Season::Spring));
    EXPECT_EQ(SeasonAt(0.30).primary, static_cast<int>(Season::Summer));
    EXPECT_EQ(SeasonAt(0.55).primary, static_cast<int>(Season::Autumn));
    EXPECT_EQ(SeasonAt(0.80).primary, static_cast<int>(Season::Winter));
    EXPECT_FLOAT_EQ(SeasonAt(0.30).yearFraction, 0.30F);

    // A year fraction is an angle, so it wraps rather than clamping: a caller that has just
    // stepped past 1.0 means the start of the next year, not the end of this one.
    EXPECT_EQ(SeasonAt(1.0).primary, static_cast<int>(Season::Spring));
    EXPECT_FLOAT_EQ(SeasonAt(1.25).yearFraction, 0.25F);
    EXPECT_EQ(SeasonAt(-0.05).primary, static_cast<int>(Season::Winter));
    EXPECT_EQ(SeasonAt(7.30).primary, static_cast<int>(Season::Summer));
    // ...and a value that is not a number does not index a table with garbage.
    EXPECT_EQ(SeasonAt(std::nan("")).primary, 0);
    EXPECT_GE(SeasonAt(1e18).primary, 0);
    EXPECT_LT(SeasonAt(1e18).primary, kSeasons);
}

TEST(SeasonPhaseTests, TheBlendIsZeroThroughTheMiddleAndRampsOverTheOuterFifths)
{
    // §36.3 in its own words: "`blend` is 0 through the middle of a season and ramps over the
    // outer 20 % at each end, so the last fifth of autumn is already partly winter."
    const double quarter = 0.25;
    for (int index = 0; index < kSeasons; ++index)
    {
        const double start = static_cast<double>(index) * quarter;
        // The middle three fifths: nothing but this season.
        for (const double within : {0.25, 0.5, 0.75})
        {
            const SeasonPhase phase = SeasonAt(start + within * quarter);
            EXPECT_EQ(phase.primary, index) << index << " at " << within;
            EXPECT_FLOAT_EQ(phase.blend, 0.0F) << index << " at " << within;
            EXPECT_FLOAT_EQ(MixBySeason(phase, kPerSeason), kPerSeason[index]);
        }
        // The closing fifth blends towards the NEXT season and reaches half at the boundary.
        const SeasonPhase late = SeasonAt(start + 0.9 * quarter);
        EXPECT_EQ(late.primary, index);
        EXPECT_EQ(late.secondary, (index + 1) % kSeasons) << "the closing fifth looks backwards";
        EXPECT_NEAR(late.blend, 0.25F, 1e-5) << "halfway through the last fifth is a quarter blend";
        // The opening fifth is still partly the one before, fading out.
        const SeasonPhase early = SeasonAt(start + 0.1 * quarter);
        EXPECT_EQ(early.primary, index);
        EXPECT_EQ(early.secondary, (index + kSeasons - 1) % kSeasons);
        EXPECT_NEAR(early.blend, 0.25F, 1e-5);
        // The ramp ends exactly where §36.3 says it does.
        EXPECT_NEAR(SeasonAt(start + static_cast<double>(kSeasonBlendFraction) * quarter).blend, 0.0F, 1e-5);
    }
}

TEST(SeasonPhaseTests, TheMixIsContinuousAcrossTheInstantTheLabelsChange)
{
    // The claim the whole design exists for. Walking the year in ten thousand steps, the mixed
    // value may never jump -- and the place it would jump is the boundary, where `primary` and
    // `secondary` swap and `blend` is 0.5 on both sides of the same instant.
    constexpr int kSteps = 10000;
    double largestJump = 0.0;
    double atBoundary = 0.0;
    float previous = MixBySeason(SeasonAt(0.0), kPerSeason);
    for (int step = 1; step <= kSteps; ++step)
    {
        const double fraction = static_cast<double>(step) / kSteps;
        const float now = MixBySeason(SeasonAt(fraction), kPerSeason);
        const double jump = std::abs(static_cast<double>(now - previous));
        largestJump = std::max(largestJump, jump);
        if (std::abs(fraction - 0.25) < 1e-9 || std::abs(fraction - 0.5) < 1e-9 ||
            std::abs(fraction - 0.75) < 1e-9)
        {
            atBoundary = std::max(atBoundary, jump);
        }
        previous = now;
    }
    // One step is 1/10 000 of a year; the steepest ramp changes the mix by half the distance
    // between two seasons over a fifth of a quarter, so a step can move it by at most ~0.1.
    EXPECT_LT(largestJump, 0.15) << "the seasonal mix stepped, which §36.3 forbids";
    EXPECT_LT(atBoundary, 0.15) << "the mix jumped exactly where the labels change";

    // ...and the two readings either side of a boundary really are the same mix under different
    // labels, which is the property that makes the above true rather than an accident of sampling.
    const SeasonPhase before = SeasonAt(0.25 - 1e-9);
    const SeasonPhase after = SeasonAt(0.25 + 1e-9);
    EXPECT_EQ(before.primary, static_cast<int>(Season::Spring));
    EXPECT_EQ(after.primary, static_cast<int>(Season::Summer));
    EXPECT_EQ(before.secondary, after.primary);
    EXPECT_EQ(after.secondary, before.primary);
    EXPECT_NEAR(before.blend, 0.5F, 1e-4);
    EXPECT_NEAR(after.blend, 0.5F, 1e-4);
    EXPECT_NEAR(MixBySeason(before, kPerSeason), MixBySeason(after, kPerSeason), 1e-3);
}

TEST(SeasonPhaseTests, TheBlendNeverExceedsAHalfAndTheYearVisitsEverySeason)
{
    int seen[4] = {0, 0, 0, 0};
    float highest = 0.0F;
    for (int step = 0; step < 4000; ++step)
    {
        const SeasonPhase phase = SeasonAt(static_cast<double>(step) / 4000.0);
        ASSERT_GE(phase.primary, 0);
        ASSERT_LT(phase.primary, kSeasons);
        ASSERT_GE(phase.secondary, 0);
        ASSERT_LT(phase.secondary, kSeasons);
        EXPECT_NE(phase.primary, phase.secondary) << "a season blending with itself says nothing";
        ++seen[phase.primary];
        highest = std::max(highest, phase.blend);
        EXPECT_GE(phase.blend, 0.0F);
    }
    EXPECT_NEAR(highest, 0.5F, 1e-3) << "the blend reaches something other than a half at a boundary";
    for (int index = 0; index < kSeasons; ++index)
    {
        EXPECT_EQ(seen[index], 1000) << "season " << index << " does not get a quarter of the year";
    }
}

TEST(SeasonPhaseTests, TheClockPutsTheEquinoxOnTheTwentiethOfMarch)
{
    // The phase read off the calendar rather than handed in, which is the join this task is for.
    SimClock clock;
    clock.calendarDaysPerSimDay = 1.0;
    CivilTime equinox;
    equinox.year = 2031;
    equinox.month = 3;
    equinox.day = 20;
    clock.SetStandard(equinox);
    EXPECT_EQ(clock.Standard().dayOfYear, kVernalEquinoxDayOfYear) << "20 March is not day 79";
    EXPECT_NEAR(clock.YearFraction(), 0.0, 1e-9);
    EXPECT_EQ(clock.Season().primary, static_cast<int>(Season::Spring));

    // A quarter of a year later is the summer solstice, near enough for a phase that blends.
    CivilTime midsummer = equinox;
    midsummer.month = 6;
    midsummer.day = 21;
    clock.SetStandard(midsummer);
    EXPECT_NEAR(clock.YearFraction(), 0.25, 0.01);
    EXPECT_EQ(clock.Season().primary, static_cast<int>(Season::Summer));

    // **The divisor is the year's OWN length**, which only shows up mid-year: on 20 March the
    // offset is zero whatever it is divided by. 2032 has 366 days, so its last day is 288 days
    // past the equinox out of 366 -- dividing by a flat 365 would place it 0.002 of a turn late,
    // every leap year, which is a fifth of a real minute of play in the wrong season and grows
    // with any quantity that integrates the phase.
    for (const auto& [year, length] : {std::pair{2031, 365.0}, std::pair{2032, 366.0}})
    {
        CivilTime lastDay;
        lastDay.year = year;
        lastDay.month = 12;
        lastDay.day = 31;
        clock.SetStandard(lastDay);
        // 31 December is day `length` counting from 1, so `length - 1` days past 1 January and
        // `length - kVernalEquinoxDayOfYear` past the equinox.
        EXPECT_NEAR(
            clock.YearFraction(), (length - static_cast<double>(kVernalEquinoxDayOfYear)) / length, 1e-9)
            << "31 December " << year << " is not " << length << " days into its own year";
    }

    // ...and the turn is exactly one per calendar year INCLUDING a leap year, which is what the
    // year's own length in the divisor is for: 2032 has 366 days.
    for (const int year : {2031, 2032, 2100})
    {
        CivilTime start;
        start.year = year;
        start.month = 3;
        start.day = 20;
        clock.SetStandard(start);
        const double first = clock.YearFraction();
        CivilTime next = start;
        next.year = year + 1;
        clock.SetStandard(next);
        EXPECT_NEAR(clock.YearFraction(), first, 0.01)
            << "a year later is not the same point in the phase, in " << year;
    }
}

TEST(SeasonPhaseTests, ASessionOfPlayCrossesABoundaryEveryNinetyOneRealMinutes)
{
    // §36.3's own number, and the reason none of this may be an enum. Six real hours of play is
    // one compressed year, so it must cross exactly four boundaries.
    // Started at the vernal equinox, which is where §35.2b says a new game begins -- the epoch
    // itself is 1 January and is 78 calendar days short of it, so a run from there would measure
    // the first season from the middle of winter.
    SimClock clock;
    clock.SetCalendar(static_cast<double>(kVernalEquinoxDayOfYear - 1));
    ASSERT_NEAR(clock.YearFraction(), 0.0, 1e-9);
    ASSERT_EQ(clock.Season().primary, static_cast<int>(Season::Spring));

    int crossings = 0;
    int primary = clock.Season().primary;
    double lastCrossingMinutes = 0.0;
    double widestGap = 0.0;
    double narrowestGap = 1e9;
    for (int minute = 1; minute <= 366; ++minute)
    {
        clock.Advance(60.0); // one real minute
        const int now = clock.Season().primary;
        if (now != primary)
        {
            ++crossings;
            const double gap = static_cast<double>(minute) - lastCrossingMinutes;
            widestGap = std::max(widestGap, gap);
            narrowestGap = std::min(narrowestGap, gap);
            lastCrossingMinutes = minute;
            primary = now;
        }
    }
    EXPECT_EQ(crossings, 4) << "a year of play did not pass through four seasons";
    EXPECT_NEAR(widestGap, 91.25, 1.5) << "§36.3's 91.25 real minutes a season";
    EXPECT_NEAR(narrowestGap, 91.25, 1.5);
}
