// SPDX-License-Identifier: MIT
//
// `HOUSE-00633`: twelve places in the house, seen through a body's eyes.
//
// **Why these are not `HOUSE-00483`'s twenty poses again.** Those are frames of the MODEL: a camera
// at six numbers somebody measured off a plan, with a 55° lens and a 0.5 m near plane, free to
// float anywhere including inside a wall. These are frames of the GAME. `--player=x,y,z,yaw,pitch`
// puts §49's capsule on the floor with its feet there, lets §49.3 settle it onto whatever it is
// standing on, and looks through §44's camera: the eye 1.68 m over the soles, a 70° lens, §10.3's
// 0.10 m near plane, and the head height a room is actually seen from.
//
// That difference is the point. A ceiling 20 mm too low is invisible from outside and obvious from
// under it; a stair with no head-room is a frame you cannot take at all if the camera has to stand
// on the treads; a doorway that is too narrow is a doorway a floating camera goes straight through.
//
// The references were generated under `LIBGL_ALWAYS_SOFTWARE=1`, which is what CI uses
// (`HOUSE-00138`). On hardware the comparison is skipped and the coverage assertions still run.
#include <array>
#include <cstdint>
#include <cstdio>
#include <set>
#include <string>

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

    /// The same 640x360 as `HOUSE-00483`'s poses, and for the same reason: what these catch is a
    /// room that changed shape, not a pixel that moved, and twelve references at 1600x900 would be
    /// 7 MB of PNG in the repository.
    constexpr int kWidth = 640;
    constexpr int kHeight = 360;

    struct Pose
    {
        const char* name;
        /// Feet x, y, z in metres, then §14's yaw and §44's pitch in degrees. Yaw 0 looks north.
        std::array<float, 5> player;
        /// The least of the frame this pose may cover, as `HOUSE-00483` defines coverage: standing
        /// in a room, the room fills the frame; standing on a landing that opens onto a stairwell
        /// or outdoors, some of what is in front of you is not a surface.
        double minCoverage = 0.90;
    };

    /// Twelve. Two below ground, six on the ground floor and the stairs, three above, one outside
    /// -- and every one of them a place a body can stand, which is why they are floor positions
    /// and headings rather than eye/target pairs.
    constexpr Pose kPoses[] = {
        {"b1-cinema", {5.45f, -2.30f, -25.05f, 90.0f, 0.0f}},
        {"b1-hall", {0.00f, -2.30f, -20.70f, 0.0f, 0.0f}},
        {"l0-hall", {0.00f, 0.60f, -20.65f, 0.0f, 0.0f}},
        // §12.1's front door, from a stride inside it. A quarter of this frame was the clear
        // colour until `HOUSE-00486` drew the leaf: the opening is a hole in the shell and the
        // drive beyond it is phase 10's, so what was behind it was nothing. The blockout leaf
        // closes it; §15's animated door and phase 10's drive replace this picture again.
        {"l0-front-door", {0.00f, 0.60f, -14.90f, 180.0f, 0.0f}, 0.90},
        // HOUSE-01040: the old centre pose lies inside the primary island's collision proxy.
        // A stride beyond the hall opening now reviews the visible island/sink run instead.
        {"l0-kitchen", {0.00f, 0.60f, -23.55f, 285.0f, 0.0f}},
        // As close to a corner as §43.1's 0.30 m capsule lets a body get, looking into it at 45°.
        //
        // It was chosen to make §10.3's 0.10 m near plane matter, and MEASURED not to: rendering
        // this pose with the near plane at 0.5 m changes two pixels of the frame, and it takes
        // 5.0 m to change half of it. The eye rides on the capsule's axis, so the nearest thing a
        // standing body can put in front of it is 0.30 m away and 1.68 m up -- the same geometry
        // that made `HOUSE-00632`'s pull-back never fire in 96 553 frames of touring the house.
        // Kept anyway, because a body in a corner is a pose worth having a picture of, and
        // because the near plane's value is asserted where it CAN be seen: `EyeProbeTests` reads
        // it back out of the projection.
        // 0.31 m from `D_L0_FAMILY`, which §65.6 starts SHUT -- so this is the pose that showed
        // `HOUSE-00486`: §25's culling correctly refused to draw the room behind the door, and the
        // shell drew no LEAF, leaving two thirds of the frame as the hole the opening was cut as.
        // With the leaf drawn it is a room again, and the shut door is a surface.
        {"l0-hall-corner", {1.89f, 0.60f, -22.69f, 135.0f, 0.0f}},
        // The main stair from the foyer it is open to (§12.2), which is where a person looks at a
        // staircase from. Standing ON a flight is a frame of the underside of the flight above.
        //
        // The black at the right of this one is NOT the stairwell: it is the 1.30 m hole
        // `HOUSE-00620` recorded in the DRAWN front elevation, which `build_collision.py` was
        // fixed for and `house_shell_gen.py` has not been. Accepted deliberately, because a
        // reference that hides a known defect is worse than one that shows it -- and when the
        // generator is fixed this frame changes, which is the reference doing its job.
        {"l0-foyer-stair", {1.20f, 0.60f, -16.30f, 90.0f, 5.0f}, 0.85},
        {"l0-garage", {12.90f, 0.60f, -17.50f, 270.0f, 0.0f}},
        {"l1-hall-w", {-5.95f, 3.65f, -19.45f, 270.0f, 0.0f}},
        {"l1-master-bed", {-2.40f, 3.65f, -24.55f, 90.0f, 0.0f}},
        {"l2-library", {-5.20f, 6.55f, -16.30f, 90.0f, 0.0f}},
        {"l3-room", {-0.55f, 9.30f, -20.50f, 90.0f, 0.0f}},
    };

    Options OptionsFor(const Pose& pose)
    {
        Options options;
        options.tier = RenderTier::S;
        options.quality = QualityPreset::Low;
        options.noAudio = true;
        options.contentRoot = CNAHOUSE_TEST_CONTENT_ROOT;
        options.scene = "walk";
        options.player = pose.player;
        // Production lighting is live now. Freeze a useful authored condition so these remain
        // geometry/material regressions rather than a comparison against whatever time elapsed
        // while each game instance loaded its content.
        options.timeOfDay = 10.5F;
        options.freezeTime = true;
        options.weather = "W_CLEAR";
        // The FIRST frame, which is the only one that is the same on every machine: the corner
        // line carries the frame time, and frame 1's is `FrameTimer`'s clamped default while
        // frame 30's is however long this machine took. Captured at 30 the twelve references
        // disagreed with themselves between two runs -- 22.43 ms one time, 24.99 the next.
        //
        // What that costs is that these frames are drawn from the pose `LoadWalk` settled rather
        // than one `UpdateWalk` produced. The frame loop is covered where a loop belongs: the
        // headless `HeadlessRunTests` walk 400 frames and assert where the body and the eye ended
        // up (`HOUSE-00633`).
        return options;
    }

    bool ContentIsBuilt()
    {
        const std::string path = std::string(CNAHOUSE_TEST_CONTENT_ROOT) + "/world/collision.bin";
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

    void CompareOnePose(const Pose& pose)
    {
        const std::string actual = std::string(CNAHOUSE_TEST_OUTPUT_DIR) + "/fp-" + pose.name + "-actual.png";
        const std::string reference = RenderHarness::ReferenceDirectory() + "/fp-" + pose.name + ".png";

        if (RenderHarness::RenderingInSoftware())
        {
            const auto diff = RenderHarness::CompareWithReference(
                OptionsFor(pose), kWidth, kHeight, actual, reference, 2, {});
            ASSERT_TRUE(diff.HasValue()) << pose.name << ": " << diff.Error().ToString();
            EXPECT_FALSE(diff->sizeMismatch) << pose.name << ": " << diff->ToString();
            EXPECT_LT(diff->DifferingFraction(), 0.002) << pose.name << ": " << diff->ToString();
        }
        else
        {
            ASSERT_TRUE(RenderHarness::CaptureFrame(OptionsFor(pose), kWidth, kHeight, actual));
        }

        const auto loaded = RenderHarness::LoadPng(actual);
        ASSERT_TRUE(loaded.HasValue()) << pose.name << ": " << loaded.Error().ToString();
        const double coverage = Coverage(*loaded);
        // A body standing in a room sees the room. Less than this and either a surface is missing
        // or -- the failure this catches that a floating camera cannot -- the body did not settle
        // where it was put and the frame is from inside the floor or halfway up a wall.
        EXPECT_GT(coverage, pose.minCoverage)
            << pose.name << " covers only " << (coverage * 100.0) << " % of the frame, under its own "
            << (pose.minCoverage * 100.0) << " %";
    }

    TEST(FirstPersonPoseRenderTests, TheTwelvePosesMatchTheirReferences)
    {
        if (!ContentIsBuilt())
        {
            GTEST_SKIP() << "no content/world/collision.bin";
        }
        for (const Pose& pose : kPoses)
        {
            SCOPED_TRACE(pose.name);
            CompareOnePose(pose);
        }
    }

    TEST(FirstPersonPoseRenderTests, ProductionSidingRetainsTiledAlbedoAfterTheHudHasDrawn)
    {
        if (!ContentIsBuilt())
        {
            GTEST_SKIP() << "no content/world/collision.bin";
        }
        Options options = OptionsFor(kPoses[0]);
        options.quality = QualityPreset::High;
        options.player = std::array<float, 5>{0.00F, 0.60F, -5.00F, 0.0F, 0.0F};
        options.seed = 6840335469064670721ULL;
        options.screenshotFrame = 3;
        const std::string actual = std::string(CNAHOUSE_TEST_OUTPUT_DIR) + "/siding-front-actual.png";
        ASSERT_TRUE(RenderHarness::CaptureFrame(options, 1600, 900, actual));
        const auto image = RenderHarness::LoadPng(actual);
        ASSERT_TRUE(image.HasValue()) << image.Error().ToString();
        ASSERT_EQ(image->width, 1600);
        ASSERT_EQ(image->height, 900);

        // A real, repeated wood source covers this central front panel. Its 40x70 crop was exactly
        // one colour after the HUD's default LinearClamp was retained in slot 0, despite 48
        // distinct deployed UV0s. Frame 3 is essential: frame 1 still has CNA's initial wrap.
        std::set<std::uint32_t> sidingColours;
        for (int y = 260; y < 330; ++y)
        {
            for (int x = 370; x < 410; ++x)
            {
                const auto& pixel =
                    image->pixels[static_cast<std::size_t>(y) * 1600u + static_cast<std::size_t>(x)];
                sidingColours.insert((static_cast<std::uint32_t>(pixel.getRProperty()) << 16u) |
                                     (static_cast<std::uint32_t>(pixel.getGProperty()) << 8u) |
                                     static_cast<std::uint32_t>(pixel.getBProperty()));
            }
        }
        EXPECT_GT(sidingColours.size(), 40u)
            << "the close production facade collapsed a physically tiled source to one texel";
        const auto& midWall = image->pixels[150u * 1600u + 400u];
        // The current 256-sample L1 atlas now contains the window/shutter and cornice occlusion
        // that the old broad white receiver omitted. This deliberately shaded texel is 52, while
        // the display-sky clear is 18; retain a useful separation without requiring direct sun.
        EXPECT_GT(midWall.getRProperty(), 40)
            << "clear daytime approved wood must remain above the dark display-sky range";
    }

    TEST(FirstPersonPoseRenderTests, ThePlayableEntryHasAReadablePaintedBalconyAndPorch)
    {
        if (!ContentIsBuilt())
        {
            GTEST_SKIP() << "no content/world/collision.bin";
        }
        Options options = OptionsFor(kPoses[0]);
        options.quality = QualityPreset::High;
        options.player = std::array<float, 5>{0.00F, 0.60F, -5.00F, 0.0F, 0.0F};
        options.seed = 6840335469064670721ULL;
        options.screenshotFrame = 3;
        const std::string actual = std::string(CNAHOUSE_TEST_OUTPUT_DIR) + "/entry-finish-actual.png";
        ASSERT_TRUE(RenderHarness::CaptureFrame(options, 1600, 900, actual));
        const auto image = RenderHarness::LoadPng(actual);
        ASSERT_TRUE(image.HasValue()) << image.Error().ToString();
        ASSERT_EQ(image->width, 1600);
        ASSERT_EQ(image->height, 900);

        // This is the same normal-game, close front approach as the visual review, after the HUD
        // has drawn. HOUSE-00931 replaced the opaque parapet band with a measured open guard. A
        // horizontal scan through its balusters must retain alternating painted uprights and open
        // facade, while the porch's painted structural trim remains free of chipped masonry.
        std::size_t guardTransitions = 0u;
        std::size_t guardBrightPixels = 0u;
        int previousLuma = -1;
        for (int x = 440; x < 1160; ++x)
        {
            const auto& pixel = image->pixels[200u * 1600u + static_cast<std::size_t>(x)];
            const int luma =
                (static_cast<int>(pixel.getRProperty()) * 54 + static_cast<int>(pixel.getGProperty()) * 183 +
                 static_cast<int>(pixel.getBProperty()) * 19) /
                256;
            if (previousLuma >= 0)
            {
                const int delta = luma > previousLuma ? luma - previousLuma : previousLuma - luma;
                if (delta > 35)
                {
                    ++guardTransitions;
                }
            }
            if (luma > 120)
            {
                ++guardBrightPixels;
            }
            previousLuma = luma;
        }
        EXPECT_GT(guardTransitions, 60u)
            << "the balcony guard must read as an open rhythm, not a solid blockout band";
        EXPECT_GT(guardBrightPixels, 300u)
            << "the painted balcony balusters must remain readable in daylight";
        const auto& support = image->pixels[380u * 1600u + 450u];
        EXPECT_GT(support.getRProperty(), 95)
            << "the painted front porch support must remain readable in normal gameplay";

        std::set<std::uint32_t> supportColours;
        for (int y = 355; y < 455; ++y)
        {
            for (int x = 446; x < 458; ++x)
            {
                const auto& pixel =
                    image->pixels[static_cast<std::size_t>(y) * 1600u + static_cast<std::size_t>(x)];
                supportColours.insert((static_cast<std::uint32_t>(pixel.getRProperty()) << 16u) |
                                      (static_cast<std::uint32_t>(pixel.getGProperty()) << 8u) |
                                      static_cast<std::uint32_t>(pixel.getBProperty()));
            }
        }
        EXPECT_LT(supportColours.size(), 200u)
            << "a painted porch column must not expose the heavily chipped masonry pattern";
    }

    /// @brief Rewrites all twelve references. DISABLED, and run by hand after an intended change:
    ///
    ///     LIBGL_ALWAYS_SOFTWARE=1 ./build/cnahouse_render_tests
    ///         --gtest_also_run_disabled_tests --gtest_filter=*RegenerateFirstPerson*
    ///
    /// `docs/screenshot-scenes.md`: render, LOOK at the image, and accept it only if it is right.
    TEST(FirstPersonPoseRenderTests, DISABLED_RegenerateFirstPersonReferences)
    {
        if (!ContentIsBuilt())
        {
            GTEST_SKIP() << "no content/world/collision.bin";
        }
        for (const Pose& pose : kPoses)
        {
            const std::string reference = RenderHarness::ReferenceDirectory() + "/fp-" + pose.name + ".png";
            ASSERT_TRUE(RenderHarness::CaptureFrame(OptionsFor(pose), kWidth, kHeight, reference))
                << pose.name;
            std::printf("  wrote %s\n", reference.c_str());
        }
    }

} // namespace
