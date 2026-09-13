// SPDX-License-Identifier: MIT
//
// `HOUSE-01610`. Real-device proof for the dynamic, one-submission catalogue path.
#include <cstdint>
#include <string>
#include <utility>

#include <gtest/gtest.h>

#include "Microsoft/Xna/Framework/Graphics/BlendState.hpp"
#include "Microsoft/Xna/Framework/Graphics/DepthStencilState.hpp"

#include "System/IO/FileStream.hpp"

#include "cnahouse/debug/Counters.hpp"
#include "cnahouse/rendering/Camera.hpp"
#include "cnahouse/rendering/StarField.hpp"
#include "cnahouse/rendering/StateTracker.hpp"

#include "integration/DeviceHost.hpp"

namespace
{
    namespace Gfx = Microsoft::Xna::Framework::Graphics;

    TEST(StarFieldPassTests, CompleteCatalogueStreamsAndSubmitsAsOneAdditiveDraw)
    {
        std::int64_t draws = 0;
        std::int64_t triangles = 0;
        std::int64_t uploads = 0;
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
                cnahouse::rendering::StarField field(camera, std::move(*catalogue));
                ASSERT_TRUE(field.IsActive());
                ASSERT_EQ(field.Vertices().size(), 6000u);

                cnahouse::rendering::StateTracker states(device);
                cnahouse::debug::Counters counters;
                cnahouse::rendering::PassContext context{device, states, counters, 1.0F / 60.0F};
                field.Draw(context);
                field.Draw(context);

                const Gfx::BlendState& blend = device.getBlendStateProperty();
                EXPECT_EQ(blend.getColorSourceBlendProperty(),
                          Gfx::BlendState::Additive.getColorSourceBlendProperty());
                EXPECT_EQ(blend.getColorDestinationBlendProperty(),
                          Gfx::BlendState::Additive.getColorDestinationBlendProperty());
                EXPECT_FALSE(device.getDepthStencilStateProperty().getDepthBufferEnableProperty());

                const auto* drawCounter = counters.Find("stars.draws");
                const auto* triangleCounter = counters.Find("stars.triangles");
                const auto* uploadCounter = counters.Find("stars.uploads");
                ASSERT_NE(drawCounter, nullptr);
                ASSERT_NE(triangleCounter, nullptr);
                ASSERT_NE(uploadCounter, nullptr);
                draws = drawCounter->current;
                triangles = triangleCounter->current;
                uploads = uploadCounter->current;
            });
        host.Run();

        ASSERT_TRUE(host.Ran());
        ASSERT_EQ(host.Failure(), "") << "the real device rejected the streamed XNA star field";
        EXPECT_EQ(draws, 1);
        EXPECT_EQ(triangles, 3000);
        EXPECT_EQ(uploads, 2) << "the dynamic field uses Discard once per rendered frame";
    }
} // namespace
