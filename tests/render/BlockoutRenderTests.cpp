// SPDX-License-Identifier: MIT
//
// `HOUSE-00475`: the first frame of the house itself. `blockout-01` is `--scene=blockout` at
// 1600x900 with Tier S, quality low and audio off -- every option pinned, so the only thing that
// can differ between two runs is the rasteriser.
//
// **The reference was generated under `LIBGL_ALWAYS_SOFTWARE=1`**, which is what `HOUSE-00138`'s CI
// job uses, because a software rasteriser is the only one whose output is reproducible on a machine
// nobody owns. On hardware the same test still runs and asserts what the frame CONTAINS rather than
// what it looks like -- and the content assertions are the ones worth having here: a house that
// failed to load draws a black frame, and a black frame passes any tolerance against a reference
// only if the reference is black too.
#include <cstdio>
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

    Options FixtureOptions()
    {
        Options options;
        options.tier = RenderTier::S;
        options.quality = QualityPreset::Low;
        options.noAudio = true;
        options.contentRoot = CNAHOUSE_TEST_CONTENT_ROOT;
        options.scene = "blockout";
        return options;
    }

    std::string OutputPath(const char* leaf)
    {
        return std::string(CNAHOUSE_TEST_OUTPUT_DIR) + "/" + leaf;
    }

    std::string ReferencePath()
    {
        return RenderHarness::ReferenceDirectory() + "/blockout-01.png";
    }

    /// Whether the content build has produced the geometry this scene draws.
    ///
    /// `chunks.bin` comes from the Blender shell by way of `build_chunks.py`, and a checkout that
    /// has not run either draws an empty frame. Skipping is right and failing is not: nothing in
    /// this test is broken then, and a suite that fails on a missing generated artefact teaches
    /// people to ignore it.
    bool ContentIsBuilt()
    {
        const std::string path = std::string(CNAHOUSE_TEST_CONTENT_ROOT) + "/world/chunks.bin";
        FILE* file = std::fopen(path.c_str(), "rb");
        if (file == nullptr)
        {
            return false;
        }
        std::fclose(file);
        return true;
    }

    TEST(BlockoutRenderTests, TheCapturedFrameMatchesTheCommittedReference)
    {
        if (!ContentIsBuilt())
        {
            GTEST_SKIP() << "no content/world/chunks.bin; run tools/ci/build_content.py --only "
                            "world after generating the shell with house_shell_gen.py";
        }
        if (!RenderHarness::RenderingInSoftware())
        {
            GTEST_SKIP() << "the reference is a software-rasteriser frame (HOUSE-00138); run this "
                            "with LIBGL_ALWAYS_SOFTWARE=1 to compare pixels. What the frame "
                            "contains is asserted by the other tests here, which do run.";
        }

        const auto diff =
            RenderHarness::CompareWithReference(FixtureOptions(),
                                                kWidth,
                                                kHeight,
                                                OutputPath("blockout-01-actual.png"),
                                                ReferencePath(),
                                                2,
                                                RenderHarness::NonDeterministicRegions(kWidth, kHeight));

        ASSERT_TRUE(diff.HasValue()) << diff.Error().ToString();
        EXPECT_FALSE(diff->sizeMismatch) << diff->ToString();
        // 0.05 % of 1 440 000 pixels is 720. A silhouette edge may move by a pixel between Mesa
        // versions; a house that moved, lost a wall or changed colour moves thousands.
        EXPECT_LT(diff->DifferingFraction(), 0.0005) << diff->ToString();
    }

    TEST(BlockoutRenderTests, TheSameFrameRenderedTwiceIsIdentical)
    {
        if (!ContentIsBuilt())
        {
            GTEST_SKIP() << "no content/world/chunks.bin";
        }
        // Without determinism the comparison above cannot mean anything. This one is worth having
        // for the blockout in particular: 418 chunks are drawn in the order the file lists them,
        // and a set or a hash map anywhere in that chain would reorder them between runs and
        // change every pixel where two surfaces are coplanar.
        const std::string first = OutputPath("blockout-01-run1.png");
        const std::string second = OutputPath("blockout-01-run2.png");
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
    }

    TEST(BlockoutRenderTests, TheFrameContainsAHouseAndNotAnEmptyRoom)
    {
        // Driver-independent, so it runs on hardware too, and it is the assertion that would catch
        // the frame this scene drew the first time it ran: a camera inside `EXT_WORLD`'s 400 m box
        // saw one flat colour over the whole frame, which is a perfectly stable picture of nothing.
        if (!ContentIsBuilt())
        {
            GTEST_SKIP() << "no content/world/chunks.bin";
        }
        const std::string path = OutputPath("blockout-01-coverage.png");
        ASSERT_TRUE(RenderHarness::CaptureFrame(FixtureOptions(), kWidth, kHeight, path));

        const auto loaded = RenderHarness::LoadPng(path);
        ASSERT_TRUE(loaded.HasValue()) << loaded.Error().ToString();
        const Image& image = *loaded;
        ASSERT_EQ(image.width, kWidth);
        ASSERT_EQ(image.height, kHeight);

        std::size_t background = 0;
        std::size_t drawn = 0;
        std::vector<std::size_t> byColumn(static_cast<std::size_t>(kWidth), 0);
        std::vector<std::size_t> byRow(static_cast<std::size_t>(kHeight), 0);
        for (int y = 0; y < kHeight; ++y)
        {
            for (int x = 0; x < kWidth; ++x)
            {
                const auto& pixel =
                    image.pixels[static_cast<std::size_t>(y) * static_cast<std::size_t>(kWidth) +
                                 static_cast<std::size_t>(x)];
                const bool isClear =
                    pixel.getRProperty() == 18 && pixel.getGProperty() == 20 && pixel.getBProperty() == 24;
                if (isClear)
                {
                    ++background;
                    continue;
                }
                ++drawn;
                ++byColumn[static_cast<std::size_t>(x)];
                ++byRow[static_cast<std::size_t>(y)];
            }
        }

        const double fraction =
            static_cast<double>(drawn) / (static_cast<double>(kWidth) * static_cast<double>(kHeight));
        // A house 22 m wide seen from 30 m away fills a good part of the frame and nothing like
        // all of it. Both ends of this range are real failures: nothing drawn, or the camera
        // inside something.
        EXPECT_GT(fraction, 0.10) << "only " << drawn
                                  << " pixels were drawn: the house did not "
                                     "load, or nothing is in front of the camera";
        EXPECT_LT(fraction, 0.75) << drawn << " pixels were drawn: the camera is inside something";
        EXPECT_GT(background, 0u);

        // The house is one connected mass, not a scatter: the drawn pixels occupy a contiguous
        // band of columns and of rows rather than the whole frame or a sprinkle across it.
        std::size_t columnsWithHouse = 0;
        for (const std::size_t count : byColumn)
        {
            columnsWithHouse += count > 20 ? 1u : 0u;
        }
        std::size_t rowsWithHouse = 0;
        for (const std::size_t count : byRow)
        {
            rowsWithHouse += count > 20 ? 1u : 0u;
        }
        EXPECT_GT(columnsWithHouse, 400u) << "the drawn geometry is too narrow to be the house";
        EXPECT_LT(columnsWithHouse, static_cast<std::size_t>(kWidth) - 100u)
            << "the drawn geometry spans the whole frame";
        EXPECT_GT(rowsWithHouse, 250u) << "the drawn geometry is too short to be the house";

        // ...and it is made of more than one material. The blockout colours a surface by its
        // material name, so a frame with one colour in it is a frame where that failed.
        std::size_t distinct = 0;
        std::vector<std::uint32_t> seen;
        for (const auto& pixel : image.pixels)
        {
            const std::uint32_t key = static_cast<std::uint32_t>(pixel.getRProperty()) << 16 |
                                      static_cast<std::uint32_t>(pixel.getGProperty()) << 8 |
                                      static_cast<std::uint32_t>(pixel.getBProperty());
            bool found = false;
            for (const std::uint32_t other : seen)
            {
                if (other == key)
                {
                    found = true;
                    break;
                }
            }
            if (!found && seen.size() < 64)
            {
                seen.push_back(key);
                ++distinct;
            }
        }
        EXPECT_GT(distinct, 6u) << "the frame has " << distinct
                                << " colours in it; a blockout coloured by material has many";
    }
} // namespace
