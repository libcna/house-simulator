// SPDX-License-Identifier: MIT
//
// `HOUSE-01606`. Device-independent geometry and bright-limb orientation for §33.3.
#include <cmath>
#include <limits>
#include <numbers>

#include <gtest/gtest.h>

#include "cnahouse/environment/MoonLight.hpp"
#include "cnahouse/rendering/Camera.hpp"
#include "cnahouse/rendering/MoonDiscPass.hpp"
#include "cnahouse/rendering/SunDiscPass.hpp"

namespace
{
    using cnahouse::environment::MoonPhase;
    using cnahouse::environment::MoonPosition;
    using cnahouse::environment::SunPosition;
    using cnahouse::rendering::BuildMoonDiscFrame;
    using cnahouse::rendering::Camera;
    using cnahouse::rendering::MoonBrightLimbAngle;

    MoonPosition MoonAt(double altitudeDeg, double azimuthDeg)
    {
        MoonPosition moon;
        moon.altitudeDeg = altitudeDeg;
        moon.azimuthDeg = azimuthDeg;
        return moon;
    }

    SunPosition SunAt(double altitudeDeg, double azimuthDeg)
    {
        SunPosition sun;
        sun.altitudeDeg = altitudeDeg;
        sun.azimuthDeg = azimuthDeg;
        return sun;
    }
} // namespace

TEST(MoonDiscTests, TheQuadIsEightHundredNinetyUnitsTowardTheMoonAtItsAngularSize)
{
    Camera camera;
    camera.eye = Microsoft::Xna::Framework::Vector3(7.0F, 2.0F, -11.0F);
    const auto frame = BuildMoonDiscFrame(camera, MoonAt(60.0, 90.0), SunAt(-20.0, 270.0));

    EXPECT_NEAR(frame.centre.X, 452.0F, 1e-3F);
    EXPECT_NEAR(frame.centre.Y, 772.7627F, 1e-3F);
    EXPECT_NEAR(frame.centre.Z, -11.0F, 1e-3F);
    const float dx = frame.centre.X - camera.eye.X;
    const float dy = frame.centre.Y - camera.eye.Y;
    const float dz = frame.centre.Z - camera.eye.Z;
    EXPECT_NEAR(std::sqrt(dx * dx + dy * dy + dz * dz), 890.0F, 1e-3F);

    const float kExpectedRadius = 890.0F * std::tan(0.52F * 0.5F * std::numbers::pi_v<float> / 180.0F);
    EXPECT_NEAR(frame.radius, kExpectedRadius, 1e-5F);
    EXPECT_FLOAT_EQ(frame.horizonScale, 1.0F);
}

TEST(MoonDiscTests, HorizonScalingAndReddeningMatchTheSunTreatment)
{
    const Camera camera;
    const auto horizon = BuildMoonDiscFrame(
        camera, MoonAt(cnahouse::environment::kMoonRefractedHorizonDeg, 0.0), SunAt(-20.0, 180.0));
    const auto high = BuildMoonDiscFrame(camera, MoonAt(60.0, 0.0), SunAt(-20.0, 180.0));

    EXPECT_TRUE(horizon.visible);
    EXPECT_TRUE(high.visible);
    const float expectedScale =
        cnahouse::rendering::SunDiscHorizonScale(cnahouse::environment::kMoonRefractedHorizonDeg);
    EXPECT_NEAR(horizon.radius / high.radius, expectedScale, 1e-5F);
    EXPECT_FLOAT_EQ(horizon.horizonScale, expectedScale);
    EXPECT_FLOAT_EQ(high.horizonScale, 1.0F);
    EXPECT_GT(horizon.tint.X, horizon.tint.Y);
    EXPECT_GT(horizon.tint.Y, horizon.tint.Z);
    EXPECT_NEAR(high.tint.X, high.tint.Y, 0.02F);
}

TEST(MoonDiscTests, TheDiscAppearsAtConventionalMoonriseAndNotBelowIt)
{
    const Camera camera;
    const SunPosition sun = SunAt(-20.0, 180.0);
    EXPECT_TRUE(BuildMoonDiscFrame(camera, MoonAt(cnahouse::environment::kMoonRefractedHorizonDeg, 90.0), sun)
                    .visible);
    EXPECT_FALSE(
        BuildMoonDiscFrame(camera, MoonAt(cnahouse::environment::kMoonRefractedHorizonDeg - 0.001, 90.0), sun)
            .visible);
}

TEST(MoonDiscTests, BrightLimbAngleProjectsTheSunIntoTheBillboardPlane)
{
    const MoonPosition northHorizon = MoonAt(0.0, 0.0);
    EXPECT_NEAR(MoonBrightLimbAngle(northHorizon, SunAt(0.0, 270.0)), 0.0, 1e-9)
        << "a western sun lies on the moon quad's +U axis";
    EXPECT_NEAR(MoonBrightLimbAngle(northHorizon, SunAt(90.0, 0.0)), std::numbers::pi / 2.0, 1e-6)
        << "a zenith sun lies on the moon quad's +V axis";

    MoonPosition invalid = northHorizon;
    invalid.altitudeDeg = std::numeric_limits<double>::quiet_NaN();
    EXPECT_DOUBLE_EQ(MoonBrightLimbAngle(invalid, SunAt(0.0, 270.0)), 0.0);
}

TEST(MoonDiscTests, PassRegeneratesItsMaskOnlyBeyondTheOneTexelPhaseThreshold)
{
    Camera camera;
    cnahouse::rendering::MoonDiscPass pass(camera);
    const MoonPosition moon = MoonAt(30.0, 0.0);
    const SunPosition sun = SunAt(-20.0, 270.0);
    MoonPhase phase;
    phase.phase = 0.25;

    pass.SetMoon(moon, phase, sun);
    EXPECT_EQ(pass.MaskGenerationCount(), 1U);
    phase.phase += cnahouse::rendering::kMoonMaskPhaseStep;
    pass.SetMoon(moon, phase, sun);
    EXPECT_EQ(pass.MaskGenerationCount(), 1U);
    phase.phase += 1e-8;
    pass.SetMoon(moon, phase, sun);
    EXPECT_EQ(pass.MaskGenerationCount(), 2U);
}
