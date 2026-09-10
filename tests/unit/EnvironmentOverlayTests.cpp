// SPDX-License-Identifier: MIT
//
// `HOUSE-01537`. §71's `F8` environment overlay -- its TIME section.
//
// A **presenter**: it owns no measurement and takes no queries of its own, so what it says can be
// asserted here rather than looked at. §71 lists `F8` as *"simulated date/time, sun/moon altitude
// and azimuth, moon phase and name, the full weather state vector, the current archetype and time
// to the next transition, RNG state"*; everything after the first clause is §35.3's sun and §36's
// weather, which are later phases. That the panel SAYS so is one of the claims below -- a debug
// overlay silently missing half its rows teaches its reader that the missing rows do not exist.
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "cnahouse/debug/EnvironmentOverlay.hpp"
#include "cnahouse/environment/DayLength.hpp"
#include "cnahouse/environment/Season.hpp"
#include "cnahouse/environment/SimClock.hpp"

namespace
{
    using cnahouse::debug::EnvironmentOverlay;
    using cnahouse::debug::SeasonName;
    using cnahouse::environment::CivilTime;
    using cnahouse::environment::Season;
    using cnahouse::environment::SimClock;
    using cnahouse::environment::TimeScaleForDayLength;

    /// Every line joined, for claims about what the panel does or does not mention at all.
    std::string Joined(const std::vector<std::string>& lines)
    {
        std::string all;
        for (const std::string& line : lines)
        {
            all += line;
            all += '\n';
        }
        return all;
    }

} // namespace

TEST(EnvironmentOverlayTests, ItStartsHiddenAndTogglesLikeEveryOtherOverlay)
{
    EnvironmentOverlay overlay;
    EXPECT_FALSE(overlay.Visible()) << "a debug overlay that starts shown is in every screenshot";
    overlay.Toggle();
    EXPECT_TRUE(overlay.Visible());
    overlay.SetVisible(false);
    EXPECT_FALSE(overlay.Visible());
}

TEST(EnvironmentOverlayTests, TheWallClockComesFirstAndTheStandardReadingIsUnderIt)
{
    // §31.4's screenshot scene, which is the fixture every clock test in the project is anchored
    // to: Saturday 14 June 2031, 09:20 local, UTC-4 DST -- so 08:20 STANDARD.
    SimClock clock;
    clock.calendarDaysPerSimDay = 1.0;
    CivilTime scene;
    scene.year = 2031;
    scene.month = 6;
    scene.day = 14;
    scene.hour = 8;
    scene.minute = 20;
    clock.SetStandard(scene);

    const std::vector<std::string> lines = EnvironmentOverlay().Lines(clock);
    ASSERT_GE(lines.size(), 3U);
    EXPECT_EQ(lines[0], "F8  environment") << "the panel does not say which key it is";
    // The wall clock is what a person means by "what time is it", so it is the first reading --
    // with the DST flag beside it, or a line an hour off the standard one below looks like a bug.
    EXPECT_NE(lines[1].find("09:20:00"), std::string::npos) << lines[1];
    EXPECT_NE(lines[1].find("Sat"), std::string::npos) << lines[1];
    EXPECT_NE(lines[1].find("2031-06-14"), std::string::npos) << lines[1];
    EXPECT_NE(lines[1].find("DST"), std::string::npos) << lines[1];
    // ...and the STANDARD reading under it, because `epochSeconds` is standard time and every
    // number below is derived from that rather than from the wall clock.
    EXPECT_NE(lines[2].find("08:20:00"), std::string::npos) << lines[2];
    EXPECT_NE(lines[2].find("day 165"), std::string::npos) << lines[2];
}

TEST(EnvironmentOverlayTests, TheRateIsShownBothWaysAndFrozenSaysSo)
{
    SimClock clock;
    clock.timeScale = TimeScaleForDayLength(24.0);
    std::string all = Joined(EnvironmentOverlay().Lines(clock));
    // A multiplier AND a day length, because §35.2's table is indexed by the second one and the
    // settings dialogue shows that; a reader should not have to divide 1440 by anything.
    EXPECT_NE(all.find("60x"), std::string::npos) << all;
    EXPECT_NE(all.find("24 real min / sim day"), std::string::npos) << all;

    clock.timeScale = TimeScaleForDayLength(0.0);
    all = Joined(EnvironmentOverlay().Lines(clock));
    EXPECT_NE(all.find("FROZEN"), std::string::npos)
        << "a frozen clock reads as a rate of 0x with no explanation: " << all;
    EXPECT_EQ(all.find("inf"), std::string::npos) << "the frozen day length printed as infinity";
}

TEST(EnvironmentOverlayTests, BothDayCountsAreShownBecauseTheWholePointIsThatTheyDiffer)
{
    // §35.2b: the calendar runs 24 times faster than the sun. An overlay showing one of the two
    // numbers would make the compression invisible in the one place built to explain it.
    SimClock clock;
    clock.SetCalendar(48.0); // two simulated days in, 48 calendar days on
    const std::string all = Joined(EnvironmentOverlay().Lines(clock));
    EXPECT_NE(all.find("48.00 days"), std::string::npos) << all;
    EXPECT_NE(all.find("24/sim day"), std::string::npos) << all;
    EXPECT_NE(all.find("2 sunrise(s)"), std::string::npos) << all;
}

TEST(EnvironmentOverlayTests, TheSeasonIsAMixAndNotALabel)
{
    // §36.3: "Season is a continuous phase, never an enum." An overlay that printed only
    // `primary` would be showing an enum, and would make a boundary look like a jump.
    SimClock clock;
    clock.calendarDaysPerSimDay = 1.0;
    // A quarter into the year from the vernal equinox is the spring/summer boundary, where the
    // mix is half and half.
    clock.SetCalendar(78.0 + 365.0 * 0.25);
    const std::string all = Joined(EnvironmentOverlay().Lines(clock));
    EXPECT_NE(all.find("spring"), std::string::npos) << all;
    EXPECT_NE(all.find("summer"), std::string::npos) << all;
    EXPECT_NE(all.find("50%"), std::string::npos) << "the boundary is not shown as half and half: " << all;

    // Deep in a season it is one season at 100 % and still names the one ahead, so a reader can
    // see what is coming without waiting for the blend to start.
    clock.SetCalendar(78.0 + 365.0 * 0.125);
    const std::string middle = Joined(EnvironmentOverlay().Lines(clock));
    EXPECT_NE(middle.find("spring 100%"), std::string::npos) << middle;
    EXPECT_NE(middle.find("summer (0%)"), std::string::npos) << middle;
}

TEST(EnvironmentOverlayTests, TheBaseTemperatureIsHereBecauseSection36DerivesTheWeatherFromIt)
{
    // §36.2 applies each archetype's Δtemp to this curve, and §36.3 gates snow on the result. A
    // person debugging "why is it not snowing" needs the base before they need the archetype.
    SimClock clock;
    clock.calendarDaysPerSimDay = 1.0;
    CivilTime january;
    january.year = 2031;
    january.month = 1;
    january.day = 18;
    january.hour = 3;
    clock.SetStandard(january);
    const std::string all = Joined(EnvironmentOverlay().Lines(clock));
    EXPECT_NE(all.find("-7.0 C base"), std::string::npos)
        << "the coldest hour of the coldest night does not read -7 C: " << all;
    EXPECT_NE(all.find("§36.2"), std::string::npos) << "the line does not say where the curve is from";
    EXPECT_NE(all.find("before weather"), std::string::npos)
        << "nothing says this is the base rather than the temperature outside";
}

TEST(EnvironmentOverlayTests, ThePanelSaysWhichOfSection71sRowsAreNotBuiltYet)
{
    // The claim that keeps this overlay honest as the phases land: §71 lists sun, moon, weather
    // and RNG on `F8`, and none of them exists. Saying so is what stops the panel looking finished.
    const std::string all = Joined(EnvironmentOverlay().Lines(SimClock()));
    EXPECT_NE(all.find("not built yet"), std::string::npos) << all;
    EXPECT_NE(all.find("§35.3"), std::string::npos) << "the missing sun is not attributed";
    EXPECT_NE(all.find("§36"), std::string::npos) << "the missing weather is not attributed";
}

TEST(EnvironmentOverlayTests, SeasonNameCoversEverySeasonAndRefusesNonsense)
{
    EXPECT_EQ(SeasonName(static_cast<int>(Season::Spring)), "spring");
    EXPECT_EQ(SeasonName(static_cast<int>(Season::Summer)), "summer");
    EXPECT_EQ(SeasonName(static_cast<int>(Season::Autumn)), "autumn");
    EXPECT_EQ(SeasonName(static_cast<int>(Season::Winter)), "winter");
    // An index out of range is a "?" and not a read past the end of a table.
    EXPECT_EQ(SeasonName(-1), "?");
    EXPECT_EQ(SeasonName(4), "?");
    EXPECT_EQ(SeasonName(9999), "?");
}
