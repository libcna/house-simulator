// SPDX-License-Identifier: MIT
//
// `HOUSE-01643`. The dome crosses a live XNA GraphicsDevice under the HEADLESS renderer. This is
// still launched with SDL's offscreen driver by the local verification command.
#include <cstdint>
#include <string>

#include <gtest/gtest.h>

#include "System/IO/FileAccess.hpp"
#include "System/IO/FileMode.hpp"
#include "System/IO/FileStream.hpp"

#include "Microsoft/Xna/Framework/Graphics/BlendState.hpp"
#include "Microsoft/Xna/Framework/Graphics/DepthStencilState.hpp"
#include "Microsoft/Xna/Framework/Graphics/GraphicsDevice.hpp"

#include "cnahouse/debug/Counters.hpp"
#include "cnahouse/environment/SunModel.hpp"
#include "cnahouse/rendering/Camera.hpp"
#include "cnahouse/rendering/RenderStates.hpp"
#include "cnahouse/rendering/SkySystem.hpp"
#include "cnahouse/rendering/StateTracker.hpp"

#include "integration/DeviceHost.hpp"

namespace
{
    namespace Gfx = Microsoft::Xna::Framework::Graphics;

    TEST(SkySystemPassTests, TheDomeSubmitsItsExactTriangleListAndReusesResources)
    {
        System::IO::FileStream stream(
            CNAHOUSE_TEST_SKY_DOME_FIXTURE, System::IO::FileMode::Open, System::IO::FileAccess::Read);
        auto mesh = cnahouse::rendering::SkyDomeReader::Read(stream, CNAHOUSE_TEST_SKY_DOME_FIXTURE);
        ASSERT_TRUE(mesh) << (mesh ? std::string() : mesh.Error().ToString());

        std::int64_t draws = 0;
        std::int64_t triangles = 0;
        cnahouse::testsupport::DeviceHost host(
            [&](Gfx::GraphicsDevice& device)
            {
                cnahouse::rendering::Camera camera;
                camera.eye = Microsoft::Xna::Framework::Vector3(20.0F, 4.0F, -30.0F);
                cnahouse::rendering::SkySystem sky(camera, std::move(*mesh));
                cnahouse::environment::SunPosition sun;
                sun.altitudeDeg = cnahouse::environment::kRefractedHorizonDeg - 1.0;
                sky.SetSun(sun, 0.0);

                cnahouse::rendering::StateTracker states(device);
                cnahouse::debug::Counters counters;
                cnahouse::rendering::PassContext context{device, states, counters, 1.0F / 60.0F};
                sky.Draw(context);
                camera.eye = Microsoft::Xna::Framework::Vector3(-50.0F, 9.0F, 120.0F);
                sky.Draw(context);

                const Gfx::BlendState& blend = device.getBlendStateProperty();
                EXPECT_EQ(blend.getColorSourceBlendProperty(),
                          Gfx::BlendState::Opaque.getColorSourceBlendProperty());
                EXPECT_EQ(blend.getColorDestinationBlendProperty(),
                          Gfx::BlendState::Opaque.getColorDestinationBlendProperty());
                const Gfx::DepthStencilState& depth = device.getDepthStencilStateProperty();
                EXPECT_FALSE(depth.getDepthBufferEnableProperty());
                EXPECT_FALSE(depth.getDepthBufferWriteEnableProperty());
                EXPECT_EQ(device.getRasterizerStateProperty().getCullModeProperty(),
                          cnahouse::rendering::StateFor(cnahouse::rendering::CullPolicy::TwoSided)
                              .getCullModeProperty());
                EXPECT_EQ(states.Current().blendApplied, 1u);
                EXPECT_EQ(states.Current().depthApplied, 1u);
                EXPECT_EQ(states.Current().rasterApplied, 1u);
                EXPECT_EQ(states.Current().blendSkipped, 1u);
                EXPECT_EQ(states.Current().depthSkipped, 1u);
                EXPECT_EQ(states.Current().rasterSkipped, 1u);

                const auto* drawCounter = counters.Find("sky.dome.draws");
                const auto* triangleCounter = counters.Find("sky.dome.triangles");
                ASSERT_NE(drawCounter, nullptr);
                ASSERT_NE(triangleCounter, nullptr);
                draws = drawCounter->current;
                triangles = triangleCounter->current;
            });
        host.Run();

        ASSERT_TRUE(host.Ran());
        ASSERT_EQ(host.Failure(), "") << "the real device rejected the sky dome pass";
        EXPECT_EQ(draws, 1) << "the counter describes this frame, not lifetime submissions";
        EXPECT_EQ(triangles, 1216);
    }
} // namespace
