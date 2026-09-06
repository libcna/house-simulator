// SPDX-License-Identifier: MIT
//
// `HOUSE-00025`'s acceptance: (1) a message repeated 1 000x in a frame is logged once with a count;
// (2) categories can be filtered at runtime.
#include <gtest/gtest.h>

#include "cnahouse/util/Log.hpp"

namespace
{
    using cnahouse::util::Log;
    using cnahouse::util::LogCat;
    using cnahouse::util::LogLevel;

    class LogTest : public ::testing::Test
    {
    protected:
        void SetUp() override
        {
            Log::ResetForTesting();
            Log::SetMinimumLevel(LogLevel::Trace);
        }

        void TearDown() override
        {
            Log::ResetForTesting();
        }
    };

    TEST_F(LogTest, AMessageRepeatedAThousandTimesInAFrameIsLoggedOnceWithACount)
    {
        Log::BeginFrame(1);
        for (int i = 0; i < 1000; ++i)
        {
            Log::Warn(LogCat::World, "portal {} is not in its plane", 7);
        }
        ASSERT_EQ(Log::Ring().size(), 1u)
            << "1 000 identical lines would bury every other message within a second at 60 Hz";
        EXPECT_EQ(Log::Ring().front().repeats, 1000u)
            << "the information survives: it happened, this many times";
        EXPECT_EQ(Log::SuppressedCount(), 999u);
    }

    TEST_F(LogTest, TheLimiterIsPerFrameSoARecurringProblemStillReportsEachFrame)
    {
        // Suppressing across frames would hide a problem that is still happening, which is the
        // opposite of what a rate limiter is for.
        for (std::uint64_t frame = 0; frame < 5; ++frame)
        {
            Log::BeginFrame(frame);
            Log::Warn(LogCat::World, "the same problem");
            Log::Warn(LogCat::World, "the same problem");
        }
        EXPECT_EQ(Log::Ring().size(), 5u) << "once per frame, not once ever";
        for (const auto& record : Log::Ring())
        {
            EXPECT_EQ(record.repeats, 2u);
        }
    }

    TEST_F(LogTest, DifferentMessagesInOneFrameAreAllKept)
    {
        Log::BeginFrame(1);
        Log::Warn(LogCat::World, "portal {} is not in its plane", 1);
        Log::Warn(LogCat::World, "portal {} is not in its plane", 2);
        EXPECT_EQ(Log::Ring().size(), 2u) << "the limiter collapses identical text, not similar text";
    }

    TEST_F(LogTest, CategoriesCanBeFilteredAtRuntime)
    {
        Log::BeginFrame(1);
        Log::SetCategoryEnabled(LogCat::Audio, false);
        Log::Info(LogCat::Audio, "muted");
        Log::Info(LogCat::World, "kept");
        ASSERT_EQ(Log::Ring().size(), 1u);
        EXPECT_EQ(Log::Ring().front().category, LogCat::World);
    }

    TEST_F(LogTest, SetEnabledCategoriesTakesTheCommandLineForm)
    {
        const auto unknown = Log::SetEnabledCategories("world, content");
        EXPECT_TRUE(unknown.empty());
        EXPECT_TRUE(Log::IsCategoryEnabled(LogCat::World));
        EXPECT_TRUE(Log::IsCategoryEnabled(LogCat::Content));
        EXPECT_FALSE(Log::IsCategoryEnabled(LogCat::Audio)) << "the named set is exact, not additive";
    }

    TEST_F(LogTest, AnUnknownCategoryIsReportedRatherThanIgnored)
    {
        // A misspelled category is indistinguishable from a subsystem that is simply quiet, which is
        // the worst possible failure for a diagnostic option.
        const auto unknown = Log::SetEnabledCategories("world,wrold");
        ASSERT_EQ(unknown.size(), 1u);
        EXPECT_EQ(unknown.front(), "wrold");
    }

    TEST_F(LogTest, CategoryNamesAreCaseInsensitive)
    {
        EXPECT_EQ(cnahouse::util::ParseLogCat("WORLD"), LogCat::World);
        EXPECT_EQ(cnahouse::util::ParseLogCat("World"), LogCat::World);
        EXPECT_EQ(cnahouse::util::ParseLogCat("nonsense"), LogCat::Count);
    }

    TEST_F(LogTest, TheMinimumLevelDropsMessagesBeforeTheyAreFormatted)
    {
        Log::BeginFrame(1);
        Log::SetMinimumLevel(LogLevel::Warn);
        EXPECT_FALSE(Log::WouldLog(LogLevel::Info, LogCat::World));
        EXPECT_TRUE(Log::WouldLog(LogLevel::Error, LogCat::World));

        // `WouldLog` is the guard a caller uses to skip building an expensive argument, so it must
        // agree with what `Write` actually does.
        Log::Info(LogCat::World, "dropped");
        Log::Error(LogCat::World, "kept");
        ASSERT_EQ(Log::Ring().size(), 1u);
        EXPECT_EQ(Log::Ring().front().level, LogLevel::Error);
    }

    TEST_F(LogTest, LevelOffSilencesEverything)
    {
        Log::BeginFrame(1);
        Log::SetMinimumLevel(LogLevel::Off);
        Log::Fatal(LogCat::App, "not even this");
        EXPECT_TRUE(Log::Ring().empty());
    }

    TEST_F(LogTest, RecordsCarryTheFrameTheyWereEmittedIn)
    {
        Log::BeginFrame(42);
        Log::Info(LogCat::App, "hello");
        ASSERT_EQ(Log::Ring().size(), 1u);
        EXPECT_EQ(Log::Ring().front().frame, 42u) << "so a log line can be correlated with a frame graph";
    }

    TEST_F(LogTest, TheRingIsBoundedAndKeepsTheNewest)
    {
        for (std::uint64_t frame = 0; frame < 2000; ++frame)
        {
            Log::BeginFrame(frame);
            Log::Info(LogCat::App, "line {}", frame);
        }
        EXPECT_LE(Log::Ring().size(), 512u) << "a ring buffer that grows is not a ring buffer";
        EXPECT_EQ(Log::Ring().back().frame, 1999u) << "and it keeps the newest, not the oldest";
    }

    TEST_F(LogTest, TheLimiterSurvivesTheRingWrappingAround)
    {
        // The failure mode this guards: the limiter holds indices into the ring, and evicting the
        // oldest record shifts every one of them. Without the fixup the limiter would update the wrong
        // record's count -- silently, and only under load, which is the worst time to find out.
        Log::BeginFrame(1);
        for (int i = 0; i < 600; ++i)
        {
            Log::Info(LogCat::App, "distinct {}", i);
        }
        Log::Info(LogCat::App, "distinct 599");
        const auto& ring = Log::Ring();
        ASSERT_FALSE(ring.empty());
        EXPECT_EQ(ring.back().message, "distinct 599");
        EXPECT_EQ(ring.back().repeats, 2u) << "the repeat landed on the right record";
    }

} // namespace
