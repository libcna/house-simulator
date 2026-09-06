// SPDX-License-Identifier: MIT
//
// `HOUSE-00028`'s acceptance: an error names the file, the JSON path and what was expected.
// Twenty malformed fixtures, because the whole value of this wrapper is what it says when the data
// is wrong -- and a wrapper that is only tested on valid input is tested on the case that does not
// matter.
#include <gtest/gtest.h>

#include "Microsoft/Xna/Framework/BoundingBox.hpp"
#include "Microsoft/Xna/Framework/Vector3.hpp"

#include "cnahouse/util/Json.hpp"

namespace
{
    using cnahouse::util::ErrorCode;
    using cnahouse::util::JsonDocument;
    using cnahouse::util::JsonValue;

    JsonDocument Parse(std::string_view text)
    {
        auto document = JsonDocument::Parse(text, "fixture.json");
        EXPECT_TRUE(document) << (document ? "" : document.Error().ToString());
        return std::move(document).Value();
    }

    constexpr std::string_view kRoom = R"({
  "id": "L0_KITCHEN",
  "floor": 0,
  "height": 2.55,
  "lit": true,
  "origin": [1.0, 0.0, -2.5],
  "bounds": { "min": [0.0, 0.0, 0.0], "max": [4.0, 2.55, 3.0] },
  "portals": [
    { "to": "L0_HALL", "width": 0.9 },
    { "to": "L0_PANTRY", "width": 0.7 }
  ],
  "nested": { "a": { "b": { "c": 7 } } }
})";

    // --- the happy path, briefly ------------------------------------------------------------------

    TEST(JsonTests, ReadsEveryRequiredType)
    {
        const JsonDocument document = Parse(kRoom);
        const JsonValue& root = document.Root();

        EXPECT_EQ(root.RequireString("id").ValueOr(""), "L0_KITCHEN");
        EXPECT_EQ(root.RequireInt("floor").ValueOr(-1), 0);
        EXPECT_FLOAT_EQ(root.RequireFloat("height").ValueOr(0.0f), 2.55f);
        EXPECT_TRUE(root.RequireBool("lit").ValueOr(false));

        auto origin = root.RequireVector3("origin");
        ASSERT_TRUE(origin);
        EXPECT_FLOAT_EQ(origin->X, 1.0f);
        EXPECT_FLOAT_EQ(origin->Z, -2.5f);

        auto bounds = root.RequireBox("bounds");
        ASSERT_TRUE(bounds);
        EXPECT_FLOAT_EQ(bounds->Max.Y, 2.55f);

        auto nested = root.RequireObject("nested");
        ASSERT_TRUE(nested);
        auto a = nested->RequireObject("a");
        ASSERT_TRUE(a);
        auto b = a->RequireObject("b");
        ASSERT_TRUE(b);
        EXPECT_EQ(b->RequireInt("c").ValueOr(-1), 7);
    }

    TEST(JsonTests, ArrayElementsCarryTheirIndexInThePath)
    {
        const JsonDocument document = Parse(kRoom);
        auto portals = document.Root().RequireArray("portals");
        ASSERT_TRUE(portals);
        auto elements = portals->Elements();
        ASSERT_TRUE(elements);
        ASSERT_EQ(elements->size(), 2u);
        EXPECT_EQ((*elements)[1].Path(), "portals[1]");
        EXPECT_EQ((*elements)[1].RequireString("to").ValueOr(""), "L0_PANTRY");
    }

    TEST(JsonTests, OptionalFieldsTakeTheirDefaultAtTheCallSite)
    {
        const JsonDocument document = Parse(kRoom);
        const JsonValue& root = document.Root();
        EXPECT_EQ(root.OptionalString("absent", "fallback").ValueOr(""), "fallback");
        EXPECT_FLOAT_EQ(root.OptionalFloat("absent", 3.5f).ValueOr(0.0f), 3.5f);
        EXPECT_TRUE(root.OptionalBool("absent", true).ValueOr(false));
        // Present-but-wrong is still an error. "Optional" means the field may be missing, not that a
        // string may quietly stand in for a number.
        EXPECT_FALSE(root.OptionalFloat("id", 1.0f));
    }

    // --- the twenty malformed fixtures ---------------------------------------------------------------

    struct BadCase
    {
        const char* name;
        const char* json;
        const char* field;
        enum class Op
        {
            String,
            Int,
            Float,
            Bool,
            Object,
            Array,
            Vector3,
            Box,
        } op;
        ErrorCode code;
        /// A substring the message must contain, so the test asserts the message is USEFUL, not merely
        /// that one exists.
        const char* mustSay;
        const char* mustPath;
    };

    class MalformedJsonTest : public ::testing::TestWithParam<BadCase>
    {
    };

    TEST_P(MalformedJsonTest, ReportsWhatWasExpectedAndWhere)
    {
        const BadCase& c = GetParam();
        auto document = JsonDocument::Parse(c.json, "fixture.json");
        ASSERT_TRUE(document) << "these fixtures are valid JSON with wrong CONTENT";
        const JsonValue& root = document->Root();

        cnahouse::util::Error error;
        bool failed = false;
        auto capture = [&](auto&& result)
        {
            if (!result)
            {
                error = result.Error();
                failed = true;
            }
        };

        switch (c.op)
        {
            case BadCase::Op::String:
                capture(root.RequireString(c.field));
                break;
            case BadCase::Op::Int:
                capture(root.RequireInt(c.field));
                break;
            case BadCase::Op::Float:
                capture(root.RequireFloat(c.field));
                break;
            case BadCase::Op::Bool:
                capture(root.RequireBool(c.field));
                break;
            case BadCase::Op::Object:
                capture(root.RequireObject(c.field));
                break;
            case BadCase::Op::Array:
                capture(root.RequireArray(c.field));
                break;
            case BadCase::Op::Vector3:
                capture(root.RequireVector3(c.field));
                break;
            case BadCase::Op::Box:
                capture(root.RequireBox(c.field));
                break;
        }

        ASSERT_TRUE(failed) << c.name << " should not have been accepted";
        EXPECT_EQ(error.Code(), c.code) << c.name;
        EXPECT_NE(error.Message().find(c.mustSay), std::string::npos)
            << c.name << ": message was '" << error.Message() << "', expected it to mention '" << c.mustSay
            << "'";
        EXPECT_NE(error.Context().find(c.mustPath), std::string::npos)
            << c.name << ": context was '" << error.Context() << "', expected the path '" << c.mustPath
            << "'";
    }

    INSTANTIATE_TEST_SUITE_P(TwentyWaysToBeWrong,
                             MalformedJsonTest,
                             ::testing::Values(BadCase{"missing string",
                                                       R"({})",
                                                       "id",
                                                       BadCase::Op::String,
                                                       ErrorCode::SchemaMismatch,
                                                       "missing",
                                                       "id"},
                                               BadCase{"missing int",
                                                       R"({})",
                                                       "floor",
                                                       BadCase::Op::Int,
                                                       ErrorCode::SchemaMismatch,
                                                       "missing",
                                                       "floor"},
                                               BadCase{"missing object",
                                                       R"({})",
                                                       "bounds",
                                                       BadCase::Op::Object,
                                                       ErrorCode::SchemaMismatch,
                                                       "missing",
                                                       "bounds"},
                                               BadCase{"missing array",
                                                       R"({})",
                                                       "portals",
                                                       BadCase::Op::Array,
                                                       ErrorCode::SchemaMismatch,
                                                       "missing",
                                                       "portals"},
                                               BadCase{"number where a string belongs",
                                                       R"({"id": 7})",
                                                       "id",
                                                       BadCase::Op::String,
                                                       ErrorCode::SchemaMismatch,
                                                       "a string",
                                                       "id"},
                                               BadCase{"string where a number belongs",
                                                       R"({"height": "tall"})",
                                                       "height",
                                                       BadCase::Op::Float,
                                                       ErrorCode::SchemaMismatch,
                                                       "a number",
                                                       "height"},
                                               BadCase{"string where a bool belongs",
                                                       R"({"lit": "yes"})",
                                                       "lit",
                                                       BadCase::Op::Bool,
                                                       ErrorCode::SchemaMismatch,
                                                       "a boolean",
                                                       "lit"},
                                               BadCase{"array where an object belongs",
                                                       R"({"bounds": []})",
                                                       "bounds",
                                                       BadCase::Op::Object,
                                                       ErrorCode::SchemaMismatch,
                                                       "an object",
                                                       "bounds"},
                                               BadCase{"object where an array belongs",
                                                       R"({"portals": {}})",
                                                       "portals",
                                                       BadCase::Op::Array,
                                                       ErrorCode::SchemaMismatch,
                                                       "an array",
                                                       "portals"},
                                               BadCase{"null where a string belongs",
                                                       R"({"id": null})",
                                                       "id",
                                                       BadCase::Op::String,
                                                       ErrorCode::SchemaMismatch,
                                                       "a string",
                                                       "id"},
                                               BadCase{"fractional value where a whole number belongs",
                                                       R"({"floor": 1.5})",
                                                       "floor",
                                                       BadCase::Op::Int,
                                                       ErrorCode::SchemaMismatch,
                                                       "whole number",
                                                       "floor"},
                                               BadCase{"vector with two elements",
                                                       R"({"origin": [1, 2]})",
                                                       "origin",
                                                       BadCase::Op::Vector3,
                                                       ErrorCode::SchemaMismatch,
                                                       "3 numbers",
                                                       "origin"},
                                               BadCase{"vector with four elements",
                                                       R"({"origin": [1, 2, 3, 4]})",
                                                       "origin",
                                                       BadCase::Op::Vector3,
                                                       ErrorCode::SchemaMismatch,
                                                       "3 numbers",
                                                       "origin"},
                                               BadCase{"vector containing a string",
                                                       R"({"origin": [1, "two", 3]})",
                                                       "origin",
                                                       BadCase::Op::Vector3,
                                                       ErrorCode::SchemaMismatch,
                                                       "a number",
                                                       "origin[1]"},
                                               BadCase{"vector that is not an array",
                                                       R"({"origin": 3})",
                                                       "origin",
                                                       BadCase::Op::Vector3,
                                                       ErrorCode::SchemaMismatch,
                                                       "an array",
                                                       "origin"},
                                               BadCase{"box missing its max",
                                                       R"({"bounds": {"min": [0,0,0]}})",
                                                       "bounds",
                                                       BadCase::Op::Box,
                                                       ErrorCode::SchemaMismatch,
                                                       "missing",
                                                       "bounds.max"},
                                               BadCase{"box whose min exceeds its max",
                                                       R"({"bounds": {"min": [1,0,0], "max": [0,1,1]}})",
                                                       "bounds",
                                                       BadCase::Op::Box,
                                                       ErrorCode::OutOfRange,
                                                       "min <= max",
                                                       "bounds"},
                                               BadCase{"box whose min is malformed",
                                                       R"({"bounds": {"min": [0,0], "max": [1,1,1]}})",
                                                       "bounds",
                                                       BadCase::Op::Box,
                                                       ErrorCode::SchemaMismatch,
                                                       "3 numbers",
                                                       "bounds.min"},
                                               BadCase{"a field on a non-object root",
                                                       R"([1, 2, 3])",
                                                       "id",
                                                       BadCase::Op::String,
                                                       ErrorCode::SchemaMismatch,
                                                       "an object",
                                                       "<root>"},
                                               BadCase{"box whose min is a string",
                                                       R"({"bounds": {"min": "origin", "max": [1,1,1]}})",
                                                       "bounds",
                                                       BadCase::Op::Box,
                                                       ErrorCode::SchemaMismatch,
                                                       "an array",
                                                       "bounds.min"}),
                             [](const ::testing::TestParamInfo<BadCase>& caseInfo)
                             {
                                 std::string name = caseInfo.param.name;
                                 for (char& c : name)
                                 {
                                     if (!std::isalnum(static_cast<unsigned char>(c)))
                                     {
                                         c = '_';
                                     }
                                 }
                                 return name;
                             });

    TEST(JsonTests, DeeplyNestedErrorsCarryTheWholePath)
    {
        // The twentieth case, checked directly because it needs three hops to reach the bad value --
        // and the path is the entire point of the wrapper.
        auto document = JsonDocument::Parse(R"({"nested": {"a": {"b": {"c": "seven"}}}})", "fixture.json");
        ASSERT_TRUE(document);
        auto nested = document->Root().RequireObject("nested");
        ASSERT_TRUE(nested);
        auto a = nested->RequireObject("a");
        ASSERT_TRUE(a);
        auto b = a->RequireObject("b");
        ASSERT_TRUE(b);
        auto c = b->RequireInt("c");
        ASSERT_FALSE(c);
        EXPECT_EQ(c.Error().Context(), "nested.a.b.c")
            << "the whole path, so an author can find the value in the file";
        EXPECT_NE(c.Error().Message().find("a number"), std::string::npos);
    }

    TEST(JsonTests, MalformedTextIsAParseFailureNamingTheFile)
    {
        auto document = JsonDocument::Parse("{ this is not json", "rooms/l0.json");
        ASSERT_FALSE(document);
        EXPECT_EQ(document.Error().Code(), ErrorCode::InvalidData);
        EXPECT_EQ(document.Error().Context(), "rooms/l0.json") << "which FILE, always -- §5.4";
    }

    TEST(JsonTests, ErrorToStringCarriesCodeContextAndMessage)
    {
        // The format is `util::Error`'s, not this wrapper's: `<code> [<context>]: <message>`. What
        // this test asserts is that all three parts survive -- the machine-readable code, the JSON
        // path a person needs to find the value, and what was expected.
        auto document = JsonDocument::Parse(R"({"id": 7})", "rooms/l0.json");
        ASSERT_TRUE(document);
        auto id = document->Root().RequireString("id");
        ASSERT_FALSE(id);
        const std::string text = id.Error().ToString();
        EXPECT_NE(text.find("[id]"), std::string::npos) << "'" << text << "'";
        EXPECT_NE(text.find("a string"), std::string::npos) << "'" << text << "'";
        EXPECT_NE(text.find("SchemaMismatch"), std::string::npos)
            << "the code is part of the line, so a log can be grepped by class: '" << text << "'";
    }

} // namespace
