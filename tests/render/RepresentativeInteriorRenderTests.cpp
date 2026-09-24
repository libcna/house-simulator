// SPDX-License-Identifier: MIT
//
// `HOUSE-01275`: the bounded representative lighting set.  These are existing fixed review poses,
// one for each planning zone plus one for each retained hero area, photographed once by day and
// once at scheduled night.  It is deliberately not an all-lights-on/off matrix.
#include <array>
#include <cstdint>
#include <cstdio>
#include <set>
#include <string>

#include <gtest/gtest.h>

#include "cnahouse/app/CommandLine.hpp"

#include "render/RenderHarness.hpp"

namespace
{
    using cnahouse::app::Options;
    using cnahouse::app::QualityPreset;
    using cnahouse::app::RenderTier;
    using cnahouse::testsupport::RenderHarness;

    constexpr int kWidth = 640;
    constexpr int kHeight = 360;
    constexpr std::uint64_t kSeed = 6840335469064670721ULL;

    struct Pose
    {
        const char* name;
        std::array<float, 5> player;
        /// Exactly one of these is non-null: this row represents a zone or a hero area.
        const char* zone;
        const char* hero;
    };

    struct Condition
    {
        const char* name;
        float hour;
    };

    /// Eleven zone representatives followed by the five retained hero representatives.  Every
    /// coordinate and heading is copied from `capture_review.py`; the test invents no new camera.
    constexpr Pose kPoses[] = {
        {"basement-workshop", {-11.90F, -2.30F, -16.30F, 90.0F, 0.0F}, "Z-B1", nullptr},
        {"butlers-from-kitchen", {-8.55F, 0.60F, -24.00F, 270.0F, 0.0F}, "Z-L0M", nullptr},
        {"ground-office", {-11.90F, 0.60F, -16.30F, 90.0F, 0.0F}, "Z-L0S", nullptr},
        {"garage-interior", {9.70F, 0.15F, -17.50F, 90.0F, 0.0F}, "Z-GAR", nullptr},
        {"bedroom-2", {3.00F, 3.65F, -25.00F, 90.0F, 0.0F}, "Z-L1", nullptr},
        {"games-room", {-6.20F, 6.55F, -24.80F, 90.0F, 0.0F}, "Z-L2", nullptr},
        {"attic-store-west", {-11.90F, 9.30F, -20.50F, 90.0F, 0.0F}, "Z-L3", nullptr},
        {"main-stair-foot", {4.10F, 0.60F, -14.70F, 0.0F, 0.0F}, "Z-STAIR", nullptr},
        {"garage-approach", {13.00F, 0.00F, -4.00F, 0.0F, 0.0F}, "Z-EXF", nullptr},
        {"garden", {-15.00F, -0.35F, -42.80F, 180.0F, 0.0F}, "Z-EXR", nullptr},
        {"neighbourhood-street", {0.00F, 0.00F, 20.00F, 0.0F, 0.0F}, "Z-STR", nullptr},
        {"exterior-front", {0.00F, 0.00F, 5.20F, 0.0F, 3.0F}, nullptr, "H1"},
        {"living-composition", {-3.10F, 0.60F, -18.50F, 270.0F, 0.0F}, nullptr, "H2"},
        {"kitchen-facing-west", {-1.10F, 0.60F, -25.05F, 270.0F, 0.0F}, nullptr, "H3"},
        {"library", {-7.40F, 6.55F, -16.30F, 90.0F, 0.0F}, nullptr, "H5"},
        {"basement-cinema", {3.00F, -2.30F, -25.00F, 90.0F, 0.0F}, nullptr, "H6"},
    };

    constexpr Condition kConditions[] = {{"day", 10.5F}, {"night", 22.0F}};

    bool ContentIsBuilt()
    {
        const std::string path = std::string(CNAHOUSE_TEST_CONTENT_ROOT) + "/world/collision.bin";
        FILE* file = std::fopen(path.c_str(), "rb");
        if (file == nullptr)
        {
            return false;
        }
        std::fclose(file);
        return true;
    }

    Options OptionsFor(const Pose& pose, const Condition& condition)
    {
        Options options;
        options.tier = RenderTier::S;
        options.quality = QualityPreset::High;
        options.noAudio = true;
        options.contentRoot = CNAHOUSE_TEST_CONTENT_ROOT;
        options.scene = "walk";
        options.player = pose.player;
        options.seed = kSeed;
        options.timeOfDay = condition.hour;
        options.freezeTime = true;
        options.weather = "W_CLEAR";
        // Frame two is the smallest deterministic warm-up: frame one still carries CNA's initial
        // sampler state, while frame three has accumulated variable wall time in dark interiors.
        options.screenshotFrame = 2;
        return options;
    }

    std::string Stem(const Pose& pose, const Condition& condition)
    {
        return std::string("representative-") + condition.name + "-" + pose.name;
    }

    std::string ReferencePath(const Pose& pose, const Condition& condition)
    {
        return RenderHarness::ReferenceDirectory() + "/" + Stem(pose, condition) + ".png";
    }

    std::string ActualPath(const Pose& pose, const Condition& condition)
    {
        return std::string(CNAHOUSE_TEST_OUTPUT_DIR) + "/" + Stem(pose, condition) + "-actual.png";
    }

    TEST(RepresentativeInteriorRenderTests, TheSuiteIsElevenZonesPlusFiveHeroAreasAtTwoTimes)
    {
        EXPECT_EQ(std::size(kPoses), 16U);
        EXPECT_EQ(std::size(kConditions), 2U);

        std::set<std::string> names;
        std::set<std::string> zones;
        std::set<std::string> heroes;
        for (const Pose& pose : kPoses)
        {
            EXPECT_TRUE(names.emplace(pose.name).second) << pose.name;
            EXPECT_NE(pose.zone == nullptr, pose.hero == nullptr) << pose.name;
            if (pose.zone != nullptr)
            {
                zones.emplace(pose.zone);
            }
            else
            {
                heroes.emplace(pose.hero);
            }
        }

        const std::set<std::string> expectedZones{
            "Z-B1", "Z-EXF", "Z-EXR", "Z-GAR", "Z-L0M", "Z-L0S", "Z-L1", "Z-L2", "Z-L3", "Z-STAIR", "Z-STR"};
        const std::set<std::string> expectedHeroes{"H1", "H2", "H3", "H5", "H6"};
        EXPECT_EQ(zones, expectedZones);
        EXPECT_EQ(heroes, expectedHeroes);
    }

    TEST(RepresentativeInteriorRenderTests, DayAndScheduledNightMatchTheirReferences)
    {
        if (!ContentIsBuilt())
        {
            GTEST_SKIP() << "no content/world/collision.bin";
        }

        for (const Condition& condition : kConditions)
        {
            for (const Pose& pose : kPoses)
            {
                SCOPED_TRACE(std::string(condition.name) + ":" + pose.name);
                const std::string actual = ActualPath(pose, condition);
                if (RenderHarness::RenderingInSoftware())
                {
                    const auto diff = RenderHarness::CompareWithReference(
                        OptionsFor(pose, condition),
                        kWidth,
                        kHeight,
                        actual,
                        ReferencePath(pose, condition),
                        2,
                        RenderHarness::NonDeterministicRegions(kWidth, kHeight));
                    ASSERT_TRUE(diff.HasValue()) << diff.Error().ToString();
                    EXPECT_FALSE(diff->sizeMismatch) << diff->ToString();
                    EXPECT_LT(diff->DifferingFraction(), 0.002) << diff->ToString();
                }
                else
                {
                    ASSERT_TRUE(
                        RenderHarness::CaptureFrame(OptionsFor(pose, condition), kWidth, kHeight, actual));
                }
            }
        }
    }

    TEST(RepresentativeInteriorRenderTests, DISABLED_RegenerateRepresentativeReferences)
    {
        ASSERT_TRUE(ContentIsBuilt()) << "no content/world/collision.bin";
        ASSERT_TRUE(RenderHarness::RenderingInSoftware())
            << "the references must be generated through the CI software renderer";
        for (const Condition& condition : kConditions)
        {
            for (const Pose& pose : kPoses)
            {
                const std::string reference = ReferencePath(pose, condition);
                ASSERT_TRUE(
                    RenderHarness::CaptureFrame(OptionsFor(pose, condition), kWidth, kHeight, reference))
                    << condition.name << ":" << pose.name;
                std::printf("  wrote %s\n", reference.c_str());
            }
        }
    }
} // namespace
