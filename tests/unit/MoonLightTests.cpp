// SPDX-License-Identifier: MIT
//
// `HOUSE-01607`. Exact arithmetic and boundaries for §33.4's lunar key light.
#include <cmath>
#include <limits>

#include <gtest/gtest.h>

#include "cnahouse/environment/MoonLight.hpp"

namespace
{
    using cnahouse::environment::DirectionToMoon;
    using cnahouse::environment::kMoonMaximumIntensity;
    using cnahouse::environment::MoonCloudFactor;
    using cnahouse::environment::MoonDirection;
    using cnahouse::environment::MoonlightMayBeKey;
    using cnahouse::environment::MoonPhase;
    using cnahouse::environment::MoonPhaseIllumination;
    using cnahouse::environment::MoonPosition;
    using cnahouse::environment::MoonShadingFor;
} // namespace

TEST(MoonLightTests, PhaseCurveIsNonLinearMonotoneAndPinnedAtBothEnds)
{
    EXPECT_DOUBLE_EQ(MoonPhaseIllumination(0.0), 0.0);
    EXPECT_DOUBLE_EQ(MoonPhaseIllumination(1.0), 1.0);
    EXPECT_NEAR(MoonPhaseIllumination(0.5), std::pow(0.5, 1.8), 1e-14);
    EXPECT_LT(MoonPhaseIllumination(0.5), 0.5) << "half moon must be much dimmer than half full";

    double previous = 0.0;
    for (int step = 0; step <= 100; ++step)
    {
        const double current = MoonPhaseIllumination(static_cast<double>(step) / 100.0);
        EXPECT_GE(current, previous);
        previous = current;
    }
    EXPECT_DOUBLE_EQ(MoonPhaseIllumination(-1.0), 0.0);
    EXPECT_DOUBLE_EQ(MoonPhaseIllumination(2.0), 1.0);
    EXPECT_DOUBLE_EQ(MoonPhaseIllumination(std::numeric_limits<double>::quiet_NaN()), 0.0);
}

TEST(MoonLightTests, AltitudeAndCloudFactorsProduceTheAuthoredIntensity)
{
    MoonPosition moon;
    MoonPhase phase;
    phase.illuminatedFraction = 1.0;

    moon.altitudeDeg = 90.0;
    EXPECT_FLOAT_EQ(MoonShadingFor(moon, phase, 0.0).intensity, kMoonMaximumIntensity);
    EXPECT_NEAR(
        MoonShadingFor(moon, phase, 1.0).intensity, static_cast<double>(kMoonMaximumIntensity) * 0.05, 1e-9);
    EXPECT_DOUBLE_EQ(MoonCloudFactor(0.0), 1.0);
    EXPECT_NEAR(MoonCloudFactor(1.0), 0.05, 1e-14);

    moon.altitudeDeg = 30.0;
    EXPECT_NEAR(MoonShadingFor(moon, phase, 0.0).intensity,
                static_cast<double>(kMoonMaximumIntensity) * std::pow(0.5, 0.6),
                1e-9);
    moon.altitudeDeg = 0.0;
    EXPECT_FLOAT_EQ(MoonShadingFor(moon, phase, 0.0).intensity, 0.0F);
    moon.altitudeDeg = -20.0;
    EXPECT_FLOAT_EQ(MoonShadingFor(moon, phase, 0.0).intensity, 0.0F);
}

TEST(MoonLightTests, NewMoonIsDarkEvenBeforeOvercastAttenuation)
{
    MoonPosition moon;
    moon.altitudeDeg = 90.0;
    MoonPhase phase;
    phase.illuminatedFraction = 0.0;
    const auto shading = MoonShadingFor(moon, phase, 1.0);
    EXPECT_FLOAT_EQ(shading.intensity, 0.0F);
    EXPECT_FLOAT_EQ(shading.color.X, 0.62F);
    EXPECT_FLOAT_EQ(shading.color.Y, 0.70F);
    EXPECT_FLOAT_EQ(shading.color.Z, 1.00F);
}

TEST(MoonLightTests, SunMustBeStrictlyBelowMinusFourDegrees)
{
    EXPECT_FALSE(MoonlightMayBeKey(-4.0));
    EXPECT_FALSE(MoonlightMayBeKey(std::nextafter(-4.0, 0.0)));
    EXPECT_TRUE(MoonlightMayBeKey(std::nextafter(-4.0, -5.0)));
    EXPECT_FALSE(MoonlightMayBeKey(std::numeric_limits<double>::quiet_NaN()));
}

TEST(MoonLightTests, CompassDirectionsUseTheProjectsWorldAxesAndLightTravelSign)
{
    MoonPosition moon;
    moon.altitudeDeg = 0.0;
    moon.azimuthDeg = 90.0;
    const auto east = DirectionToMoon(moon);
    EXPECT_NEAR(east.X, 1.0F, 1e-6F);
    EXPECT_NEAR(east.Y, 0.0F, 1e-6F);
    EXPECT_NEAR(east.Z, 0.0F, 1e-6F);
    const auto travel = MoonDirection(moon);
    EXPECT_NEAR(travel.X, -1.0F, 1e-6F);
    EXPECT_NEAR(travel.Y, 0.0F, 1e-6F);
    EXPECT_NEAR(travel.Z, 0.0F, 1e-6F);
}
