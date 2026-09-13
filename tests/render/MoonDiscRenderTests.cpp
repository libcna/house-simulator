// SPDX-License-Identifier: MIT
//
// `HOUSE-01606`. Pixel proof for §33.3's phase mask, horizon scale and atmospheric tint.
#include <array>
#include <cstdint>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include "Microsoft/Xna/Framework/Color.hpp"
#include "Microsoft/Xna/Framework/Graphics/DepthFormat.hpp"
#include "Microsoft/Xna/Framework/Graphics/RenderTarget2D.hpp"
#include "Microsoft/Xna/Framework/Graphics/RenderTargetUsage.hpp"
#include "Microsoft/Xna/Framework/Graphics/SurfaceFormat.hpp"
#include "Microsoft/Xna/Framework/Graphics/Texture2D.hpp"

#include "cnahouse/debug/Counters.hpp"
#include "cnahouse/environment/MoonLight.hpp"
#include "cnahouse/environment/MoonModel.hpp"
#include "cnahouse/rendering/Camera.hpp"
#include "cnahouse/rendering/MoonDiscPass.hpp"
#include "cnahouse/rendering/StateTracker.hpp"

#include "integration/DeviceHost.hpp"

namespace
{
    namespace Gfx = Microsoft::Xna::Framework::Graphics;
    namespace Xna = Microsoft::Xna::Framework;

    struct MoonRaster
    {
        int litPixels = 0;
        std::int64_t leftRed = 0;
        std::int64_t rightRed = 0;
        int brightestRed = 0;
        int brightestGreen = 0;
        int brightestBlue = 0;
    };

    TEST(MoonDiscRenderTests, MaskOrientationHorizonScaleAndReddeningReachTheRenderTarget)
    {
        MoonRaster quarter;
        MoonRaster full;
        MoonRaster horizon;
        cnahouse::testsupport::DeviceHost host(
            [&](Gfx::GraphicsDevice& device)
            {
                Gfx::Texture2D albedo(device, 8, 8);
                std::array<Xna::Color, 64> texels;
                texels.fill(Xna::Color::White);
                albedo.SetData(texels.data(), static_cast<int>(texels.size()));

                cnahouse::rendering::Camera camera;
                camera.eye = Xna::Vector3(0.0F, 2.0F, 0.0F);
                camera.fieldOfViewDegrees = 4.0F;
                cnahouse::rendering::MoonDiscPass pass(camera, std::move(albedo));
                cnahouse::rendering::StateTracker states(device);
                cnahouse::debug::Counters counters;
                cnahouse::rendering::PassContext context{device, states, counters, 1.0F / 60.0F};

                cnahouse::environment::MoonPosition moon;
                moon.azimuthDeg = 0.0;
                cnahouse::environment::SunPosition sun;
                // Looking north, west is screen-left. The projected bright limb must point there.
                sun.altitudeDeg = 0.0;
                sun.azimuthDeg = 270.0;
                cnahouse::environment::MoonPhase phase;

                auto rasterise = [&](double altitudeDeg, double phaseValue)
                {
                    moon.altitudeDeg = altitudeDeg;
                    phase.phase = phaseValue;
                    const Xna::Vector3 direction = cnahouse::environment::DirectionToMoon(moon);
                    camera.target = Xna::Vector3(
                        camera.eye.X + direction.X, camera.eye.Y + direction.Y, camera.eye.Z + direction.Z);
                    pass.SetMoon(moon, phase, sun);

                    Gfx::RenderTarget2D target(device,
                                               320,
                                               240,
                                               false,
                                               Gfx::SurfaceFormat::Color,
                                               Gfx::DepthFormat::None,
                                               0,
                                               Gfx::RenderTargetUsage::PreserveContents);
                    device.SetRenderTarget(&target);
                    device.Clear(Xna::Color::Black);
                    states.Invalidate();
                    pass.Draw(context);
                    device.SetRenderTarget(nullptr);

                    std::vector<Xna::Color> pixels(320U * 240U);
                    target.GetData(pixels.data(), static_cast<int>(pixels.size()));
                    MoonRaster raster;
                    int brightest = -1;
                    for (int y = 0; y < 240; ++y)
                    {
                        for (int x = 0; x < 320; ++x)
                        {
                            const Xna::Color& pixel = pixels[static_cast<std::size_t>(y * 320 + x)];
                            const int red = pixel.getRProperty();
                            const int green = pixel.getGProperty();
                            const int blue = pixel.getBProperty();
                            raster.litPixels += red != 0 || green != 0 || blue != 0 ? 1 : 0;
                            if (x < 160)
                            {
                                raster.leftRed += red;
                            }
                            else
                            {
                                raster.rightRed += red;
                            }
                            if (red + green + blue > brightest)
                            {
                                brightest = red + green + blue;
                                raster.brightestRed = red;
                                raster.brightestGreen = green;
                                raster.brightestBlue = blue;
                            }
                        }
                    }
                    return raster;
                };

                quarter = rasterise(30.0, 0.25);
                full = rasterise(30.0, 0.5);
                horizon = rasterise(cnahouse::environment::kMoonRefractedHorizonDeg, 0.5);
            });
        host.Run();

        ASSERT_TRUE(host.Ran());
        ASSERT_EQ(host.Failure(), "") << "the renderer rejected the lunar quad";
        EXPECT_GT(quarter.litPixels, 0);
        EXPECT_GT(quarter.rightRed, 0) << "the dark hemisphere lost §33.3's earthshine";
        EXPECT_GT(quarter.leftRed, quarter.rightRed * 10)
            << "the bright limb did not rotate towards the projected western sun";
        EXPECT_GT(full.leftRed + full.rightRed, (quarter.leftRed + quarter.rightRed) * 3 / 2)
            << "the generated phase mask did not alter rendered illumination";
        EXPECT_GT(horizon.litPixels, full.litPixels * 4)
            << "the shared 2.6x horizon diameter did not materially enlarge the moon";
        EXPECT_GT(horizon.brightestRed, horizon.brightestGreen);
        EXPECT_GT(horizon.brightestGreen, horizon.brightestBlue)
            << "the horizon moon did not receive the shared red-orange atmospheric tint";
    }
} // namespace
