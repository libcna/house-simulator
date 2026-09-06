// SPDX-License-Identifier: MIT
//
// `HOUSE-00161`. ADR-0003's promise is that Tier S is COMPLETE, so a build whose compiled effects
// are missing must be a fully playable Tier-S session rather than a failed start. That promise is
// only worth anything if it is exercised, and it cannot be exercised from a unit test: the decision
// is made inside `LoadContent`, so the whole game has to run for it to happen at all.
#include <string>

#include <gtest/gtest.h>

#include "cnahouse/app/CnaHouseGame.hpp"
#include "cnahouse/app/CommandLine.hpp"
#include "cnahouse/app/Settings.hpp"
#include "cnahouse/rendering/RenderTier.hpp"
#include "cnahouse/util/Log.hpp"

namespace
{
    using cnahouse::app::CnaHouseGame;
    using cnahouse::app::Options;
    using cnahouse::app::RenderTier;
    using cnahouse::app::Settings;

    Settings SmallSettings()
    {
        Settings settings = Settings::Defaults();
        settings.backBufferWidth = 320;
        settings.backBufferHeight = 240;
        settings.verticalSync = false;
        return settings;
    }

    bool RingContains(std::string_view fragment)
    {
        for (const auto& record : cnahouse::util::Log::Ring())
        {
            if (record.message.find(fragment) != std::string::npos)
            {
                return true;
            }
        }
        return false;
    }

    TEST(TierFallbackTests, MissingEffectFallsBackToTierS)
    {
        // The effect root is pointed at a directory that exists and holds no effects, which is
        // exactly the shape of a build whose `content-fx` tree was never generated -- the real
        // failure this guards, and the one that actually happened during HOUSE-00161's first run.
        cnahouse::util::Log::ClearRing();

        Options options;
        options.headless = true;
        options.contentRoot = CNAHOUSE_TEST_CONTENT_ROOT;
        options.effectRoot = CNAHOUSE_TEST_CONTENT_ROOT; // no Effects/ subtree under here
        options.tier = RenderTier::E;

        CnaHouseGame game(options, SmallSettings());
        game.SetFrameLimit(30);
        game.Run();

        EXPECT_EQ(game.ExitCode(), 0) << "a missing effect set is a narrower session, not a crash";
        EXPECT_GE(game.FramesDrawn(), 30u) << "and the session is fully playable afterwards";
        EXPECT_EQ(game.Tier().Active(), RenderTier::S);

        if constexpr (cnahouse::rendering::RenderTier::CompiledIn())
        {
            // Only a Tier-E binary can fall BACK; on a Tier-S build there was nothing to narrow and
            // demanding a fallback message would be demanding a lie.
            EXPECT_FALSE(game.Tier().FallbackReason().empty())
                << "the reason is what makes the log line actionable";
            EXPECT_FALSE(game.Tier().TierEselectable())
                << "the settings toggle must be off once the effect set is known not to load";
            EXPECT_TRUE(RingContains("fallen back to Tier S"))
                << "the narrowing has to be visible in a bug report, not silent";
        }
    }

    TEST(TierFallbackTests, TierSIsForcedByTheCommandLineWithoutTouchingTheContent)
    {
        cnahouse::util::Log::ClearRing();

        Options options;
        options.headless = true;
        options.contentRoot = CNAHOUSE_TEST_CONTENT_ROOT;
        options.tier = RenderTier::S;

        CnaHouseGame game(options, SmallSettings());
        game.SetFrameLimit(10);
        game.Run();

        EXPECT_EQ(game.ExitCode(), 0);
        EXPECT_EQ(game.Tier().Active(), RenderTier::S);
        EXPECT_TRUE(game.Tier().FallbackReason().empty())
            << "asking for Tier S is a choice, not a failure, and must not be reported as one";
        EXPECT_EQ(game.Tier().TierEselectable(), cnahouse::rendering::RenderTier::CompiledIn())
            << "choosing S must leave the option to switch back where the build has one";
    }

    TEST(TierFallbackTests, TheDrawnLineSaysWhichTierTheSessionIsActuallyRunning)
    {
        // A screenshot is the primary bug-report artefact, so the corner line has to distinguish
        // "this build has Tier E" from "this frame was drawn with it".
        Options options;
        options.headless = true;
        options.contentRoot = CNAHOUSE_TEST_CONTENT_ROOT;
        options.tier = RenderTier::S;

        CnaHouseGame game(options, SmallSettings());
        const std::string line = game.SessionLine();
        EXPECT_NE(line.find(CnaHouseGame::VersionLine()), std::string::npos)
            << "the build facts stay on the line: " << line;
        if constexpr (cnahouse::rendering::RenderTier::CompiledIn())
        {
            EXPECT_NE(line.find("running S"), std::string::npos) << line;
        }
        else
        {
            EXPECT_EQ(line, CnaHouseGame::VersionLine())
                << "with no Tier E in the build there is nothing to disambiguate: " << line;
        }
    }
} // namespace
