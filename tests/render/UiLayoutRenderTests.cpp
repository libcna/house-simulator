// SPDX-License-Identifier: MIT
//
// `HOUSE-02527`: the application shell at the two representative display shapes. These are
// deliberately screen-only captures: a menu regression should not be hidden by a changing house
// frame behind it, and the forced insets make the safe-area contract visible in every reference.
#include <cstddef>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include <gtest/gtest.h>

#include "Microsoft/Xna/Framework/Color.hpp"
#include "Microsoft/Xna/Framework/Game.hpp"
#include "Microsoft/Xna/Framework/GameTime.hpp"
#include "Microsoft/Xna/Framework/Graphics/GraphicsDevice.hpp"
#include "Microsoft/Xna/Framework/Graphics/RenderTarget2D.hpp"
#include "Microsoft/Xna/Framework/Graphics/SpriteBatch.hpp"
#include "Microsoft/Xna/Framework/Graphics/SpriteFont.hpp"
#include "Microsoft/Xna/Framework/GraphicsDeviceManager.hpp"
#include "Microsoft/Xna/Framework/Rectangle.hpp"

#include "cnahouse/app/Settings.hpp"
#include "cnahouse/debug/Screenshot.hpp"
#include "cnahouse/ui/MenuStack.hpp"
#include "cnahouse/ui/TextRenderer.hpp"

#include "render/ImageCompare.hpp"
#include "render/RenderHarness.hpp"

namespace
{
    namespace Gfx = Microsoft::Xna::Framework::Graphics;
    using cnahouse::testsupport::Compare;
    using cnahouse::testsupport::Image;
    using cnahouse::testsupport::RenderHarness;
    using Microsoft::Xna::Framework::Color;
    using Microsoft::Xna::Framework::Rectangle;

    enum class UiScene
    {
        MainMenu,
        PauseMenu,
        Settings,
    };

    struct UiFixture
    {
        std::string_view name;
        UiScene scene;
        int width;
        int height;
        Rectangle safeArea;
    };

    class UiCapture final : public Microsoft::Xna::Framework::Game
    {
    public:
        UiCapture(const UiFixture& fixture, std::string outputPath)
            : graphics_(this)
            , fixture_(fixture)
            , outputPath_(std::move(outputPath))
        {
            graphics_.setPreferredBackBufferWidthProperty(fixture.width);
            graphics_.setPreferredBackBufferHeightProperty(fixture.height);
            graphics_.setSynchronizeWithVerticalRetraceProperty(false);
            setIsFixedTimeStepProperty(false);
            getContentProperty().setRootDirectoryProperty(CNAHOUSE_TEST_CONTENT_ROOT);
        }

        [[nodiscard]] const Image& Captured() const noexcept
        {
            return captured_;
        }

        [[nodiscard]] const std::string& Failure() const noexcept
        {
            return failure_;
        }

    protected:
        void Draw(const Microsoft::Xna::Framework::GameTime& gameTime) override
        {
            Game::Draw(gameTime);
            if (ran_)
            {
                return;
            }
            ran_ = true;
            try
            {
                Gfx::GraphicsDevice& device = getGraphicsDeviceProperty();
                Gfx::RenderTarget2D target(device,
                                           fixture_.width,
                                           fixture_.height,
                                           false,
                                           Gfx::SurfaceFormat::Color,
                                           Gfx::DepthFormat::None);
                device.SetRenderTarget(&target);
                device.Clear(Color(18, 20, 24, 255));

                // The production HUD/menu face loaded by `CnaHouseGame::LoadHudFont`.
                const Gfx::SpriteFont font = getContentProperty().Load<Gfx::SpriteFont>("Fonts/ui-16");
                Gfx::SpriteBatch batch(device);
                cnahouse::ui::TextRenderer text;
                text.SetFont(&font);
                text.SetViewport(fixture_.width, fixture_.height, fixture_.safeArea);

                std::unique_ptr<cnahouse::ui::IScreen> screen;
                if (fixture_.scene == UiScene::MainMenu)
                {
                    screen = std::make_unique<cnahouse::ui::MainMenuScreen>(nullptr);
                }
                else if (fixture_.scene == UiScene::PauseMenu)
                {
                    screen = std::make_unique<cnahouse::ui::PauseMenuScreen>(nullptr);
                }
                else
                {
                    settings_ = cnahouse::app::Settings::Defaults();
                    screen = std::make_unique<cnahouse::ui::SettingsScreen>(settings_);
                }

                batch.Begin();
                screen->Draw(batch, text);
                batch.End();

                device.SetRenderTarget(nullptr);
                captured_.width = fixture_.width;
                captured_.height = fixture_.height;
                captured_.pixels.resize(static_cast<std::size_t>(fixture_.width) *
                                        static_cast<std::size_t>(fixture_.height));
                target.GetData(captured_.pixels.data(), static_cast<int>(captured_.pixels.size()));

                const auto saved = cnahouse::debug::Screenshot::Save(device, target, outputPath_);
                if (!saved)
                {
                    failure_ = saved.Error().ToString();
                }
            }
            catch (const std::exception& exception)
            {
                failure_ = exception.what();
            }
            Exit();
        }

    private:
        Microsoft::Xna::Framework::GraphicsDeviceManager graphics_;
        UiFixture fixture_;
        std::string outputPath_;
        cnahouse::app::Settings settings_{};
        Image captured_;
        std::string failure_;
        bool ran_ = false;
    };

    [[nodiscard]] std::size_t BrightPixels(const Image& image)
    {
        std::size_t bright = 0;
        for (const Color& pixel : image.pixels)
        {
            if (pixel.getRProperty() > 120 || pixel.getGProperty() > 120 || pixel.getBProperty() > 120)
            {
                ++bright;
            }
        }
        return bright;
    }

    [[nodiscard]] std::size_t BrightPixelsOutside(const Image& image, const Rectangle& bounds)
    {
        std::size_t bright = 0;
        for (int y = 0; y < image.height; ++y)
        {
            for (int x = 0; x < image.width; ++x)
            {
                if (bounds.Contains(x, y))
                {
                    continue;
                }
                const Color& pixel =
                    image.pixels[static_cast<std::size_t>(y) * static_cast<std::size_t>(image.width) +
                                 static_cast<std::size_t>(x)];
                if (pixel.getRProperty() > 120 || pixel.getGProperty() > 120 || pixel.getBProperty() > 120)
                {
                    ++bright;
                }
            }
        }
        return bright;
    }

    class UiLayoutRenderTests : public ::testing::TestWithParam<UiFixture>
    {
    };

    [[nodiscard]] std::string FixtureName(const ::testing::TestParamInfo<UiFixture>& parameter)
    {
        std::string name(parameter.param.name);
        for (char& character : name)
        {
            if (character == '-')
            {
                character = '_';
            }
        }
        return name;
    }

    TEST_P(UiLayoutRenderTests, MatchesTheInspectedReferenceInsideTheSafeArea)
    {
        const UiFixture& fixture = GetParam();
        const std::string output =
            std::string(CNAHOUSE_TEST_OUTPUT_DIR) + "/ui-" + std::string(fixture.name) + "-actual.png";
        UiCapture game(fixture, output);
        game.Run();

        ASSERT_TRUE(game.Failure().empty()) << game.Failure();
        const Image& actual = game.Captured();
        ASSERT_EQ(actual.width, fixture.width);
        ASSERT_EQ(actual.height, fixture.height);
        EXPECT_GT(BrightPixels(actual), 300U) << "the menu drew no readable text";
        EXPECT_EQ(BrightPixelsOutside(actual, fixture.safeArea), 0U)
            << "text crossed a forced safe-area inset";

        if (!RenderHarness::RenderingInSoftware())
        {
            GTEST_SKIP() << "the committed UI references use the software rasteriser; geometry and "
                            "safe-area coverage passed above";
        }

        const std::string reference =
            RenderHarness::ReferenceDirectory() + "/ui-" + std::string(fixture.name) + ".png";
        const auto loaded = RenderHarness::LoadPng(reference);
        ASSERT_TRUE(loaded.HasValue()) << loaded.Error().ToString();
        const auto diff = Compare(actual, *loaded, 2);
        EXPECT_FALSE(diff.sizeMismatch) << diff.ToString();
        EXPECT_LT(diff.DifferingFraction(), 0.0005) << diff.ToString();
    }

    INSTANTIATE_TEST_SUITE_P(
        RepresentativeScreens,
        UiLayoutRenderTests,
        ::testing::Values(
            UiFixture{"main-16x9", UiScene::MainMenu, 1600, 900, Rectangle(48, 27, 1504, 846)},
            UiFixture{"pause-16x9", UiScene::PauseMenu, 1600, 900, Rectangle(48, 27, 1504, 846)},
            UiFixture{"settings-16x9", UiScene::Settings, 1600, 900, Rectangle(48, 27, 1504, 846)},
            UiFixture{"main-20x9", UiScene::MainMenu, 2000, 900, Rectangle(120, 45, 1760, 810)},
            UiFixture{"pause-20x9", UiScene::PauseMenu, 2000, 900, Rectangle(120, 45, 1760, 810)},
            UiFixture{"settings-20x9", UiScene::Settings, 2000, 900, Rectangle(120, 45, 1760, 810)}),
        FixtureName);
} // namespace
