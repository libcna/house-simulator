// SPDX-License-Identifier: MIT
//
// `HOUSE-00027`'s acceptance, literally: (1) the same seed reproduces the same 10^6 draws;
// (2) the state round-trips through JSON.
#include <gtest/gtest.h>

#include <bit>
#include <set>
#include <unordered_map>

#include "cnahouse/util/Rng.hpp"

namespace
{
    using cnahouse::util::Bag;
    using cnahouse::util::Rng;

    TEST(RngTests, TheSameSeedReproducesAMillionDraws)
    {
        // A million, not a hundred: a generator with a subtly wrong state update can agree for a long
        // time and then diverge, and the whole point of `--seed` is that a bug report replays exactly.
        Rng a(0xC0FFEEull);
        Rng b(0xC0FFEEull);
        for (int i = 0; i < 1'000'000; ++i)
        {
            ASSERT_EQ(a.NextUInt64(), b.NextUInt64()) << "streams diverged at draw " << i;
        }
    }

    TEST(RngTests, DifferentSeedsProduceDifferentStreams)
    {
        Rng a(1);
        Rng b(2);
        int same = 0;
        for (int i = 0; i < 1000; ++i)
        {
            if (a.NextUInt64() == b.NextUInt64())
            {
                ++same;
            }
        }
        EXPECT_EQ(same, 0);
    }

    TEST(RngTests, ASmallSeedIsAsGoodAsAnyOther)
    {
        // Seeding the four state words straight from one integer would leave the state near-zero for a
        // small seed, and xoshiro recovers from that only slowly -- so `--seed 1` would produce visibly
        // poor randomness for hundreds of draws. SplitMix64 is what prevents it, and this is the test
        // that would notice if someone removed it.
        Rng rng(1);
        int ones = 0;
        constexpr int kDraws = 100'000;
        for (int i = 0; i < kDraws; ++i)
        {
            ones += static_cast<int>(std::popcount(rng.NextUInt64()));
        }
        const double meanBits = static_cast<double>(ones) / kDraws;
        EXPECT_NEAR(meanBits, 32.0, 0.5) << "a healthy stream sets about half its bits";
    }

    TEST(RngTests, StateRoundTripsThroughHex)
    {
        Rng rng(0xDEADBEEFull);
        for (int i = 0; i < 17; ++i)
        {
            (void)rng.NextUInt64();
        }
        const std::string hex = rng.ToHex();
        ASSERT_EQ(hex.size(), 64u);

        Rng restored(0);
        ASSERT_TRUE(restored.FromHex(hex));
        for (int i = 0; i < 1000; ++i)
        {
            ASSERT_EQ(rng.NextUInt64(), restored.NextUInt64()) << "restored stream diverged at " << i;
        }
    }

    TEST(RngTests, BadHexLeavesTheStateUntouched)
    {
        // A half-restored generator is worse than a rejected one, because it looks like it worked.
        Rng rng(42);
        const auto before = rng.GetState();
        EXPECT_FALSE(rng.FromHex("too short"));
        EXPECT_FALSE(rng.FromHex(std::string(64, 'z')));
        EXPECT_EQ(rng.GetState(), before);
    }

    TEST(RngTests, NextIntCoversItsWholeInclusiveRange)
    {
        Rng rng(7);
        std::set<std::int32_t> seen;
        for (int i = 0; i < 10'000; ++i)
        {
            const std::int32_t value = rng.NextInt(-3, 3);
            ASSERT_GE(value, -3);
            ASSERT_LE(value, 3);
            seen.insert(value);
        }
        EXPECT_EQ(seen.size(), 7u) << "both ends of the range must be reachable";
    }

    TEST(RngTests, NextIntIsUnbiasedAcrossAnAwkwardRange)
    {
        // 3 does not divide 2^64, so a plain `% range` would favour the low values. The bias is small
        // enough to be invisible in casual use and large enough to matter in a table lookup, which is
        // why the rejection method is there and why this test exists.
        Rng rng(0x1234);
        std::unordered_map<std::int32_t, int> counts;
        constexpr int kDraws = 300'000;
        for (int i = 0; i < kDraws; ++i)
        {
            ++counts[rng.NextInt(0, 2)];
        }
        for (const auto& [value, count] : counts)
        {
            const double share = static_cast<double>(count) / kDraws;
            EXPECT_NEAR(share, 1.0 / 3.0, 0.01) << "value " << value << " is over-represented";
        }
    }

    TEST(RngTests, NextIntHandlesADegenerateRange)
    {
        Rng rng(1);
        EXPECT_EQ(rng.NextInt(5, 5), 5);
        EXPECT_EQ(rng.NextInt(9, 3), 9) << "an inverted range returns the low bound, never garbage";
    }

    TEST(RngTests, NextFloatStaysInsideItsHalfOpenRange)
    {
        Rng rng(99);
        for (int i = 0; i < 200'000; ++i)
        {
            const float value = rng.NextFloat();
            ASSERT_GE(value, 0.0f);
            ASSERT_LT(value, 1.0f) << "the range is half-open; 1.0 must never appear";
        }
    }

    TEST(BagTests, EveryItemIsDrawnOncePerCycle)
    {
        Rng rng(3);
        Bag<int> bag(std::vector<int>{1, 2, 3, 4, 5});
        std::multiset<int> drawn;
        for (int i = 0; i < 5; ++i)
        {
            drawn.insert(bag.Draw(rng));
        }
        EXPECT_EQ(drawn.size(), 5u);
        for (int value = 1; value <= 5; ++value)
        {
            EXPECT_EQ(drawn.count(value), 1u) << value << " should appear exactly once per cycle";
        }
    }

    TEST(BagTests, NeverRepeatsAcrossACycleBoundary)
    {
        // This is the whole reason the type exists: a repeat at the boundary is what a player hears as
        // "the dog barked the same way twice", and uniform selection produces it regularly.
        Rng rng(11);
        Bag<int> bag(std::vector<int>{1, 2, 3, 4});
        int previous = -1;
        for (int i = 0; i < 4000; ++i)
        {
            const int value = bag.Draw(rng);
            ASSERT_NE(value, previous) << "immediate repeat at draw " << i;
            previous = value;
        }
    }

    TEST(BagTests, ASingleItemBagIsNotAnError)
    {
        Rng rng(1);
        Bag<int> bag(std::vector<int>{42});
        for (int i = 0; i < 10; ++i)
        {
            EXPECT_EQ(bag.Draw(rng), 42) << "one variation is a legitimate authoring choice";
        }
    }

} // namespace
