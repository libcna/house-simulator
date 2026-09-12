// SPDX-License-Identifier: MIT
//
// `HOUSE-01562` and `HOUSE-01563`. §32.2's world-space sun direction and its 64-entry colour and
// intensity LUT.
//
// The direction is the part of §32 most likely to be wrong in a way that looks almost right: a
// flipped sign gives shadows of the correct length pointing the wrong way, and a swapped axis puts
// the morning sun in the west, neither of which a render test would obviously fail. So the vectors
// here are asserted against §10.1's COMPASS -- north is `−Z`, east is `+X` -- and never against
// each other or against a second copy of the same formula.
#include <algorithm>
#include <cmath>
#include <cstdio>

#include <gtest/gtest.h>

#include "Microsoft/Xna/Framework/Vector3.hpp"

#include "cnahouse/environment/SunLight.hpp"
#include "cnahouse/environment/SunModel.hpp"

namespace
{
    using cnahouse::environment::CivilTime;
    using cnahouse::environment::DirectCloudFactor;
    using cnahouse::environment::DirectionToSun;
    using cnahouse::environment::kStarsBeginAtSunAltitudeDeg;
    using cnahouse::environment::kStarsFullAtSunAltitudeDeg;
    using cnahouse::environment::kSunLightingAnchors;
    using cnahouse::environment::kSunLutHighestDeg;
    using cnahouse::environment::kSunLutLowestDeg;
    using cnahouse::environment::kSunLutSize;
    using cnahouse::environment::SkyDiffuseCloudFactor;
    using cnahouse::environment::StarVisibilityForSunAltitude;
    using cnahouse::environment::SunDirection;
    using cnahouse::environment::SunLighting;
    using cnahouse::environment::SunLightingAt;
    using cnahouse::environment::SunLightingTable;
    using cnahouse::environment::SunObserver;
    using cnahouse::environment::SunPosition;
    using cnahouse::environment::SunShadingFor;
    using cnahouse::environment::TwilightAmbientFactor;
    using cnahouse::environment::TwilightPhase;
    using cnahouse::environment::TwilightPhaseFor;
    using Microsoft::Xna::Framework::Vector3;

    /// @brief A position with only the two angles the direction depends on filled in.
    SunPosition At(double altitudeDeg, double azimuthDeg)
    {
        SunPosition sun;
        sun.altitudeDeg = altitudeDeg;
        sun.azimuthDeg = azimuthDeg;
        return sun;
    }

    void ExpectVectorNear(const Vector3& actual, float x, float y, float z, float tolerance = 1e-5F)
    {
        EXPECT_NEAR(actual.X, x, tolerance) << "X";
        EXPECT_NEAR(actual.Y, y, tolerance) << "Y";
        EXPECT_NEAR(actual.Z, z, tolerance) << "Z";
    }

} // namespace

TEST(SunLightTests, TheSunOnTheHorizonPointsAtTheCompassPointItsAzimuthNames)
{
    // §10.1: north `−Z`, east `+X`, south `+Z`, west `−X`. Azimuth is measured from north,
    // clockwise. Four cardinal points, on the horizon, so altitude cannot hide a swapped axis.
    ExpectVectorNear(DirectionToSun(At(0.0, 0.0)), 0.0F, 0.0F, -1.0F);
    ExpectVectorNear(DirectionToSun(At(0.0, 90.0)), 1.0F, 0.0F, 0.0F);
    ExpectVectorNear(DirectionToSun(At(0.0, 180.0)), 0.0F, 0.0F, 1.0F);
    ExpectVectorNear(DirectionToSun(At(0.0, 270.0)), -1.0F, 0.0F, 0.0F);
}

TEST(SunLightTests, StraightOverheadIsUpAndNothingElse)
{
    ExpectVectorNear(DirectionToSun(At(90.0, 0.0)), 0.0F, 1.0F, 0.0F);
    // ...and the azimuth stops mattering there, which is the degenerate case a `tan` in the
    // azimuth formula would have turned into a NaN.
    ExpectVectorNear(DirectionToSun(At(90.0, 217.0)), 0.0F, 1.0F, 0.0F);
}

TEST(SunLightTests, TheLightTravelsTheOppositeWayFromWhereTheSunIs)
{
    // `sunDirection` is what an XNA `DirectionalLight` takes, and it is the direction the light
    // TRAVELS. A sun in the east lights the world westward.
    const SunPosition east = At(0.0, 90.0);
    ExpectVectorNear(DirectionToSun(east), 1.0F, 0.0F, 0.0F);
    ExpectVectorNear(SunDirection(east), -1.0F, 0.0F, 0.0F);

    // A sun overhead lights the world downward, which is the sign most worth stating outright.
    ExpectVectorNear(SunDirection(At(90.0, 0.0)), 0.0F, -1.0F, 0.0F);
}

TEST(SunLightTests, EveryDirectionIsAUnitVector)
{
    for (double altitude = -90.0; altitude <= 90.0; altitude += 7.5)
    {
        for (double azimuth = 0.0; azimuth < 360.0; azimuth += 13.0)
        {
            const Vector3 toSun = DirectionToSun(At(altitude, azimuth));
            const double length = std::sqrt(static_cast<double>(toSun.X) * static_cast<double>(toSun.X) +
                                            static_cast<double>(toSun.Y) * static_cast<double>(toSun.Y) +
                                            static_cast<double>(toSun.Z) * static_cast<double>(toSun.Z));
            EXPECT_NEAR(length, 1.0, 1e-5) << "altitude " << altitude << ", azimuth " << azimuth;
        }
    }
}

TEST(SunLightTests, TheRealSunIsBelowTheHorizonAtMidnightAndAboveItAtNoon)
{
    // The direction wired to the real model rather than to hand-made angles: the Y component IS
    // `sin(altitude)`, so a sun that is up has a positive one. Midnight and noon on the same day.
    const SunObserver observer;
    CivilTime date;
    date.year = 2031;
    date.month = 6;
    date.day = 21;
    const auto day = cnahouse::environment::SunDayFor(date, observer);
    const double dayStart =
        cnahouse::environment::DaysSinceJ2000ForEpochSeconds(0.0, observer.utcOffsetMinutes) +
        static_cast<double>(cnahouse::environment::DaysFromCivil(2031, 6, 21) -
                            cnahouse::environment::DaysFromCivil(2031, 1, 1));

    const Vector3 atNoon = DirectionToSun(
        cnahouse::environment::SunPositionAt(dayStart + day.transit.minutesOfDay / 1440.0, observer));
    EXPECT_GT(atNoon.Y, 0.9F) << "the June noon sun is 73° up, so Y is sin(73°)";
    EXPECT_GT(atNoon.Z, 0.0F) << "and it is in the SOUTH, which is +Z";

    const Vector3 atMidnight = DirectionToSun(cnahouse::environment::SunPositionAt(dayStart, observer));
    EXPECT_LT(atMidnight.Y, 0.0F) << "the sun is down at midnight";
    EXPECT_LT(atMidnight.Z, 0.0F) << "and it is in the NORTH, which is −Z";
}

TEST(SunLightTests, TheLutReproducesEverySixAnchorsSectionThirtyTwoStates)
{
    // §32.2's table is the specification, so the LUT has to give those six rows back at those six
    // altitudes. The tolerance is the LUT's own resolution: 64 entries over 66° is 1.05° a step,
    // and an anchor that does not land on an entry is interpolated to within a fraction of the
    // step's change.
    for (const auto& anchor : kSunLightingAnchors)
    {
        const SunLighting lighting = SunLightingAt(static_cast<double>(anchor.altitudeDeg));
        EXPECT_NEAR(lighting.color.X, anchor.red, 0.02F) << "altitude " << anchor.altitudeDeg;
        EXPECT_NEAR(lighting.color.Y, anchor.green, 0.02F) << "altitude " << anchor.altitudeDeg;
        EXPECT_NEAR(lighting.color.Z, anchor.blue, 0.02F) << "altitude " << anchor.altitudeDeg;
        EXPECT_NEAR(lighting.intensity, anchor.intensity, 0.02F) << "altitude " << anchor.altitudeDeg;
    }
}

TEST(SunLightTests, TheTableIsSixtyFourEntriesAndItsEndsSitOnTheFirstAndLastAnchor)
{
    const auto& table = SunLightingTable();
    ASSERT_EQ(table.size(), kSunLutSize);
    EXPECT_EQ(kSunLutSize, 64U) << "§32.2 says a 64-entry LUT";
    EXPECT_FLOAT_EQ(kSunLutLowestDeg, kSunLightingAnchors.front().altitudeDeg);
    EXPECT_FLOAT_EQ(kSunLutHighestDeg, kSunLightingAnchors.back().altitudeDeg);
    EXPECT_NEAR(table.front().intensity, kSunLightingAnchors.front().intensity, 1e-6F);
    EXPECT_NEAR(table.back().intensity, kSunLightingAnchors.back().intensity, 1e-6F);
}

TEST(SunLightTests, IntensityNeverGoesDownAsTheSunGoesUp)
{
    // Monotone by construction: every anchor's intensity is larger than the one below it, so any
    // interpolation between them is too. A LUT built with the anchors out of order, or indexed off
    // by one, breaks this and nothing about a single lookup would show it.
    double previous = -1.0;
    for (double altitude = -20.0; altitude <= 90.0; altitude += 0.25)
    {
        const double intensity = static_cast<double>(SunLightingAt(altitude).intensity);
        EXPECT_GE(intensity, previous - 1e-6) << "altitude " << altitude;
        previous = intensity;
    }
    // ...and it spans the full range the anchors give.
    EXPECT_NEAR(SunLightingAt(-30.0).intensity, 0.02F, 1e-6F);
    EXPECT_NEAR(SunLightingAt(89.0).intensity, 1.00F, 1e-6F);
}

TEST(SunLightTests, TheCurveIsContinuousSoADuskDoesNotStep)
{
    // §36.3's rule about seasons is the same rule here: a boundary crossed every simulated minute
    // has to be a blend and not a switch. The largest step between two neighbouring quarter-degrees
    // bounds what a frame can ever jump by.
    double worstIntensityStep = 0.0;
    double worstColourStep = 0.0;
    double worstAt = 0.0;
    SunLighting previous = SunLightingAt(-30.0);
    for (double altitude = -30.0; altitude <= 90.0; altitude += 0.25)
    {
        const SunLighting current = SunLightingAt(altitude);
        const double intensityStep =
            std::abs(static_cast<double>(current.intensity) - static_cast<double>(previous.intensity));
        const double colourStep =
            std::abs(static_cast<double>(current.color.X) - static_cast<double>(previous.color.X)) +
            std::abs(static_cast<double>(current.color.Y) - static_cast<double>(previous.color.Y)) +
            std::abs(static_cast<double>(current.color.Z) - static_cast<double>(previous.color.Z));
        if (intensityStep > worstIntensityStep)
        {
            worstIntensityStep = intensityStep;
            worstAt = altitude;
        }
        worstColourStep = std::max(worstColourStep, colourStep);
        previous = current;
    }
    // The steepest stretch of §32.2's table is −0.83° to +2°, where intensity goes 0.10 → 0.35 in
    // 2.83°: about 0.022 a quarter-degree. Anything much larger than that is a discontinuity.
    EXPECT_LT(worstIntensityStep, 0.04) << "the steepest intensity step is at altitude " << worstAt;
    EXPECT_LT(worstColourStep, 0.12);
    std::printf("  worst quarter-degree step: intensity %.4f at %.2f°, colour %.4f\n",
                worstIntensityStep,
                worstAt,
                worstColourStep);
}

TEST(SunLightTests, BelowCivilTwilightTheAnswerIsTheTwilightRowAndNotBlack)
{
    // Deliberate, and worth a test of its own because "clamp to zero below the horizon" is the
    // obvious thing to write. §32.2 feeds this LUT to the SKY-DIFFUSE term as well as the direct
    // one, and a night sky is dim blue rather than black; clamping to zero would also put a 2 %
    // step into every dusk at exactly −6°.
    const SunLighting deepNight = SunLightingAt(-40.0);
    EXPECT_NEAR(deepNight.intensity, 0.02F, 1e-6F);
    EXPECT_NEAR(deepNight.color.X, 0.22F, 1e-6F);
    EXPECT_NEAR(deepNight.color.Z, 0.38F, 1e-6F);
    EXPECT_GT(deepNight.color.Z, deepNight.color.X) << "the twilight row is blue, not red";

    // ...and the LUT is exactly equal on both sides of the lowest anchor, so there is no step.
    EXPECT_FLOAT_EQ(SunLightingAt(-6.0).intensity, SunLightingAt(-6.001).intensity);
}

TEST(SunLightTests, TheHorizonRowIsOrangeAndTheNoonRowIsWhite)
{
    // §32.2's whole reason for a colour table. Checked as a RELATION between channels rather than
    // as the numbers again, so it fails if the table is ever reordered or an index slips.
    const SunLighting sunset = SunLightingAt(-0.83);
    EXPECT_GT(sunset.color.X, sunset.color.Y);
    EXPECT_GT(sunset.color.Y, sunset.color.Z) << "a sunset is red > green > blue";

    const SunLighting noon = SunLightingAt(60.0);
    EXPECT_NEAR(noon.color.X, noon.color.Y, 0.02F);
    EXPECT_NEAR(noon.color.Y, noon.color.Z, 0.02F) << "the high sun is white";
}

TEST(SunLightTests, CloudTakesMostOfTheBeamAndLittleOfTheAmbient)
{
    // §32.2: `1 − 0.85·cloudCover` direct, `1 − 0.35·cloudCover` sky-diffuse. The GAP between them
    // is the look of an overcast day -- the world flattens rather than darkening -- so the test is
    // about the gap and not only about the two numbers.
    EXPECT_DOUBLE_EQ(DirectCloudFactor(0.0), 1.0);
    EXPECT_DOUBLE_EQ(SkyDiffuseCloudFactor(0.0), 1.0);
    EXPECT_NEAR(DirectCloudFactor(1.0), 0.15, 1e-12);
    EXPECT_NEAR(SkyDiffuseCloudFactor(1.0), 0.65, 1e-12);
    EXPECT_GT(SkyDiffuseCloudFactor(1.0), DirectCloudFactor(1.0));

    // Out-of-range cover is clamped rather than extrapolated: a weather system that overshoots
    // must not make the sun brighter than clear sky or give it a negative intensity.
    EXPECT_DOUBLE_EQ(DirectCloudFactor(-5.0), 1.0);
    EXPECT_NEAR(DirectCloudFactor(9.0), 0.15, 1e-12);
    EXPECT_DOUBLE_EQ(DirectCloudFactor(std::nan("")), 1.0);
}

TEST(SunLightTests, ShadingAttenuatesTheIntensityAndLeavesTheColourAlone)
{
    // The decision recorded on `SunShading`: a renderer uses `colour × intensity`, so putting the
    // cloud factor into both would apply it twice.
    const SunPosition high = At(60.0, 180.0);
    const auto clear = SunShadingFor(high, 0.0);
    const auto overcast = SunShadingFor(high, 1.0);

    EXPECT_FLOAT_EQ(clear.color.X, overcast.color.X);
    EXPECT_FLOAT_EQ(clear.color.Y, overcast.color.Y);
    EXPECT_FLOAT_EQ(clear.color.Z, overcast.color.Z);

    EXPECT_NEAR(clear.directIntensity, 1.0F, 1e-5F);
    EXPECT_NEAR(overcast.directIntensity, 0.15F, 1e-5F);
    EXPECT_NEAR(overcast.skyDiffuseIntensity, 0.65F, 1e-5F);

    // At night the cloud still attenuates, and 15 % of 2 % is not zero -- a scene lit only by this
    // is very dark and is not black, which is what §31.4's night screenshots expect.
    const auto night = SunShadingFor(At(-20.0, 0.0), 1.0);
    EXPECT_GT(night.directIntensity, 0.0F);
    EXPECT_LT(night.directIntensity, 0.01F);
    EXPECT_FLOAT_EQ(night.skyDiffuseIntensity, 0.0F)
        << "HOUSE-01574 fades the solar ambient out by astronomical night";
}

TEST(SunLightTests, TwilightBandsUseTheThreeStandardThresholds)
{
    EXPECT_EQ(TwilightPhaseFor(20.0), TwilightPhase::Day);
    EXPECT_EQ(TwilightPhaseFor(-1.0), TwilightPhase::Civil);
    EXPECT_EQ(TwilightPhaseFor(-6.0), TwilightPhase::Nautical);
    EXPECT_EQ(TwilightPhaseFor(-12.0), TwilightPhase::Astronomical);
    EXPECT_EQ(TwilightPhaseFor(-18.0), TwilightPhase::Night);
    EXPECT_EQ(TwilightPhaseFor(std::nan("")), TwilightPhase::Night);
}

TEST(SunLightTests, TwilightAmbientFadesSmoothlyToAstronomicalNight)
{
    EXPECT_DOUBLE_EQ(TwilightAmbientFactor(-6.0), 1.0);
    EXPECT_DOUBLE_EQ(TwilightAmbientFactor(-12.0), 0.5);
    EXPECT_DOUBLE_EQ(TwilightAmbientFactor(-18.0), 0.0);
    EXPECT_DOUBLE_EQ(TwilightAmbientFactor(-30.0), 0.0);

    double previous = 1.0;
    double worstStep = 0.0;
    for (double altitude = -6.0; altitude >= -18.0; altitude -= 0.05)
    {
        const double current = TwilightAmbientFactor(altitude);
        EXPECT_LE(current, previous + 1e-12);
        worstStep = std::max(worstStep, previous - current);
        previous = current;
    }
    EXPECT_LT(worstStep, 0.007) << "the twilight ambient contains a visible step";

    const auto civil = SunShadingFor(At(-6.0, 0.0), 0.0);
    const auto nautical = SunShadingFor(At(-12.0, 0.0), 0.0);
    const auto night = SunShadingFor(At(-18.0, 0.0), 0.0);
    EXPECT_NEAR(civil.skyDiffuseIntensity, 0.02F, 1e-6F);
    EXPECT_NEAR(nautical.skyDiffuseIntensity, 0.01F, 1e-6F);
    EXPECT_FLOAT_EQ(night.skyDiffuseIntensity, 0.0F);
}

TEST(SunLightTests, StarsAppearAcrossTwilightAndAreFullBeforeAstronomicalNight)
{
    EXPECT_DOUBLE_EQ(kStarsBeginAtSunAltitudeDeg, -4.0);
    EXPECT_DOUBLE_EQ(kStarsFullAtSunAltitudeDeg, -14.0);
    EXPECT_DOUBLE_EQ(StarVisibilityForSunAltitude(-4.0), 0.0);
    EXPECT_DOUBLE_EQ(StarVisibilityForSunAltitude(-9.0), 0.5);
    EXPECT_DOUBLE_EQ(StarVisibilityForSunAltitude(-14.0), 1.0);
    EXPECT_DOUBLE_EQ(StarVisibilityForSunAltitude(-18.0), 1.0);
    EXPECT_DOUBLE_EQ(StarVisibilityForSunAltitude(std::nan("")), 0.0);

    double previous = 0.0;
    for (double altitude = -4.0; altitude >= -14.0; altitude -= 0.05)
    {
        const double current = StarVisibilityForSunAltitude(altitude);
        EXPECT_GE(current, previous - 1e-12);
        previous = current;
    }
}
