// SPDX-License-Identifier: MIT
//
// `HOUSE-01704` / `HOUSE-01697` / `HOUSE-01701`: long runs through the deployed transition table. They need
// the authored world but no GraphicsDevice, so these are integration tests without a graphical surface.
#include <algorithm>
#include <gtest/gtest.h>

#include <string>
#include <vector>

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
    using cnahouse::weather::PrecipTypeAtTemperature;
    using cnahouse::weather::WeatherSampler;
    using cnahouse::weather::WeatherSystem;
    using cnahouse::world::WorldLoader;

    [[nodiscard]] float DirectSunlight(const SimClock& clock, float cloudCover)
    {
        const auto sun = cnahouse::environment::SunPositionFor(clock);
        const auto shading = cnahouse::environment::SunShadingFor(sun, static_cast<double>(cloudCover));
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

    TEST(WeatherSeasonTests, StabilityCycleUsesSixtyTimesClockAndClearOvercastRain)
    {
        cnahouse::world::WorldData::Contents contents;
        const std::string directory = std::string(CNAHOUSE_TEST_CONTENT_ROOT) + "/world";
        const auto weather = WorldLoader::LoadWeather(directory, contents);
        ASSERT_TRUE(weather) << weather.Error().ToString();
        const auto initial = WorldLoader::LoadInitialState(directory, contents);
        ASSERT_TRUE(initial) << initial.Error().ToString();

        const auto& start = contents.initialState.weather;
        auto made = WeatherSystem::Create(contents.weatherArchetypes,
                                          contents.weatherTransitions,
                                          contents.weatherRates,
                                          start.state,
                                          start.target,
                                          start.targetExpiryMinutes);
        ASSERT_TRUE(made) << made.Error().ToString();
        WeatherSystem system = std::move(made.Value());
        system.SetAutomaticTransitions(false);

        SimClock clock;
        clock.SetCalendar(cnahouse::environment::kNewGameCalendarDays);
        ASSERT_DOUBLE_EQ(clock.timeScale, 60.0);
        std::vector<std::string> reached;
        for (const std::string_view name : {"W_CLEAR", "W_OVERCAST", "W_RAIN"})
        {
            const Id requested = Intern(name);
            ASSERT_NE(std::ranges::find(
                          contents.weatherArchetypes, requested, &cnahouse::weather::WeatherArchetype::id),
                      contents.weatherArchetypes.end())
                << name;
            system.TargetArchetypeControl() = requested;

            bool settled = false;
            for (int realSecond = 0; realSecond < 180; ++realSecond)
            {
                const double before = clock.epochSeconds;
                clock.Advance(1.0);
                ASSERT_DOUBLE_EQ(clock.epochSeconds - before, 60.0);
                const float sunlight = DirectSunlight(clock, system.State().cloudCover);
                ASSERT_TRUE(system.Advance(
                    1.0F, clock.Season(), static_cast<float>(clock.OutdoorBaseTemperatureC()), sunlight));
                if (system.TargetArchetype() == requested && system.TransitionRemainingMinutes() == 0.0F)
                {
                    settled = true;
                    break;
                }
            }
            ASSERT_TRUE(settled) << name << " did not settle within three simulated hours";
            reached.emplace_back(name);
        }

        EXPECT_EQ(reached, (std::vector<std::string>{"W_CLEAR", "W_OVERCAST", "W_RAIN"}));
        EXPECT_EQ(system.State().precipType, PrecipType::Rain);
        EXPECT_GT(system.State().precipIntensity, 0.0F);
        std::printf("  stability clock: 60x; weather: clear -> overcast -> rain\n");
    }

    TEST(WeatherSeasonTests, ThirtyDaysAreVariedAndNeverContradictTheWeatherRules)
    {
        cnahouse::world::WorldData::Contents contents;
        const std::string directory = std::string(CNAHOUSE_TEST_CONTENT_ROOT) + "/world";
        const auto weather = WorldLoader::LoadWeather(directory, contents);
        ASSERT_TRUE(weather) << weather.Error().ToString();
        const auto initial = WorldLoader::LoadInitialState(directory, contents);
        ASSERT_TRUE(initial) << initial.Error().ToString();

        const auto& start = contents.initialState.weather;
        auto systemResult = WeatherSystem::Create(contents.weatherArchetypes,
                                                  contents.weatherTransitions,
                                                  contents.weatherRates,
                                                  start.state,
                                                  start.target,
                                                  start.targetExpiryMinutes);
        ASSERT_TRUE(systemResult) << systemResult.Error().ToString();
        WeatherSystem system = std::move(systemResult.Value());

        SimClock clock;
        clock.SetCalendar(cnahouse::environment::kNewGameCalendarDays);
        std::vector<Id> visited{system.TargetArchetype()};
        constexpr std::size_t kThirtyDaysInMinutes = 30U * 24U * 60U;
        for (std::size_t minute = 1U; minute <= kThirtyDaysInMinutes; ++minute)
        {
            SCOPED_TRACE(::testing::Message() << "simulated minute " << minute);
            clock.Advance(1.0);
            const float sunlight = DirectSunlight(clock, system.State().cloudCover);
            ASSERT_TRUE(system.Advance(
                1.0F, clock.Season(), static_cast<float>(clock.OutdoorBaseTemperatureC()), sunlight));
            const auto& state = system.State();
            const auto valid = state.Validate();
            ASSERT_TRUE(valid) << valid.Error().ToString();
            if (state.precipType == PrecipType::None)
            {
                ASSERT_LE(state.precipIntensity, cnahouse::weather::kPrecipTypeChangeIntensity);
            }
            if (state.precipType == PrecipType::Hail)
            {
                ASSERT_GT(state.thunderIntensity, 0.3F);
            }
            if (state.precipIntensity <= cnahouse::weather::kPrecipTypeChangeIntensity &&
                (state.precipType == PrecipType::Rain || state.precipType == PrecipType::Snow ||
                 state.precipType == PrecipType::Sleet))
            {
                const auto phase = PrecipTypeAtTemperature(state.precipType, state.temperatureC);
                ASSERT_TRUE(phase) << phase.Error().ToString();
                ASSERT_EQ(*phase, state.precipType);
            }

            if (std::ranges::find(visited, system.TargetArchetype()) == visited.end())
            {
                visited.push_back(system.TargetArchetype());
            }
            const auto target = std::ranges::find(contents.weatherArchetypes,
                                                  system.TargetArchetype(),
                                                  &cnahouse::weather::WeatherArchetype::id);
            ASSERT_NE(target, contents.weatherArchetypes.end());
            ASSERT_FALSE(target->modifier) << "the wind modifier became a standalone weather state";
        }

        EXPECT_GE(visited.size(), 8U);
    }

    TEST(WeatherSeasonTests, CalmAuthoredTargetsContainNeitherHiddenRainNorThunder)
    {
        cnahouse::world::WorldData::Contents contents;
        const auto loaded =
            WorldLoader::LoadWeather(std::string(CNAHOUSE_TEST_CONTENT_ROOT) + "/world", contents);
        ASSERT_TRUE(loaded) << loaded.Error().ToString();

        for (const auto& archetype : contents.weatherArchetypes)
        {
            if (archetype.modifier)
            {
                continue;
            }
            SCOPED_TRACE(cnahouse::util::IdRegistry::NameOf(archetype.id));
            if (archetype.precipType == PrecipType::None)
            {
                EXPECT_FLOAT_EQ(archetype.precipIntensity.minimum, 0.0F);
                EXPECT_FLOAT_EQ(archetype.precipIntensity.maximum, 0.0F);
            }
            if (archetype.id != Intern("W_RAIN") && archetype.id != Intern("W_HEAVY_RAIN") &&
                archetype.id != Intern("W_THUNDERSTORM") && archetype.id != Intern("W_HAIL"))
            {
                EXPECT_FLOAT_EQ(archetype.thunderProbability.minimum, 0.0F);
                EXPECT_FLOAT_EQ(archetype.thunderProbability.maximum, 0.0F);
            }
        }
    }

    TEST(WeatherSeasonTests, TheCanonicalSevenDayReviewHasReadablePacing)
    {
        cnahouse::world::WorldData::Contents contents;
        const std::string directory = std::string(CNAHOUSE_TEST_CONTENT_ROOT) + "/world";
        const auto weather = WorldLoader::LoadWeather(directory, contents);
        ASSERT_TRUE(weather) << weather.Error().ToString();
        const auto initial = WorldLoader::LoadInitialState(directory, contents);
        ASSERT_TRUE(initial) << initial.Error().ToString();

        const auto& start = contents.initialState.weather;
        auto systemResult = WeatherSystem::Create(contents.weatherArchetypes,
                                                  contents.weatherTransitions,
                                                  contents.weatherRates,
                                                  start.state,
                                                  start.target,
                                                  start.targetExpiryMinutes);
        ASSERT_TRUE(systemResult) << systemResult.Error().ToString();
        WeatherSystem system = std::move(systemResult.Value());

        SimClock clock;
        clock.SetCalendar(cnahouse::environment::kNewGameCalendarDays);
        Id previousTarget = system.TargetArchetype();
        std::vector<Id> visited{previousTarget};
        std::size_t episodes = 1U;
        std::size_t precipitationTargetMinutes = 0U;
        std::size_t severeTargetMinutes = 0U;
        std::size_t fogTargetMinutes = 0U;
        constexpr std::size_t kSevenDaysInMinutes = 7U * 24U * 60U;
        for (std::size_t minute = 1U; minute <= kSevenDaysInMinutes; ++minute)
        {
            SCOPED_TRACE(::testing::Message() << "simulated minute " << minute);
            clock.Advance(1.0);
            const float sunlight = DirectSunlight(clock, system.State().cloudCover);
            ASSERT_TRUE(system.Advance(
                1.0F, clock.Season(), static_cast<float>(clock.OutdoorBaseTemperatureC()), sunlight));

            const Id targetId = system.TargetArchetype();
            const auto target = std::ranges::find(
                contents.weatherArchetypes, targetId, &cnahouse::weather::WeatherArchetype::id);
            ASSERT_NE(target, contents.weatherArchetypes.end());
            if (targetId != previousTarget)
            {
                ++episodes;
                previousTarget = targetId;
            }
            if (std::ranges::find(visited, targetId) == visited.end())
            {
                visited.push_back(targetId);
            }
            if (target->precipType != PrecipType::None)
            {
                ++precipitationTargetMinutes;
            }
            if (targetId == Intern("W_HEAVY_RAIN") || targetId == Intern("W_THUNDERSTORM") ||
                targetId == Intern("W_HAIL") || targetId == Intern("W_HEAVY_SNOW"))
            {
                ++severeTargetMinutes;
            }
            if (targetId == Intern("W_FOG"))
            {
                ++fogTargetMinutes;
            }
        }

        EXPECT_GE(visited.size(), 10U);
        EXPECT_GE(episodes, 55U);
        EXPECT_LE(episodes, 90U);
        EXPECT_GE(precipitationTargetMinutes, 1'200U);
        EXPECT_LE(precipitationTargetMinutes, 2'500U);
        EXPECT_GE(severeTargetMinutes, 100U);
        EXPECT_LE(severeTargetMinutes, 600U);
        EXPECT_GE(fogTargetMinutes, 100U);
        EXPECT_LE(fogTargetMinutes, 800U);
    }

} // namespace
