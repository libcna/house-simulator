// SPDX-License-Identifier: MIT
//
// `HOUSE-00135`'s target: the application itself, run for real frames, with no window.
//
// This is the test that would catch the failures unit tests structurally cannot -- a `Game` that
// throws during `Initialize`, content that cannot be found from the working directory, a `Draw`
// that leaves the device in a state the next frame rejects. It runs under the `headless` preset,
// which `HOUSE-00105` measured doing 600 frames with `DISPLAY` unset.
#include <cstdio>

#include <gtest/gtest.h>

#include "cnahouse/app/CnaHouseGame.hpp"
#include "cnahouse/app/CommandLine.hpp"
#include "cnahouse/app/Settings.hpp"
#include "cnahouse/player/FirstPersonView.hpp"
#include "cnahouse/player/IInputSource.hpp"
#include "cnahouse/util/Ids.hpp"
#include "cnahouse/util/Log.hpp"

namespace
{
    using cnahouse::app::CnaHouseGame;
    using cnahouse::app::Options;
    using cnahouse::app::Settings;

    TEST(HeadlessRunTests, TheGameRunsRealFramesAndExitsCleanly)
    {
        cnahouse::util::Log::ResetForTesting();

        Options options;
        options.headless = true;
        options.contentRoot = CNAHOUSE_TEST_CONTENT_ROOT;
        Settings settings = Settings::Defaults();
        // Small, because the frame count is what is under test and the resolution is not.
        settings.backBufferWidth = 640;
        settings.backBufferHeight = 360;
        settings.verticalSync = false;

        CnaHouseGame game(options, settings);
        game.SetFrameLimit(120);
        game.Run();

        EXPECT_GE(game.FramesDrawn(), 120u) << "the frame limit must actually stop the loop";
        EXPECT_EQ(game.ExitCode(), 0) << "and it must stop cleanly, not by throwing";
    }

    TEST(HeadlessRunTests, TheWalkSceneStandsABodyInTheHouseAndLooksThroughItsEyes)
    {
        // `HOUSE-00633`'s wiring, end to end and with no window: §16's world and §49.2's collision
        // load, a body settles onto §12's floor, §49.3's fixed step runs from the frame time, and
        // §44's camera ends up where the eye is. The render poses assert what the frame LOOKS
        // like; this asserts that the thing behind it is a simulation and not a fixed camera.
        cnahouse::util::Log::ResetForTesting();

        Options options;
        options.headless = true;
        options.contentRoot = CNAHOUSE_TEST_CONTENT_ROOT;
        options.noAudio = true;
        options.scene = "walk";
        // §12's kitchen, on the floor, looking east.
        options.player = std::array<float, 5>{-3.00f, 0.60f, -25.05f, 90.0f, 0.0f};
        Settings settings = Settings::Defaults();
        settings.backBufferWidth = 320;
        settings.backBufferHeight = 180;
        settings.verticalSync = false;

        CnaHouseGame game(options, settings);
        game.SetFrameLimit(120);
        game.Run();
        ASSERT_EQ(game.ExitCode(), 0);

        const auto& player = game.PlayerForTesting();
        EXPECT_EQ(cnahouse::util::IdRegistry::NameOf(game.CellForTesting()), "L0_KITCHEN")
            << "the body did not end up in the cell it was put in";
        EXPECT_TRUE(player.onGround) << "the body never landed on §12's floor";
        // §12's L0 finished floor level is +0.60, and a body standing on it has its feet there.
        EXPECT_NEAR(player.Feet().Y, 0.60f, 0.02f);
        EXPECT_NEAR(player.Feet().X, -3.00f, 0.05f) << "nothing pushed the body sideways";

        // §44's eye: 1.68 m over the soles, and the camera at exactly that point.
        const auto& camera = game.ViewForTesting().Camera();
        EXPECT_NEAR(camera.Pose().eye.Y, player.Feet().Y + cnahouse::player::kPlayerEyeHeight, 0.01f);
        EXPECT_NEAR(camera.Pose().eye.X, player.position.X, 1e-3f);
        // §14: yaw 90° looks east, so forward is +X.
        EXPECT_NEAR(camera.Pose().forward.X, 1.0f, 1e-3f);
        EXPECT_NEAR(camera.Pose().forward.Z, 0.0f, 1e-3f);
        // §44's lens and §10.3's near plane, which is what makes this a player camera rather than
        // the blockout's 55° one.
        EXPECT_FLOAT_EQ(camera.EffectiveFieldOfViewDegrees(), cnahouse::player::kDefaultFovDegrees);
        EXPECT_LE(camera.NearPlane(), cnahouse::player::kNearPlane);
    }

    /// Holds one intent for the whole session. `HOUSE-00140`'s interface is what makes this
    /// possible: everything after it is expressed in game terms, so a test can walk the body.
    class ScriptedInput final : public cnahouse::player::IInputSource
    {
    public:
        explicit ScriptedInput(cnahouse::player::InputState state)
            : state_(state)
        {
        }

        void Update(float) override {}

        [[nodiscard]] const cnahouse::player::InputState& Current() const noexcept override
        {
            return state_;
        }

        [[nodiscard]] bool LookAvailable() const noexcept override
        {
            return false;
        }

    private:
        cnahouse::player::InputState state_;
    };

    TEST(HeadlessRunTests, HoldingForwardWalksTheBodyAcrossTheRoomAtSectionFortyThreesSpeed)
    {
        // The wiring end to end, with the body MOVING: §49.3's fixed step driven from the frame
        // time, §43.2's speed, §16.4's cell tracking following the feet, and §44's camera arriving
        // where the eye is. Standing still proves none of those -- a game that never ran a step
        // would pass a test that only looked at where the body was put.
        cnahouse::util::Log::ResetForTesting();

        cnahouse::player::InputState forward;
        forward.move.Y = 1.0F;
        ScriptedInput input(forward);

        Options options;
        options.headless = true;
        options.contentRoot = CNAHOUSE_TEST_CONTENT_ROOT;
        options.noAudio = true;
        options.scene = "walk";
        // §12's basement hallway, at its south end: 12.8 m of straight, empty corridor, which is
        // the longest clear run in the house and the only place a speed can be measured without
        // the far wall stopping the body first.
        options.player = std::array<float, 5>{0.00f, -2.30f, -15.00f, 0.0f, 0.0f};
        Settings settings = Settings::Defaults();
        settings.backBufferWidth = 320;
        settings.backBufferHeight = 180;
        settings.verticalSync = false;

        CnaHouseGame game(options, settings);
        game.SetInputSourceForTesting(&input);
        // Enough to walk several metres at any plausible frame rate, and few enough that the far
        // wall is out of reach even if every frame were four steps long.
        game.SetFrameLimit(250);
        game.Run();
        ASSERT_EQ(game.ExitCode(), 0);

        const auto& player = game.PlayerForTesting();
        // North is -Z (§14). How FAR it got depends on how long 400 headless frames took, so what
        // is asserted is that it walked at all, that it walked the right way, and that it did not
        // walk through the wall at the end of the hall (z = -27.10, less the capsule's radius).
        EXPECT_LT(player.Feet().Z, -16.50f) << "the body did not walk north";
        EXPECT_GT(player.Feet().Z, -27.10f + cnahouse::player::kPlayerRadius - 0.01f)
            << "the body walked through the back of the house";
        EXPECT_TRUE(game.CellForTesting().IsValid());
        EXPECT_NEAR(player.Feet().X, 0.0f, 0.30f) << "it wandered sideways";

        // §49.3's clock: the distance walked is §43.2's speed over the SIMULATED time, which is
        // the fixed steps and not the frame times. They agree only if each step advanced by
        // 1/120 s -- a step given the whole frame's time instead runs the world fast, and the body
        // arrives further down the hall than the simulation says it walked.
        const double simulated = static_cast<double>(game.FixedStepsForTesting()) *
                                 static_cast<double>(cnahouse::player::kFixedStepSeconds);
        const double walked = static_cast<double>(-15.00f - player.Feet().Z);
        // Said out loud rather than left to make the speed look wrong: a body stopped by the far
        // wall walked as far as the hall is long and no further, whatever its speed was.
        ASSERT_GT(player.Feet().Z, -26.0f) << "the walk reached the end of the hall; the speed below "
                                              "would be measuring §12's geometry and not §43.2's";
        std::printf("  %llu step(s) = %.3f s simulated, %.3f m walked (%.3f m/s)\n",
                    static_cast<unsigned long long>(game.FixedStepsForTesting()),
                    simulated,
                    walked,
                    walked / simulated);
        ASSERT_GT(simulated, 0.5) << "the game ran no steps to measure";
        // §43.2's 1.35 m/s, minus the 0.15 s the body spends reaching it (§43.2's 9.0 m/s²).
        EXPECT_LT(walked / simulated, 1.35 * 1.05) << "the body covered more ground than it had time for";
        EXPECT_GT(walked / simulated, 1.35 * 0.80) << "it walked slower than §43.2's speed";
        EXPECT_NEAR(player.Feet().Y, -2.30f, 0.02f) << "it left §12's basement floor";
        EXPECT_TRUE(game.CellForTesting().IsValid());

        // §44's camera followed it, and the eye is over the feet it has now rather than the ones
        // it started with.
        const auto& camera = game.ViewForTesting().Camera();
        EXPECT_NEAR(camera.Pose().eye.Z, player.position.Z, 1e-3f);
        EXPECT_NEAR(camera.Pose().eye.Y, player.Feet().Y + cnahouse::player::kPlayerEyeHeight, 0.02f);
    }

    TEST(HeadlessRunTests, WalkingThroughADoorwayTakesTheCellTrackingWithIt)
    {
        // §16.4's tracking, from the game loop rather than from a unit test: the body walks north
        // out of §12's foyer, and what it collides against on the far side of the opening is the
        // NEXT cell's geometry. A tracker that stopped updating would leave the body colliding
        // against the foyer -- which is to say, walking into the wall it just went through.
        cnahouse::util::Log::ResetForTesting();

        cnahouse::player::InputState forward;
        forward.move.Y = 1.0F;
        ScriptedInput input(forward);

        Options options;
        options.headless = true;
        options.contentRoot = CNAHOUSE_TEST_CONTENT_ROOT;
        options.noAudio = true;
        options.scene = "walk";
        options.player = std::array<float, 5>{0.00f, 0.60f, -15.00f, 0.0f, 0.0f};
        Settings settings = Settings::Defaults();
        settings.backBufferWidth = 320;
        settings.backBufferHeight = 180;
        settings.verticalSync = false;

        CnaHouseGame game(options, settings);
        game.SetInputSourceForTesting(&input);
        game.SetFrameLimit(400);
        game.Run();
        ASSERT_EQ(game.ExitCode(), 0);

        const auto& player = game.PlayerForTesting();
        std::printf("  ended in %s at z %.2f\n",
                    std::string(cnahouse::util::IdRegistry::NameOf(game.CellForTesting())).c_str(),
                    static_cast<double>(player.Feet().Z));
        EXPECT_LT(player.Feet().Z, -18.30f) << "the body never left the foyer";
        EXPECT_TRUE(game.CellForTesting().IsValid());
        EXPECT_NE(cnahouse::util::IdRegistry::NameOf(game.CellForTesting()), "L0_FOYER")
            << "the body walked out of the foyer and the cell tracking stayed in it";
    }

    TEST(HeadlessRunTests, AWalkSceneAskedForAnImpossiblePlaceDrawsAFrameAnyway)
    {
        // Half a kilometre down the road is not in any cell -- §10.3's world box is 400 m across.
        // The scene says so and falls back to the fixed camera rather than refusing to start: a
        // screenshot of the wrong place is a bug report, and a game that will not run is a
        // mystery.
        cnahouse::util::Log::ResetForTesting();

        Options options;
        options.headless = true;
        options.contentRoot = CNAHOUSE_TEST_CONTENT_ROOT;
        options.noAudio = true;
        options.scene = "walk";
        options.player = std::array<float, 5>{500.0f, 0.0f, 500.0f, 0.0f, 0.0f};
        Settings settings = Settings::Defaults();
        settings.backBufferWidth = 320;
        settings.backBufferHeight = 180;
        settings.verticalSync = false;

        CnaHouseGame game(options, settings);
        game.SetFrameLimit(10);
        game.Run();

        EXPECT_EQ(game.ExitCode(), 0);
        EXPECT_GE(game.FramesDrawn(), 10u);
        EXPECT_FALSE(game.CellForTesting().IsValid()) << "it claimed to be walking somewhere";
    }

    TEST(HeadlessRunTests, TheVersionLineNamesTheBuildItCameFrom)
    {
        // Drawn in the corner and printed at startup, so it is the first thing on a screenshot and in
        // a bug report. It has to say which renderer and which tier, or a report is unattributable.
        const std::string line = CnaHouseGame::VersionLine();
        EXPECT_NE(line.find(CNAHOUSE_VERSION), std::string::npos) << line;
        EXPECT_NE(line.find(CNAHOUSE_RENDERER_NAME), std::string::npos) << line;
        EXPECT_NE(line.find("Tier"), std::string::npos) << line;
    }

    TEST(HeadlessRunTests, TheHudFontLoadsFromTheBuiltContentTree)
    {
        // The Phase-2 exit criterion has the version string on screen, which needs the font to be
        // there. Asserting on the log is what makes "it loaded" checkable without a GPU readback:
        // `LoadContent` logs a warning if and only if the font is absent.
        cnahouse::util::Log::ResetForTesting();
        Options options;
        options.contentRoot = CNAHOUSE_TEST_CONTENT_ROOT;
        Settings settings = Settings::Defaults();
        settings.backBufferWidth = 320;
        settings.backBufferHeight = 240;

        CnaHouseGame game(options, settings);
        game.SetFrameLimit(5);
        game.Run();

        for (const auto& record : cnahouse::util::Log::Ring())
        {
            EXPECT_EQ(record.message.find("did not load"), std::string::npos)
                << "content that the build produced must be findable at run time: " << record.message;
        }
    }

    TEST(HeadlessRunTests, AMissingHudFontDoesNotStopTheGameStarting)
    {
        // A build whose content tree has not been generated must still start and still say so. The
        // first thing a new contributor sees should not be a crash.
        cnahouse::util::Log::ResetForTesting();
        Options options;
        options.contentRoot = "no-such-content-directory";
        Settings settings = Settings::Defaults();
        settings.backBufferWidth = 320;
        settings.backBufferHeight = 240;

        CnaHouseGame game(options, settings);
        game.SetFrameLimit(10);
        EXPECT_NO_THROW(game.Run());
        EXPECT_GE(game.FramesDrawn(), 10u);
    }

} // namespace
