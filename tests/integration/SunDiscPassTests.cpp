// SPDX-License-Identifier: MIT
//
// `HOUSE-01566`. The quad goes through a real XNA GraphicsDevice under the HEADLESS renderer.
#include <gtest/gtest.h>

#include "cnahouse/debug/Counters.hpp"
#include "cnahouse/environment/SunModel.hpp"
#include "cnahouse/rendering/Camera.hpp"
#include "cnahouse/rendering/StateTracker.hpp"
#include "cnahouse/rendering/SunDiscPass.hpp"

#include "integration/DeviceHost.hpp"

namespace
{
    namespace Gfx = Microsoft::Xna::Framework::Graphics;

    TEST(SunDiscPassTests, AVisibleSunSubmitsOneAdditiveQuadAndReusesItsResources)
    {
        std::int64_t draws = 0;
        std::int64_t scale = 0;
        cnahouse::testsupport::DeviceHost host(
            [&](Gfx::GraphicsDevice& device)
            {
                cnahouse::rendering::Camera camera;
                camera.eye = Microsoft::Xna::Framework::Vector3(0.0F, 2.0F, 0.0F);
                camera.target = Microsoft::Xna::Framework::Vector3(1.0F, 2.0F, 0.0F);
                cnahouse::rendering::SunDiscPass pass(camera);
                cnahouse::environment::SunPosition sun;
                sun.altitudeDeg = 0.0;
                sun.azimuthDeg = 90.0;
                pass.SetSun(sun, 0.0);
                ASSERT_TRUE(pass.IsActive());

                cnahouse::rendering::StateTracker states(device);
                cnahouse::debug::Counters counters;
                cnahouse::rendering::PassContext context{device, states, counters, 1.0F / 60.0F};
                pass.Draw(context);
                // A second draw proves that resources retained texture/buffer/effect ownership and
                // were not transient objects dangling after the first submission.
                pass.Draw(context);

                const auto* drawCounter = counters.Find("sun.disc.draws");
                const auto* scaleCounter = counters.Find("sun.disc.scaleMilli");
                ASSERT_NE(drawCounter, nullptr);
                ASSERT_NE(scaleCounter, nullptr);
                draws = drawCounter->current;
                scale = scaleCounter->current;
            });
        host.Run();

        ASSERT_TRUE(host.Ran());
        ASSERT_EQ(host.Failure(), "") << "the real device rejected the sun pass";
        EXPECT_EQ(draws, 1) << "the counter is per frame, not cumulative per submission";
        EXPECT_EQ(scale, 2600);
    }

    TEST(SunDiscPassTests, ASetSunBelowSunriseMakesThePassInactive)
    {
        cnahouse::rendering::Camera camera;
        cnahouse::rendering::SunDiscPass pass(camera);
        cnahouse::environment::SunPosition sun;
        sun.altitudeDeg = cnahouse::environment::kRefractedHorizonDeg - 0.01;
        pass.SetSun(sun, 0.0);
        EXPECT_FALSE(pass.IsActive());
    }
} // namespace
