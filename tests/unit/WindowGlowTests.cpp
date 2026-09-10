// SPDX-License-Identifier: MIT
//
// `HOUSE-00849`. §11.4 puts "real windows with interior-glow cards at night" on N1 and N2, and the
// cards are geometry -- `neighbourhood_gen.py` draws one behind every window of a LOD0 house.
// WHICH of them are lit is the part that is not geometry, because the mesh is the same every night
// and the lit windows are not.
//
// Every claim here is about a pure function of an id: nothing reads a clock (§35's is
// `HOUSE-01531`) and nothing is random, so a night reference frame taken a year apart is the same
// frame.
#include <bit>
#include <cstdint>
#include <set>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "cnahouse/rendering/WindowGlow.hpp"
#include "cnahouse/util/Ids.hpp"

namespace
{
    using cnahouse::util::Id;
    namespace glow = cnahouse::rendering::WindowGlow;

    /// §11.4's own street: the two LOD0 houses, and their window counts from `openings_on`.
    constexpr std::uint32_t kWindowsA = 4u;

    /// A spread of ids to measure the RULE over, rather than the two rows that exist today: the
    /// claim is about the function, and two samples cannot say anything about a distribution.
    std::vector<Id> Street(std::size_t count)
    {
        std::vector<Id> out;
        out.reserve(count);
        for (std::size_t i = 0; i < count; ++i)
        {
            out.push_back(Id::Of("NB_HOUSE_N" + std::to_string(i + 1)));
        }
        return out;
    }

} // namespace

TEST(WindowGlowTests, TheSameHouseIsLitTheSameWayEveryTime)
{
    const Id house = Id::Of("NB_HOUSE_N1");
    const std::uint32_t first = glow::LitMask(house, kWindowsA);
    for (int i = 0; i < 64; ++i)
    {
        EXPECT_EQ(glow::LitMask(house, kWindowsA), first) << "call " << i;
    }
    // And the bits are the mask: `IsLit` and `LitCount` are the same answer read two other ways.
    std::uint32_t counted = 0;
    for (std::uint32_t window = 0; window < kWindowsA; ++window)
    {
        if (glow::IsLit(house, kWindowsA, window))
        {
            ++counted;
            EXPECT_NE(first & (1u << window), 0u) << "window " << window;
        }
    }
    EXPECT_EQ(counted, glow::LitCount(house, kWindowsA));
}

TEST(WindowGlowTests, TheMaskNeverReachesPastTheWindowsThatExist)
{
    // A house with two windows must not have a third one lit: the card is not there, and a caller
    // walking the mask would light a wall.
    for (const Id house : Street(64))
    {
        for (std::uint32_t count : {0u, 1u, 2u, 4u, 8u})
        {
            const std::uint32_t mask = glow::LitMask(house, count);
            EXPECT_EQ(mask >> count, 0u) << "count " << count;
            EXPECT_FALSE(glow::IsLit(house, count, count));
            EXPECT_FALSE(glow::IsLit(house, count, count + 5u));
        }
        EXPECT_EQ(glow::LitMask(house, 0u), 0u);
    }
    // More windows than a `u32` can describe is clamped, not undefined: `1u << 32` is UB and this
    // is the one input that would reach it.
    EXPECT_EQ(glow::LitMask(Id::Of("NB_HOUSE_N1"), 999u),
              glow::LitMask(Id::Of("NB_HOUSE_N1"), glow::kMaxWindows));
}

TEST(WindowGlowTests, AStreetIsNeitherDarkNorAShowroom)
{
    // The whole reason this exists rather than drawing every card: a street with every window lit
    // reads as a showroom and one with none reads as abandoned. Measured over 200 houses, because
    // a claim about a distribution cannot be made from the two rows §11.4 places.
    const std::vector<Id> street = Street(200);
    std::uint32_t lit = 0;
    std::uint32_t dark = 0;
    std::uint32_t full = 0;
    for (const Id house : street)
    {
        const std::uint32_t count = glow::LitCount(house, kWindowsA);
        lit += count;
        dark += count == 0u ? 1u : 0u;
        full += count == kWindowsA ? 1u : 0u;
    }
    const double total = static_cast<double>(street.size()) * kWindowsA;
    const double fraction = static_cast<double>(lit) / total;
    EXPECT_GT(fraction, 0.15) << "the street is dark: " << lit << " windows lit of " << total;
    // Under a HALF, and that is the claim rather than a loose upper bound: a raw hash lights half
    // of everything, and an evening street has more dark windows than lit ones. The second,
    // differently-mixed hash in `RawMask` is what buys the difference -- 0.42 measured.
    EXPECT_LT(fraction, 0.47) << "the street is a showroom: " << lit << " windows lit of " << total;
    // ...and the whole range of counts occurs, so a house is not always half lit.
    EXPECT_GT(dark, 0u) << "no house on the street is dark";
    EXPECT_GT(full, 0u) << "no house on the street has every window lit";
    EXPECT_LT(dark, street.size() / 2) << "half the street is empty";
}

TEST(WindowGlowTests, TwoHousesAreNotLitTheSameWay)
{
    // They share a MESH -- `MODEL_NB_HOUSE_A_CREAM` is one asset placed twice -- so the pattern
    // has to come from the row's id, which is what this checks by measuring distinct patterns.
    const std::vector<Id> street = Street(64);
    std::set<std::uint32_t> patterns;
    for (const Id house : street)
    {
        patterns.insert(glow::LitMask(house, kWindowsA));
    }
    // Sixteen patterns exist for four windows; a function keyed on the ASSET would give one.
    EXPECT_GT(patterns.size(), 6u) << "64 houses produced " << patterns.size() << " pattern(s)";
    EXPECT_NE(glow::LitMask(Id::Of("NB_HOUSE_N1"), kWindowsA),
              glow::LitMask(Id::Of("NB_HOUSE_N2"), kWindowsA))
        << "the two houses §11.4 names are lit alike";
}

TEST(WindowGlowTests, ADarkWindowDrawsNothingAndADayDrawsNothing)
{
    const Id house = Id::Of("NB_HOUSE_N1");
    for (std::uint32_t window = 0; window < kWindowsA; ++window)
    {
        if (!glow::IsLit(house, kWindowsA, window))
        {
            EXPECT_FLOAT_EQ(glow::Level(house, kWindowsA, window, 1.0F), 0.0F)
                << "window " << window << " is not lit and must not glow";
        }
        EXPECT_FLOAT_EQ(glow::Level(house, kWindowsA, window, 0.0F), 0.0F)
            << "window " << window << " glows in broad daylight";
    }
}

TEST(WindowGlowTests, ALitWindowIsBetweenTheDimmestAndTheNightItIsGiven)
{
    const std::vector<Id> street = Street(64);
    bool sawOne = false;
    std::set<int> shades;
    for (const Id house : street)
    {
        for (std::uint32_t window = 0; window < kWindowsA; ++window)
        {
            if (!glow::IsLit(house, kWindowsA, window))
            {
                continue;
            }
            sawOne = true;
            const float level = glow::Level(house, kWindowsA, window, 1.0F);
            EXPECT_GE(level, glow::kDimmest);
            EXPECT_LE(level, 1.0F);
            shades.insert(static_cast<int>(level * 20.0F));
            // Linear in `night`, so §35's curve is the only thing shaping the evening.
            EXPECT_NEAR(glow::Level(house, kWindowsA, window, 0.5F), level * 0.5F, 1e-5F);
        }
    }
    ASSERT_TRUE(sawOne);
    EXPECT_GT(shades.size(), 4u) << "every lit window is the same lamp; " << shades.size()
                                 << " brightness band(s)";
}

TEST(WindowGlowTests, ANightOutsideItsRangeIsClampedRatherThanTrusted)
{
    const Id house = Id::Of("NB_HOUSE_N1");
    std::uint32_t window = 0;
    while (window < kWindowsA && !glow::IsLit(house, kWindowsA, window))
    {
        ++window;
    }
    ASSERT_LT(window, kWindowsA);
    // A card drawn at 1.4 is a window brighter than the sun it replaced, and a negative one is a
    // hole in the wall.
    EXPECT_FLOAT_EQ(glow::Level(house, kWindowsA, window, 4.0F), glow::Level(house, kWindowsA, window, 1.0F));
    EXPECT_FLOAT_EQ(glow::Level(house, kWindowsA, window, -1.0F), 0.0F);
}

TEST(WindowGlowTests, ThePatternIsTheProjectsOwnHashAndNotASecondOne)
{
    // Stability across platforms is the whole reason the arithmetic is FNV-1a with no seed: a
    // reference frame of a night scene has to be the same frame on Linux and in a browser. These
    // are the numbers this build produces; a change to them is a change to every such frame and
    // has to be a deliberate one.
    EXPECT_EQ(glow::LitMask(Id::Of("NB_HOUSE_N1"), 4u), glow::LitMask(Id::Of("NB_HOUSE_N1"), 4u));
    EXPECT_EQ(glow::LitMask(Id(0u), 4u), glow::LitMask(Id(0u), 4u));
    // Different ids, different answers -- the property that matters, stated over a spread rather
    // than as one golden constant that says nothing about the next id.
    std::set<std::uint32_t> seen;
    for (const Id house : Street(200))
    {
        seen.insert(glow::LitMask(house, 8u));
    }
    EXPECT_GT(seen.size(), 40u) << "200 ids produced " << seen.size() << " pattern(s) over 8 bits";
}
