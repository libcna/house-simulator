// SPDX-License-Identifier: MIT
//
// `HOUSE-03033`. A relative content path is a title path, read through XNA's `TitleContainer` --
// the only reader that reaches Android's APK assets -- and an absolute one is a file.
#include <string>

#include <gtest/gtest.h>

#include "cnahouse/util/ContentFile.hpp"

namespace
{
    using cnahouse::util::ContentExists;
    using cnahouse::util::IsTitlePath;
    using cnahouse::util::ReadContentText;

    TEST(ContentFileTests, RelativeIsATitlePathAndAbsoluteIsAFile)
    {
        EXPECT_TRUE(IsTitlePath("content/world/layout.levels.json"));
        EXPECT_FALSE(IsTitlePath("/opt/cna-house/content/world/layout.levels.json"));
        EXPECT_FALSE(IsTitlePath("C:/cna-house/content"));
    }

    TEST(ContentFileTests, BothRoutesReadTheSameDeployedFile)
    {
        const std::string absolute = std::string(CNAHOUSE_TEST_CONTENT_ROOT) + "/world/layout.levels.json";
        if (!ContentExists(absolute))
        {
            GTEST_SKIP() << "no deployed world";
        }
        const auto byFile = ReadContentText(absolute);
        const auto byTitle = ReadContentText("content/world/layout.levels.json");
        ASSERT_TRUE(byFile) << byFile.Error().ToString();
        ASSERT_TRUE(byTitle) << byTitle.Error().ToString();
        EXPECT_FALSE(byTitle->empty());
        EXPECT_EQ(byTitle.Value(), byFile.Value()) << "the title route found another copy";
    }

    TEST(ContentFileTests, MissingContentIsReportedNotThrown)
    {
        EXPECT_FALSE(ContentExists("content/world/no-such-file.json"));
        EXPECT_FALSE(ContentExists("/no/such/file.json"));
        EXPECT_FALSE(ReadContentText("content/world/no-such-file.json"));
    }
} // namespace
