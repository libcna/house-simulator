// SPDX-License-Identifier: MIT
//
// `HOUSE-01704`: a ten-year run through the deployed transition table. It needs the authored
// world but no GraphicsDevice, so this is an integration test without a graphical surface.
#include <algorithm>
#include <gtest/gtest.h>

#include <string>

#include "cnahouse/environment/SimClock.hpp"
#include "cnahouse/util/Ids.hpp"
#include "cnahouse/util/Rng.hpp"
#include "cnahouse/weather/WeatherSampler.hpp"
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
    using cnahouse::world::WorldLoader;

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

} // namespace
