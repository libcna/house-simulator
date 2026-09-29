// SPDX-License-Identifier: MIT
//
// `HOUSE-01643` through `HOUSE-01649`. The dome, celestial colour, cloud
// textures and later buffer uploads cross a live XNA GraphicsDevice. This is always launched with
// SDL's offscreen driver by the local verification command.
#include <array>
#include <cstdint>
#include <fstream>
#include <functional>
#include <iterator>
#include <memory>
#include <string>

#include <gtest/gtest.h>

#include "System/IO/File.hpp"
#include "System/IO/FileAccess.hpp"
#include "System/IO/FileMode.hpp"
#include "System/IO/FileStream.hpp"

#include "Microsoft/Xna/Framework/Content/ContentManager.hpp"
#include "Microsoft/Xna/Framework/Game.hpp"
#include "Microsoft/Xna/Framework/Graphics/BlendState.hpp"
#include "Microsoft/Xna/Framework/Graphics/DepthStencilState.hpp"
#include "Microsoft/Xna/Framework/Graphics/GraphicsDevice.hpp"
#include "Microsoft/Xna/Framework/Graphics/SamplerState.hpp"
#include "Microsoft/Xna/Framework/Graphics/Texture2D.hpp"
#include "Microsoft/Xna/Framework/GraphicsDeviceManager.hpp"

#include "cnahouse/debug/Counters.hpp"
#include "cnahouse/environment/MoonModel.hpp"
#include "cnahouse/environment/SunModel.hpp"
#include "cnahouse/rendering/Camera.hpp"
#include "cnahouse/rendering/RenderStates.hpp"
#include "cnahouse/rendering/SkySystem.hpp"
#include "cnahouse/rendering/StateTracker.hpp"

#include "integration/DeviceHost.hpp"

namespace
{
    namespace Gfx = Microsoft::Xna::Framework::Graphics;

    class SkyContentHost final : public Microsoft::Xna::Framework::Game
    {
    public:
        using Body =
            std::function<void(Microsoft::Xna::Framework::Content::ContentManager&, Gfx::GraphicsDevice&)>;

        explicit SkyContentHost(Body body)
            : gdm_(this)
            , body_(std::move(body))
        {
            gdm_.setPreferredBackBufferWidthProperty(320);
            gdm_.setPreferredBackBufferHeightProperty(240);
            gdm_.setSynchronizeWithVerticalRetraceProperty(false);
            setIsFixedTimeStepProperty(false);
            getContentProperty().setRootDirectoryProperty(CNAHOUSE_TEST_CONTENT_ROOT);
        }

        [[nodiscard]] bool Ran() const noexcept
        {
            return ran_;
        }

        [[nodiscard]] const std::string& Failure() const noexcept
        {
            return failure_;
        }

    protected:
        void Draw(const Microsoft::Xna::Framework::GameTime& gameTime) override
        {
            Game::Draw(gameTime);
            if (!ran_)
            {
                ran_ = true;
                try
                {
                    body_(getContentProperty(), getGraphicsDeviceProperty());
                }
                catch (const std::exception& e)
                {
                    failure_ = e.what();
                }
            }
            Exit();
        }

    private:
        Microsoft::Xna::Framework::GraphicsDeviceManager gdm_;
        Body body_;
        bool ran_ = false;
        std::string failure_;
    };

    TEST(SkySystemPassTests, TheDomeSubmitsItsExactTriangleListAndReusesResources)
    {
        System::IO::FileStream stream(
            CNAHOUSE_TEST_SKY_DOME_FIXTURE, System::IO::FileMode::Open, System::IO::FileAccess::Read);
        auto mesh = cnahouse::rendering::SkyDomeReader::Read(stream, CNAHOUSE_TEST_SKY_DOME_FIXTURE);
        ASSERT_TRUE(mesh) << (mesh ? std::string() : mesh.Error().ToString());
        const std::string skyJson = System::IO::File::ReadAllText("content/world/layout.sky.json");
        auto colourModel = cnahouse::rendering::SkyColourModelReader::Read(skyJson, "layout.sky.json");
        ASSERT_TRUE(colourModel) << (colourModel ? std::string() : colourModel.Error().ToString());

        std::int64_t draws = 0;
        std::int64_t triangles = 0;
        cnahouse::testsupport::DeviceHost host(
            [&](Gfx::GraphicsDevice& device)
            {
                cnahouse::rendering::Camera camera;
                camera.eye = Microsoft::Xna::Framework::Vector3(20.0F, 4.0F, -30.0F);
                cnahouse::rendering::SkySystem sky(camera, std::move(*mesh), std::move(*colourModel));
                cnahouse::environment::SunPosition sun;
                sun.altitudeDeg = cnahouse::environment::kRefractedHorizonDeg - 1.0;
                sky.SetSun(sun, 0.0);

                cnahouse::rendering::StateTracker states(device);
                cnahouse::debug::Counters counters;
                cnahouse::rendering::PassContext context{device, states, counters, 1.0F / 60.0F};
                sky.Draw(context);
                camera.eye = Microsoft::Xna::Framework::Vector3(-50.0F, 9.0F, 120.0F);
                sky.Draw(context);
                ASSERT_TRUE(sky.SetSky(45.0, 0.5));
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
                EXPECT_EQ(states.Current().blendSkipped, 2u);
                EXPECT_EQ(states.Current().depthSkipped, 2u);
                EXPECT_EQ(states.Current().rasterSkipped, 2u);

                const auto* drawCounter = counters.Find("sky.dome.draws");
                const auto* triangleCounter = counters.Find("sky.dome.triangles");
                const auto* colourUpdatesCounter = counters.Find("sky.colour.updates");
                const auto* colourMicrosCounter = counters.Find("sky.colour.micros");
                ASSERT_NE(drawCounter, nullptr);
                ASSERT_NE(triangleCounter, nullptr);
                ASSERT_NE(colourUpdatesCounter, nullptr);
                ASSERT_NE(colourMicrosCounter, nullptr);
                draws = drawCounter->current;
                triangles = triangleCounter->current;
                EXPECT_GE(colourUpdatesCounter->current, 2);
                EXPECT_GE(colourMicrosCounter->current, 0);
            });
        host.Run();

        ASSERT_TRUE(host.Ran());
        ASSERT_EQ(host.Failure(), "") << "the real device rejected the sky dome pass";
        EXPECT_EQ(draws, 1) << "the counter describes this frame, not lifetime submissions";
        EXPECT_EQ(triangles, 1216);
    }

    TEST(SkySystemPassTests, ThreeCompiledCloudRingsScrollTintAndSubmitThroughStockXna)
    {
        std::int64_t draws = 0;
        std::int64_t triangles = 0;
        std::int64_t uploads = 0;
        std::int64_t moonDraws = 0;
        SkyContentHost host(
            [&](Microsoft::Xna::Framework::Content::ContentManager& content, Gfx::GraphicsDevice& device)
            {
                System::IO::FileStream stream(
                    CNAHOUSE_TEST_SKY_DOME_FIXTURE, System::IO::FileMode::Open, System::IO::FileAccess::Read);
                auto mesh = cnahouse::rendering::SkyDomeReader::Read(stream, CNAHOUSE_TEST_SKY_DOME_FIXTURE);
                ASSERT_TRUE(mesh) << (mesh ? std::string() : mesh.Error().ToString());
                const std::string skyJson = System::IO::File::ReadAllText("content/world/layout.sky.json");
                auto model = cnahouse::rendering::SkyColourModelReader::Read(skyJson, "layout.sky.json");
                ASSERT_TRUE(model) << (model ? std::string() : model.Error().ToString());

                cnahouse::rendering::SkySystem::CloudTextures textures{
                    std::unique_ptr<Gfx::Texture2D>(
                        new Gfx::Texture2D(content.Load<Gfx::Texture2D>(model->cloudLayers[0].texture))),
                    std::unique_ptr<Gfx::Texture2D>(
                        new Gfx::Texture2D(content.Load<Gfx::Texture2D>(model->cloudLayers[1].texture))),
                    std::unique_ptr<Gfx::Texture2D>(
                        new Gfx::Texture2D(content.Load<Gfx::Texture2D>(model->cloudLayers[2].texture)))};
                for (const std::unique_ptr<Gfx::Texture2D>& texture : textures)
                {
                    ASSERT_NE(texture, nullptr);
                    EXPECT_EQ(texture->getWidthProperty(), 1024);
                    EXPECT_EQ(texture->getHeightProperty(), 1024);
                    EXPECT_GT(texture->getLevelCountProperty(), 1)
                        << "the offline content build dropped HOUSE-01646's mip chain";
                }
                std::unique_ptr<Gfx::Texture2D> moonAlbedo(
                    new Gfx::Texture2D(content.Load<Gfx::Texture2D>("Textures/Sky/moon_albedo")));
                EXPECT_EQ(moonAlbedo->getWidthProperty(), 1024);
                EXPECT_EQ(moonAlbedo->getHeightProperty(), 1024);
                EXPECT_GT(moonAlbedo->getLevelCountProperty(), 1);

                cnahouse::rendering::Camera camera;
                cnahouse::rendering::SkySystem sky(
                    camera, std::move(*mesh), std::move(*model), std::move(textures), std::move(moonAlbedo));
                ASSERT_TRUE(sky.SetWind(8.0, 225.0));
                ASSERT_TRUE(sky.SetCloudState(0.325, 0.5));
                cnahouse::rendering::StateTracker states(device);
                cnahouse::debug::Counters counters;
                cnahouse::rendering::PassContext context{device, states, counters, 0.5F};
                sky.Draw(context);
                const auto firstOffsets = sky.CloudOffsets();
                sky.Draw(context);
                EXPECT_NE(sky.CloudOffsets(), firstOffsets);
                ASSERT_TRUE(sky.SetWind(0.0, 225.0));
                ASSERT_TRUE(sky.SetSky(-0.58, 0.0));
                sky.Draw(context);

                const auto* drawsCounter = counters.Find("sky.cloud.draws");
                const auto* trianglesCounter = counters.Find("sky.cloud.triangles");
                const auto* uploadsCounter = counters.Find("sky.cloud.uploads");
                const auto* alphaUpdatesCounter = counters.Find("sky.cloud.alpha_updates");
                ASSERT_NE(drawsCounter, nullptr);
                ASSERT_NE(trianglesCounter, nullptr);
                ASSERT_NE(uploadsCounter, nullptr);
                ASSERT_NE(alphaUpdatesCounter, nullptr);
                draws = drawsCounter->current;
                triangles = trianglesCounter->current;
                uploads = uploadsCounter->current;
                EXPECT_EQ(alphaUpdatesCounter->current, 1);

                const Gfx::BlendState& blend = device.getBlendStateProperty();
                EXPECT_EQ(blend.getColorSourceBlendProperty(),
                          Gfx::BlendState::AlphaBlend.getColorSourceBlendProperty());
                EXPECT_EQ(blend.getColorDestinationBlendProperty(),
                          Gfx::BlendState::AlphaBlend.getColorDestinationBlendProperty());
                EXPECT_FALSE(device.getDepthStencilStateProperty().getDepthBufferEnableProperty());
                EXPECT_EQ(device.getSamplerStatesProperty()[0].getFilterProperty(),
                          Gfx::SamplerState::LinearWrap.getFilterProperty());
                EXPECT_EQ(device.getSamplerStatesProperty()[0].getAddressUProperty(),
                          Gfx::SamplerState::LinearWrap.getAddressUProperty());

                cnahouse::environment::SunPosition sun;
                sun.altitudeDeg = -20.0;
                sun.azimuthDeg = 270.0;
                cnahouse::environment::MoonPosition moon;
                moon.altitudeDeg = 30.0;
                moon.azimuthDeg = 0.0;
                cnahouse::environment::MoonPhase phase;
                phase.phase = 0.25;
                sky.SetCelestial(sun, moon, phase, 0.0);
                sky.Draw(context);
                const auto* moonDrawCounter = counters.Find("moon.disc.draws");
                ASSERT_NE(moonDrawCounter, nullptr);
                moonDraws = moonDrawCounter->current;
            });
        host.Run();

        ASSERT_TRUE(host.Ran());
        ASSERT_EQ(host.Failure(), "") << host.Failure();
        EXPECT_EQ(draws, 3);
        EXPECT_EQ(triangles, 2160);
        EXPECT_EQ(uploads, 9) << "three UV buffers moved twice, then all three accepted a stationary tint";
        EXPECT_EQ(moonDraws, 1) << "the compiled lunar albedo reached SkySystem's moon pass";
    }
} // namespace
