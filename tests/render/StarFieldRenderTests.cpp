// SPDX-License-Identifier: MIT
//
// `HOUSE-01617`. Pixel proof for §34's clear/overcast night and three-point twilight ramp.
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include "Microsoft/Xna/Framework/Color.hpp"
#include "Microsoft/Xna/Framework/Graphics/DepthFormat.hpp"
#include "Microsoft/Xna/Framework/Graphics/RenderTarget2D.hpp"
#include "Microsoft/Xna/Framework/Graphics/RenderTargetUsage.hpp"
#include "Microsoft/Xna/Framework/Graphics/SurfaceFormat.hpp"

#include "System/IO/FileStream.hpp"

#include "cnahouse/debug/Counters.hpp"
#include "cnahouse/environment/MoonModel.hpp"
#include "cnahouse/environment/SimClock.hpp"
#include "cnahouse/environment/SunModel.hpp"
#include "cnahouse/rendering/Camera.hpp"
#include "cnahouse/rendering/StarField.hpp"
#include "cnahouse/rendering/StateTracker.hpp"

#include "integration/DeviceHost.hpp"

namespace
{
    namespace Gfx = Microsoft::Xna::Framework::Graphics;
    namespace Xna = Microsoft::Xna::Framework;

    struct StarRaster
    {
        std::size_t submittedStars = 0;
        std::size_t litPixels = 0;
        std::int64_t totalLight = 0;
    };

    TEST(StarFieldRenderTests, ClearAndOvercastNightsShowTheThreeTwilightStages)
    {
        StarRaster daylightEdge;
        StarRaster halfTwilight;
        StarRaster clearNight;
        StarRaster overcastNight;
        cnahouse::testsupport::DeviceHost host(
            [&](Gfx::GraphicsDevice& device)
            {
                System::IO::FileStream stream(std::string(CNAHOUSE_TEST_STAR_CATALOGUE_FIXTURE),
                                              System::IO::FileMode::Open,
                                              System::IO::FileAccess::Read);
                auto catalogue = cnahouse::rendering::StarCatalogueReader::Read(
                    stream, CNAHOUSE_TEST_STAR_CATALOGUE_FIXTURE);
                ASSERT_TRUE(catalogue) << (catalogue ? std::string() : catalogue.Error().ToString());

                cnahouse::rendering::Camera camera;
                camera.fieldOfViewDegrees = 30.0F;
                cnahouse::rendering::StarField field(camera, std::move(*catalogue));
                cnahouse::environment::SimClock clock;
                cnahouse::environment::SunPosition sun;
                cnahouse::environment::MoonPosition moon;
                moon.altitudeDeg = -10.0;
                cnahouse::environment::MoonPhase phase;
                phase.illuminatedFraction = 1.0;
                cnahouse::rendering::StateTracker states(device);
                cnahouse::debug::Counters counters;
                cnahouse::rendering::PassContext context{device, states, counters, 0.0F};

                auto rasterise = [&](double sunAltitudeDeg, double cloudCover)
                {
                    sun.altitudeDeg = sunAltitudeDeg;
                    static_cast<void>(field.SetCelestial(clock, sun, moon, phase, cloudCover));

                    Gfx::RenderTarget2D target(device,
                                               640,
                                               480,
                                               false,
                                               Gfx::SurfaceFormat::Color,
                                               Gfx::DepthFormat::None,
                                               0,
                                               Gfx::RenderTargetUsage::PreserveContents);
                    device.SetRenderTarget(&target);
                    device.Clear(Xna::Color::Black);
                    states.Invalidate();
                    field.Draw(context);
                    device.SetRenderTarget(nullptr);

                    std::vector<Xna::Color> pixels(640U * 480U);
                    target.GetData(pixels.data(), static_cast<int>(pixels.size()));
                    StarRaster result;
                    result.submittedStars = field.VisibleStarCount();
                    for (const Xna::Color& pixel : pixels)
                    {
                        const int light = static_cast<int>(pixel.getRProperty()) +
                                          static_cast<int>(pixel.getGProperty()) +
                                          static_cast<int>(pixel.getBProperty());
                        result.litPixels += light > 0 ? 1U : 0U;
                        result.totalLight += light;
                    }
                    return result;
                };

                daylightEdge = rasterise(-4.0, 0.0);
                halfTwilight = rasterise(-9.0, 0.0);
                clearNight = rasterise(-14.0, 0.0);
                overcastNight = rasterise(-14.0, 1.0);
            });
        host.Run();

        ASSERT_TRUE(host.Ran());
        ASSERT_EQ(host.Failure(), "") << "the renderer rejected a twilight star field";
        EXPECT_EQ(daylightEdge.submittedStars, 0U);
        EXPECT_EQ(daylightEdge.litPixels, 0U);
        EXPECT_GT(halfTwilight.submittedStars, 0U);
        EXPECT_GT(halfTwilight.litPixels, 0U);
        EXPECT_GT(clearNight.submittedStars, halfTwilight.submittedStars);
        EXPECT_GT(clearNight.litPixels, halfTwilight.litPixels);
        EXPECT_GT(clearNight.totalLight, halfTwilight.totalLight);
        EXPECT_EQ(overcastNight.submittedStars, 0U);
        EXPECT_EQ(overcastNight.litPixels, 0U);
        EXPECT_GT(clearNight.litPixels, overcastNight.litPixels);
    }
} // namespace
