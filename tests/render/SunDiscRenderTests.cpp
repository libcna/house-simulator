// SPDX-License-Identifier: MIT
//
// `HOUSE-01566`. Pixel proof for §32.3's visible size and red-orange horizon tint.
#include <array>
#include <cstdio>
#include <vector>

#include <gtest/gtest.h>

#include "Microsoft/Xna/Framework/Color.hpp"
#include "Microsoft/Xna/Framework/Graphics/DepthFormat.hpp"
#include "Microsoft/Xna/Framework/Graphics/RenderTarget2D.hpp"
#include "Microsoft/Xna/Framework/Graphics/RenderTargetUsage.hpp"
#include "Microsoft/Xna/Framework/Graphics/SurfaceFormat.hpp"

#include "cnahouse/debug/Counters.hpp"
#include "cnahouse/environment/SunLight.hpp"
#include "cnahouse/environment/SunModel.hpp"
#include "cnahouse/rendering/Camera.hpp"
#include "cnahouse/rendering/StateTracker.hpp"
#include "cnahouse/rendering/SunDiscPass.hpp"

#include "integration/DeviceHost.hpp"

namespace
{
    namespace Gfx = Microsoft::Xna::Framework::Graphics;

    TEST(SunDiscRenderTests, HorizonScalingAndReddeningReachTheRenderTarget)
    {
        std::array<int, 4> noonRaster{};
        std::array<int, 4> horizonRaster{};
        cnahouse::testsupport::DeviceHost host(
            [&](Gfx::GraphicsDevice& device)
            {
                cnahouse::rendering::Camera camera;
                camera.eye = Microsoft::Xna::Framework::Vector3(0.0F, 2.0F, 0.0F);
                cnahouse::rendering::SunDiscPass pass(camera);
                cnahouse::rendering::StateTracker states(device);
                cnahouse::debug::Counters counters;
                cnahouse::rendering::PassContext context{device, states, counters, 1.0F / 60.0F};
                cnahouse::environment::SunPosition sun;
                sun.azimuthDeg = 90.0;

                auto rasterise = [&](double altitudeDeg)
                {
                    sun.altitudeDeg = altitudeDeg;
                    const auto direction = cnahouse::environment::DirectionToSun(sun);
                    camera.target = Microsoft::Xna::Framework::Vector3(
                        camera.eye.X + direction.X, camera.eye.Y + direction.Y, camera.eye.Z + direction.Z);
                    pass.SetSun(sun, 0.0);
                    Gfx::RenderTarget2D target(device,
                                               320,
                                               240,
                                               false,
                                               Gfx::SurfaceFormat::Color,
                                               Gfx::DepthFormat::None,
                                               0,
                                               Gfx::RenderTargetUsage::PreserveContents);
                    device.SetRenderTarget(&target);
                    device.Clear(Microsoft::Xna::Framework::Color::Black);
                    states.Invalidate();
                    pass.Draw(context);
                    device.SetRenderTarget(nullptr);

                    std::vector<Microsoft::Xna::Framework::Color> pixels(320U * 240U);
                    target.GetData(pixels.data(), static_cast<int>(pixels.size()));
                    std::array<int, 4> raster{}; // lit pixels, then brightest pixel R/G/B
                    int brightest = -1;
                    for (const auto& pixel : pixels)
                    {
                        const int red = pixel.getRProperty();
                        const int green = pixel.getGProperty();
                        const int blue = pixel.getBProperty();
                        raster[0] += red != 0 || green != 0 || blue != 0 ? 1 : 0;
                        if (red + green + blue > brightest)
                        {
                            brightest = red + green + blue;
                            raster[1] = red;
                            raster[2] = green;
                            raster[3] = blue;
                        }
                    }
                    return raster;
                };

                noonRaster = rasterise(30.0);
                horizonRaster = rasterise(0.0);
            });
        host.Run();

        ASSERT_TRUE(host.Ran());
        ASSERT_EQ(host.Failure(), "") << "the renderer rejected the sun quad";
        EXPECT_GT(noonRaster[0], 0) << "the submitted quad changed no render-target pixel";
        EXPECT_GT(horizonRaster[0], noonRaster[0] * 4)
            << "the 2.6x horizon diameter did not materially enlarge the rasterised disc";
        EXPECT_GT(horizonRaster[1], horizonRaster[2]);
        EXPECT_GT(horizonRaster[2], horizonRaster[3])
            << "the horizon pixels did not receive §32.2's red-orange tint";
        std::printf("  sun disc: noon %d px RGB(%d,%d,%d), horizon %d px RGB(%d,%d,%d)\n",
                    noonRaster[0],
                    noonRaster[1],
                    noonRaster[2],
                    noonRaster[3],
                    horizonRaster[0],
                    horizonRaster[1],
                    horizonRaster[2],
                    horizonRaster[3]);
    }
} // namespace
