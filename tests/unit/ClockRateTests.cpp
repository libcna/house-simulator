// SPDX-License-Identifier: MIT
//
// `HOUSE-01539`. §35.2's mnemonic, delivered the way a running game delivers it.
//
// *"1 real second = 1 simulated minute... every test that says 'wait 3 seconds and check the clock
// advanced 3 minutes' becomes trivially readable."* `SimClockTests` and `DayLengthTests` both check
// that in ONE call to `Advance`. A game does not: it delivers three seconds as 180 frames of
// `1/60 s`, each of them a `float` that is not exactly 1/60, and the question this file asks is
// whether the clock still lands on 180 simulated seconds after all of them.
//
// That is not a restatement -- it is the failure mode single-call tests cannot see. A clock that
// accumulated in `float` would be out by a measurable margin here and by minutes over a session.
#include <cmath>
#include <cstdint>

#include <gtest/gtest.h>

#include "cnahouse/app/FrameTimer.hpp"
#include "cnahouse/app/Settings.hpp"
#include "cnahouse/environment/DayLength.hpp"
#include "cnahouse/environment/SimClock.hpp"

namespace
{
    using cnahouse::app::FrameContext;
    using cnahouse::app::FrameTimer;
    using cnahouse::app::Settings;
    using cnahouse::environment::kDefaultTimeScale;
    using cnahouse::environment::SimClock;
    using cnahouse::environment::TimeScaleForDayLength;

    /// A clock at §35.2's chosen rate, taken from the SETTING rather than from the constant, so
    /// this file fails if the default setting and the default scale ever stop agreeing.
    SimClock DefaultRateClock()
    {
        SimClock clock;
        clock.timeScale =
            TimeScaleForDayLength(static_cast<double>(Settings::Defaults().dayLengthRealMinutes));
        return clock;
    }

} // namespace

TEST(ClockRateTests, TheDefaultSettingAndTheDefaultScaleAgree)
{
    EXPECT_DOUBLE_EQ(DefaultRateClock().timeScale, kDefaultTimeScale);
    EXPECT_DOUBLE_EQ(DefaultRateClock().timeScale, 60.0) << "§35.2's chosen 60x";
}

TEST(ClockRateTests, ThreeRealSecondsAreThreeSimulatedMinutesInOneCall)
{
    SimClock clock = DefaultRateClock();
    clock.Advance(3.0);
    EXPECT_DOUBLE_EQ(clock.epochSeconds, 180.0);
    EXPECT_EQ(clock.Standard().minute, 3);
    EXPECT_EQ(clock.Standard().second, 0);
    EXPECT_EQ(clock.Standard().hour, 0) << "three minutes is not an hour";
}

TEST(ClockRateTests, AndInAHundredAndEightyFramesOfSixtyHertz)
{
    // The same three seconds through `FrameTimer`, which is how they actually arrive. Each frame's
    // delta is a `float` 1/60 -- 0.0166666considerably-more-digits -- so 180 of them do not sum to
    // exactly 3.0, and the question is how far off the clock ends up.
    FrameTimer timer;
    SimClock clock = DefaultRateClock();
    double realElapsed = 0.0;
    for (int frame = 0; frame < 180; ++frame)
    {
        const FrameContext context = timer.Advance(1.0F / 60.0F);
        clock.Advance(static_cast<double>(context.realDeltaSeconds));
        realElapsed += static_cast<double>(context.realDeltaSeconds);
    }
    // **Nine microseconds of simulated time OVER, not under.** The nearest `float` to 1/60 is
    // 0.016666667535901070, which is 8.7e-10 s too LARGE, so 180 of them come to 3.0000000156 real
    // seconds and the clock lands 9.4 µs past three simulated minutes. The direction is worth
    // saying: a clock that ran fast would drift the sun forward over a long session, and the
    // number below is what says it does not -- 9 µs in three seconds is 0.27 s in a day of play.
    EXPECT_NEAR(clock.epochSeconds, 180.0, 1e-4)
        << "180 frames of 60 Hz did not come to three simulated minutes";
    EXPECT_GT(clock.epochSeconds, 180.0) << "the residue changed sign, so this comment is stale";
    EXPECT_NEAR(clock.epochSeconds, realElapsed * kDefaultTimeScale, 1e-9)
        << "the clock is not exactly the elapsed time times the scale, so it is losing precision "
           "somewhere of its own";
    EXPECT_EQ(clock.Standard().minute, 3) << "three minutes of play does not read as three minutes";
    EXPECT_EQ(clock.Standard().second, 0);
}

TEST(ClockRateTests, ASimulatedDayIsTwentyFourRealMinutesOfFrames)
{
    // §35.2's other row of the same decision, over 86 400 frames -- a whole simulated day at
    // 60 Hz. This is where an accumulation error becomes visible if there is one, and the SHAPE of
    // the error is the claim: 480 times as many frames as the three-second case, and 480 times the
    // offset -- 4.5 ms of simulated time -- which is linear. An error that compounded would be
    // 480² times worse and put the sunrise four minutes out.
    FrameTimer timer;
    SimClock clock = DefaultRateClock();
    for (int frame = 0; frame < 24 * 60 * 60; ++frame)
    {
        const FrameContext context = timer.Advance(1.0F / 60.0F);
        clock.Advance(static_cast<double>(context.realDeltaSeconds));
    }
    EXPECT_NEAR(clock.epochSeconds, 86400.0, 0.01)
        << "24 real minutes of frames did not come to a simulated day";
    EXPECT_EQ(clock.SimDayIndex(), 1) << "the day did not turn over, or turned over twice";
    // The linear shape, stated as a ratio rather than as a second tolerance: 480 times the frames
    // for within a whisker of 480 times the offset.
    const double offset = clock.epochSeconds - 86400.0;
    EXPECT_GT(offset, 0.0);
    EXPECT_NEAR(offset / 9.4e-6, 480.0, 20.0)
        << "the accumulated error is " << offset << " s, which is not 480 times the three-second "
        << "case -- so it is not the float delta's residue adding up";
}

TEST(ClockRateTests, TheOtherPresetsScaleTheSameThreeSecondsProportionally)
{
    // The mnemonic is a property of 60x rather than of the clock, and this is what says so: the
    // same three real seconds at each preset advance the clock by three seconds times that
    // preset's scale, exactly.
    for (const double minutes : {20.0, 24.0, 48.0, 96.0, 1440.0})
    {
        SimClock clock;
        clock.timeScale = TimeScaleForDayLength(minutes);
        clock.Advance(3.0);
        EXPECT_DOUBLE_EQ(clock.epochSeconds, 3.0 * (1440.0 / minutes)) << minutes;
    }
    // ...and at the frozen preset three seconds are no seconds at all, which is the row that makes
    // a pixel-regression test possible.
    SimClock frozen;
    frozen.timeScale = TimeScaleForDayLength(0.0);
    frozen.Advance(3.0);
    EXPECT_DOUBLE_EQ(frozen.epochSeconds, 0.0);
}
