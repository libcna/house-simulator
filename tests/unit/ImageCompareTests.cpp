// SPDX-License-Identifier: MIT
//
// `HOUSE-00164`'s comparison, tested without rendering anything. A regression harness whose
// comparison has never been shown to FAIL on a real difference is a harness that reports green
// forever, so the tests here are mostly about what it must catch rather than what it must pass.
#include <gtest/gtest.h>

#include "render/ImageCompare.hpp"

namespace
{
    using cnahouse::testsupport::Compare;
    using cnahouse::testsupport::Image;
    using Colour = Microsoft::Xna::Framework::Color;

    Image Solid(int width, int height, Colour colour)
    {
        Image image;
        image.width = width;
        image.height = height;
        image.pixels.assign(static_cast<std::size_t>(width) * static_cast<std::size_t>(height), colour);
        return image;
    }

    TEST(ImageCompareTests, IdenticalFramesDifferByNothing)
    {
        const Image a = Solid(16, 16, Colour(18, 20, 24, 255));
        const auto diff = Compare(a, a, 0);
        EXPECT_FALSE(diff.sizeMismatch);
        EXPECT_EQ(diff.comparedPixels, 256u);
        EXPECT_EQ(diff.differingPixels, 0u);
        EXPECT_EQ(diff.maxChannelDelta, 0);
        EXPECT_DOUBLE_EQ(diff.meanChannelDelta, 0.0);
    }

    TEST(ImageCompareTests, ADifferentSizeIsReportedAsSuchAndNotAsPixelDifferences)
    {
        // Comparing a 1600x900 capture against a 320x180 reference must say "different sizes", not
        // "everything differs" -- the second reads as a rendering regression and sends someone
        // looking at the renderer instead of at the test's own arguments.
        const auto diff = Compare(Solid(16, 16, Colour::White), Solid(8, 8, Colour::White), 0);
        EXPECT_TRUE(diff.sizeMismatch);
        EXPECT_EQ(diff.comparedPixels, 0u);
        EXPECT_NE(diff.ToString().find("different sizes"), std::string::npos);
    }

    TEST(ImageCompareTests, ToleranceAbsorbsRoundingButNotARealChange)
    {
        // The distinction the whole harness exists for: every pixel off by one is Mesa version
        // drift, and a few pixels off by 200 is a missing object.
        Image drifted = Solid(8, 8, Colour(100, 100, 100, 255));
        for (auto& pixel : drifted.pixels)
        {
            pixel = Colour(101, 99, 100, 255);
        }
        const Image reference = Solid(8, 8, Colour(100, 100, 100, 255));

        const auto absorbed = Compare(drifted, reference, 1);
        EXPECT_EQ(absorbed.differingPixels, 0u);
        EXPECT_EQ(absorbed.maxChannelDelta, 1) << "the delta is still REPORTED, just not counted";

        const auto strict = Compare(drifted, reference, 0);
        EXPECT_EQ(strict.differingPixels, 64u);
    }

    TEST(ImageCompareTests, OneVeryWrongPixelIsCaughtEvenThoughTheMeanIsTiny)
    {
        // 1 pixel in 4 096 at maximum error is a mean channel delta under 0.02. A harness that
        // asserted only on the mean would pass this, and it is exactly the shape of "one object
        // failed to draw" in a mostly empty frame.
        Image actual = Solid(64, 64, Colour::Black);
        actual.pixels[1234] = Colour::White;
        const Image reference = Solid(64, 64, Colour::Black);

        const auto diff = Compare(actual, reference, 4);
        EXPECT_EQ(diff.differingPixels, 1u);
        EXPECT_EQ(diff.maxChannelDelta, 255);
        EXPECT_LT(diff.meanChannelDelta, 0.05) << "which is why the mean is not the assertion";
        EXPECT_GT(diff.DifferingFraction(), 0.0);
    }

    TEST(ImageCompareTests, AlphaIsComparedToo)
    {
        // A frame that lost its alpha channel looks identical in a viewer that ignores it and is
        // wrong everywhere it is composited.
        const Image opaque = Solid(4, 4, Colour(10, 20, 30, 255));
        const Image transparent = Solid(4, 4, Colour(10, 20, 30, 0));
        const auto diff = Compare(opaque, transparent, 0);
        EXPECT_EQ(diff.differingPixels, 16u);
        EXPECT_EQ(diff.maxChannelDelta, 255);
    }

    TEST(ImageCompareTests, TheSummaryNamesTheNumbersAReaderNeeds)
    {
        Image actual = Solid(10, 10, Colour::Black);
        actual.pixels[0] = Colour::White;
        const auto diff = Compare(actual, Solid(10, 10, Colour::Black), 0);
        const std::string summary = diff.ToString();
        EXPECT_NE(summary.find("1 of 100"), std::string::npos) << summary;
        EXPECT_NE(summary.find("255"), std::string::npos) << summary;
    }

    TEST(ImageCompareTests, AnEmptyPairComparesWithoutDividingByZero)
    {
        const Image empty;
        const auto diff = Compare(empty, empty, 0);
        EXPECT_FALSE(diff.sizeMismatch);
        EXPECT_EQ(diff.comparedPixels, 0u);
        EXPECT_DOUBLE_EQ(diff.DifferingFraction(), 0.0);
        EXPECT_DOUBLE_EQ(diff.meanChannelDelta, 0.0) << "0.0/0.0 would be a NaN, which reads as "
                                                        "\"not greater than the threshold\"";
    }
} // namespace
