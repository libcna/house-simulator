// SPDX-License-Identifier: MIT
#include "render/RenderHarness.hpp"

#include <cstdlib>
#include <format>
#include <vector>

#include "Microsoft/Xna/Framework/Game.hpp"
#include "Microsoft/Xna/Framework/Graphics/GraphicsAdapter.hpp"
#include "Microsoft/Xna/Framework/Graphics/GraphicsDevice.hpp"
#include "Microsoft/Xna/Framework/Graphics/Texture2D.hpp"
#include "Microsoft/Xna/Framework/GraphicsDeviceManager.hpp"
#include "System/IO/FileAccess.hpp"
#include "System/IO/FileMode.hpp"
#include "System/IO/FileStream.hpp"

#include "cnahouse/app/CnaHouseGame.hpp"
#include "cnahouse/rendering/Quality.hpp"

namespace cnahouse::testsupport
{
    namespace
    {
        using util::Err;
        using util::ErrorCode;

        /// A minimal `Game` whose only job is to exist long enough to decode a PNG.
        class DecodeHost final : public Microsoft::Xna::Framework::Game
        {
        public:
            DecodeHost(const std::string& first, const std::string& second)
                : gdm_(this)
                , first_(first)
                , second_(second)
            {
                gdm_.setPreferredBackBufferWidthProperty(64);
                gdm_.setPreferredBackBufferHeightProperty(64);
                gdm_.setSynchronizeWithVerticalRetraceProperty(false);
                setIsFixedTimeStepProperty(false);
            }

            Image firstImage;
            Image secondImage;
            std::string failure;

        protected:
            void Draw(const Microsoft::Xna::Framework::GameTime& gameTime) override
            {
                Game::Draw(gameTime);
                if (done_)
                {
                    return;
                }
                done_ = true;
                try
                {
                    firstImage = Decode(first_);
                    if (!second_.empty())
                    {
                        secondImage = Decode(second_);
                    }
                }
                catch (const std::exception& e)
                {
                    failure = e.what();
                }
                Exit();
            }

        private:
            Image Decode(const std::string& path)
            {
                // `Texture2D::FromStream(device, stream)` is plain XNA 4.0; the `assetName`
                // constructors and `SaveAsPng(filename)` are the CNAEXT ones.
                System::IO::FileStream stream(path, System::IO::FileMode::Open, System::IO::FileAccess::Read);
                Microsoft::Xna::Framework::Graphics::Texture2D texture =
                    Microsoft::Xna::Framework::Graphics::Texture2D::FromStream(getGraphicsDeviceProperty(),
                                                                               stream);

                Image image;
                image.width = texture.getWidthProperty();
                image.height = texture.getHeightProperty();
                image.pixels.resize(static_cast<std::size_t>(image.width) *
                                    static_cast<std::size_t>(image.height));
                texture.GetData(image.pixels.data(), static_cast<int>(image.pixels.size()));
                return image;
            }

            Microsoft::Xna::Framework::GraphicsDeviceManager gdm_;
            std::string first_;
            std::string second_;
            bool done_ = false;
        };

    } // namespace

    std::vector<Region> RenderHarness::NonDeterministicRegions(int width, int height)
    {
        // The frame time and FPS, drawn top-right at (12, 10) in virtual units on the 1600x900
        // canvas. The box is taken generously -- 460x60 virtual units -- because the string's width
        // depends on the numbers in it and a mask that clipped one digit would be worse than none.
        const double sx = static_cast<double>(width) / 1600.0;
        const double sy = static_cast<double>(height) / 900.0;
        Region frameTime;
        frameTime.x = static_cast<int>((1600.0 - 480.0) * sx);
        frameTime.y = 0;
        frameTime.width = width - frameTime.x;
        frameTime.height = static_cast<int>(60.0 * sy);
        return {frameTime};
    }

    std::string RenderHarness::ReferenceDirectory()
    {
        // Tests run with the repository root as their working directory (`tests/CMakeLists.txt`
        // pins it), so this is stable wherever the build tree lives.
        return "tests/render/reference";
    }

    bool RenderHarness::RenderingInSoftware()
    {
        if (const char* forced = std::getenv("LIBGL_ALWAYS_SOFTWARE"); forced != nullptr && forced[0] == '1')
        {
            return true;
        }
        const auto& adapter =
            Microsoft::Xna::Framework::Graphics::GraphicsAdapter::getDefaultAdapterProperty();
        return rendering::IsSoftwareRasteriser(adapter.getDescriptionProperty());
    }

    util::Result<void>
    RenderHarness::CaptureFrame(app::Options options, int width, int height, const std::string& pngPath)
    {
        options.screenshot = pngPath;

        app::Settings settings = app::Settings::Defaults();
        settings.backBufferWidth = width;
        settings.backBufferHeight = height;
        settings.verticalSync = false;
        // A reference scene owns every pixel it presents. Keep the general-purpose harness free
        // of the player-facing environment readout; its dedicated render fixture enables and
        // verifies that HUD explicitly. Otherwise changing the default setting invalidates every
        // unrelated geometry reference without changing the geometry it is meant to protect.
        settings.showEnvironmentReadout = false;

        try
        {
            app::CnaHouseGame game(options, settings);
            // `--screenshot` already exits after the capture; the limit is a backstop so a broken
            // capture path fails as a test rather than as a hung CI job.
            game.SetFrameLimit(120);
            game.Run();
            if (game.ExitCode() != 0)
            {
                return Err(ErrorCode::IoFailure,
                           std::format("the capture session exited with {}", game.ExitCode()),
                           pngPath);
            }
        }
        catch (const std::exception& e)
        {
            return Err(ErrorCode::IoFailure, e.what(), pngPath);
        }
        return util::Ok();
    }

    util::Result<Image> RenderHarness::LoadPng(const std::string& pngPath)
    {
        DecodeHost host(pngPath, "");
        host.Run();
        if (!host.failure.empty())
        {
            return Err(ErrorCode::ContentLoadFailure, host.failure, pngPath);
        }
        if (host.firstImage.Empty())
        {
            return Err(ErrorCode::InvalidData, "the decoded image has no pixels", pngPath);
        }
        return host.firstImage;
    }

    util::Result<ImageDiff> RenderHarness::CompareWithReference(app::Options options,
                                                                int width,
                                                                int height,
                                                                const std::string& pngPath,
                                                                const std::string& referencePath,
                                                                int channelTolerance,
                                                                const std::vector<Region>& ignore)
    {
        if (auto captured = CaptureFrame(std::move(options), width, height, pngPath); !captured)
        {
            return captured.Error();
        }

        // BOTH decoded in one device session. Creating a third `Game` for the second file would
        // double the cost of every render test for nothing.
        DecodeHost host(pngPath, referencePath);
        host.Run();
        if (!host.failure.empty())
        {
            return Err(ErrorCode::ContentLoadFailure, host.failure, referencePath);
        }
        if (host.firstImage.Empty() || host.secondImage.Empty())
        {
            return Err(ErrorCode::InvalidData, "one of the two frames decoded empty", referencePath);
        }
        return Compare(host.firstImage, host.secondImage, channelTolerance, ignore);
    }

} // namespace cnahouse::testsupport
