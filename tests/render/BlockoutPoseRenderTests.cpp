// SPDX-License-Identifier: MIT
//
// `HOUSE-00483`: the blockout from twenty places. Eight outside and twelve inside, each captured
// through `--scene=blockout --camera=...` and compared with a committed reference.
//
// **Why twenty small frames rather than one big one.** `blockout-01` is the house from the road and
// says nothing at all about the inside of it: a wall that faces the wrong way, a ceiling at the
// wrong height, a stair that arrives in a wall are all invisible from outside the front door. Each
// pose here stands somewhere a person would and looks at what is in front of them. They are 640x360
// on purpose -- twenty references at 1600x900 would be 12 MB of PNG in the repository, and what
// these catch is a room that changed shape, not a pixel that moved.
//
// The references were generated under `LIBGL_ALWAYS_SOFTWARE=1`, which is what CI uses
// (`HOUSE-00138`). On hardware the comparison is skipped and the coverage assertions still run.
#include <algorithm>
#include <array>
#include <cstdio>
#include <cstdlib>
#include <format>
#include <string>
#include <string_view>
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

    /// Small on purpose: see the file comment.
    constexpr int kWidth = 640;
    constexpr int kHeight = 360;
    constexpr int kChannelTolerance = 2;
    constexpr double kDifferingFractionLimit = 0.002;

    struct Pose
    {
        const char* name;
        std::array<float, 6> camera;
        /// @brief The least of the frame this pose may cover. Standing in a ROOM, the room fills
        ///        it; standing on a landing that opens onto a stairwell, some of what is in front
        ///        of you is the well and beyond it the sky, and that is the cell being what it is.
        double minCoverage = 0.90;
    };

    /// Eight outside. The plot is 45 m of road frontage at z = 0..13.4 and the house sits between
    /// z = -11.6 and z = -27.1, so these ring it at a distance that fits it in the frame.
    constexpr Pose kExterior[] = {
        {"ext-road", {2.00f, 12.00f, 16.00f, 0.00f, 5.00f, -19.00f}},
        {"ext-front-door", {0.00f, 2.60f, -6.00f, 0.00f, 2.00f, -13.00f}},
        {"ext-southeast", {22.00f, 10.00f, 8.00f, 2.00f, 5.00f, -20.00f}},
        {"ext-east", {26.00f, 8.00f, -18.00f, 8.00f, 5.00f, -19.00f}},
        {"ext-northeast", {20.00f, 12.00f, -40.00f, 2.00f, 6.00f, -22.00f}},
        {"ext-north", {-2.00f, 9.00f, -42.00f, -2.00f, 5.00f, -26.00f}},
        {"ext-west", {-30.00f, 8.00f, -20.00f, -10.00f, 5.00f, -20.00f}},
        {"ext-above", {14.00f, 30.00f, 6.00f, -1.00f, 6.00f, -21.00f}},
    };

    /// Twelve inside: one per level at least, standing at eye height in the middle of the room and
    /// looking down its longer axis. Derived from the layout's own boxes, which is why the numbers
    /// are not round.
    constexpr Pose kInterior[] = {
        {"b1-cinema", {3.10f, -0.70f, -25.05f, 8.30f, -0.85f, -25.05f}},
        // HOUSE-01040: the old floating eye stood over the new sink run. Keep the explicit
        // debug/blockout view, but inspect both kitchen built-ins from the clear east aisle.
        {"l0-kitchen", {-1.10f, 2.20f, -25.05f, -7.30f, 2.05f, -25.05f}},
        {"l0-living", {-7.30f, 2.20f, -17.25f, -2.60f, 2.05f, -17.25f}},
        {"l0-hall", {0.00f, 2.20f, -22.10f, 0.00f, 2.05f, -18.70f}},
        // HOUSE-00489: the former floating eye is inside the mirrored U's return. Look up the
        // first run from its actual east-side foot, matching the playable review camera.
        {"l0-stair-main", {4.10f, 2.20f, -14.70f, 4.10f, 2.05f, -18.50f}},
        {"l0-garage", {9.60f, 1.75f, -17.50f, 16.70f, 1.60f, -17.50f}},
        {"l1-master-bed", {-6.10f, 5.25f, -24.55f, 1.80f, 5.10f, -24.55f}},
        {"l1-master-bath", {-11.05f, 5.25f, -26.20f, -11.05f, 5.10f, -22.40f}},
        // `visibilityHint: open`. The right of this frame is the main stairwell and the sky over
        // it, which is what an open landing looks like -- 75 %, measured.
        {"l2-landing", {-1.30f, 8.15f, -16.30f, 1.80f, 8.00f, -16.30f}, 0.60},
        {"l2-library", {-7.30f, 8.15f, -16.30f, -2.60f, 8.00f, -16.30f}},
        {"l3-room", {-5.10f, 10.90f, -20.50f, 4.50f, 10.75f, -20.50f}},
        {"l3-store-w", {-9.35f, 10.90f, -26.20f, -9.35f, 10.75f, -14.70f}},
    };

    Options OptionsFor(const Pose& pose)
    {
        Options options;
        options.tier = RenderTier::S;
        options.quality = QualityPreset::Low;
        options.noAudio = true;
        options.contentRoot = CNAHOUSE_TEST_CONTENT_ROOT;
        options.scene = "blockout";
        options.camera = pose.camera;
        return options;
    }

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

    /// How much of @p image is not the clear colour.
    double Coverage(const Image& image)
    {
        std::size_t drawn = 0;
        for (const auto& pixel : image.pixels)
        {
            const bool isClear =
                pixel.getRProperty() == 18 && pixel.getGProperty() == 20 && pixel.getBProperty() == 24;
            drawn += isClear ? 0u : 1u;
        }
        return static_cast<double>(drawn) / static_cast<double>(image.pixels.size());
    }

    /// How many distinct colours @p image contains, counting no further than @p cap.
    ///
    /// The blockout colours a surface by its material, so this is "how many different things are
    /// in shot". One is a camera inside geometry -- the failure the coverage ceiling used to
    /// catch, before the property under the house made a full frame an ordinary thing to see.
    std::size_t DistinctColours(const Image& image, std::size_t cap)
    {
        std::vector<std::uint32_t> seen;
        for (const auto& pixel : image.pixels)
        {
            const std::uint32_t key = static_cast<std::uint32_t>(pixel.getRProperty()) << 16 |
                                      static_cast<std::uint32_t>(pixel.getGProperty()) << 8 |
                                      static_cast<std::uint32_t>(pixel.getBProperty());
            if (std::find(seen.begin(), seen.end(), key) == seen.end())
            {
                seen.push_back(key);
                if (seen.size() >= cap)
                {
                    break;
                }
            }
        }
        return seen.size();
    }

    void CompareOnePose(const Pose& pose, bool interior, std::string_view referencePose = {})
    {
        const std::string actual =
            std::string(CNAHOUSE_TEST_OUTPUT_DIR) + "/blockout-" + pose.name + "-actual.png";
        const std::string reference = RenderHarness::ReferenceDirectory() + "/blockout-" +
                                      std::string(referencePose.empty() ? pose.name : referencePose) + ".png";

        if (RenderHarness::RenderingInSoftware())
        {
            const auto diff = RenderHarness::CompareWithReference(
                OptionsFor(pose), kWidth, kHeight, actual, reference, kChannelTolerance, {});
            ASSERT_TRUE(diff.HasValue()) << pose.name << ": " << diff.Error().ToString();
            EXPECT_FALSE(diff->sizeMismatch) << pose.name << ": " << diff->ToString();
            if (!diff->sizeMismatch && diff->DifferingFraction() >= kDifferingFractionLimit)
            {
                const std::string difference =
                    std::string(CNAHOUSE_TEST_OUTPUT_DIR) + "/blockout-" + pose.name + "-diff.png";
                const auto written =
                    RenderHarness::WriteDifferenceImage(actual, reference, difference, kChannelTolerance, {});
                EXPECT_TRUE(written) << pose.name << ": could not write " << difference << ": "
                                     << written.Error().ToString();
                const std::string diagnostics =
                    std::format("{}: {}; budget < {:.4f}% pixels above channel tolerance {}; "
                                "difference image {}",
                                pose.name,
                                diff->ToString(),
                                kDifferingFractionLimit * 100.0,
                                kChannelTolerance,
                                difference);
                EXPECT_LT(diff->DifferingFraction(), kDifferingFractionLimit) << diagnostics;
            }
        }
        else
        {
            ASSERT_TRUE(RenderHarness::CaptureFrame(OptionsFor(pose), kWidth, kHeight, actual));
        }

        const auto loaded = RenderHarness::LoadPng(actual);
        ASSERT_TRUE(loaded.HasValue()) << pose.name << ": " << loaded.Error().ToString();
        const double coverage = Coverage(*loaded);
        if (interior)
        {
            // Standing in a room, the room fills the frame. Anything less means a wall is missing
            // or the camera is inside the geometry rather than in the room.
            EXPECT_GT(coverage, pose.minCoverage)
                << pose.name << " covers only " << (coverage * 100.0) << " % of the frame, under its own "
                << (pose.minCoverage * 100.0) << " %: a surface is missing";
        }
        else
        {
            // Outside, the house is in front of a background. Both ends are failures: nothing
            // drawn, or the camera inside something.
            EXPECT_GT(coverage, 0.05) << pose.name << " drew almost nothing";
            // `HOUSE-00495`: the ceiling on this used to be 0.98, and "the camera is inside
            // something" was what a full frame meant while the house stood in a void -- every
            // outdoor pose had sky round it because there was nothing else. `HOUSE-00780` put the
            // lot in the picture, and `ext-front-door` stands 7 m from the front door on the walk:
            // house above, path below, and not a pixel of sky, legitimately. What the ceiling was
            // protecting is asked directly instead -- a camera inside geometry sees ONE colour.
            EXPECT_GT(DistinctColours(*loaded, 8u), 2u)
                << pose.name << " is one or two flat colours: the camera is inside something";
        }
    }

    TEST(BlockoutPoseRenderTests, TheEightExteriorPosesMatchTheirReferences)
    {
        if (!ContentIsBuilt())
        {
            GTEST_SKIP() << "no content/world/chunks.bin";
        }
        for (const Pose& pose : kExterior)
        {
            SCOPED_TRACE(pose.name);
            CompareOnePose(pose, false);
        }
    }

    TEST(BlockoutPoseRenderTests, TheTwelveInteriorPosesMatchTheirReferences)
    {
        if (!ContentIsBuilt())
        {
            GTEST_SKIP() << "no content/world/chunks.bin";
        }
        for (const Pose& pose : kInterior)
        {
            SCOPED_TRACE(pose.name);
            CompareOnePose(pose, true);
        }
    }

    TEST(RenderDiagnosticsTests, PerturbedGoldenFailsWithANamedPoseAndDifferenceImage)
    {
        const char* verify = std::getenv("CNAHOUSE_VERIFY_RENDER_DIAGNOSTICS");
        if (verify == nullptr || verify[0] != '1')
        {
            GTEST_SKIP() << "set CNAHOUSE_VERIFY_RENDER_DIAGNOSTICS=1 to prove the failure path";
        }
        ASSERT_TRUE(RenderHarness::RenderingInSoftware())
            << "the controlled diagnostic uses the committed software references";

        // A real but deliberately wrong golden proves the whole capture/decode/compare/PNG path
        // without editing a committed reference. This test is expected to fail when enabled.
        CompareOnePose(kExterior[0], false, "ext-east");
    }

    /// @brief Rewrites all twenty references. DISABLED, and run by hand after a geometry change:
    ///
    ///     LIBGL_ALWAYS_SOFTWARE=1 ./build/cnahouse_render_tests
    ///         --gtest_also_run_disabled_tests --gtest_filter=*Regenerate*
    ///
    /// A reference suite this size is regenerated often -- every change to the shell moves all
    /// twenty -- and the alternative to a documented way of doing it is twenty hand-run commands
    /// whose back-buffer size somebody eventually gets wrong.
    TEST(BlockoutPoseRenderTests, DISABLED_RegenerateReferences)
    {
        ASSERT_TRUE(ContentIsBuilt()) << "no content/world/chunks.bin";
        ASSERT_TRUE(RenderHarness::RenderingInSoftware())
            << "the references are software-rasteriser frames (HOUSE-00138); run this with "
               "LIBGL_ALWAYS_SOFTWARE=1 or they will not match anywhere else";
        std::size_t written = 0;
        for (const Pose* group : {kExterior + 0, kInterior + 0})
        {
            const std::size_t count = group == kExterior ? std::size(kExterior) : std::size(kInterior);
            for (std::size_t i = 0; i < count; ++i)
            {
                const Pose& pose = group[i];
                const std::string path =
                    RenderHarness::ReferenceDirectory() + "/blockout-" + pose.name + ".png";
                ASSERT_TRUE(RenderHarness::CaptureFrame(OptionsFor(pose), kWidth, kHeight, path))
                    << pose.name;
                ++written;
            }
        }
        EXPECT_EQ(written, std::size(kExterior) + std::size(kInterior));
    }

    TEST(BlockoutPoseRenderTests, TheSuiteIsTwentyPosesAndEveryNameIsDistinct)
    {
        // The count is §-mandated -- `HOUSE-00483` asks for 8 and 12 -- and a duplicate name would
        // have two poses share one reference, so one of them would never be compared with anything.
        EXPECT_EQ(std::size(kExterior), 8u);
        EXPECT_EQ(std::size(kInterior), 12u);
        std::vector<std::string> names;
        for (const Pose& pose : kExterior)
        {
            names.emplace_back(pose.name);
        }
        for (const Pose& pose : kInterior)
        {
            names.emplace_back(pose.name);
        }
        std::sort(names.begin(), names.end());
        EXPECT_EQ(std::adjacent_find(names.begin(), names.end()), names.end())
            << "two poses share a name, so they share a reference";
        // An eye and a target in the same place has no direction to look in.
        for (const Pose& pose : kExterior)
        {
            const auto& c = pose.camera;
            EXPECT_GT((c[3] - c[0]) * (c[3] - c[0]) + (c[4] - c[1]) * (c[4] - c[1]) +
                          (c[5] - c[2]) * (c[5] - c[2]),
                      1.0f)
                << pose.name << " looks at its own eye";
        }
    }
} // namespace
