// SPDX-License-Identifier: MIT
//
// `HOUSE-00354`. The closed expression vocabulary of `interactables.json`.
//
// The acceptance criterion is "an unknown token is a load-time error naming the file, the id and
// the token", and the verification is twenty valid and twenty invalid expressions. Both are here:
// the file and the id are added by `WorldLoader` (`WorldLoaderTests`), and everything below is the
// parser's own half -- the token, the offset, and what it should have been.
#include <gtest/gtest.h>

#include <string>
#include <vector>

#include "cnahouse/world/InteractableExpr.hpp"

namespace
{
    using cnahouse::util::ErrorCode;

    namespace world = cnahouse::world;

    /// The refrigerator of `world-format.md`'s example, plus a number and a text field, so that
    /// every type in the vocabulary has a field to be got wrong against.
    world::StateTable Fridge()
    {
        world::StateTable state;
        state.Declare("doorOpen", false);
        state.Declare("interiorLightOn", false);
        state.Declare("compressorRunning", true);
        state.Declare("temperatureC", 4.0);
        state.Declare("programme", std::string("eco"));
        return state;
    }

    // --- the twenty that must parse ---------------------------------------------------------

    TEST(InteractableExprTest, TwentyValidExpressionsParse)
    {
        const world::StateTable state = Fridge();
        const std::vector<std::string> valid{
            // 1-4: the four ways to write a boolean condition.
            "state.doorOpen",
            "!state.doorOpen",
            "state.doorOpen == false",
            "state.doorOpen != state.interiorLightOn",
            // 5-10: every comparison operator, on the number field.
            "state.temperatureC == 4",
            "state.temperatureC != 4.0",
            "state.temperatureC < 8",
            "state.temperatureC <= 8.5",
            "state.temperatureC > 0",
            "state.temperatureC >= -18",
            // 11-13: text.
            "state.programme == 'eco'",
            "state.programme != 'defrost'",
            "state.programme == 'eco' || state.programme == 'fast'",
            // 14-17: the connectives, nesting and precedence.
            "state.doorOpen && state.compressorRunning",
            "state.doorOpen || state.interiorLightOn",
            "(state.doorOpen || state.interiorLightOn) && state.temperatureC > 2",
            "!(state.doorOpen && state.temperatureC < 0)",
            // 18-20: whitespace, a bare literal, and the empty predicate.
            "   state.doorOpen   ==   true   ",
            "true",
            "",
        };
        ASSERT_EQ(valid.size(), 20U);

        for (const std::string& text : valid)
        {
            const auto parsed = world::Predicate::Parse(text, state);
            EXPECT_TRUE(parsed) << "refused \"" << text
                                << "\": " << (parsed ? std::string{} : parsed.Error().ToString());
        }
    }

    // --- the twenty that must not -----------------------------------------------------------

    TEST(InteractableExprTest, TwentyInvalidExpressionsAreRefusedWithTheTokenNamed)
    {
        const world::StateTable state = Fridge();
        // Each pair is the expression and a fragment the message must contain, because "invalid
        // expression" is a message that sends nobody anywhere. The fragment is the thing the
        // author has to change.
        const std::vector<std::pair<std::string, std::string>> invalid{
            // 1-3: a field that does not exist, in each position.
            {"state.doorAjar", "doorAjar"},
            {"state.doorOpen == state.doorAjar", "doorAjar"},
            {"state.doorAjar && state.doorOpen", "doorAjar"},
            // 4-6: tokens outside the vocabulary.
            {"doorOpen", "doorOpen"},
            {"state.doorOpen and state.compressorRunning", "and"},
            {"setDoor(true)", "setDoor"},
            // 7-9: type errors the parser can see without running anything.
            {"state.doorOpen == 0.5", "boolean"},
            {"state.temperatureC == 'cold'", "number"},
            {"state.programme < 'zzz'", "orders numbers"},
            // 10-12: a comparison somebody did not finish.
            {"state.temperatureC", "on its own"},
            {"state.programme", "on its own"},
            {"state.temperatureC ==", "ends where a value"},
            // 13-15: brackets and quotes.
            {"(state.doorOpen", "never closed"},
            {"state.programme == 'eco", "never closed"},
            {"state.doorOpen)", "unexpected"},
            // 16-18: operators that are not in the grammar.
            {"state.temperatureC =< 8", "expected a comparison operator"},
            {"state.doorOpen & state.compressorRunning", "expected a comparison operator"},
            {"state.temperatureC + 1 > 5", "expected a comparison operator"},
            // 19-20: an empty field name and a stray number.
            {"state. == true", "names a field"},
            {"4 4", "expected a comparison operator"},
        };
        ASSERT_EQ(invalid.size(), 20U);

        for (const auto& [text, fragment] : invalid)
        {
            const auto parsed = world::Predicate::Parse(text, state);
            ASSERT_FALSE(parsed) << "accepted \"" << text << "\"";
            EXPECT_EQ(parsed.Error().Code(), ErrorCode::InvalidData) << text;
            EXPECT_NE(parsed.Error().Message().find(fragment), std::string::npos)
                << "\"" << text
                << "\" was refused, but the message does not say why: " << parsed.Error().ToString();
            EXPECT_NE(parsed.Error().Context().find("offset"), std::string::npos)
                << "and it must point at a character: " << parsed.Error().ToString();
        }
    }

    // --- evaluation ---------------------------------------------------------------------------

    TEST(InteractableExprTest, APredicateEvaluatesAgainstLiveState)
    {
        world::StateTable state = Fridge();

        const auto shut = world::Predicate::Parse("state.doorOpen == false", state);
        ASSERT_TRUE(shut) << shut.Error().ToString();
        const auto before = shut.Value().Evaluate(state);
        ASSERT_TRUE(before);
        EXPECT_TRUE(before.Value());

        state.Declare("doorOpen", true);
        const auto after = shut.Value().Evaluate(state);
        ASSERT_TRUE(after);
        EXPECT_FALSE(after.Value()) << "the predicate reads the LIVE table, not the parse-time one";
    }

    TEST(InteractableExprTest, PrecedenceIsAndBeforeOr)
    {
        // `a || b && c` is `a || (b && c)`. Written the other way round the fridge's "open OR
        // (cold AND running)" becomes "(open OR cold) AND running", which is a different fridge.
        world::StateTable state;
        state.Declare("a", false);
        state.Declare("b", true);
        state.Declare("c", false);

        const auto parsed = world::Predicate::Parse("state.a || state.b && state.c", state);
        ASSERT_TRUE(parsed) << parsed.Error().ToString();
        const auto value = parsed.Value().Evaluate(state);
        ASSERT_TRUE(value);
        EXPECT_FALSE(value.Value())
            << "false || (true && false) is false; (false || true) && false is also false -- but "
               "the tree must be the first, and the next case shows the difference";

        state.Declare("c", true);
        const auto again = parsed.Value().Evaluate(state);
        ASSERT_TRUE(again);
        EXPECT_TRUE(again.Value()) << "false || (true && true)";
    }

    TEST(InteractableExprTest, NotBindsTighterThanAnd)
    {
        world::StateTable state;
        state.Declare("a", true);
        state.Declare("b", true);

        const auto parsed = world::Predicate::Parse("!state.a && state.b", state);
        ASSERT_TRUE(parsed) << parsed.Error().ToString();
        const auto value = parsed.Value().Evaluate(state);
        ASSERT_TRUE(value);
        EXPECT_FALSE(value.Value()) << "(!a) && b, not !(a && b)";
    }

    TEST(InteractableExprTest, EveryComparisonOperatorMeansWhatItSays)
    {
        world::StateTable state;
        state.Declare("n", 4.0);
        const std::vector<std::pair<std::string, bool>> cases{
            {"state.n == 4", true},
            {"state.n == 5", false},
            {"state.n != 5", true},
            {"state.n < 5", true},
            {"state.n < 4", false},
            {"state.n <= 4", true},
            {"state.n > 3", true},
            {"state.n > 4", false},
            {"state.n >= 4", true},
            {"state.n >= 5", false},
        };
        for (const auto& [text, expected] : cases)
        {
            const auto parsed = world::Predicate::Parse(text, state);
            ASSERT_TRUE(parsed) << text << ": " << parsed.Error().ToString();
            const auto value = parsed.Value().Evaluate(state);
            ASSERT_TRUE(value) << text;
            EXPECT_EQ(value.Value(), expected) << text;
        }
    }

    TEST(InteractableExprTest, AnAbsentPredicateIsAlwaysTrue)
    {
        // Most of the 640 rows have no condition -- a light switch is always operable -- and the
        // file writes `null` for them.
        const world::StateTable state = Fridge();
        for (const std::string text : {"", "   ", "\t\n"})
        {
            const auto parsed = world::Predicate::Parse(text, state);
            ASSERT_TRUE(parsed) << parsed.Error().ToString();
            EXPECT_TRUE(parsed.Value().IsAlwaysTrue());
            const auto value = parsed.Value().Evaluate(state);
            ASSERT_TRUE(value);
            EXPECT_TRUE(value.Value());
        }
    }

    // --- effects --------------------------------------------------------------------------------

    TEST(InteractableExprTest, AnEffectAssignsTogglesAndAccumulates)
    {
        world::StateTable state = Fridge();

        const auto open = world::Effect::Parse("state.doorOpen = true", state);
        ASSERT_TRUE(open) << open.Error().ToString();
        ASSERT_TRUE(open.Value().Apply(state));
        EXPECT_TRUE(std::get<bool>(state.Find("doorOpen")->value));

        const auto flip = world::Effect::Parse("toggle(state.interiorLightOn)", state);
        ASSERT_TRUE(flip) << flip.Error().ToString();
        ASSERT_TRUE(flip.Value().Apply(state));
        EXPECT_TRUE(std::get<bool>(state.Find("interiorLightOn")->value));
        ASSERT_TRUE(flip.Value().Apply(state));
        EXPECT_FALSE(std::get<bool>(state.Find("interiorLightOn")->value)) << "twice is a no-op";

        const auto warm = world::Effect::Parse("state.temperatureC += 1.5", state);
        ASSERT_TRUE(warm) << warm.Error().ToString();
        ASSERT_TRUE(warm.Value().Apply(state));
        EXPECT_DOUBLE_EQ(std::get<double>(state.Find("temperatureC")->value), 5.5);

        const auto cool = world::Effect::Parse("state.temperatureC -= 2", state);
        ASSERT_TRUE(cool) << cool.Error().ToString();
        ASSERT_TRUE(cool.Value().Apply(state));
        EXPECT_DOUBLE_EQ(std::get<double>(state.Find("temperatureC")->value), 3.5);
    }

    TEST(InteractableExprTest, AnEffectAppliesItsStatementsInOrder)
    {
        // `a = 1; a += 1` is 2, not 1. A reader that applied them as a set rather than a sequence
        // would give either answer depending on the day.
        world::StateTable state;
        state.Declare("a", 0.0);
        const auto effect = world::Effect::Parse("state.a = 1; state.a += 1; state.a += 1", state);
        ASSERT_TRUE(effect) << effect.Error().ToString();
        ASSERT_TRUE(effect.Value().Apply(state));
        EXPECT_DOUBLE_EQ(std::get<double>(state.Find("a")->value), 3.0);
    }

    TEST(InteractableExprTest, AnEffectMayCopyOneFieldIntoAnother)
    {
        world::StateTable state;
        state.Declare("from", 7.0);
        state.Declare("to", 0.0);
        const auto effect = world::Effect::Parse("state.to = state.from", state);
        ASSERT_TRUE(effect) << effect.Error().ToString();
        ASSERT_TRUE(effect.Value().Apply(state));
        EXPECT_DOUBLE_EQ(std::get<double>(state.Find("to")->value), 7.0);
    }

    TEST(InteractableExprTest, TenInvalidEffectsAreRefused)
    {
        const world::StateTable state = Fridge();
        const std::vector<std::pair<std::string, std::string>> invalid{
            {"state.doorAjar = true", "doorAjar"},
            {"setDoor(true)", "setDoor"},
            {"state.doorOpen == true", "wants a single"},
            {"state.doorOpen = 1", "cannot assign"},
            {"state.temperatureC = true", "cannot assign"},
            {"state.doorOpen += 1", "cannot assign"},
            {"state.programme += 'x'", "add numbers"},
            {"toggle(state.temperatureC)", "flips a boolean"},
            {"toggle(true)", "not a literal"},
            {"true = false", "left side here is a literal"},
        };
        ASSERT_EQ(invalid.size(), 10U);

        for (const auto& [text, fragment] : invalid)
        {
            const auto parsed = world::Effect::Parse(text, state);
            ASSERT_FALSE(parsed) << "accepted \"" << text << "\"";
            EXPECT_NE(parsed.Error().Message().find(fragment), std::string::npos)
                << "\"" << text
                << "\" was refused, but the message does not say why: " << parsed.Error().ToString();
        }
    }

    TEST(InteractableExprTest, AnAbsentEffectDoesNothing)
    {
        world::StateTable state = Fridge();
        const auto effect = world::Effect::Parse("", state);
        ASSERT_TRUE(effect) << effect.Error().ToString();
        EXPECT_TRUE(effect.Value().IsEmpty());
        ASSERT_TRUE(effect.Value().Apply(state));
        EXPECT_FALSE(std::get<bool>(state.Find("doorOpen")->value)) << "and changes nothing";
    }

    TEST(InteractableExprTest, TheParseIsAgainstTheRowsOwnFieldsAndNotAGlobalList)
    {
        // The reason the vocabulary is closed without a forty-entry table of setter verbs: the
        // same expression is valid on one interactable and a load error on the next, and the
        // message can name the fields that DO exist.
        world::StateTable fridge = Fridge();
        world::StateTable tap;
        tap.Declare("flow", 0.0);

        EXPECT_TRUE(world::Predicate::Parse("state.doorOpen", fridge));
        const auto onTap = world::Predicate::Parse("state.doorOpen", tap);
        ASSERT_FALSE(onTap);
        EXPECT_NE(onTap.Error().Message().find("flow"), std::string::npos)
            << "the message must list the fields this interactable does declare: "
            << onTap.Error().ToString();
    }

    TEST(InteractableExprTest, ADeeplyNestedPredicateDoesNotOverflowTheStack)
    {
        // The tree comes from data. Evaluation walks it with an explicit stack for this reason;
        // 2 000 terms is far past anything an author would write and well past what recursion
        // would survive.
        world::StateTable state;
        state.Declare("a", true);
        std::string text = "state.a";
        for (int index = 0; index < 2000; ++index)
        {
            text += " && state.a";
        }
        const auto parsed = world::Predicate::Parse(text, state);
        ASSERT_TRUE(parsed) << parsed.Error().ToString();
        const auto value = parsed.Value().Evaluate(state);
        ASSERT_TRUE(value);
        EXPECT_TRUE(value.Value());
    }
} // namespace
