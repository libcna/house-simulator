// SPDX-License-Identifier: MIT
//
// `HOUSE-01535`. §36.2's seasonal base temperature curve:
//
//     base(doy, h) = 11 + 12·cos(2π(doy−201)/365) + 6·cos(2π(h−15)/24)
//
// **The curve is load-bearing rather than decorative**, which is why it is worth a file of its
// own. §36.2 derives `precipType` from the temperature -- above 2.5 °C rain, below 0 snow, between
// them sleet -- so *"it snows in January and rains in July without any special-casing"*, and §36.3
// says `W_SNOW`'s probability is zero in summer *"by construction rather than by a special case"*.
// Both of those are properties of this function, and both are checked here as such.
#include <algorithm>
#include <cmath>
#include <limits>
#include <string_view>
#include <utility>

#include <gtest/gtest.h>

#include "cnahouse/environment/SimClock.hpp"
#include "cnahouse/environment/Temperature.hpp"

namespace
{
    using cnahouse::environment::BaseTemperatureC;
    using cnahouse::environment::CivilTime;
    using cnahouse::environment::kAnnualAmplitudeC;
    using cnahouse::environment::kAnnualMeanC;
    using cnahouse::environment::kDiurnalAmplitudeC;
    using cnahouse::environment::kWarmestDayOfYear;
    using cnahouse::environment::kWarmestHourOfDay;
    using cnahouse::environment::SimClock;

    /// §36.2's own derivation, so the claims below can be about weather and not about arithmetic.
    [[nodiscard]] const char* PrecipitationAt(double celsius) noexcept
    {
        if (celsius > 2.5)
        {
            return "Rain";
        }
        return celsius < 0.0 ? "Snow" : "Sleet";
    }

} // namespace

TEST(TemperatureCurveTests, TheThreeConstantsAreSection36Point2s)
{
    EXPECT_DOUBLE_EQ(kAnnualMeanC, 11.0);
    EXPECT_DOUBLE_EQ(kAnnualAmplitudeC, 12.0);
    EXPECT_DOUBLE_EQ(kDiurnalAmplitudeC, 6.0);
    EXPECT_DOUBLE_EQ(kWarmestDayOfYear, 201.0) << "20 July, three weeks after the solstice";
    EXPECT_DOUBLE_EQ(kWarmestHourOfDay, 15.0) << "15:00 and not noon, for the same reason";
    // The extremes the base curve alone can reach: 11 ± 12 ± 6. §36.1's `temperatureC` runs
    // -18..38 because each archetype's Δtemp is applied on top of this.
    EXPECT_DOUBLE_EQ(BaseTemperatureC(kWarmestDayOfYear, kWarmestHourOfDay), 29.0);
    EXPECT_DOUBLE_EQ(BaseTemperatureC(kWarmestDayOfYear - 365.0 / 2.0, kWarmestHourOfDay - 12.0), -7.0);
}

TEST(TemperatureCurveTests, TheAnnualSwingIsLargerThanTheDailyOne)
{
    // Which is what makes a January afternoon colder than a July night, and is the reason the two
    // amplitudes are not interchangeable numbers.
    const double julyNight = BaseTemperatureC(201.0, 3.0);
    const double januaryAfternoon = BaseTemperatureC(18.0, 15.0);
    EXPECT_GT(julyNight, januaryAfternoon)
        << "a July night is colder than a January afternoon, so the amplitudes are the wrong way "
           "round";
    EXPECT_GT(kAnnualAmplitudeC, kDiurnalAmplitudeC);

    // Both terms are cosines about their own peak, so each is symmetric either side of it.
    for (const double offset : {1.0, 10.0, 60.0, 120.0})
    {
        EXPECT_NEAR(BaseTemperatureC(kWarmestDayOfYear + offset, 12.0),
                    BaseTemperatureC(kWarmestDayOfYear - offset, 12.0),
                    1e-9)
            << offset;
    }
    for (const double offset : {1.0, 3.0, 6.0})
    {
        EXPECT_NEAR(BaseTemperatureC(100.0, kWarmestHourOfDay + offset),
                    BaseTemperatureC(100.0, kWarmestHourOfDay - offset),
                    1e-9)
            << offset;
    }
}

TEST(TemperatureCurveTests, ItSnowsInJanuaryAndRainsInJulyWithoutASpecialCase)
{
    // §36.2's sentence, made a measurement. The precipitation type is DERIVED from this curve, so
    // the seasons of snow and rain are a property of these three constants and of nothing else.
    int snowyHours = 0;
    int rainyHours = 0;
    for (int hour = 0; hour < 24; ++hour)
    {
        // Mid-January, and the coldest week of the year is around day 18.
        const double january = BaseTemperatureC(18.0, static_cast<double>(hour));
        snowyHours += PrecipitationAt(january) == std::string_view("Snow") ? 1 : 0;
        // Mid-July.
        const double july = BaseTemperatureC(201.0, static_cast<double>(hour));
        rainyHours += PrecipitationAt(july) == std::string_view("Rain") ? 1 : 0;
        EXPECT_GT(july, 2.5) << "July hour " << hour << " is cold enough to sleet";
    }
    // **Thirteen of January's twenty-four hours, not all of them, and that is the curve being
    // honest.** Mid-January peaks at +5 °C at three in the afternoon -- 11 − 12 + 6 -- so the base
    // curve alone sleets or rains through the middle of the day and snows from late afternoon to
    // mid-morning. §36.2's *"it snows in January"* is the archetype's Δtemp applied on top of
    // this; what the base has to guarantee is that January CAN snow and July cannot, and the
    // asymmetry between these two numbers is that guarantee.
    EXPECT_EQ(snowyHours, 13) << "January's night is not below freezing";
    EXPECT_EQ(rainyHours, 24) << "July is not above 2.5 C at every hour of the day";
    EXPECT_LT(BaseTemperatureC(18.0, 6.0), 0.0) << "a January dawn is not below freezing";
    EXPECT_GT(BaseTemperatureC(18.0, 15.0), 2.5) << "a January afternoon is not above sleet";

    // §36.3: "`W_SNOW` has probability zero in summer... snow in July is impossible by
    // construction". Over the whole of meteorological summer, at every hour, the curve never
    // reaches freezing -- and it is not close.
    double warmestSummerLow = 100.0;
    for (int day = 152; day <= 243; ++day) // June, July, August
    {
        for (int hour = 0; hour < 24; ++hour)
        {
            warmestSummerLow = std::min(
                warmestSummerLow, BaseTemperatureC(static_cast<double>(day), static_cast<double>(hour)));
        }
    }
    EXPECT_GT(warmestSummerLow, 2.5) << "summer reaches " << warmestSummerLow << " C, which sleets or snows";
}

TEST(TemperatureCurveTests, BothArgumentsArePeriodicAndNaNIsRefused)
{
    // A caller integrating a day at a time should not have to wrap, and a caller that wraps
    // differently should not get a different answer.
    EXPECT_NEAR(BaseTemperatureC(1.0, 6.0), BaseTemperatureC(366.0, 6.0), 1e-9);
    EXPECT_NEAR(BaseTemperatureC(50.0, 1.0), BaseTemperatureC(50.0, 25.0), 1e-9);
    EXPECT_NEAR(BaseTemperatureC(50.0, 0.0), BaseTemperatureC(50.0, -24.0), 1e-9);
    EXPECT_NEAR(BaseTemperatureC(-315.0, 6.0), BaseTemperatureC(50.0, 6.0), 1e-9);

    // A NaN here would reach `precipType`, `snowDepth` and the furnace's thermostat inside a
    // frame, and none of those has a way back.
    EXPECT_DOUBLE_EQ(BaseTemperatureC(std::nan(""), 12.0), kAnnualMeanC);
    EXPECT_DOUBLE_EQ(BaseTemperatureC(100.0, std::nan("")), kAnnualMeanC);
    EXPECT_TRUE(std::isfinite(BaseTemperatureC(std::numeric_limits<double>::infinity(), 12.0)));
}

TEST(TemperatureCurveTests, TheClockReadsTheAnnualTermFromTheCalendarAndTheDailyOneFromItsFace)
{
    // §35.2b's decoupling, arriving somewhere it can be felt. Over one simulated day of play the
    // sun rises and sets once -- so the diurnal term completes exactly one cycle -- while the
    // season moves 24 days underneath it.
    SimClock clock;
    clock.SetCalendar(200.0); // deep summer, so the annual term is near its peak and moving slowly
    const double startDay = clock.CalendarDays();
    double warmest = -100.0;
    double coldest = 100.0;
    for (int minute = 0; minute < 24; ++minute)
    {
        clock.Advance(60.0); // one real minute = one simulated hour
        warmest = std::max(warmest, clock.OutdoorBaseTemperatureC());
        coldest = std::min(coldest, clock.OutdoorBaseTemperatureC());
    }
    EXPECT_NEAR(clock.CalendarDays() - startDay, 24.0, 1e-6) << "a simulated day is 24 calendar days";
    // One full diurnal cycle in that time: the swing is about the full 2 x 6 C, not a fraction of
    // it and not several cycles' worth.
    EXPECT_GT(warmest - coldest, 9.0) << "the day did not swing, so the diurnal term is not moving";
    EXPECT_LT(warmest - coldest, 14.0)
        << "the swing is wider than the diurnal amplitude allows, so the annual term is being "
           "driven by the clock face as well";

    // **And it is CONTINUOUS.** The diurnal term moves 1.6 °C in a simulated hour at its steepest,
    // so a curve fed the truncated hour instead of the clock face steps by that much once an hour
    // -- the same class of defect §36.3 forbids for the season, arriving in the quantity the
    // furnace's thermostat and §5's house creaks both read.
    SimClock minute;
    minute.SetCalendar(200.0);
    double largestStep = 0.0;
    double previous = minute.OutdoorBaseTemperatureC();
    for (int step = 0; step < 24 * 60; ++step)
    {
        minute.Advance(1.0); // one real second = one simulated minute
        const double now = minute.OutdoorBaseTemperatureC();
        largestStep = std::max(largestStep, std::abs(now - previous));
        previous = now;
    }
    EXPECT_LT(largestStep, 0.1) << "the temperature moved " << largestStep
                                << " C in a simulated minute, so it is stepping";

    // ...and with the compression off, a whole simulated day moves the season by exactly one day.
    SimClock plain;
    plain.calendarDaysPerSimDay = 1.0;
    plain.SetCalendar(200.0);
    const double before = plain.OutdoorBaseTemperatureC();
    plain.Advance(24.0 * 60.0);
    EXPECT_NEAR(plain.CalendarDays(), 201.0, 1e-6);
    EXPECT_NEAR(plain.OutdoorBaseTemperatureC() - before, 0.0, 0.05)
        << "the same hour a calendar day later is a different temperature by more than a day's "
           "worth of the annual term";
}

TEST(TemperatureCurveTests, TheYearThroughTheClockPeaksInJulyAndBottomsInJanuary)
{
    // The curve read where a caller will read it: off the clock, at a fixed hour, through a year.
    SimClock clock;
    clock.calendarDaysPerSimDay = 1.0;
    double warmest = -100.0;
    double coldest = 100.0;
    int warmestDay = 0;
    int coldestDay = 0;
    for (int day = 1; day <= 365; ++day)
    {
        CivilTime noon;
        noon.year = 2031;
        noon.month = 1;
        noon.day = 1;
        noon.hour = 15;
        clock.SetStandard(noon);
        clock.epochSeconds += static_cast<double>(day - 1) * 86400.0;
        const double now = clock.OutdoorBaseTemperatureC();
        if (now > warmest)
        {
            warmest = now;
            warmestDay = day;
        }
        if (now < coldest)
        {
            coldest = now;
            coldestDay = day;
        }
    }
    // Within a day of §36.2's own peak: the samples are taken at 15:00, which is 0.625 of a day
    // into each one, so the nearest SAMPLE to day 201.0 is day 200 rather than 201. Asserting the
    // exact index would be asserting where the sampling grid happens to fall.
    EXPECT_NEAR(warmestDay, 201, 1) << "the warmest afternoon of the year is not around 20 July";
    EXPECT_NEAR(coldestDay, 18, 1) << "the coldest afternoon is not around mid-January";
    // 15:00 IS the warmest hour, so the warmest afternoon is the curve's own maximum. The coldest
    // afternoon is not its minimum: it is 11 − 12 + 6, because the diurnal term is at its peak.
    EXPECT_NEAR(warmest, 29.0, 1e-3);
    EXPECT_NEAR(coldest, 5.0, 1e-3);
    EXPECT_NEAR(BaseTemperatureC(18.5, 3.0), -7.0, 0.01) << "the coldest hour of the coldest day";

    // **The clock feeds the curve the day of the year the CALENDAR says it is**, which the
    // sampling above cannot see: the annual cosine is flat at its peak, so a whole day's error
    // there moves the temperature by 0.0004 °C. Compared at midnight against `dayOfYear` as
    // `Standard()` reports it -- a field 500 conversions from Python already agree with
    // (`HOUSE-01532`) -- the comparison is exact wherever on the curve the date falls.
    for (const auto& [month, day] :
         {std::pair{1, 1}, std::pair{4, 20}, std::pair{7, 20}, std::pair{10, 21}, std::pair{12, 31}})
    {
        CivilTime midnight;
        midnight.year = 2031;
        midnight.month = month;
        midnight.day = day;
        clock.SetStandard(midnight);
        EXPECT_NEAR(clock.OutdoorBaseTemperatureC(),
                    BaseTemperatureC(static_cast<double>(clock.Standard().dayOfYear), 0.0),
                    1e-9)
            << month << "/" << day << " reaches the curve as a different day of the year";
    }
}
