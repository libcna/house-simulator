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
#include <array>
#include <cstdio>
#include <filesystem>
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

/// ENABLED since `HOUSE-00488` and, since `HOUSE-00786`, with NO pose pinned: all eighteen match.
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
/// Seventeen of the eighteen went clean when `HOUSE-00700` put the outdoors on §25.6's instance
/// path: the ground was per-cell CHUNKS, thirteen pairs of exterior cells abut with no portal
/// between them, and §25.6 is explicit that portals cannot help there -- so authoring those
/// portals would have been the wrong fix and is recorded as such. `ext-backyard` recovered
/// 31.23 % of its frame and `ext-terrace` 2.00 %.
///
/// The eighteenth was never a culling defect at all. `b1-gym` differed because §10.2's height
/// field had no hole under the house, so half a metre of lawn hung inside the basement and the
/// UNCULLED frame -- which hides nothing -- drew it in front of the room's own ceiling. With
/// culling on it was correctly absent, so for once the reference was the wrong picture and the
/// culled frame was right. `HOUSE-00786` excavated the ground and the pose closed with it.
///
/// **Nothing is pinned now**, which is the state this test was written to reach: every pose the
/// door state lets it compare is identical culled and unculled, and the worst of the eighteen is
/// `l0-sunroom` at 0.1546 % of its frame -- silhouette pixels on a rasteriser's edge, under the
/// 0.2 % a committed reference is allowed. §25.8 calls this the single most important test in the
/// project; from here a pose that differs at all is a new hole and fails the day it appears.
TEST(CullingSanityRenderTests, EveryPoseLooksTheSameCulledAndUnculled)
{
    if (!ContentIsBuilt())
    {
        GTEST_SKIP() << "no content/world/chunks.bin";
    }

    int compared = 0;
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
        EXPECT_LT(diff->DifferingFraction(), 0.002)
            << name << " differs with culling on: " << diff->ToString() << " -- something visible was culled";
    }
    std::printf("  %d pose(s) compared culled against unculled; worst %s at %.4f %% of the frame\n",
                compared,
                worstPose,
                worst * 100.0);
    EXPECT_GE(compared, 15) << "too few poses were comparable for this to mean anything";
}
