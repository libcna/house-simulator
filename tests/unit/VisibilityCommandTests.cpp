// SPDX-License-Identifier: MIT
//
// `HOUSE-00684`. §71's `cull off|on`.
//
// The command is three lines of logic and one decision: what does `cull` with no argument do. It
// REPORTS. A toggle would make the command's effect depend on a state the person typing it cannot
// see, and the one thing they are about to do with it is render the same pose twice
// (`HOUSE-00688`) -- where getting the second one backwards is a comparison of two identical
// frames that proves nothing.
#include <string>

#include <gtest/gtest.h>

#include "cnahouse/debug/VisibilityCommands.hpp"

namespace
{
    using cnahouse::debug::CommandResult;
    using cnahouse::debug::Console;
    using cnahouse::debug::RegisterVisibilityCommands;
    using cnahouse::debug::VisibilityCommandContext;

    TEST(VisibilityCommandTests, OffAndOnSetTheFlagAndSayWhatTheyDid)
    {
        bool culling = true;
        Console console;
        RegisterVisibilityCommands(console, VisibilityCommandContext{&culling});

        const CommandResult off = console.Execute("cull off");
        EXPECT_TRUE(off.ok) << off.message;
        EXPECT_FALSE(culling);
        EXPECT_NE(off.message.find("off"), std::string::npos) << off.message;

        const CommandResult on = console.Execute("cull on");
        EXPECT_TRUE(on.ok) << on.message;
        EXPECT_TRUE(culling);
        EXPECT_NE(on.message.find("on"), std::string::npos) << on.message;

        // Idempotent: `cull off` twice is off, not back on.
        EXPECT_TRUE(console.Execute("cull off").ok);
        EXPECT_TRUE(console.Execute("cull off").ok);
        EXPECT_FALSE(culling) << "the command toggles instead of setting";
    }

    TEST(VisibilityCommandTests, WithNoArgumentItReportsRatherThanToggling)
    {
        bool culling = true;
        Console console;
        RegisterVisibilityCommands(console, VisibilityCommandContext{&culling});

        const CommandResult reported = console.Execute("cull");
        EXPECT_TRUE(reported.ok) << reported.message;
        EXPECT_TRUE(culling) << "asking what the setting is changed it";
        EXPECT_NE(reported.message.find("on"), std::string::npos) << reported.message;

        culling = false;
        const CommandResult again = console.Execute("cull");
        EXPECT_TRUE(again.ok);
        EXPECT_FALSE(culling);
        EXPECT_NE(again.message.find("off"), std::string::npos) << again.message;
    }

    TEST(VisibilityCommandTests, AnythingElseFailsAndSaysWhatItWanted)
    {
        bool culling = true;
        Console console;
        RegisterVisibilityCommands(console, VisibilityCommandContext{&culling});

        const CommandResult wrong = console.Execute("cull yes");
        EXPECT_FALSE(wrong.ok);
        EXPECT_TRUE(culling) << "a rejected argument changed the setting anyway";
        // The message names what was typed AND what was wanted: a bare "error" makes the user
        // guess the syntax, which is `docs/conventions.md` §5.4's rule.
        EXPECT_NE(wrong.message.find("yes"), std::string::npos) << wrong.message;
        EXPECT_NE(wrong.message.find("off"), std::string::npos) << wrong.message;
        EXPECT_NE(console.Usage("cull").find("off|on"), std::string::npos) << console.Usage("cull");
    }

    TEST(VisibilityCommandTests, ASessionWithNoVisibilityRefusesRatherThanCrashing)
    {
        // The blockout scene has no walk -- the camera is on the road, in no cell -- so the
        // context has nothing to point at. A null dereference here would be a debug command that
        // crashes the thing it is meant to debug.
        Console console;
        RegisterVisibilityCommands(console, VisibilityCommandContext{});
        const CommandResult result = console.Execute("cull off");
        EXPECT_FALSE(result.ok);
        EXPECT_NE(result.message.find("cull"), std::string::npos) << result.message;
    }

    TEST(VisibilityCommandTests, ItIsRegisteredUnderItsOwnNameAndNoOther)
    {
        bool culling = true;
        Console console;
        RegisterVisibilityCommands(console, VisibilityCommandContext{&culling});
        const std::vector<std::string_view> names = console.Names();
        EXPECT_EQ(names.size(), 1U);
        EXPECT_EQ(names.front(), "cull");
        EXPECT_FALSE(console.Execute("culling off").ok) << "a near miss was accepted";
    }
} // namespace
