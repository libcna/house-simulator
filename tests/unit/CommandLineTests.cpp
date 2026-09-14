// SPDX-License-Identifier: MIT
//
// `HOUSE-00132`'s acceptance: no option can change the renderer of a built binary, and
// `--renderer-info` calls no CNA-specific API.
#include <gtest/gtest.h>

#include "cnahouse/app/CommandLine.hpp"

namespace
{
    using cnahouse::app::Options;
    using cnahouse::app::ParseCommandLine;
    using cnahouse::app::QualityPreset;
    using cnahouse::app::RenderTier;
    using cnahouse::util::ErrorCode;

    cnahouse::util::Result<Options> Parse(std::vector<const char*> args)
    {
        args.insert(args.begin(), "cna-house");
        return ParseCommandLine(static_cast<int>(args.size()), args.data());
    }

    TEST(CommandLineTests, NoArgumentsGivesTheDefaults)
    {
        auto options = Parse({});
        ASSERT_TRUE(options);
        // UNSET, not `High`: `cna-house.md` §68 gives the quality preset the default
        // "auto-detected", and a fixed default here would make `rendering::AutoDetect` unreachable
        // for anyone who did not know to ask for it.
        EXPECT_FALSE(options->quality.has_value());
        EXPECT_FALSE(options->headless);
        EXPECT_FALSE(options->seed.has_value());
    }

    TEST(CommandLineTests, ParsesEveryDocumentedOption)
    {
        auto options = Parse({"--quality=low",
                              "--tier=s",
                              "--headless",
                              "--no-audio",
                              "--debug-blockout-materials",
                              "--scene=kitchen",
                              "--seed=12345",
                              "--time=6.5",
                              "--freeze-time",
                              "--weather=rain",
                              "--screenshot=/tmp/shot.png",
                              "--log=world,content"});
        ASSERT_TRUE(options) << options.Error().ToString();
        EXPECT_EQ(options->quality.value_or(QualityPreset::High), QualityPreset::Low);
        EXPECT_EQ(options->tier, RenderTier::S);
        EXPECT_TRUE(options->headless);
        EXPECT_TRUE(options->noAudio);
        EXPECT_TRUE(options->debugBlockoutMaterials);
        EXPECT_EQ(options->scene.value_or(""), "kitchen");
        EXPECT_EQ(options->seed.value_or(0), 12345u);
        EXPECT_FLOAT_EQ(options->timeOfDay.value_or(0.0f), 6.5f);
        EXPECT_TRUE(options->freezeTime);
        EXPECT_EQ(options->weather.value_or(""), "rain");
        EXPECT_EQ(options->screenshot.value_or(""), "/tmp/shot.png");
        EXPECT_EQ(options->logCategories.value_or(""), "world,content");
    }

    TEST(CommandLineTests, AnUnknownOptionIsRefusedRatherThanIgnored)
    {
        // A mistyped `--quailty=low` that ran anyway at the default would produce a bug report about a
        // setting that was never applied.
        auto options = Parse({"--quailty=low"});
        ASSERT_FALSE(options);
        EXPECT_EQ(options.Error().Code(), ErrorCode::InvalidData);
        EXPECT_NE(options.Error().Message().find("--quailty"), std::string::npos)
            << "the message must name what was not understood";
    }

    TEST(CommandLineTests, RendererCannotBeChangedByAnOptionAndTheRefusalExplainsWhy)
    {
        // `HOUSE-00132`'s acceptance criterion, directly. `--renderer` is not merely unknown: it is
        // named, so a user reaching for a reasonable-sounding option is told why it cannot exist.
        auto options = Parse({"--renderer=opengl33"});
        ASSERT_FALSE(options);
        EXPECT_EQ(options.Error().Code(), ErrorCode::Unsupported);
        const std::string message = options.Error().Message();
        EXPECT_NE(message.find("fixed at build time"), std::string::npos) << message;
        EXPECT_NE(message.find("--renderer-info"), std::string::npos) << "and told what to use instead";
    }

    TEST(CommandLineTests, AnOptionThatNeedsAValueSaysSo)
    {
        auto options = Parse({"--quality"});
        ASSERT_FALSE(options);
        EXPECT_NE(options.Error().Message().find("expected a value"), std::string::npos);
        EXPECT_EQ(options.Error().Context(), "--quality");
    }

    TEST(CommandLineTests, BadValuesAreRejectedWithTheirRange)
    {
        // `ultra` became a real preset with HOUSE-00157, so this asserts on a name that is not one
        // rather than on a name that merely had not been implemented yet.
        EXPECT_FALSE(Parse({"--quality=cinematic"}));
        EXPECT_TRUE(Parse({"--quality=ultra"})) << "ultra is a §68 row and must parse";
        EXPECT_FALSE(Parse({"--tier=x"}));
        EXPECT_FALSE(Parse({"--seed=abc"}));
        EXPECT_FALSE(Parse({"--time=notanumber"}));

        auto tooLate = Parse({"--time=25"});
        ASSERT_FALSE(tooLate);
        EXPECT_EQ(tooLate.Error().Code(), ErrorCode::OutOfRange);
        EXPECT_NE(tooLate.Error().Message().find("0..24"), std::string::npos);
    }

    TEST(CommandLineTests, TierEIsAsymmetric)
    {
        // A user may force Tier E OFF, never ON. A binary built without it has no compiled effects in
        // its content, so honouring `--tier=e` would fail at the first draw -- and deciding at
        // configure time exists precisely so that cannot happen.
        EXPECT_EQ(cnahouse::app::ResolveTier(RenderTier::S), RenderTier::S)
            << "asking for Tier S is always honoured";
#if CNAHOUSE_TIER_E
        EXPECT_EQ(cnahouse::app::ResolveTier(RenderTier::E), RenderTier::E);
#else
        EXPECT_EQ(cnahouse::app::ResolveTier(RenderTier::E), RenderTier::S)
            << "asking for Tier E in a build without it resolves DOWN, never fails at draw time";
#endif
    }

    TEST(CommandLineTests, RendererInfoReportsOnlyBuildFacts)
    {
        auto options = Parse({});
        ASSERT_TRUE(options);
        const std::string info = cnahouse::app::RendererInfo(*options);
        EXPECT_NE(info.find(CNAHOUSE_RENDERER_NAME), std::string::npos);
        EXPECT_NE(info.find("fixed at configure time"), std::string::npos);
        EXPECT_NE(info.find(CNAHOUSE_VERSION), std::string::npos);
    }

    TEST(CommandLineTests, UsageTextExplainsWhyThereIsNoRendererOption)
    {
        const std::string usage = cnahouse::app::UsageText();
        EXPECT_NE(usage.find("no --renderer option"), std::string::npos)
            << "an absence this deliberate has to be documented where a user looks for it";
    }

} // namespace
