// SPDX-License-Identifier: MIT
//
// `HOUSE-00201`: the render fixture `content-smoke-01`. One model, one texture, one vendored Noto
// face and one compiled effect, in one frame, at 1600x900 with every option pinned.
//
// **The video panel is excluded from the comparison, by name.** Which frame the decoder has reached
// depends on wall-clock time, so it is genuinely not reproducible -- and `ImageCompare.hpp`'s rule
// is that such a region is named rather than absorbed by widening the tolerance. What the video
// *is* checked for here is that the panel is not empty; that it ADVANCES is checked by
// `ContentSmokeTests`, which can watch two hundred frames instead of one.
//
// The reference was generated under `LIBGL_ALWAYS_SOFTWARE=1`, which is what `HOUSE-00138`'s CI job
// uses, for the reason `title-01` records: a software rasteriser is the only one whose output is
// reproducible on a machine nobody owns.
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "cnahouse/app/CommandLine.hpp"
#include "cnahouse/content/SmokeScene.hpp"

#include "render/RenderHarness.hpp"

namespace
{
    using cnahouse::app::Options;
    using cnahouse::app::QualityPreset;
    using cnahouse::app::RenderTier;
    using cnahouse::content::SmokeScene;
    using cnahouse::testsupport::Image;
    using cnahouse::testsupport::Region;
    using cnahouse::testsupport::RenderHarness;

    constexpr int kWidth = 1600;
    constexpr int kHeight = 900;

    /// The first frame is too early for the video: `VideoPlayer` produces a texture when the
    /// decoder produces one, not when `Play` returns, so a capture of frame 1 shows an empty panel
    /// however healthy the pipeline is. Measured here: frame 1 is empty, frame 60 is not.
    constexpr std::uint64_t kCaptureFrame = 60;

    Options FixtureOptions()
    {
        Options options;
        options.scene = SmokeScene::kSceneName;
        options.tier = RenderTier::E;
        options.quality = QualityPreset::Low;
        options.noAudio = true;
        options.screenshotFrame = kCaptureFrame;
        options.contentRoot = CNAHOUSE_TEST_CONTENT_ROOT;
        options.effectRoot = CNAHOUSE_TEST_EFFECT_ROOT;
        return options;
    }

    std::string OutputPath(const char* suffix)
    {
        return std::string(CNAHOUSE_TEST_OUTPUT_DIR) + "/content-smoke-01-" + suffix + ".png";
    }

    std::string ReferencePath()
    {
        return RenderHarness::ReferenceDirectory() + "/content-smoke-01.png";
    }

    Region VideoPanel()
    {
        // A two-pixel margin, because the panel's edge lands on a rounded destination rectangle and
        // an edge texel is not what this exclusion is about.
        return Region{SmokeScene::kVideoPanelX - 2,
                      SmokeScene::kVideoPanelY - 2,
                      SmokeScene::kVideoPanelSize + 4,
                      SmokeScene::kVideoPanelSize + 4};
    }

    std::vector<Region> IgnoredRegions()
    {
        std::vector<Region> regions = RenderHarness::NonDeterministicRegions(kWidth, kHeight);
        regions.push_back(VideoPanel());
        return regions;
    }

    const Microsoft::Xna::Framework::Color& At(const Image& image, int x, int y)
    {
        return image.pixels[static_cast<std::size_t>(y) * static_cast<std::size_t>(image.width) +
                            static_cast<std::size_t>(x)];
    }

    /// How close a pixel is to a colour, as a squared distance. Generous slack is applied by the
    /// caller: the quadrants pass through a tint, a mip chain and a rasteriser.
    int DistanceTo(const Microsoft::Xna::Framework::Color& pixel, int r, int g, int b)
    {
        const int dr = pixel.getRProperty() - r;
        const int dg = pixel.getGProperty() - g;
        const int db = pixel.getBProperty() - b;
        return dr * dr + dg * dg + db * db;
    }

    TEST(ContentSmokeRenderTests, TheCapturedFrameMatchesTheCommittedReference)
    {
        if (!RenderHarness::RenderingInSoftware())
        {
            GTEST_SKIP() << "the reference is a software-rasteriser frame (HOUSE-00138); run this "
                            "with LIBGL_ALWAYS_SOFTWARE=1 to compare pixels. What the frame "
                            "CONTAINS is asserted by the other tests in this file, which do run "
                            "here.";
        }

        const auto diff = RenderHarness::CompareWithReference(
            FixtureOptions(), kWidth, kHeight, OutputPath("actual"), ReferencePath(), 2, IgnoredRegions());

        ASSERT_TRUE(diff.HasValue()) << diff.Error().ToString();
        EXPECT_FALSE(diff->sizeMismatch) << diff->ToString();
        // The same budget `title-01` uses: 0.05 % of 1 440 000 pixels is 720, which covers glyph
        // and triangle edges and does not cover a missing object.
        EXPECT_LT(diff->DifferingFraction(), 0.0005) << diff->ToString();
    }

    TEST(ContentSmokeRenderTests, TheSameFrameRenderedTwiceIsIdentical)
    {
        // Determinism first: without it the comparison above cannot mean anything. The video panel
        // is excluded, because it is the one part of this frame that legitimately is not.
        const std::string first = OutputPath("run1");
        ASSERT_TRUE(RenderHarness::CaptureFrame(FixtureOptions(), kWidth, kHeight, first));
        const auto diff = RenderHarness::CompareWithReference(
            FixtureOptions(), kWidth, kHeight, OutputPath("run2"), first, 0, IgnoredRegions());

        ASSERT_TRUE(diff.HasValue()) << diff.Error().ToString();
        EXPECT_EQ(diff->differingPixels, 0u)
            << "two runs of the same build must be bit-identical outside the video panel: "
            << diff->ToString();
        EXPECT_EQ(diff->maxChannelDelta, 0) << diff->ToString();
    }

    TEST(ContentSmokeRenderTests, TheModelIsDrawnWithItsOwnTextureThroughTheCompiledEffect)
    {
        // Driver-independent, so it runs on hardware too, and it asserts what the frame CONTAINS.
        // A stale reference plus a tolerance would pass a frame with no model in it; this does not.
        const std::string path = OutputPath("coverage");
        ASSERT_TRUE(RenderHarness::CaptureFrame(FixtureOptions(), kWidth, kHeight, path));
        const auto loaded = RenderHarness::LoadPng(path);
        ASSERT_TRUE(loaded.HasValue()) << loaded.Error().ToString();
        const Image& image = *loaded;
        ASSERT_EQ(image.width, kWidth);
        ASSERT_EQ(image.height, kHeight);

        // The four authored quadrant colours (`make_smoke_assets.py`), each in two forms: as the
        // texture holds them, and multiplied by the effect's tint (1.0, 0.86, 0.62).
        //
        // Both forms are counted, and that is the whole design of this check. Finding the TINTED
        // colours on the model proves the texture reached the sampler; finding NO untinted ones
        // there proves the tint reached the shader -- which counting only "is it roughly this
        // colour" cannot, because red tinted and red untinted are 16 units apart and any slack
        // wide enough for a rasteriser swallows the difference. Blue is the pair that carries the
        // claim: 143 against 230 in the blue channel.
        struct Quadrant
        {
            const char* name;
            int r;
            int g;
            int b;
            int rawR;
            int rawG;
            int rawB;
        };

        const Quadrant quadrants[] = {
            {"red", 220, 34, 25, 220, 40, 40},
            {"green", 40, 172, 37, 40, 200, 60},
            {"blue", 50, 77, 143, 50, 90, 230},
            {"yellow", 235, 172, 25, 235, 200, 40},
        };

        // The two SpriteBatch panels draw the raw texture and the decoded video, untinted and by
        // design. They are excluded, or the untinted count below would find them and say the
        // shader had failed.
        const Region texturePanel{SmokeScene::kTexturePanelX - 2,
                                  SmokeScene::kTexturePanelY - 2,
                                  SmokeScene::kTexturePanelSize + 4,
                                  SmokeScene::kTexturePanelSize + 4};

        std::size_t tinted[4] = {0, 0, 0, 0};
        std::size_t untinted[4] = {0, 0, 0, 0};
        std::size_t clear = 0;
        for (int y = 0; y < image.height; ++y)
        {
            for (int x = 0; x < image.width; ++x)
            {
                if (VideoPanel().Contains(x, y) || texturePanel.Contains(x, y))
                {
                    continue;
                }
                const auto& pixel = At(image, x, y);
                if (pixel.getRProperty() == 18 && pixel.getGProperty() == 20 && pixel.getBProperty() == 24)
                {
                    ++clear;
                    continue;
                }
                for (std::size_t index = 0; index < 4; ++index)
                {
                    // 12 per channel. The shader point-samples and does not filter, so the rendered
                    // texel is the authored one exactly; the slack is for a rasteriser edge, and it
                    // is deliberately narrower than the gap between a tinted and an untinted texel.
                    if (DistanceTo(pixel, quadrants[index].r, quadrants[index].g, quadrants[index].b) <=
                        3 * 12 * 12)
                    {
                        ++tinted[index];
                        break;
                    }
                    if (DistanceTo(
                            pixel, quadrants[index].rawR, quadrants[index].rawG, quadrants[index].rawB) <=
                        3 * 12 * 12)
                    {
                        ++untinted[index];
                        break;
                    }
                }
            }
        }

        for (std::size_t index = 0; index < 4; ++index)
        {
            EXPECT_GT(tinted[index], 2000u)
                << "the " << quadrants[index].name
                << " quadrant is missing from the frame: the model is not drawn, or it is not "
                   "drawn with Textures/Smoke/quadrants through the tinted effect";
        }
        // The blue pair is the one whose two forms are far enough apart to be unambiguous, so its
        // failure message is the one that names what went wrong.
        EXPECT_EQ(untinted[2], 0u)
            << "the model carries the texture's own blue (50, 90, 230) rather than the tinted "
               "(50, 77, 143): the effect's TintColor parameter never reached the shader, or the "
               "model was drawn with a stock effect instead";
        EXPECT_GT(clear, image.pixels.size() / 3)
            << "most of a smoke frame should still be the clear colour; a full frame means "
               "something drew over everything";
    }

    TEST(ContentSmokeRenderTests, TheFontAndTheVideoPanelBothCarryPixels)
    {
        const std::string path = OutputPath("panels");
        ASSERT_TRUE(RenderHarness::CaptureFrame(FixtureOptions(), kWidth, kHeight, path));
        const auto loaded = RenderHarness::LoadPng(path);
        ASSERT_TRUE(loaded.HasValue()) << loaded.Error().ToString();
        const Image& image = *loaded;

        auto countIn = [&image](Region region, bool bright)
        {
            std::size_t count = 0;
            for (int y = region.y; y < region.y + region.height; ++y)
            {
                for (int x = region.x; x < region.x + region.width; ++x)
                {
                    const auto& pixel = At(image, x, y);
                    const bool isClear = pixel.getRProperty() == 18 && pixel.getGProperty() == 20 &&
                                         pixel.getBProperty() == 24;
                    if (bright ? pixel.getRProperty() > 120 : !isClear)
                    {
                        ++count;
                    }
                }
            }
            return count;
        };

        // The scene's own font draws six report lines down the left. `Fonts/ui-22` is a different
        // face from the HUD's `ui-16`, so a build where ui-22 failed shows the corner lines and
        // nothing else -- which this band would catch and a whole-frame count would not.
        //
        // BRIGHT pixels, not non-clear ones. `DrawShadowed` draws each glyph twice and the
        // antialiased tail of a shadow is a pixel one or two levels off the clear colour; counting
        // those made a band of ordinary text read as 27 % covered, which is a threshold that
        // measures the antialiaser rather than the text. The band also stops at x = 500, short of
        // the model, so the count is text and nothing else.
        const Region textBand{40, 110, 460, 260};
        const std::size_t textPixels = countIn(textBand, true);
        EXPECT_GT(textPixels, 4000u) << "the six report lines are missing: Fonts/ui-22 did not reach a glyph";
        EXPECT_LT(textPixels,
                  static_cast<std::size_t>(textBand.width) * static_cast<std::size_t>(textBand.height) / 3)
            << "text should not fill its band";

        // The video panel, checked for CONTENT rather than for content it cannot promise. At frame
        // 60 the decoder has produced something; which frame is the integration test's question.
        const std::size_t videoPixels = countIn(VideoPanel(), false);
        EXPECT_GT(videoPixels,
                  static_cast<std::size_t>(SmokeScene::kVideoPanelSize) *
                      static_cast<std::size_t>(SmokeScene::kVideoPanelSize) / 2)
            << "the video panel is empty at frame " << kCaptureFrame
            << ": VideoPlayer::GetTexture never returned a frame in time";

        // ...and the source texture is drawn beside it, so the two can be compared by eye when a
        // human looks at the fixture.
        const std::size_t texturePixels = countIn(Region{SmokeScene::kTexturePanelX,
                                                         SmokeScene::kTexturePanelY,
                                                         SmokeScene::kTexturePanelSize,
                                                         SmokeScene::kTexturePanelSize},
                                                  false);
        EXPECT_GT(texturePixels,
                  static_cast<std::size_t>(SmokeScene::kTexturePanelSize) *
                      static_cast<std::size_t>(SmokeScene::kTexturePanelSize) / 2)
            << "the source texture panel is empty";
    }

} // namespace
