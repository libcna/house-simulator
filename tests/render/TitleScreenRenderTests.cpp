// SPDX-License-Identifier: MIT
//
// `HOUSE-00164`: the first render regression fixture. `title-01` is the loading/title screen at
// 1600x900 with Tier S, quality low and audio off -- every option pinned, so the only thing that can
// differ between two runs is the rasteriser.
//
// **The reference was generated under `LIBGL_ALWAYS_SOFTWARE=1`**, which is what `HOUSE-00138`'s CI
// job uses, because a software rasteriser is the only one whose output is reproducible on a machine
// nobody owns. On hardware the same test still runs, but it asserts geometry and coverage rather
// than pixels and says so in its failure message -- exactly what `HOUSE-00115` warns about.
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "cnahouse/app/CommandLine.hpp"

#include "render/RenderHarness.hpp"

namespace
{
    using cnahouse::app::Options;
    using cnahouse::app::QualityPreset;
    using cnahouse::app::RenderTier;
    using cnahouse::testsupport::Image;
    using cnahouse::testsupport::RenderHarness;

    constexpr int kWidth = 1600;
    constexpr int kHeight = 900;

    /// Everything pinned. A fixture with one unpinned option is a fixture that fails on a Tuesday.
    Options FixtureOptions()
    {
        Options options;
        options.tier = RenderTier::S;
        options.quality = QualityPreset::Low;
        options.noAudio = true;
        options.contentRoot = CNAHOUSE_TEST_CONTENT_ROOT;
        return options;
    }

    std::string OutputPath()
    {
        return std::string(CNAHOUSE_TEST_OUTPUT_DIR) + "/title-01-actual.png";
    }

    std::string ReferencePath()
    {
        return RenderHarness::ReferenceDirectory() + "/title-01.png";
    }

    TEST(TitleScreenRenderTests, TheCapturedFrameMatchesTheCommittedReference)
    {
        if (!RenderHarness::RenderingInSoftware())
        {
            GTEST_SKIP() << "the reference is a software-rasteriser frame (HOUSE-00138); run this "
                            "with LIBGL_ALWAYS_SOFTWARE=1 to compare pixels. Coverage is asserted "
                            "by the other tests in this file, which do run here.";
        }

        // 2 rather than 0: the tolerance is for Mesa version drift, and NOT slack to widen when a
        // test fails. The reference comes from the same rasteriser, so a real difference is real.
        const auto diff =
            RenderHarness::CompareWithReference(FixtureOptions(),
                                                kWidth,
                                                kHeight,
                                                OutputPath(),
                                                ReferencePath(),
                                                2,
                                                RenderHarness::NonDeterministicRegions(kWidth, kHeight));

        ASSERT_TRUE(diff.HasValue()) << diff.Error().ToString();
        EXPECT_FALSE(diff->sizeMismatch) << diff->ToString();
        // A handful of pixels may sit on an anti-aliased glyph edge; a real regression moves
        // thousands. 0.05% of 1 440 000 pixels is 720.
        EXPECT_LT(diff->DifferingFraction(), 0.0005) << diff->ToString();
    }

    TEST(TitleScreenRenderTests, TheSameFrameRenderedTwiceIsIdentical)
    {
        // Determinism first: without it the reference comparison above cannot mean anything, and a
        // flaky regression test is worse than none because it teaches people to ignore it.
        const std::string first = std::string(CNAHOUSE_TEST_OUTPUT_DIR) + "/title-01-run1.png";
        const std::string second = std::string(CNAHOUSE_TEST_OUTPUT_DIR) + "/title-01-run2.png";

        ASSERT_TRUE(RenderHarness::CaptureFrame(FixtureOptions(), kWidth, kHeight, first));
        const auto diff =
            RenderHarness::CompareWithReference(FixtureOptions(),
                                                kWidth,
                                                kHeight,
                                                second,
                                                first,
                                                0,
                                                RenderHarness::NonDeterministicRegions(kWidth, kHeight));

        ASSERT_TRUE(diff.HasValue()) << diff.Error().ToString();
        EXPECT_EQ(diff->differingPixels, 0u)
            << "two runs of the same build must be bit-identical: " << diff->ToString();
        EXPECT_EQ(diff->maxChannelDelta, 0) << diff->ToString();
    }

    TEST(TitleScreenRenderTests, TheFrameHasTheCoverageTheTitleScreenShouldHave)
    {
        // Driver-independent, so it runs on hardware too. It asserts what the frame CONTAINS rather
        // than what it looks like: a background that is exactly the clear colour, and text in the
        // middle. A frame where the HUD failed to draw passes a tolerance-based comparison against
        // a stale reference; it does not pass this.
        const std::string path = std::string(CNAHOUSE_TEST_OUTPUT_DIR) + "/title-01-coverage.png";
        ASSERT_TRUE(RenderHarness::CaptureFrame(FixtureOptions(), kWidth, kHeight, path));

        const auto loaded = RenderHarness::LoadPng(path);
        ASSERT_TRUE(loaded.HasValue()) << loaded.Error().ToString();
        const Image& image = *loaded;
        ASSERT_EQ(image.width, kWidth);
        ASSERT_EQ(image.height, kHeight);

        // The clear colour, from `CnaHouseGame::ClearColour()`. Deliberately not CornflowerBlue, so
        // a frame that IS cornflower blue is some other XNA program's.
        std::size_t background = 0;
        std::size_t lit = 0;
        for (const auto& pixel : image.pixels)
        {
            const bool isClear =
                pixel.getRProperty() == 18 && pixel.getGProperty() == 20 && pixel.getBProperty() == 24;
            if (isClear)
            {
                ++background;
            }
            else if (pixel.getRProperty() > 120)
            {
                ++lit;
            }
        }

        EXPECT_GT(background, image.pixels.size() * 9 / 10)
            << "the title screen is mostly empty; a filled frame means something drew over it";
        EXPECT_GT(lit, 2000u) << "no bright pixels at all means no text was drawn";
        EXPECT_LT(lit, image.pixels.size() / 20) << "text should not cover 5% of the screen";
    }

    TEST(TitleScreenRenderTests, TheCentreBandCarriesThePromptAndTheCornersDoNot)
    {
        // Geometry rather than shading, which is what `HOUSE-00115` says a render test should assert
        // when the driver is not the one phase 1 measured against.
        const std::string path = std::string(CNAHOUSE_TEST_OUTPUT_DIR) + "/title-01-bands.png";
        ASSERT_TRUE(RenderHarness::CaptureFrame(FixtureOptions(), kWidth, kHeight, path));
        const auto loaded = RenderHarness::LoadPng(path);
        ASSERT_TRUE(loaded.HasValue()) << loaded.Error().ToString();
        const Image& image = *loaded;

        auto brightIn = [&image](int x0, int y0, int x1, int y1)
        {
            std::size_t count = 0;
            for (int y = y0; y < y1; ++y)
            {
                for (int x = x0; x < x1; ++x)
                {
                    const auto& pixel =
                        image.pixels[static_cast<std::size_t>(y) * static_cast<std::size_t>(image.width) +
                                     static_cast<std::size_t>(x)];
                    if (pixel.getRProperty() > 120)
                    {
                        ++count;
                    }
                }
            }
            return count;
        };

        // The version line sits at centre y-80 and the prompt at centre y+60, both centred in x.
        EXPECT_GT(brightIn(kWidth / 4, kHeight / 2 - 140, 3 * kWidth / 4, kHeight / 2 + 140), 1000u)
            << "the centre band is empty; the title screen did not draw";
        // The bottom-left corner has nothing in it on this screen.
        EXPECT_EQ(brightIn(0, kHeight - 100, 300, kHeight), 0u) << "something drew where nothing should be";
    }
} // namespace
