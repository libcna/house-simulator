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

    /// The ONE pose that still differs, and it is not over-culling.
    ///
    /// `HOUSE-00700` put the outdoors on §25.6's instance path and two of the three went clean the
    /// same hour: `ext-backyard` had been losing 31.23 % of its frame and `ext-terrace` 2.00 %.
    /// `b1-gym` did not, and measuring it rather than assuming showed it was never the same
    /// defect. The pose stands in the basement gym at eye y −0.70 and the 1 132 differing pixels
    /// are `TERRAIN_grass` drawn IN FRONT of the gym's own ceiling (897 px), trim (156 px) and
    /// wall (29 px) -- so the unculled frame is not seeing the lawn through anything, it is
    /// standing inside it. §10.2's height field has no hole under the house: over the gym's
    /// footprint the ground runs y −0.537…−0.006 and B1's ceiling is at +0.25, so half a metre of
    /// lawn hangs inside the room. That is `HOUSE-00786`, and until the excavation lands the
    /// UNCULLED frame is the wrong picture to match -- which is why this pin cannot come out with
    /// the other two.
    constexpr std::array<const char*, 1> kOutdoorsPending{"b1-gym"};
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

/// ENABLED since `HOUSE-00488` (2026-09-10), with the one pose that still differs PINNED.
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
/// Seventeen of the eighteen are clean since `HOUSE-00700` put the outdoors on §25.6's instance
/// path: the ground was per-cell CHUNKS, thirteen pairs of exterior cells abut with no portal
/// between them, and §25.6 is explicit that portals cannot help there -- so authoring those
/// portals would have been the wrong fix and is recorded as such. `ext-backyard` recovered
/// 31.23 % of its frame and `ext-terrace` 2.00 %. The eighteenth, `b1-gym`, is a hole in the
/// GROUND rather than in the culling and is `HOUSE-00786`'s; see `kOutdoorsPending` above.
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
            // Still differing, and it has to STAY differing: the day `HOUSE-00786` excavates
            // the ground under the house this line is what says the pin can go.
            EXPECT_GT(diff->DifferingFraction(), 0.002)
                << name << " no longer differs. `HOUSE-00786` has landed, or something else fixed "
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
