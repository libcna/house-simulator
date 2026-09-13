// SPDX-License-Identifier: MIT
//
// `HOUSE-01606`, `HOUSE-01617`. Pixel proof for §33.3's phase mask, all eight presentation
// phases, horizon scale and atmospheric tint.
#include <array>
#include <cstdint>
#include <string_view>
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
        std::int64_t totalRed = 0;
        int brightestRed = 0;
        int brightestGreen = 0;
        int brightestBlue = 0;
    };

    MoonRaster RasteriseMoon(Gfx::GraphicsDevice& device,
                             cnahouse::rendering::MoonDiscPass& pass,
                             cnahouse::rendering::Camera& camera,
                             cnahouse::rendering::StateTracker& states,
                             cnahouse::rendering::PassContext& context,
                             cnahouse::environment::MoonPosition& moon,
                             const cnahouse::environment::SunPosition& sun,
                             double altitudeDeg,
                             double phaseValue)
    {
        moon.altitudeDeg = altitudeDeg;
        cnahouse::environment::MoonPhase phase;
        phase.phase = phaseValue;
        const Xna::Vector3 direction = cnahouse::environment::DirectionToMoon(moon);
        camera.target =
            Xna::Vector3(camera.eye.X + direction.X, camera.eye.Y + direction.Y, camera.eye.Z + direction.Z);
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
                raster.totalRed += red;
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
    }

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
                cnahouse::rendering::MoonDiscPass pass(camera, &albedo);
                cnahouse::rendering::StateTracker states(device);
                cnahouse::debug::Counters counters;
                cnahouse::rendering::PassContext context{device, states, counters, 1.0F / 60.0F};

                cnahouse::environment::MoonPosition moon;
                moon.azimuthDeg = 0.0;
                cnahouse::environment::SunPosition sun;
                // Looking north, west is screen-left. The projected bright limb must point there.
                sun.altitudeDeg = 0.0;
                sun.azimuthDeg = 270.0;
                quarter = RasteriseMoon(device, pass, camera, states, context, moon, sun, 30.0, 0.25);
                full = RasteriseMoon(device, pass, camera, states, context, moon, sun, 30.0, 0.5);
                horizon = RasteriseMoon(device,
                                        pass,
                                        camera,
                                        states,
                                        context,
                                        moon,
                                        sun,
                                        cnahouse::environment::kMoonRefractedHorizonDeg,
                                        0.5);
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

    TEST(MoonDiscRenderTests, AllEightNamedPhasesReachTheRenderTargetInContinuousOrder)
    {
        struct NamedPhase
        {
            double phase;
            std::string_view name;
        };

        constexpr std::array<NamedPhase, 8> kPhases{{
            {0.000, "New moon"},
            {0.125, "Waxing crescent"},
            {0.250, "First quarter"},
            {0.375, "Waxing gibbous"},
            {0.500, "Full moon"},
            {0.625, "Waning gibbous"},
            {0.750, "Last quarter"},
            {0.875, "Waning crescent"},
        }};
        std::array<MoonRaster, kPhases.size()> rasters;

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
                cnahouse::rendering::MoonDiscPass pass(camera, &albedo);
                cnahouse::rendering::StateTracker states(device);
                cnahouse::debug::Counters counters;
                cnahouse::rendering::PassContext context{device, states, counters, 0.0F};
                cnahouse::environment::MoonPosition moon;
                moon.azimuthDeg = 0.0;
                cnahouse::environment::SunPosition sun;
                // West is screen-left, making waxing and waning terminators distinguishable.
                sun.altitudeDeg = 0.0;
                sun.azimuthDeg = 270.0;

                for (std::size_t index = 0; index < kPhases.size(); ++index)
                {
                    EXPECT_EQ(cnahouse::environment::MoonPhaseName(kPhases[index].phase),
                              kPhases[index].name);
                    rasters[index] = RasteriseMoon(
                        device, pass, camera, states, context, moon, sun, 30.0, kPhases[index].phase);
                }
            });
        host.Run();

        ASSERT_TRUE(host.Ran());
        ASSERT_EQ(host.Failure(), "") << "the renderer rejected one of the eight lunar phases";
        for (std::size_t index = 0; index < rasters.size(); ++index)
        {
            EXPECT_GT(rasters[index].litPixels, 0) << kPhases[index].name;
        }
        for (std::size_t index = 1; index <= 4; ++index)
        {
            EXPECT_GT(rasters[index].totalRed, rasters[index - 1].totalRed)
                << kPhases[index].name << " did not brighten continuously toward full moon";
        }
        for (std::size_t index = 5; index < rasters.size(); ++index)
        {
            EXPECT_LT(rasters[index].totalRed, rasters[index - 1].totalRed)
                << kPhases[index].name << " did not darken continuously toward new moon";
        }
        for (std::size_t index : {1U, 2U, 3U})
        {
            EXPECT_GT(rasters[index].leftRed, rasters[index].rightRed) << kPhases[index].name;
        }
        for (std::size_t index : {5U, 6U, 7U})
        {
            EXPECT_GT(rasters[index].rightRed, rasters[index].leftRed) << kPhases[index].name;
        }
    }
} // namespace
