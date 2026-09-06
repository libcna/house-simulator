// SPDX-License-Identifier: MIT
//
// `HOUSE-00135`'s target: the application itself, run for real frames, with no window.
//
// This is the test that would catch the failures unit tests structurally cannot -- a `Game` that
// throws during `Initialize`, content that cannot be found from the working directory, a `Draw`
// that leaves the device in a state the next frame rejects. It runs under the `headless` preset,
// which `HOUSE-00105` measured doing 600 frames with `DISPLAY` unset.
#include <gtest/gtest.h>

#include "cnahouse/app/CnaHouseGame.hpp"
#include "cnahouse/app/CommandLine.hpp"
#include "cnahouse/app/Settings.hpp"
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
