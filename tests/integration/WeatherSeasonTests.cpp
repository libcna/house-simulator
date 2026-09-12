// SPDX-License-Identifier: MIT
//
// `HOUSE-01704` / `HOUSE-01697`: long runs through the deployed transition table. They need the
// authored world but no GraphicsDevice, so these are integration tests without a graphical surface.
#include <algorithm>
#include <gtest/gtest.h>

#include <string>

#include "cnahouse/environment/SimClock.hpp"
#include "cnahouse/environment/SunLight.hpp"
#include "cnahouse/environment/SunModel.hpp"
#include "cnahouse/util/Ids.hpp"
#include "cnahouse/util/Rng.hpp"
#include "cnahouse/weather/WeatherSampler.hpp"
#include "cnahouse/weather/WeatherSystem.hpp"
#include "cnahouse/world/WorldLoader.hpp"

namespace
{
    using cnahouse::environment::CivilTime;
    using cnahouse::environment::SimClock;
    using cnahouse::util::Id;
    using cnahouse::util::Intern;
    using cnahouse::util::Rng;
    using cnahouse::weather::PrecipType;
    using cnahouse::weather::WeatherSampler;
    using cnahouse::weather::WeatherSystem;
    using cnahouse::world::WorldLoader;

    [[nodiscard]] float DirectSunlight(const SimClock& clock, float cloudCover)
    {
        const auto sun = cnahouse::environment::SunPositionFor(clock);
        const auto shading = cnahouse::environment::SunShadingFor(sun, cloudCover);
        return sun.altitudeDeg > cnahouse::environment::kRefractedHorizonDeg ? shading.directIntensity : 0.0F;
    }

    TEST(WeatherSeasonTests, NoSummerSnowOverTenYearsAtTheDefaultLocation)
    {
        cnahouse::world::WorldData::Contents contents;
        const auto loaded =
            WorldLoader::LoadWeather(std::string(CNAHOUSE_TEST_CONTENT_ROOT) + "/world", contents);
        ASSERT_TRUE(loaded) << loaded.Error().ToString();
        const WeatherSampler sampler(contents.weatherArchetypes, contents.weatherTransitions);
        Rng rng(0x1704U);
        Id current = Intern("W_PARTLY");
        std::size_t summerDraws = 0U;
        std::size_t frozenWaterDraws = 0U;

        SimClock clock;
        clock.calendarDaysPerSimDay = 1.0;
        for (int year = 2031; year < 2041; ++year)
        {
            for (int day = 152; day <= 243; ++day)
            {
                for (int hour : {0, 6, 12, 18})
                {
                    CivilTime instant;
                    instant.year = year;
                    instant.month = 1;
                    instant.day = 1;
                    instant.hour = hour;
                    clock.SetStandard(instant);
                    clock.epochSeconds += static_cast<double>(day - 1) * 86400.0;
                    const float temperature = static_cast<float>(clock.OutdoorTemperatureC());
                    ASSERT_GT(temperature, 0.0F);

                    const auto next = sampler.SampleNext(current, clock.Season(), temperature, rng);
                    ASSERT_TRUE(next) << next.Error().ToString();
                    current = *next;
                    ++summerDraws;
                    const auto archetype = std::ranges::find(
                        contents.weatherArchetypes, current, &cnahouse::weather::WeatherArchetype::id);
                    ASSERT_NE(archetype, contents.weatherArchetypes.end());
                    frozenWaterDraws += archetype->precipType == PrecipType::Snow ? 1U : 0U;
                }
            }
        }

        EXPECT_EQ(summerDraws, 3'680U);
        EXPECT_EQ(frozenWaterDraws, 0U)
            << "a snow archetype was selected while measured outdoor temperature was above freezing";
    }

    TEST(WeatherSeasonTests, TheSameSeedProducesTheSameTenThousandMinuteHistory)
    {
        cnahouse::world::WorldData::Contents contents;
        const std::string directory = std::string(CNAHOUSE_TEST_CONTENT_ROOT) + "/world";
        const auto weather = WorldLoader::LoadWeather(directory, contents);
        ASSERT_TRUE(weather) << weather.Error().ToString();
        const auto initial = WorldLoader::LoadInitialState(directory, contents);
        ASSERT_TRUE(initial) << initial.Error().ToString();

        const auto& start = contents.initialState.weather;
        auto firstResult = WeatherSystem::Create(contents.weatherArchetypes,
                                                 contents.weatherTransitions,
                                                 contents.weatherRates,
                                                 start.state,
                                                 start.target,
                                                 start.targetExpiryMinutes);
        auto secondResult = WeatherSystem::Create(contents.weatherArchetypes,
                                                  contents.weatherTransitions,
                                                  contents.weatherRates,
                                                  start.state,
                                                  start.target,
                                                  start.targetExpiryMinutes);
        ASSERT_TRUE(firstResult) << firstResult.Error().ToString();
        ASSERT_TRUE(secondResult) << secondResult.Error().ToString();
        WeatherSystem first = std::move(firstResult.Value());
        WeatherSystem second = std::move(secondResult.Value());

        SimClock clock;
        clock.SetCalendar(cnahouse::environment::kNewGameCalendarDays);
        const auto initialRng = start.state.rngState;
        Id previousTarget = first.TargetArchetype();
        std::size_t targetChanges = 0U;

        for (std::size_t minute = 1U; minute <= 10'000U; ++minute)
        {
            SCOPED_TRACE(::testing::Message() << "simulated minute " << minute);
            // One real second is one simulated minute at the default 60x rate. Advance the clock
            // first, exactly as CnaHouseGame does, then feed both histories independently derived
            // sunlight from their own live cloud cover.
            clock.Advance(1.0);
            const float baseTemperature = static_cast<float>(clock.OutdoorBaseTemperatureC());
            const float firstSunlight = DirectSunlight(clock, first.State().cloudCover);
            const float secondSunlight = DirectSunlight(clock, second.State().cloudCover);
            ASSERT_FLOAT_EQ(firstSunlight, secondSunlight);
            ASSERT_TRUE(first.Advance(1.0F, clock.Season(), baseTemperature, firstSunlight));
            ASSERT_TRUE(second.Advance(1.0F, clock.Season(), baseTemperature, secondSunlight));

            ASSERT_EQ(first.State(), second.State());
            ASSERT_EQ(first.TargetArchetype(), second.TargetArchetype());
            ASSERT_FLOAT_EQ(first.TargetExpiryMinutes(), second.TargetExpiryMinutes());
            ASSERT_FLOAT_EQ(first.TransitionRemainingMinutes(), second.TransitionRemainingMinutes());
            ASSERT_EQ(first.TransitionSnapshot(), second.TransitionSnapshot());

            if (first.TargetArchetype() != previousTarget)
            {
                ++targetChanges;
                previousTarget = first.TargetArchetype();
            }
        }

        EXPECT_GT(targetChanges, 0U) << "the claimed history never left its authored initial state";
        EXPECT_NE(first.State().rngState, initialRng)
            << "the claimed history never consumed its persisted random stream";
    }

} // namespace
