// SPDX-License-Identifier: MIT
//
// `HOUSE-00781`: the PROPERTY from the eight places a person stands on it -- the road, the drive,
// the front walk, both side yards, the terrace, the garden and the orchard.
//
// **Why these are not `HOUSE-00483`'s eight.** Those ring the house at a distance and frame the
// building; every one of them was chosen before the lot existed, and what they photograph is the
// silhouette. These stand ON the property at eye height and look along it, which is the only way
// to see what `HOUSE-00761`…`HOUSE-00780` built: the ground falling 0.92 m from the porch to the
// north fence, the board fence and the ornamental one, the kerbs, the drive, the shed, the garden
// structures and the road. A frame from 30 m up says nothing about whether the terrace paving
// meets the sunroom door.
//
// Eye height is §43.1's 1.68 m over the ground AT THAT POINT, read from §11.5's height field --
// the orchard is 0.29 m below the front walk, and a camera at a fixed +1.68 world would be
// looking out of a person's chest there.
//
// 640x360, as `HOUSE-00483`'s are and for the same reason: what these catch is a lawn that
// stopped being drawn, not a pixel that moved. The references are software-rasteriser frames
// (`HOUSE-00138`); on hardware the comparison skips and the coverage claims still run.
#include <algorithm>
#include <array>
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

    constexpr int kWidth = 640;
    constexpr int kHeight = 360;

    struct Pose
    {
        const char* name;
        /// Eye and target: `ex, ey, ez, tx, ty, tz`.
        std::array<float, 6> camera;
        /// What the pose is for, in the failure message when it stops showing it.
        const char* looking;
        /// Whether any sky is expected at the top of the frame.
        ///
        /// Five of the eight look ACROSS the property and have sky over what they look at. Three
        /// stand within 8 m of a 13.5 m elevation -- the front walk 6 m from the porch, and both
        /// side yards 8 m from a three-storey wall -- and a 70° lens there is filled by the
        /// house, which is what standing next to a house looks like. Written down per pose rather
        /// than dropped for all eight, because "the lawn stopped being drawn" is exactly what the
        /// other five would catch.
        bool sky = true;
    };

    /// The eight §11 places, each at §43.1's eye height over the ground under it.
    constexpr Pose kProperty[] = {
        {"road", {0.00f, 1.68f, 8.00f, 0.00f, 4.50f, -14.00f}, "the house across the front fence"},
        {"drive", {13.00f, 1.68f, -4.00f, 13.00f, 3.00f, -14.00f}, "the garage door up the drive"},
        {"front-walk", {0.00f, 1.68f, -6.00f, 0.00f, 2.20f, -12.00f}, "the porch and the front door", false},
        {"sideyard-west",
         {-17.00f, 1.62f, -20.00f, -9.00f, 4.00f, -20.00f},
         "the west elevation over the side yard",
         false},
        {"sideyard-east",
         {19.00f, 1.62f, -20.00f, 11.00f, 3.50f, -20.00f},
         "the garage wing over the east side yard",
         false},
        // ALONG the terrace and not across it: the paving is 3.9 m deep and the sunroom's wall is
        // 3.3 m tall, so a pose facing it from the far edge is a wall filling the frame and says
        // nothing about either.
        {"terrace",
         {-5.50f, 2.13f, -34.60f, 5.50f, 3.20f, -33.20f},
         "the terrace paving, the sunroom's flank and the garden beyond it"},
        // Back far enough to see the shed AND what is round it: from 3 m its 3.2 m wall is the
        // whole frame, which is a picture of a wall and not of a garden.
        {"garden",
         {-13.20f, 1.68f, -37.60f, -18.80f, 1.30f, -42.60f},
         "the shed across the raised beds, with the boundary fence beyond"},
        {"orchard",
         {14.50f, 1.39f, -42.00f, 4.00f, 5.00f, -30.00f},
         "the house from the orchard, across the back lawn"},
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

    bool IsClear(const Image& image, std::size_t index)
    {
        const auto& pixel = image.pixels[index];
        return pixel.getRProperty() == 18 && pixel.getGProperty() == 20 && pixel.getBProperty() == 24;
    }

    std::string ReferencePath(const Pose& pose)
    {
        return RenderHarness::ReferenceDirectory() + "/property-" + pose.name + ".png";
    }

    std::string ActualPath(const Pose& pose)
    {
        return std::string(CNAHOUSE_TEST_OUTPUT_DIR) + "/property-" + pose.name + "-actual.png";
    }

    TEST(PropertyPoseRenderTests, TheEightPlacesMatchTheirReferences)
    {
        if (!ContentIsBuilt())
        {
            GTEST_SKIP() << "no content/world/chunks.bin";
        }
        if (!RenderHarness::RenderingInSoftware())
        {
            GTEST_SKIP() << "the references are software-rasteriser frames (HOUSE-00138); the "
                            "claims about what each frame CONTAINS run below on any driver";
        }
        for (const Pose& pose : kProperty)
        {
            SCOPED_TRACE(pose.name);
            const auto diff = RenderHarness::CompareWithReference(
                OptionsFor(pose), kWidth, kHeight, ActualPath(pose), ReferencePath(pose), 2, {});
            ASSERT_TRUE(diff.HasValue()) << pose.name << ": " << diff.Error().ToString();
            EXPECT_FALSE(diff->sizeMismatch) << pose.name << ": " << diff->ToString();
            EXPECT_LT(diff->DifferingFraction(), 0.0005)
                << pose.name << " (" << pose.looking << "): " << diff->ToString();
        }
    }

    TEST(PropertyPoseRenderTests, EveryPlaceHasGroundUnderItAndSkyOverIt)
    {
        // The claim that survives a driver: standing outdoors you see ground below and sky above.
        // A pose that lost its lawn draws sky to the bottom of the frame; one that ended up inside
        // the house or under a roof has no sky at all. Both were live failures this phase --
        // `HOUSE-00774` walled the yards, `HOUSE-00496` roofed the sky over.
        if (!ContentIsBuilt())
        {
            GTEST_SKIP() << "no content/world/chunks.bin";
        }
        for (const Pose& pose : kProperty)
        {
            SCOPED_TRACE(pose.name);
            ASSERT_TRUE(RenderHarness::CaptureFrame(OptionsFor(pose), kWidth, kHeight, ActualPath(pose)));
            const auto loaded = RenderHarness::LoadPng(ActualPath(pose));
            ASSERT_TRUE(loaded.HasValue()) << loaded.Error().ToString();

            std::size_t drawnInBottomRow = 0;
            std::size_t clearInTopRow = 0;
            for (int x = 0; x < kWidth; ++x)
            {
                const std::size_t bottom =
                    static_cast<std::size_t>(kHeight - 1) * static_cast<std::size_t>(kWidth) +
                    static_cast<std::size_t>(x);
                drawnInBottomRow += IsClear(*loaded, bottom) ? 0u : 1u;
                clearInTopRow += IsClear(*loaded, static_cast<std::size_t>(x)) ? 1u : 0u;
            }
            EXPECT_GT(drawnInBottomRow, static_cast<std::size_t>(kWidth) * 9u / 10u)
                << pose.name << " (" << pose.looking << ") has no ground under it: " << drawnInBottomRow
                << " of " << kWidth << " pixels along the bottom are drawn";
            if (pose.sky)
            {
                EXPECT_GT(clearInTopRow, static_cast<std::size_t>(kWidth) / 4u)
                    << pose.name << " (" << pose.looking << ") has no sky over it: " << clearInTopRow
                    << " of " << kWidth << " pixels along the top are background";
            }
            else
            {
                // ...and the three that stand next to the house must still be looking AT it: sky
                // across the top from here is a frame the house has vanished from.
                EXPECT_LT(clearInTopRow, static_cast<std::size_t>(kWidth) / 2u)
                    << pose.name << " (" << pose.looking << ") is all sky at the top: the house it "
                    << "stands next to is not being drawn (" << clearInTopRow << " of " << kWidth << ")";
            }
        }
    }

    TEST(PropertyPoseRenderTests, TheSuiteIsEightPlacesAndEveryNameIsDistinct)
    {
        // `HOUSE-00781` asks for eight, and a duplicate name would give two poses one reference,
        // so one of them would never be compared with anything.
        EXPECT_EQ(std::size(kProperty), 8u);
        std::vector<std::string> names;
        for (const Pose& pose : kProperty)
        {
            names.emplace_back(pose.name);
            const auto& c = pose.camera;
            EXPECT_GT((c[3] - c[0]) * (c[3] - c[0]) + (c[4] - c[1]) * (c[4] - c[1]) +
                          (c[5] - c[2]) * (c[5] - c[2]),
                      1.0f)
                << pose.name << " looks at its own eye";
            // §10.3's playable volume, which is where a person can stand.
            EXPECT_GE(c[0], -40.0f);
            EXPECT_LE(c[0], 40.0f);
            EXPECT_GE(c[2], -52.0f);
            EXPECT_LE(c[2], 12.0f);
        }
        std::sort(names.begin(), names.end());
        EXPECT_EQ(std::adjacent_find(names.begin(), names.end()), names.end())
            << "two poses share a name, so they share a reference";
    }

    TEST(DISABLED_PropertyPoseRenderTests, RegeneratePropertyReferences)
    {
        ASSERT_TRUE(ContentIsBuilt()) << "no content/world/chunks.bin";
        ASSERT_TRUE(RenderHarness::RenderingInSoftware())
            << "the references are software-rasteriser frames (HOUSE-00138); run this with "
               "LIBGL_ALWAYS_SOFTWARE=1 or they will not match anywhere else";
        for (const Pose& pose : kProperty)
        {
            ASSERT_TRUE(RenderHarness::CaptureFrame(OptionsFor(pose), kWidth, kHeight, ReferencePath(pose)))
                << pose.name;
        }
    }
} // namespace
