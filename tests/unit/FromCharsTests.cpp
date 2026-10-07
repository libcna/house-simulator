// SPDX-License-Identifier: MIT
//
// `cnahouse::util::FromChars` is `std::from_chars` where the standard library allows it and a strtod
// reconstruction where Apple's libc++ does not. These cases pin the contract both must share, so the
// same expectations hold whichever one this platform compiled.
#include <gtest/gtest.h>

#include <limits>
#include <string_view>
#include <system_error>

#include "cnahouse/util/FromChars.hpp"

namespace
{
    using cnahouse::util::FromChars;

    TEST(FromChars, ParsesTheWholeRange)
    {
        constexpr std::string_view text = "12.5";
        double value = 0.0;
        const auto result = FromChars(text.data(), text.data() + text.size(), value);
        EXPECT_EQ(result.ec, std::errc{});
        EXPECT_EQ(result.ptr, text.data() + text.size());
        EXPECT_DOUBLE_EQ(value, 12.5);

        float single = 0.0F;
        constexpr std::string_view exponent = "-1.25e2";
        const auto singleResult = FromChars(exponent.data(), exponent.data() + exponent.size(), single);
        EXPECT_EQ(singleResult.ec, std::errc{});
        EXPECT_EQ(singleResult.ptr, exponent.data() + exponent.size());
        EXPECT_FLOAT_EQ(single, -125.0F);
    }

    TEST(FromChars, StopsAtTheEndOfTheRangeNotAtTheEndOfTheString)
    {
        // The digits after the range must not be read: "12" restricted to its first character is 1.
        constexpr std::string_view text = "12";
        double value = 0.0;
        const auto result = FromChars(text.data(), text.data() + 1, value);
        EXPECT_EQ(result.ec, std::errc{});
        EXPECT_EQ(result.ptr, text.data() + 1);
        EXPECT_DOUBLE_EQ(value, 1.0);
    }

    TEST(FromChars, ReportsTrailingCharactersThroughPtr)
    {
        constexpr std::string_view text = "12abc";
        double value = 0.0;
        const auto result = FromChars(text.data(), text.data() + text.size(), value);
        EXPECT_EQ(result.ec, std::errc{});
        EXPECT_EQ(result.ptr, text.data() + 2);
        EXPECT_DOUBLE_EQ(value, 12.0);
    }

    TEST(FromChars, RefusesWhatStrtodAcceptsButFromCharsDoesNot)
    {
        for (const std::string_view text :
             {std::string_view(" 1"), std::string_view("+1"), std::string_view("")})
        {
            double value = 7.0;
            const auto result = FromChars(text.data(), text.data() + text.size(), value);
            EXPECT_EQ(result.ec, std::errc::invalid_argument) << '"' << text << '"';
            EXPECT_EQ(result.ptr, text.data()) << '"' << text << '"';
            EXPECT_DOUBLE_EQ(value, 7.0) << '"' << text << '"';
        }
    }

    TEST(FromChars, ReadsOnlyTheZeroOfAHexadecimalPrefix)
    {
        constexpr std::string_view text = "0x10";
        double value = 7.0;
        const auto result = FromChars(text.data(), text.data() + text.size(), value);
        EXPECT_EQ(result.ec, std::errc{});
        EXPECT_EQ(result.ptr, text.data() + 1);
        EXPECT_DOUBLE_EQ(value, 0.0);
    }

    TEST(FromChars, ReportsOverflowAndLeavesTheValueAlone)
    {
        constexpr std::string_view text = "1e999";
        double value = 7.0;
        const auto result = FromChars(text.data(), text.data() + text.size(), value);
        EXPECT_EQ(result.ec, std::errc::result_out_of_range);
        EXPECT_EQ(result.ptr, text.data() + text.size());
        EXPECT_DOUBLE_EQ(value, 7.0);
    }

    TEST(FromChars, ReadsInfinity)
    {
        constexpr std::string_view text = "-inf";
        double value = 0.0;
        const auto result = FromChars(text.data(), text.data() + text.size(), value);
        EXPECT_EQ(result.ec, std::errc{});
        EXPECT_EQ(result.ptr, text.data() + text.size());
        EXPECT_EQ(value, -std::numeric_limits<double>::infinity());
    }
} // namespace
