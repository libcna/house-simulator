// SPDX-License-Identifier: MIT
//
// `HOUSE-01615`. Real-device proof for the one-draw additive satellite/meteor stream.
#include <cstdint>

#include <gtest/gtest.h>

#include "Microsoft/Xna/Framework/Graphics/BlendState.hpp"

#include "cnahouse/debug/Counters.hpp"
#include "cnahouse/environment/SimClock.hpp"
#include "cnahouse/environment/SunModel.hpp"
#include "cnahouse/rendering/Camera.hpp"
#include "cnahouse/rendering/SkyTransientPass.hpp"
#include "cnahouse/rendering/StateTracker.hpp"

#include "integration/DeviceHost.hpp"

namespace
{
    namespace Gfx = Microsoft::Xna::Framework::Graphics;

    TEST(SkyTransientPassTests, TwoSatellitesAndMeteorStreamAsOneAdditiveDraw)
    {
        std::int64_t nightDraws = -1;
        std::int64_t nightTriangles = -1;
        std::int64_t visibleSatellites = -1;
        std::int64_t visibleMeteors = -1;
        std::int64_t dayDraws = -1;
        std::int64_t uploads = -1;
        cnahouse::testsupport::DeviceHost host(
            [&](Gfx::GraphicsDevice& device)
            {
                cnahouse::rendering::Camera camera;
                cnahouse::rendering::SkyTransientPass pass(camera);
                cnahouse::environment::SimClock clock;
                clock.epochSeconds = cnahouse::rendering::kMeteorDurationSimSeconds * 0.5;
                cnahouse::environment::SunPosition sun;
                sun.altitudeDeg = -18.0;
                ASSERT_TRUE(pass.SetCelestial(clock, sun, 0.0));

                cnahouse::rendering::StateTracker states(device);
                cnahouse::debug::Counters counters;
                cnahouse::rendering::PassContext context{device, states, counters, 1.0F / 60.0F};
                pass.Draw(context);

                EXPECT_EQ(device.getBlendStateProperty().getColorSourceBlendProperty(),
                          Gfx::BlendState::Additive.getColorSourceBlendProperty());
                EXPECT_EQ(device.getBlendStateProperty().getColorDestinationBlendProperty(),
                          Gfx::BlendState::Additive.getColorDestinationBlendProperty());
                EXPECT_FALSE(device.getDepthStencilStateProperty().getDepthBufferEnableProperty());
                nightDraws = counters.Find("sky.transients.draws")->current;
                nightTriangles = counters.Find("sky.transients.triangles")->current;
                visibleSatellites = counters.Find("sky.transients.satellites")->current;
                visibleMeteors = counters.Find("sky.transients.meteors")->current;

                sun.altitudeDeg = 0.0;
                ASSERT_TRUE(pass.SetCelestial(clock, sun, 0.0));
                pass.Draw(context);
                dayDraws = counters.Find("sky.transients.draws")->current;
                uploads = counters.Find("sky.transients.uploads")->current;
            });
        host.Run();

        ASSERT_TRUE(host.Ran());
        ASSERT_EQ(host.Failure(), "") << host.Failure();
        EXPECT_EQ(nightDraws, 1);
        EXPECT_EQ(nightTriangles, 6);
        EXPECT_EQ(visibleSatellites, 2);
        EXPECT_EQ(visibleMeteors, 1);
        EXPECT_EQ(dayDraws, 0);
        EXPECT_EQ(uploads, 1) << "the hidden daytime frame must not stream another buffer";
    }
} // namespace
