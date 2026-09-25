// SPDX-License-Identifier: MIT
//
// `HOUSE-01741`. A real XNA device proves one Discard upload per non-empty frame and exactly one
// indexed draw for each particle material, independent of how submissions were interleaved.
#include <array>
#include <cstdint>

#include <gtest/gtest.h>

#include "Microsoft/Xna/Framework/Color.hpp"
#include "Microsoft/Xna/Framework/Graphics/BlendState.hpp"
#include "Microsoft/Xna/Framework/Graphics/DepthStencilState.hpp"
#include "Microsoft/Xna/Framework/Graphics/Texture2D.hpp"

#include "cnahouse/debug/Counters.hpp"
#include "cnahouse/rendering/Camera.hpp"
#include "cnahouse/rendering/ParticleRenderer.hpp"
#include "cnahouse/rendering/StateTracker.hpp"

#include "integration/DeviceHost.hpp"

namespace
{
    namespace Xna = Microsoft::Xna::Framework;
    namespace Gfx = Microsoft::Xna::Framework::Graphics;

    cnahouse::rendering::ParticleQuad Quad(float x, std::uint8_t material)
    {
        cnahouse::rendering::ParticleQuad quad;
        quad.centre = Xna::Vector3(x, 1.0F, -4.0F);
        quad.halfSize = Xna::Vector2(0.1F, 0.3F);
        quad.colour = Xna::Color(160, 160, 160, 160);
        quad.material = material;
        return quad;
    }

    TEST(ParticleRendererIntegrationTests, StreamsOnceAndDrawsOncePerUsedMaterial)
    {
        std::uint32_t draws = 0u;
        std::uint64_t uploads = 0u;
        cnahouse::testsupport::DeviceHost host(
            [&](Gfx::GraphicsDevice& device)
            {
                Gfx::Texture2D rain(device, 1, 1);
                Gfx::Texture2D mist(device, 1, 1);
                const std::array<Xna::Color, 1> rainPixel{{Xna::Color(160, 160, 160, 160)}};
                const std::array<Xna::Color, 1> mistPixel{{Xna::Color(96, 96, 96, 96)}};
                rain.SetData(rainPixel.data(), static_cast<int>(rainPixel.size()));
                mist.SetData(mistPixel.data(), static_cast<int>(mistPixel.size()));

                cnahouse::rendering::Camera camera;
                cnahouse::rendering::ParticleRenderer renderer(camera);
                ASSERT_TRUE(renderer.SetMaterial(0u, &rain));
                ASSERT_TRUE(renderer.SetMaterial(1u, &mist));
                EXPECT_FALSE(renderer.SetMaterial(cnahouse::rendering::kMaximumParticleMaterials, &rain));
                renderer.BeginFrame(cnahouse::rendering::ParticleQuality::High);
                ASSERT_TRUE(renderer.Submit(Quad(-0.4F, 0u)));
                ASSERT_TRUE(renderer.Submit(Quad(-0.2F, 1u)));
                ASSERT_TRUE(renderer.Submit(Quad(0.0F, 0u)));
                ASSERT_TRUE(renderer.Submit(Quad(0.2F, 1u)));
                ASSERT_TRUE(renderer.Submit(Quad(0.4F, 0u)));

                cnahouse::rendering::StateTracker states(device);
                cnahouse::debug::Counters counters;
                cnahouse::rendering::PassContext context{device, states, counters, 1.0F / 60.0F};
                renderer.Draw(context);
                draws = renderer.LastDrawCount();
                uploads = renderer.UploadCount();

                EXPECT_EQ(device.getBlendStateProperty().getColorSourceBlendProperty(),
                          Gfx::BlendState::AlphaBlend.getColorSourceBlendProperty());
                EXPECT_EQ(device.getBlendStateProperty().getColorDestinationBlendProperty(),
                          Gfx::BlendState::AlphaBlend.getColorDestinationBlendProperty());
                EXPECT_TRUE(device.getDepthStencilStateProperty().getDepthBufferEnableProperty());
                EXPECT_FALSE(device.getDepthStencilStateProperty().getDepthBufferWriteEnableProperty());
                ASSERT_NE(counters.Find("particles.draws"), nullptr);
                ASSERT_NE(counters.Find("particles.quads"), nullptr);
                EXPECT_EQ(counters.Find("particles.draws")->current, 2);
                EXPECT_EQ(counters.Find("particles.quads")->current, 5);

                renderer.BeginFrame(cnahouse::rendering::ParticleQuality::Low);
                renderer.Draw(context);
                EXPECT_EQ(renderer.LastDrawCount(), 0u);
                EXPECT_EQ(renderer.UploadCount(), uploads)
                    << "an empty frame must neither allocate nor stream another buffer";
            });
        host.Run();

        ASSERT_TRUE(host.Ran());
        ASSERT_EQ(host.Failure(), "") << host.Failure();
        EXPECT_EQ(draws, 2u);
        EXPECT_EQ(uploads, 1u);
    }
} // namespace
