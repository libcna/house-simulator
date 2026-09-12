// SPDX-License-Identifier: MIT
#include <gtest/gtest.h>

#include <vector>

#include "cnahouse/environment/Season.hpp"
#include "cnahouse/util/Ids.hpp"
#include "cnahouse/util/Rng.hpp"
#include "cnahouse/weather/WeatherSystem.hpp"

namespace
{
    using cnahouse::environment::SeasonAt;
    using cnahouse::util::Id;
    using cnahouse::util::IdRegistry;
    using cnahouse::util::Intern;
    using cnahouse::util::Rng;
    using cnahouse::weather::PrecipType;
    using cnahouse::weather::WeatherArchetype;
    using cnahouse::weather::WeatherRates;
    using cnahouse::weather::WeatherState;
    using cnahouse::weather::WeatherSystem;
    using cnahouse::weather::WeatherTransitionRow;

    class WeatherSystemTests : public ::testing::Test
    {
    protected:
        void SetUp() override
        {
            IdRegistry::ResetForTesting();
            clear_ = Intern("W_CLEAR");
            rain_ = Intern("W_RAIN");

            WeatherArchetype clear;
            clear.id = clear_;
            clear.cloudCover = {0.0F, 0.0F};
            clear.cloudCumuliform = {0.0F, 0.0F};
            clear.precipIntensity = {0.0F, 0.0F};
            clear.windSpeed = {1.0F, 1.0F};
            clear.gustFactor = {0.1F, 0.1F};
            clear.fogDensity = {0.0F, 0.0F};
            clear.temperatureOffsetC = {1.0F, 1.0F};
            clear.humidity = {0.3F, 0.3F};
            clear.thunderProbability = {0.0F, 0.0F};
            clear.weight = 1.0F;
            clear.dwellMinutes = {25.0F, 25.0F};
            clear.transitionMinutes = {5.0F, 5.0F};

            WeatherArchetype rain = clear;
            rain.id = rain_;
            rain.cloudCover = {1.0F, 1.0F};
            rain.cloudCumuliform = {0.8F, 0.8F};
            rain.precipType = PrecipType::Rain;
            rain.precipIntensity = {0.8F, 0.8F};
            rain.windSpeed = {8.0F, 8.0F};
            rain.gustFactor = {0.6F, 0.6F};
            rain.fogDensity = {0.3F, 0.3F};
            rain.temperatureOffsetC = {-2.0F, -2.0F};
            rain.humidity = {0.9F, 0.9F};
            rain.thunderProbability = {0.4F, 0.4F};

            WeatherArchetype windy;
            windy.id = Intern("W_WINDY");
            windy.modifier = true;
            windy.windSpeed = {12.0F, 12.0F};
            windy.gustFactor = {0.8F, 0.8F};

            archetypes_ = {clear, rain, windy};
            transitions_ = {
                WeatherTransitionRow{clear_, {{rain_, 1.0F}}},
                WeatherTransitionRow{rain_, {{clear_, 1.0F}}},
            };

            rates_ = WeatherRates{1.0F, 1.0F, 1.0F, 30.0F, 360.0F, 1.0F, 1.0F, 1.0F, 56.0F, 1.0F};
            initial_.cloudCover = 0.2F;
            initial_.cloudCumuliform = 0.4F;
            initial_.windSpeed = 2.0F;
            initial_.windDirectionDeg = 225.0F;
            initial_.gustFactor = 0.2F;
            initial_.fogDensity = 0.02F;
            initial_.temperatureC = 12.0F;
            initial_.humidity = 0.5F;
            initial_.rngState = Rng(0x123456U).GetState();
        }

        void TearDown() override
        {
            IdRegistry::ResetForTesting();
        }

        [[nodiscard]] WeatherSystem Make(float expiry = 0.0F) const
        {
            auto made = WeatherSystem::Create(archetypes_, transitions_, rates_, initial_, clear_, expiry);
            EXPECT_TRUE(made) << made.Error().ToString();
            return std::move(made.Value());
        }

        Id clear_;
        Id rain_;
        WeatherRates rates_;
        WeatherState initial_;
        std::vector<WeatherArchetype> archetypes_;
        std::vector<WeatherTransitionRow> transitions_;
    };

    TEST_F(WeatherSystemTests, InitialReachedStateConsumesNoRngAndWaitsForItsExpiry)
    {
        WeatherSystem system = Make(10.0F);
        ASSERT_TRUE(system.Advance(2.0F, SeasonAt(0.1), 11.0F, 0.5F));
        EXPECT_EQ(system.TargetArchetype(), clear_);
        EXPECT_FLOAT_EQ(system.TargetExpiryMinutes(), 8.0F);
        EXPECT_EQ(system.State().rngState, initial_.rngState);
        EXPECT_FLOAT_EQ(system.State().cloudCover, initial_.cloudCover);
        EXPECT_FLOAT_EQ(system.TransitionRemainingMinutes(), 0.0F);
    }

    TEST_F(WeatherSystemTests, ExpiryStartsOneSampledTransitionFromAnImmutableSnapshot)
    {
        WeatherSystem system = Make();
        const auto initialRng = system.State().rngState;
        Rng oracle(initialRng);
        for (int draw = 0; draw < 13; ++draw)
        {
            (void)oracle.NextFloat();
        }
        ASSERT_TRUE(system.Advance(1.0F, SeasonAt(0.1), 11.0F, 0.0F));
        EXPECT_EQ(system.TargetArchetype(), rain_);
        EXPECT_NE(system.State().rngState, initialRng);
        EXPECT_EQ(system.State().rngState, oracle.GetState());
        EXPECT_FLOAT_EQ(system.TargetExpiryMinutes(), 24.0F);
        EXPECT_FLOAT_EQ(system.TransitionRemainingMinutes(), 4.0F);
        const WeatherState snapshot = system.TransitionSnapshot();

        ASSERT_TRUE(system.Advance(1.0F, SeasonAt(0.1), 12.0F, 0.0F));
        EXPECT_FLOAT_EQ(system.TransitionRemainingMinutes(), 3.0F);
        EXPECT_FLOAT_EQ(system.TransitionSnapshot().cloudCover, snapshot.cloudCover);
        EXPECT_FLOAT_EQ(system.TransitionSnapshot().temperatureC, snapshot.temperatureC);
        EXPECT_EQ(system.TransitionSnapshot().rngState, snapshot.rngState);
        // smoothstep(2/5) = .352, evaluated from the original .2 snapshot: .2 + .8*.352.
        // Re-starting from the already advanced live state would produce .5355136 instead.
        EXPECT_FLOAT_EQ(system.State().cloudCover, 0.4816F);
        EXPECT_GT(system.State().surfaceWetness, 0.0F);
    }

    TEST_F(WeatherSystemTests, ConsoleControlsPauseAndTheSameRequestedTarget)
    {
        WeatherSystem system = Make(20.0F);
        system.SetAutomaticTransitions(false);
        system.TargetArchetypeControl() = rain_;
        system.TransitionsPausedControl() = true;
        const WeatherState frozen = system.State();

        ASSERT_TRUE(system.Advance(10.0F, SeasonAt(0.2), 15.0F, 1.0F));
        EXPECT_EQ(system.State(), frozen);
        EXPECT_EQ(system.TargetArchetype(), clear_);
        EXPECT_FLOAT_EQ(system.TargetExpiryMinutes(), 20.0F);

        system.TransitionsPausedControl() = false;
        ASSERT_TRUE(system.Advance(1.0F, SeasonAt(0.2), 15.0F, 1.0F));
        EXPECT_EQ(system.TargetArchetype(), rain_);
        EXPECT_FLOAT_EQ(system.TransitionRemainingMinutes(), 4.0F);
        EXPECT_FALSE(system.AutomaticTransitions());
    }

    TEST_F(WeatherSystemTests, ALongFrameCannotSkipTheExpiryBoundary)
    {
        WeatherSystem system = Make(0.5F);
        ASSERT_TRUE(system.Advance(2.0F, SeasonAt(0.3), 18.0F, 0.0F));
        EXPECT_EQ(system.TargetArchetype(), rain_);
        EXPECT_FLOAT_EQ(system.TargetExpiryMinutes(), 23.5F);
        EXPECT_FLOAT_EQ(system.TransitionRemainingMinutes(), 3.5F);
    }

    TEST_F(WeatherSystemTests, InvalidCreationAndAdvanceInputsAreRejectedWithoutMutation)
    {
        EXPECT_FALSE(
            WeatherSystem::Create(archetypes_, transitions_, rates_, initial_, Intern("W_UNKNOWN"), 10.0F));
        WeatherSystem system = Make(10.0F);
        const WeatherState before = system.State();
        EXPECT_FALSE(system.Advance(-1.0F, SeasonAt(0.0), 10.0F, 0.0F));
        EXPECT_FALSE(system.Advance(1.0F, SeasonAt(0.0), 39.0F, 0.0F));
        EXPECT_FALSE(system.Advance(1.0F, SeasonAt(0.0), 10.0F, 1.1F));
        EXPECT_EQ(system.State(), before);
        EXPECT_FLOAT_EQ(system.TargetExpiryMinutes(), 10.0F);
    }

} // namespace
