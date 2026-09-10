// SPDX-License-Identifier: MIT
//
// `HOUSE-00688`. §25.8's *"no over-culling"* test, and §70.4's *"culling sanity"* family: render
// each pose normally and with culling disabled, and require the two images to match.
//
// **This is the single most important test in the project.** Everything else about §25 measures how
// much it saves; this is the one that says what it saves was not being looked at. A failure means
// something visible was culled, and no amount of budget makes that acceptable.
//
// **It needs no committed reference.** The two frames are of the same pose in the same session
// shape, so one IS the other's reference -- which also means the test cannot be quietly satisfied
// by regenerating a picture.
#include <algorithm>
#include <array>
#include <cstdio>
#include <filesystem>
#include <iterator>
#include <string>

#include <gtest/gtest.h>

#include "cnahouse/app/CommandLine.hpp"

#include "render/RenderHarness.hpp"
#include "unit/VisibilityPoses.hpp"

namespace
{
    using cnahouse::app::Options;
    using cnahouse::app::QualityPreset;
    using cnahouse::testsupport::kVisibilityPoses;

    /// The three poses that still differ, and every one of them is the OUTDOORS rather than a
    /// cell's boundary surface. Measured 2026-09-10, after `HOUSE-00488`: `ext-backyard` 31.23 %,
    /// `ext-terrace` 2.00 %, `b1-gym` 0.50 % -- the last one losing `TERRAIN_grass` through a
    /// basement window well. The ground is per-cell CHUNKS until `HOUSE-00852` puts the outdoors
    /// on §25.6's instance path, and thirteen pairs of exterior cells abut with no portal between
    /// them; §25.6 says portals cannot help there, so authoring them would be the wrong fix.
    constexpr std::array<const char*, 3> kOutdoorsPending{"b1-gym", "ext-terrace", "ext-backyard"};
    using cnahouse::testsupport::RenderHarness;
    using cnahouse::testsupport::VisibilityPose;

    constexpr int kWidth = 640;
    constexpr int kHeight = 360;

    bool ContentIsBuilt()
    {
        return std::filesystem::exists("content/world/collision.bin") &&
               std::filesystem::exists("content/world/chunks.bin");
    }

    Options OptionsFor(const VisibilityPose& pose, bool cull)
    {
        Options options;
        options.tier = cnahouse::app::RenderTier::S;
        options.quality = QualityPreset::Low;
        options.noAudio = true;
        options.contentRoot = CNAHOUSE_TEST_CONTENT_ROOT;
        options.scene = "walk";
        options.player = std::array<float, 5>{pose.x, pose.y, pose.z, pose.yawDegrees, 0.0F};
        options.noCull = !cull;
        return options;
    }

    std::string OutputPath(const std::string& leaf)
    {
        return std::string(CNAHOUSE_TEST_OUTPUT_DIR) + "/" + leaf;
    }

} // namespace

/// ENABLED since `HOUSE-00488` (2026-09-10), with the three poses that still differ PINNED.
///
/// It was disabled because it failed on eight of the eighteen poses §65.6's door state lets it
/// compare, for two reasons that were both real and neither of them the culling: 38 431 pixels
/// changing SURFACE (`HOUSE-00485`'s coplanar pairs, since fixed) and 8 450 pixels becoming the
/// CLEAR COLOUR (`HOUSE-00488`, geometry a cell looks at but does not own).
///
/// Fifteen of the eighteen are clean now. `HOUSE-00488` closed the interior half: `L1_LANDING` and
/// `L2_LANDING` got their own walls, a nested cell's shell is drawn against its parent's cones,
/// and -- last -- a rafter-bounded attic draws the RAFTERS it looks up at instead of leaving them
/// in `ROOF_MAIN`, which is a file the outdoors owns and `l3-room` cannot see. That one was 4 456
/// pixels.
///
/// The three that remain are all the same thing and it is not a cell's boundary surface: the
/// OUTDOORS. `ext-backyard`, `ext-terrace` and `b1-gym` each lose ground -- `b1-gym` loses
/// `TERRAIN_grass` through a basement window well -- because the ground is per-cell CHUNKS today
/// and thirteen pairs of exterior cells abut with no portal between them. §25.6 is explicit that
/// portals cannot help there and that the outdoors is culled by the exterior hierarchy over
/// INSTANCES; that path has no content in it until `HOUSE-00852`. Authoring the portals would be
/// the wrong fix, and is recorded as such.
///
/// So the set is PINNED rather than the test left off. A pose not on this list that differs is a
/// new hole and fails the day it appears; a pinned pose that stops differing fails too, so the
/// list cannot outlive the defect. §25.8 calls this the single most important test in the project
/// and it is running again.
TEST(CullingSanityRenderTests, EveryPoseLooksTheSameCulledAndUnculled)
{
    if (!ContentIsBuilt())
    {
        GTEST_SKIP() << "no content/world/chunks.bin";
    }

    int compared = 0;
    int stillPending = 0;
    double worst = 0.0;
    const char* worstPose = "-";
    for (const VisibilityPose& pose : kVisibilityPoses)
    {
        // §65.6's door state is what the walk scene starts in and the only one the command line
        // can ask for, so the poses that want every door open are left to `HOUSE-00686`, which
        // asserts their sets directly.
        if (pose.doorsOpen)
        {
            continue;
        }
        const std::string name(pose.name);
        const std::string unculled = OutputPath("cull-" + name + "-off.png");
        const std::string culled = OutputPath("cull-" + name + "-on.png");

        // The unculled frame first, as the thing to match: it is the whole house, which is what
        // the picture would be if §25 did not exist.
        ASSERT_TRUE(RenderHarness::CaptureFrame(OptionsFor(pose, false), kWidth, kHeight, unculled)) << name;

        const auto diff =
            RenderHarness::CompareWithReference(OptionsFor(pose, true),
                                                kWidth,
                                                kHeight,
                                                culled,
                                                unculled,
                                                2,
                                                RenderHarness::NonDeterministicRegions(kWidth, kHeight));
        ASSERT_TRUE(diff.HasValue()) << name << ": " << diff.Error().ToString();
        EXPECT_FALSE(diff->sizeMismatch) << name;
        ++compared;
        if (diff->DifferingFraction() > worst)
        {
            worst = diff->DifferingFraction();
            worstPose = pose.name.data();
        }
        // The same tolerance the committed references use, and for the same reason: a silhouette
        // edge may land on either side of a pixel between two runs of a rasteriser. A ROOM that
        // was culled is thousands of pixels, not tens.
        const bool pinned = std::find(std::begin(kOutdoorsPending), std::end(kOutdoorsPending), name) !=
                            std::end(kOutdoorsPending);
        if (pinned)
        {
            // Still differing, and it has to STAY differing: the day `HOUSE-00852` puts the
            // outdoors on §25.6's instance path this line is what says the pin can go.
            EXPECT_GT(diff->DifferingFraction(), 0.002)
                << name << " no longer differs. `HOUSE-00852` has landed, or something else fixed "
                << "the outdoors: take it out of kOutdoorsPending";
            ++stillPending;
            continue;
        }
        EXPECT_LT(diff->DifferingFraction(), 0.002)
            << name << " differs with culling on: " << diff->ToString() << " -- something visible was culled";
    }
    EXPECT_EQ(stillPending, std::size(kOutdoorsPending))
        << "a pinned pose was not among the ones compared, so the pin means nothing";
    std::printf("  %d pose(s) compared culled against unculled; worst %s at %.4f %% of the frame\n",
                compared,
                worstPose,
                worst * 100.0);
    EXPECT_GE(compared, 15) << "too few poses were comparable for this to mean anything";
}
