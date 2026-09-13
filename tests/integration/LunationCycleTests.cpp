// SPDX-License-Identifier: MIT
//
// `HOUSE-01618`: the §70.3 month-long lunar acceptance test through the public debug command.
#include <cmath>
#include <cstdio>

#include <gtest/gtest.h>

#include "cnahouse/debug/Console.hpp"
#include "cnahouse/debug/TimeCommands.hpp"
#include "cnahouse/environment/MoonModel.hpp"
#include "cnahouse/environment/SimClock.hpp"

namespace
{
    using cnahouse::debug::Console;
    using cnahouse::debug::RegisterTimeCommands;
    using cnahouse::debug::TimeCommandContext;
    using cnahouse::environment::MoonPhaseFor;
    using cnahouse::environment::SimClock;

    TEST(LunationCycleTests, ThirtyDayAdvanceCompletesOneLunation)
    {
        SimClock clock;
        clock.SetCalendar(cnahouse::environment::kNewGameCalendarDays);
        Console console;
        RegisterTimeCommands(console, TimeCommandContext{&clock});

        double previousPhase = MoonPhaseFor(clock).phase;
        double completedTurns = 0.0;
        for (int day = 1; day <= 30; ++day)
        {
            SCOPED_TRACE(::testing::Message() << "simulated day " << day);
            const auto advanced = console.Execute("time advance 1");
            ASSERT_TRUE(advanced.ok) << advanced.message;

            const double phase = MoonPhaseFor(clock).phase;
            double dailyTurns = phase - previousPhase;
            if (dailyTurns < 0.0)
            {
                dailyTurns += 1.0;
            }
            ASSERT_GT(dailyTurns, 0.0) << "the continuous phase ran backwards";
            ASSERT_LT(dailyTurns, 0.1) << "a daily sample skipped enough phase to hide a wrap";
            completedTurns += dailyTurns;
            previousPhase = phase;
        }

        EXPECT_NEAR(clock.CalendarDays() - cnahouse::environment::kNewGameCalendarDays, 30.0, 1e-9);
        EXPECT_NEAR(completedTurns, 1.0, 0.03)
            << "thirty simulated days did not complete §33.5's one lunation";
        std::printf("  30 one-day commands advanced the continuous phase by %.9f lunations\n",
                    completedTurns);
    }

} // namespace
