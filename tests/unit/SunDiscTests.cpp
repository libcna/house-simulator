// SPDX-License-Identifier: MIT
//
// `HOUSE-01566`. §32.3's sun disc geometry and appearance without a graphics device.
#include <cmath>

#include <gtest/gtest.h>

#include "cnahouse/environment/SunLight.hpp"
#include "cnahouse/environment/SunModel.hpp"
#include "cnahouse/rendering/Camera.hpp"
#include "cnahouse/rendering/SunDiscPass.hpp"

namespace
{
    using cnahouse::environment::SunPosition;
    using cnahouse::rendering::BuildSunDiscFrame;
    using cnahouse::rendering::Camera;
    using cnahouse::rendering::SunDiscHorizonScale;
    using cnahouse::rendering::SunDiscRadialOpacity;

    SunPosition At(double altitudeDeg, double azimuthDeg)
    {
        SunPosition sun;
        sun.altitudeDeg = altitudeDeg;
        sun.azimuthDeg = azimuthDeg;
        return sun;
    }
} // namespace

TEST(SunDiscTests, HorizonMagnificationEasesFromTwoPointSixToTrueSize)
{
    EXPECT_FLOAT_EQ(SunDiscHorizonScale(-1.0), 2.6F);
    EXPECT_FLOAT_EQ(SunDiscHorizonScale(0.0), 2.6F);
    EXPECT_FLOAT_EQ(SunDiscHorizonScale(10.0), 1.0F);
    EXPECT_FLOAT_EQ(SunDiscHorizonScale(40.0), 1.0F);

    float previous = SunDiscHorizonScale(0.0);
    for (double altitude = 0.1; altitude <= 10.0; altitude += 0.1)
    {
        const float scale = SunDiscHorizonScale(altitude);
        EXPECT_LE(scale, previous) << "altitude " << altitude;
        EXPECT_GE(scale, 1.0F);
        previous = scale;
    }
}

TEST(SunDiscTests, TheQuadIsEightHundredNinetyUnitsTowardTheSunAtItsAngularSize)
{
    Camera camera;
    camera.eye = Microsoft::Xna::Framework::Vector3(7.0F, 2.0F, -11.0F);
    const auto frame = BuildSunDiscFrame(camera, At(60.0, 90.0), 0.0);

    // East is +X (§10.1), and §32.3 places the centre exactly 890 units from the eye.
    EXPECT_NEAR(frame.centre.X, 452.0F, 1e-3F);
    EXPECT_NEAR(frame.centre.Y, 772.7627F, 1e-3F);
    EXPECT_NEAR(frame.centre.Z, -11.0F, 1e-3F);
    const float dx = frame.centre.X - camera.eye.X;
    const float dy = frame.centre.Y - camera.eye.Y;
    const float dz = frame.centre.Z - camera.eye.Z;
    EXPECT_NEAR(std::sqrt(dx * dx + dy * dy + dz * dz), 890.0F, 1e-3F);

    // radius = distance * tan(0.53 degrees / 2), at true size above 10 degrees.
    EXPECT_NEAR(frame.radius, 4.1168F, 1e-3F);
    EXPECT_FLOAT_EQ(frame.horizonScale, 1.0F);
}

TEST(SunDiscTests, TheHorizonDiscIsLargerRedderAndDimmerThanTheNoonDisc)
{
    const Camera camera;
    const auto horizon = BuildSunDiscFrame(camera, At(-0.83, 90.0), 0.0);
    const auto noon = BuildSunDiscFrame(camera, At(60.0, 180.0), 0.0);

    EXPECT_TRUE(horizon.visible);
    EXPECT_TRUE(noon.visible);
    EXPECT_NEAR(horizon.radius / noon.radius, 2.6F, 1e-5F);
    EXPECT_GT(horizon.tint.X, horizon.tint.Y);
    EXPECT_GT(horizon.tint.Y, horizon.tint.Z) << "the horizon uses §32.2's orange LUT row";
    EXPECT_NEAR(noon.tint.X, noon.tint.Y, 0.02F);
    EXPECT_LT(horizon.opacity, noon.opacity);
    EXPECT_NEAR(horizon.opacity, 0.10F, 0.01F);
    EXPECT_NEAR(noon.opacity, 1.0F, 1e-6F);
}

TEST(SunDiscTests, CloudsAttenuateTheDiscOnceThroughTheDirectTerm)
{
    const Camera camera;
    const auto clear = BuildSunDiscFrame(camera, At(30.0, 180.0), 0.0);
    const auto overcast = BuildSunDiscFrame(camera, At(30.0, 180.0), 1.0);

    EXPECT_FLOAT_EQ(clear.tint.X, overcast.tint.X);
    EXPECT_FLOAT_EQ(clear.tint.Y, overcast.tint.Y);
    EXPECT_FLOAT_EQ(clear.tint.Z, overcast.tint.Z);
    EXPECT_NEAR(overcast.opacity / clear.opacity, 0.15F, 1e-6F);
}

TEST(SunDiscTests, TheDiscAppearsAtRefractedSunriseAndNotBelowIt)
{
    const Camera camera;
    EXPECT_TRUE(
        BuildSunDiscFrame(camera, At(cnahouse::environment::kRefractedHorizonDeg, 90.0), 0.0).visible);
    EXPECT_FALSE(BuildSunDiscFrame(camera, At(cnahouse::environment::kRefractedHorizonDeg - 0.001, 90.0), 0.0)
                     .visible);
}

TEST(SunDiscTests, TheProceduralTextureHasASolidCentreAndASoftLimb)
{
    EXPECT_FLOAT_EQ(SunDiscRadialOpacity(0.0F), 1.0F);
    EXPECT_FLOAT_EQ(SunDiscRadialOpacity(0.72F), 1.0F);
    EXPECT_GT(SunDiscRadialOpacity(0.80F), SunDiscRadialOpacity(0.90F));
    EXPECT_GT(SunDiscRadialOpacity(0.90F), SunDiscRadialOpacity(0.99F));
    EXPECT_FLOAT_EQ(SunDiscRadialOpacity(1.0F), 0.0F);
    EXPECT_FLOAT_EQ(SunDiscRadialOpacity(1.5F), 0.0F);
}
