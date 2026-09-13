// SPDX-License-Identifier: MIT
//
// `HOUSE-01606`. The two-texture lunar quad through a real XNA GraphicsDevice.
#include <array>
#include <cstdint>
#include <memory>
#include <utility>

#include <gtest/gtest.h>

#include "Microsoft/Xna/Framework/Color.hpp"
#include "Microsoft/Xna/Framework/Graphics/Texture2D.hpp"

#include "cnahouse/debug/Counters.hpp"
#include "cnahouse/environment/MoonModel.hpp"
#include "cnahouse/rendering/Camera.hpp"
#include "cnahouse/rendering/MoonDiscPass.hpp"
#include "cnahouse/rendering/StateTracker.hpp"

#include "integration/DeviceHost.hpp"

namespace
{
    namespace Gfx = Microsoft::Xna::Framework::Graphics;
    namespace Xna = Microsoft::Xna::Framework;

    TEST(MoonDiscPassTests, VisibleMoonSubmitsOneAdditiveQuadAndUploadsOnlyChangedMasks)
    {
        std::int64_t draws = 0;
        std::int64_t scale = 0;
        std::int64_t uploads = 0;
        cnahouse::testsupport::DeviceHost host(
            [&](Gfx::GraphicsDevice& device)
            {
                auto albedo = std::make_unique<Gfx::Texture2D>(device, 8, 8);
                std::array<Xna::Color, 64> texels;
                texels.fill(Xna::Color::White);
                albedo->SetData(texels.data(), static_cast<int>(texels.size()));

                cnahouse::rendering::Camera camera;
                camera.eye = Xna::Vector3(0.0F, 2.0F, 0.0F);
                camera.target = Xna::Vector3(0.0F, 2.5F, -0.8660254F);
                cnahouse::rendering::MoonDiscPass pass(camera, std::move(albedo));
                cnahouse::environment::MoonPosition moon;
                moon.altitudeDeg = 30.0;
                moon.azimuthDeg = 0.0;
                cnahouse::environment::SunPosition sun;
                sun.altitudeDeg = -20.0;
                sun.azimuthDeg = 270.0;
                cnahouse::environment::MoonPhase phase;
                phase.phase = 0.25;
                pass.SetMoon(moon, phase, sun);
                ASSERT_TRUE(pass.IsActive());

                cnahouse::rendering::StateTracker states(device);
                cnahouse::debug::Counters counters;
                cnahouse::rendering::PassContext context{device, states, counters, 1.0F / 60.0F};
                pass.Draw(context);
                pass.Draw(context);

                phase.phase += cnahouse::rendering::kMoonMaskPhaseStep + 1e-8;
                pass.SetMoon(moon, phase, sun);
                pass.Draw(context);

                const auto* drawCounter = counters.Find("moon.disc.draws");
                const auto* scaleCounter = counters.Find("moon.disc.scaleMilli");
                const auto* uploadCounter = counters.Find("moon.disc.mask_uploads");
                ASSERT_NE(drawCounter, nullptr);
                ASSERT_NE(scaleCounter, nullptr);
                ASSERT_NE(uploadCounter, nullptr);
                draws = drawCounter->current;
                scale = scaleCounter->current;
                uploads = uploadCounter->current;
            });
        host.Run();

        ASSERT_TRUE(host.Ran());
        ASSERT_EQ(host.Failure(), "") << "the real device rejected the lunar dual-texture pass";
        EXPECT_EQ(draws, 1);
        EXPECT_EQ(scale, 1000);
        EXPECT_EQ(uploads, 2) << "initial upload plus the one phase change beyond 1/128";
    }

    TEST(MoonDiscPassTests, ASetMoonBelowMoonriseMakesThePassInactive)
    {
        cnahouse::rendering::Camera camera;
        cnahouse::rendering::MoonDiscPass pass(camera);
        cnahouse::environment::MoonPosition moon;
        moon.altitudeDeg = cnahouse::environment::kMoonRefractedHorizonDeg - 0.01;
        cnahouse::environment::MoonPhase phase;
        cnahouse::environment::SunPosition sun;
        pass.SetMoon(moon, phase, sun);
        EXPECT_FALSE(pass.IsActive());
    }
} // namespace
