// SPDX-License-Identifier: MIT
#include <gtest/gtest.h>

#include <array>
#include <vector>

#include "cnahouse/environment/Season.hpp"
#include "cnahouse/util/Ids.hpp"
#include "cnahouse/weather/WeatherSampler.hpp"

namespace
{
    using cnahouse::environment::SeasonAt;
    using cnahouse::util::Id;
    using cnahouse::util::IdRegistry;
    using cnahouse::util::Intern;
    using cnahouse::util::Rng;
    using cnahouse::weather::WeatherArchetype;
    using cnahouse::weather::WeatherSampler;
    using cnahouse::weather::WeatherTransition;
    using cnahouse::weather::WeatherTransitionRow;

    class WeatherSeasonTests : public ::testing::Test
    {
    protected:
        void SetUp() override
        {
            IdRegistry::ResetForTesting();
            a_ = Intern("W_A");
            b_ = Intern("W_B");
            c_ = Intern("W_C");

            WeatherArchetype a;
            a.id = a_;
            a.weight = 1.0F;
            a.dwellMinutes = {100.0F, 200.0F};
            a.transitionMinutes = {10.0F, 20.0F};
            WeatherArchetype b;
            b.id = b_;
            b.weight = 1.0F;
            b.seasonalWeights = {2.0F, 0.5F, 1.0F, 1.0F};
            b.dwellMinutes = {40.0F, 80.0F};
            b.transitionMinutes = {30.0F, 40.0F};
            b.seasonalDwellScales = {1.0F, 0.75F, 1.25F, 1.5F};
            WeatherArchetype c;
            c.id = c_;
            c.weight = 1.0F;
            c.seasonalWeights = {0.5F, 2.0F, 1.0F, 1.0F};
            c.dwellMinutes = {60.0F, 120.0F};
            c.transitionMinutes = {20.0F, 30.0F};
            archetypes_ = {a, b, c};
            transitions_ = {
                WeatherTransitionRow{a_, {{b_, 0.5F}, {c_, 0.5F}}},
                WeatherTransitionRow{b_, {{a_, 1.0F}}},
                WeatherTransitionRow{c_, {{a_, 1.0F}}},
            };
        }

        void TearDown() override
        {
            IdRegistry::ResetForTesting();
        }

        [[nodiscard]] WeatherSampler Sampler() const
        {
            return WeatherSampler(archetypes_, transitions_);
        }

        Id a_;
        Id b_;
        Id c_;
        std::vector<WeatherArchetype> archetypes_;
        std::vector<WeatherTransitionRow> transitions_;
    };

    TEST_F(WeatherSeasonTests, SeasonalWeightsTurnOneBaseRowIntoDifferentMatrices)
    {
        const WeatherSampler sampler = Sampler();
        const auto spring = sampler.Distribution(a_, SeasonAt(0.125));
        const auto summer = sampler.Distribution(a_, SeasonAt(0.375));
        ASSERT_TRUE(spring) << spring.Error().ToString();
        ASSERT_TRUE(summer) << summer.Error().ToString();
        ASSERT_EQ(spring.Value().size(), 2U);
        ASSERT_EQ(summer.Value().size(), 2U);

        EXPECT_EQ(spring.Value()[0].archetype, b_);
        EXPECT_FLOAT_EQ(spring.Value()[0].probability, 0.8F);
        EXPECT_FLOAT_EQ(spring.Value()[1].probability, 0.2F);
        EXPECT_FLOAT_EQ(summer.Value()[0].probability, 0.2F);
        EXPECT_FLOAT_EQ(summer.Value()[1].probability, 0.8F);
    }

    TEST_F(WeatherSeasonTests, BlendedMatrices)
    {
        const WeatherSampler sampler = Sampler();
        cnahouse::environment::SeasonPhase phase;
        phase.primary = 0;
        phase.secondary = 1;
        phase.blend = 0.25F;
        const auto distribution = sampler.Distribution(a_, phase);
        ASSERT_TRUE(distribution) << distribution.Error().ToString();

        // B: mix(2, .5, .25) = 1.625; C: mix(.5, 2, .25) = .875. Equal base
        // probabilities therefore normalise to .65 and .35.
        EXPECT_FLOAT_EQ(distribution.Value()[0].probability, 0.65F);
        EXPECT_FLOAT_EQ(distribution.Value()[1].probability, 0.35F);
    }

    TEST_F(WeatherSeasonTests, CrossingASeasonBoundaryChangesNoProbabilityDiscontinuously)
    {
        const WeatherSampler sampler = Sampler();
        const auto before = sampler.Distribution(a_, SeasonAt(0.25 - 1e-7));
        const auto after = sampler.Distribution(a_, SeasonAt(0.25 + 1e-7));
        ASSERT_TRUE(before);
        ASSERT_TRUE(after);
        ASSERT_EQ(before.Value().size(), after.Value().size());
        for (std::size_t index = 0; index < before.Value().size(); ++index)
        {
            EXPECT_EQ(before.Value()[index].archetype, after.Value()[index].archetype);
            EXPECT_NEAR(before.Value()[index].probability, after.Value()[index].probability, 2e-6F);
        }
    }

    TEST_F(WeatherSeasonTests, OneSeedProducesOneExactArchetypeSequence)
    {
        const WeatherSampler sampler = Sampler();
        Rng rng(0xC0FFEEU);
        std::array<Id, 12> actual{};
        for (Id& value : actual)
        {
            const auto sampled = sampler.SampleNext(a_, SeasonAt(0.125), rng);
            ASSERT_TRUE(sampled) << sampled.Error().ToString();
            value = sampled.Value();
        }

        const std::array expected{b_, c_, b_, b_, b_, b_, b_, c_, b_, b_, b_, c_};
        EXPECT_EQ(actual, expected);
    }

    TEST_F(WeatherSeasonTests, EverySampleConsumesExactlyOneWeatherRngDraw)
    {
        const WeatherSampler sampler = Sampler();
        Rng sampled(42);
        Rng oracle(42);
        ASSERT_TRUE(sampler.SampleNext(a_, SeasonAt(0.125), sampled));
        (void)oracle.NextFloat();
        EXPECT_EQ(sampled.GetState(), oracle.GetState());
    }

    TEST_F(WeatherSeasonTests, TimingDrawsStayInsideTheAuthoredGlobalBounds)
    {
        const WeatherSampler sampler = Sampler();
        for (std::uint64_t seed = 1; seed <= 10000U; ++seed)
        {
            Rng rng(seed);
            const auto timing =
                sampler.SampleTiming(a_, b_, SeasonAt(static_cast<double>(seed) / 10000.0), rng);
            ASSERT_TRUE(timing) << timing.Error().ToString();
            EXPECT_GE(timing.Value().dwellMinutes, 30.0F);
            EXPECT_LE(timing.Value().dwellMinutes, 120.0F);
            EXPECT_GE(timing.Value().transitionMinutes, 20.0F);
            EXPECT_LE(timing.Value().transitionMinutes, 30.0F);
        }
    }

    TEST_F(WeatherSeasonTests, SeasonalDwellScaleIsBlendedContinuously)
    {
        const WeatherSampler sampler = Sampler();
        cnahouse::environment::SeasonPhase blended;
        blended.primary = 0;
        blended.secondary = 1;
        blended.blend = 0.25F;
        Rng springRng(77);
        Rng blendedRng(77);
        const auto spring = sampler.SampleTiming(a_, b_, SeasonAt(0.125), springRng);
        const auto between = sampler.SampleTiming(a_, b_, blended, blendedRng);
        ASSERT_TRUE(spring);
        ASSERT_TRUE(between);
        EXPECT_FLOAT_EQ(between.Value().dwellMinutes, spring.Value().dwellMinutes * 0.9375F);
        EXPECT_FLOAT_EQ(between.Value().transitionMinutes, spring.Value().transitionMinutes);
    }

    TEST_F(WeatherSeasonTests, TransitionTimingUsesBothArchetypesAndExactlyTwoDraws)
    {
        const WeatherSampler sampler = Sampler();
        Rng sampled(42);
        Rng oracle(42);
        const float expectedDwell = 40.0F + 40.0F * oracle.NextFloat();
        // The source's [10, 20] and destination's [30, 40] become [20, 30].
        const float expectedTransition = 20.0F + 10.0F * oracle.NextFloat();
        const auto timing = sampler.SampleTiming(a_, b_, SeasonAt(0.125), sampled);
        ASSERT_TRUE(timing) << timing.Error().ToString();
        EXPECT_FLOAT_EQ(timing.Value().dwellMinutes, expectedDwell);
        EXPECT_FLOAT_EQ(timing.Value().transitionMinutes, expectedTransition);
        EXPECT_EQ(sampled.GetState(), oracle.GetState());
    }

    TEST_F(WeatherSeasonTests, InvalidTimingIsRefusedBeforeTheRngMoves)
    {
        archetypes_[1].dwellMinutes = {24.0F, 80.0F};
        const WeatherSampler sampler = Sampler();
        Rng sampled(91);
        Rng oracle(91);
        const auto timing = sampler.SampleTiming(a_, b_, SeasonAt(0.125), sampled);
        ASSERT_FALSE(timing);
        EXPECT_EQ(sampled.GetState(), oracle.GetState());
    }

    TEST_F(WeatherSeasonTests, AnUnknownCurrentStateAndAZeroRowAreErrors)
    {
        const WeatherSampler sampler = Sampler();
        EXPECT_FALSE(sampler.Distribution(Intern("W_UNKNOWN"), SeasonAt(0.0)));

        archetypes_[1].seasonalWeights = {0.0F, 0.0F, 0.0F, 0.0F};
        archetypes_[2].seasonalWeights = {0.0F, 0.0F, 0.0F, 0.0F};
        const WeatherSampler zero(archetypes_, transitions_);
        const auto distribution = zero.Distribution(a_, SeasonAt(0.0));
        ASSERT_FALSE(distribution);
        EXPECT_NE(distribution.Error().Message().find("zero probability"), std::string::npos)
            << distribution.Error().ToString();
    }

} // namespace
