// SPDX-License-Identifier: MIT
//
// `HOUSE-01533`. §35.2's day-length presets and the free numeric entry, and the setting they live
// in.
//
// §35.2 is a table of six rows and a decision: *"24 real minutes per simulated day
// (`timeScale = 60.0`) is the default, configurable from the settings menu with the presets above
// and a free numeric entry"*, and the reason 24 wins is *"not aesthetic -- it is that `1 s = 1 min`
// removes an entire class of arithmetic mistakes from every test, log and settings dialogue"*.
// That sentence is the first claim here, in the form a player would check it.
#include <cmath>
#include <cstddef>
#include <limits>
#include <string>

#include <gtest/gtest.h>

#include "cnahouse/app/Settings.hpp"
#include "cnahouse/environment/DayLength.hpp"
#include "cnahouse/environment/SimClock.hpp"

namespace
{
    using cnahouse::app::Settings;
    using cnahouse::environment::DayLengthForTimeScale;
    using cnahouse::environment::DayLengthPresetIndex;
    using cnahouse::environment::kDayLengthPresetCount;
    using cnahouse::environment::kDayLengthPresetsRealMinutes;
    using cnahouse::environment::kDefaultDayLengthRealMinutes;
    using cnahouse::environment::kDefaultTimeScale;
    using cnahouse::environment::kMaxDayLengthRealMinutes;
    using cnahouse::environment::kMinDayLengthRealMinutes;
    using cnahouse::environment::SimClock;
    using cnahouse::environment::TimeScaleForDayLength;

} // namespace

TEST(DayLengthTests, TheSixPresetsAreSection35Point2sOwnTable)
{
    ASSERT_EQ(kDayLengthPresetCount, 6U);
    EXPECT_DOUBLE_EQ(kDayLengthPresetsRealMinutes[0], 20.0) << "the brief's original, kept";
    EXPECT_DOUBLE_EQ(kDayLengthPresetsRealMinutes[1], 24.0) << "the chosen default";
    EXPECT_DOUBLE_EQ(kDayLengthPresetsRealMinutes[2], 48.0) << "slow, for screenshots";
    EXPECT_DOUBLE_EQ(kDayLengthPresetsRealMinutes[3], 96.0) << "very slow";
    EXPECT_DOUBLE_EQ(kDayLengthPresetsRealMinutes[4], 1440.0) << "real time";
    EXPECT_DOUBLE_EQ(kDayLengthPresetsRealMinutes[5], 0.0) << "frozen, which the table writes as inf";

    // §35.2's `timeScale` column, which is the whole point of the table: each row's scale, checked
    // against the row rather than against the formula that produced it.
    EXPECT_DOUBLE_EQ(TimeScaleForDayLength(20.0), 72.0);
    EXPECT_DOUBLE_EQ(TimeScaleForDayLength(24.0), 60.0);
    EXPECT_DOUBLE_EQ(TimeScaleForDayLength(48.0), 30.0);
    EXPECT_DOUBLE_EQ(TimeScaleForDayLength(96.0), 15.0);
    EXPECT_DOUBLE_EQ(TimeScaleForDayLength(1440.0), 1.0);
    EXPECT_DOUBLE_EQ(TimeScaleForDayLength(0.0), 0.0) << "frozen is a scale of zero, not infinity";
}

TEST(DayLengthTests, TheDefaultIsTheOneRealSecondIsOneSimulatedMinuteSetting)
{
    EXPECT_DOUBLE_EQ(kDefaultDayLengthRealMinutes, 24.0);
    EXPECT_DOUBLE_EQ(TimeScaleForDayLength(kDefaultDayLengthRealMinutes), kDefaultTimeScale);

    // §35.2's reason, in the form a player would check it: set the clock from the default setting
    // and three real seconds have to be three simulated minutes exactly.
    SimClock clock;
    clock.timeScale = TimeScaleForDayLength(kDefaultDayLengthRealMinutes);
    clock.Advance(3.0);
    EXPECT_DOUBLE_EQ(clock.epochSeconds, 180.0) << "3 real seconds are not 3 simulated minutes";
    // ...and a real minute is a simulated hour, which is the other half of the mnemonic.
    SimClock hour;
    hour.timeScale = TimeScaleForDayLength(kDefaultDayLengthRealMinutes);
    hour.Advance(60.0);
    EXPECT_DOUBLE_EQ(hour.epochSeconds, 3600.0);
}

TEST(DayLengthTests, EveryPresetTakesTheClockRoundOnceInItsOwnNumberOfRealMinutes)
{
    // The claim the table is actually making, measured by playing rather than by dividing: at each
    // preset, that many real minutes must advance the clock by exactly one simulated day.
    for (std::size_t index = 0; index + 1 < kDayLengthPresetCount; ++index)
    {
        const double minutes = kDayLengthPresetsRealMinutes[index];
        SimClock clock;
        clock.timeScale = TimeScaleForDayLength(minutes);
        clock.Advance(minutes * 60.0);
        EXPECT_DOUBLE_EQ(clock.epochSeconds, 86400.0) << minutes << " real minutes is not a day";
        EXPECT_EQ(clock.SimDayIndex(), 1) << minutes;
    }
    // The frozen preset is the row that is not a rate at all: an hour of real time moves nothing,
    // which is what §35.2 calls "essential for pixel-regression tests".
    SimClock frozen;
    frozen.timeScale = TimeScaleForDayLength(0.0);
    frozen.Advance(3600.0);
    EXPECT_DOUBLE_EQ(frozen.epochSeconds, 0.0);
    EXPECT_EQ(frozen.Standard().hour, 0);
}

TEST(DayLengthTests, TheFreeEntryIsANumberAndThePresetsAreNamedPointsOnIt)
{
    // §35.2 asks for "the presets above and a free numeric entry". A preset is therefore an INDEX
    // INTO the number and not a second field: a value that is one of the six is that preset, and
    // anything else is the free entry, so a file can never say "slow" and 24 at the same time.
    for (std::size_t index = 0; index < kDayLengthPresetCount; ++index)
    {
        EXPECT_EQ(DayLengthPresetIndex(kDayLengthPresetsRealMinutes[index]), index);
    }
    for (const double custom : {23.0, 24.5, 30.0, 100.0, 1439.0})
    {
        EXPECT_EQ(DayLengthPresetIndex(custom), kDayLengthPresetCount) << custom;
    }

    // The two conversions are inverses wherever the scale is a rate, so a settings dialogue that
    // has one can show the other without keeping a third copy of the table.
    for (std::size_t index = 0; index + 1 < kDayLengthPresetCount; ++index)
    {
        const double minutes = kDayLengthPresetsRealMinutes[index];
        EXPECT_DOUBLE_EQ(DayLengthForTimeScale(TimeScaleForDayLength(minutes)), minutes);
    }
    EXPECT_DOUBLE_EQ(DayLengthForTimeScale(0.0), 0.0) << "frozen goes both ways as zero";
}

TEST(DayLengthTests, ASettingsFileCannotHandTheClockSomethingItCannotDivideBy)
{
    // A settings file is user-editable text, and this number ends up as a DIVISOR. Negative, zero
    // where zero is not meant, and non-finite all have to come out as a clock that still runs.
    for (const double hostile : {-1.0, -0.0, std::nan(""), std::numeric_limits<double>::infinity()})
    {
        const double scale = TimeScaleForDayLength(hostile);
        EXPECT_TRUE(std::isfinite(scale)) << hostile;
        EXPECT_GE(scale, 0.0) << hostile;
        SimClock clock;
        clock.timeScale = scale;
        clock.Advance(1.0);
        EXPECT_TRUE(std::isfinite(clock.epochSeconds)) << hostile;
    }
}

TEST(DayLengthTests, TheSettingRoundTripsThroughTheFileAndIsClampedIntoItsBand)
{
    Settings settings = Settings::Defaults();
    EXPECT_FLOAT_EQ(settings.dayLengthRealMinutes, 24.0F);
    EXPECT_GE(Settings::kCurrentVersion, 5) << "the day length entered the schema in version 5";

    settings.dayLengthRealMinutes = 48.0F;
    const auto reread = Settings::FromJson(settings.ToJson(), "round-trip");
    ASSERT_TRUE(reread) << reread.Error().ToString();
    EXPECT_FLOAT_EQ(reread->dayLengthRealMinutes, 48.0F) << "the setting did not survive the file";

    // Zero is LEFT ALONE, because zero is the frozen preset rather than a value out of range --
    // and it is the one a pixel-regression test writes into the file on purpose.
    Settings frozen = Settings::Defaults();
    frozen.dayLengthRealMinutes = 0.0F;
    EXPECT_TRUE(frozen.ClampToSupportedRanges().empty()) << "frozen was corrected away";
    EXPECT_FLOAT_EQ(frozen.dayLengthRealMinutes, 0.0F);

    // Everything else outside §35.2's band is corrected AND reported, because the file is what the
    // player edits and being silently overruled is worse than being told.
    for (const float hostile : {-5.0F, 0.001F, 5000.0F})
    {
        Settings bad = Settings::Defaults();
        bad.dayLengthRealMinutes = hostile;
        const std::string changed = bad.ClampToSupportedRanges();
        EXPECT_NE(changed.find("dayLengthRealMinutes"), std::string::npos) << hostile;
        EXPECT_GE(bad.dayLengthRealMinutes, static_cast<float>(kMinDayLengthRealMinutes)) << hostile;
        EXPECT_LE(bad.dayLengthRealMinutes, static_cast<float>(kMaxDayLengthRealMinutes)) << hostile;
    }
}

TEST(DayLengthTests, AFileFromBeforeVersionFiveGetsTheDefaultRatherThanAZero)
{
    // The migration's own claim. A v4 file has no `dayLengthRealMinutes` at all, and the failure
    // this guards against is not a missing field -- it is a field that arrives as 0.0 and freezes
    // the clock of every player who upgrades.
    const auto older = Settings::FromJson(R"({
        "version": 4,
        "backBufferWidth": 1280,
        "fieldOfView": 80
    })",
                                          "v4");
    ASSERT_TRUE(older) << older.Error().ToString();
    EXPECT_EQ(older->version, Settings::kCurrentVersion);
    EXPECT_FLOAT_EQ(older->dayLengthRealMinutes, 24.0F)
        << "an upgraded settings file froze the clock or asked for the wrong day length";
    EXPECT_EQ(older->backBufferWidth, 1280) << "the migration lost what the file did say";
}
