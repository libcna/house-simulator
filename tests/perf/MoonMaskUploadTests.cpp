// SPDX-License-Identifier: MIT
//
// `HOUSE-01604`. The actual 64 KiB mask upload, measured through a live XNA GraphicsDevice.
#include <algorithm>
#include <chrono>
#include <cstdio>
#include <vector>

#include <gtest/gtest.h>

#include "Microsoft/Xna/Framework/Color.hpp"
#include "Microsoft/Xna/Framework/Graphics/DepthFormat.hpp"
#include "Microsoft/Xna/Framework/Graphics/RenderTarget2D.hpp"
#include "Microsoft/Xna/Framework/Graphics/RenderTargetUsage.hpp"
#include "Microsoft/Xna/Framework/Graphics/SurfaceFormat.hpp"
#include "Microsoft/Xna/Framework/Graphics/Texture2D.hpp"
#include "Microsoft/Xna/Framework/Rectangle.hpp"

#include "cnahouse/rendering/MoonMask.hpp"

#include "integration/DeviceHost.hpp"

namespace
{
    namespace Xna = Microsoft::Xna::Framework;
    namespace Gfx = Microsoft::Xna::Framework::Graphics;
    using Clock = std::chrono::steady_clock;

    TEST(MoonMaskUploadTests, SixtyFourKiBUploadIsNegligibleAgainstAFrame)
    {
        constexpr int kWarmUp = 3;
        constexpr int kSamples = 21;
        double medianMilliseconds = 0.0;
        cnahouse::testsupport::DeviceHost host(
            [&](Gfx::GraphicsDevice& device)
            {
                const auto pixels = cnahouse::rendering::GenerateMoonMask(0.375, 0.37);
                Gfx::Texture2D texture(
                    device, cnahouse::rendering::kMoonMaskSize, cnahouse::rendering::kMoonMaskSize);
                Gfx::RenderTarget2D syncTarget(device,
                                               1,
                                               1,
                                               false,
                                               Gfx::SurfaceFormat::Color,
                                               Gfx::DepthFormat::None,
                                               0,
                                               Gfx::RenderTargetUsage::PreserveContents);
                Xna::Color onePixel;
                const Xna::Rectangle oneTexel(0, 0, 1, 1);
                auto sync = [&] { syncTarget.GetData(0, &oneTexel, &onePixel, 0, 1); };

                for (int sample = 0; sample < kWarmUp; ++sample)
                {
                    texture.SetData(pixels.data(), static_cast<int>(pixels.size()));
                    sync();
                }
                std::vector<double> samples;
                samples.reserve(kSamples);
                for (int sample = 0; sample < kSamples; ++sample)
                {
                    const Clock::time_point started = Clock::now();
                    texture.SetData(pixels.data(), static_cast<int>(pixels.size()));
                    sync();
                    samples.push_back(
                        std::chrono::duration<double, std::milli>(Clock::now() - started).count());
                }
                std::sort(samples.begin(), samples.end());
                medianMilliseconds = samples[samples.size() / 2];
            });
        host.Run();

        ASSERT_TRUE(host.Ran());
        ASSERT_EQ(host.Failure(), "") << "the real device rejected the moon mask upload";
        std::printf("  moon mask: 64 KiB Texture2D::SetData median %.3f ms (%d samples)\n",
                    medianMilliseconds,
                    kSamples);
        EXPECT_LT(medianMilliseconds, 1.0)
            << "a mask upload must remain negligible against the 16.67 ms frame budget";
    }
} // namespace
