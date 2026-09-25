// SPDX-License-Identifier: MIT
//
// HOUSE-03520: the bounded representative environment set.  It reuses the production capture
// path and the same thirteen rows as tools/visual/capture_review.py: no weather/time cross product.
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

    struct Scene
    {
        const char* name;
        std::array<float, 5> player;
        float hour;
        const char* weather;
    };

    // Clear carries the time-of-day comparison from both requested viewpoints. Overcast and rain
    // add the noon weather comparison; the final four frames directly guard D5's shelter, wetness,
    // indoor-rain and non-particle snow-state boundaries.
    constexpr Scene kScenes[] = {
        {"front-clear-noon", {0.00F, 0.00F, 5.20F, 0.0F, 3.0F}, 12.0F, "W_CLEAR"},
        {"front-clear-dusk", {0.00F, 0.00F, 5.20F, 0.0F, 3.0F}, 17.0F, "W_CLEAR"},
        {"front-clear-night", {0.00F, 0.00F, 5.20F, 0.0F, 3.0F}, 22.0F, "W_CLEAR"},
        {"front-overcast-noon", {0.00F, 0.00F, 5.20F, 0.0F, 3.0F}, 12.0F, "W_OVERCAST"},
        {"front-rain-noon", {0.00F, 0.00F, 5.20F, 0.0F, 3.0F}, 12.0F, "W_RAIN"},
        {"upper-window-clear-noon", {-6.20F, 3.65F, -24.80F, 0.0F, 0.0F}, 12.0F, "W_CLEAR"},
        {"upper-window-clear-dusk", {-6.20F, 3.65F, -24.80F, 0.0F, 0.0F}, 17.0F, "W_CLEAR"},
        {"upper-window-clear-night", {-6.20F, 3.65F, -24.80F, 0.0F, 0.0F}, 22.0F, "W_CLEAR"},
        {"upper-window-overcast-noon", {-6.20F, 3.65F, -24.80F, 0.0F, 0.0F}, 12.0F, "W_OVERCAST"},
        {"upper-window-heavy-rain", {-6.20F, 3.65F, -24.80F, 0.0F, 0.0F}, 14.0F, "W_HEAVY_RAIN"},
        {"under-porch-rain", {0.00F, 0.60F, -12.10F, 180.0F, 0.0F}, 14.0F, "W_RAIN"},
        {"wet-driveway-rain", {13.00F, 0.00F, -4.00F, 0.0F, 0.0F}, 14.0F, "W_HEAVY_RAIN"},
        {"front-snow-state-dawn", {0.00F, 0.00F, 5.20F, 0.0F, 3.0F}, 6.0F, "W_SNOW"},
    };

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

    Options OptionsFor(const Scene& scene)
    {
        Options options;
        options.tier = RenderTier::S;
        options.quality = QualityPreset::High;
        options.noAudio = true;
        options.contentRoot = CNAHOUSE_TEST_CONTENT_ROOT;
        options.scene = "walk";
        options.player = scene.player;
        options.seed = kSeed;
        options.timeOfDay = scene.hour;
        options.freezeTime = true;
        options.weather = scene.weather;
        options.screenshotFrame = 2;
        return options;
    }

    std::string Stem(const Scene& scene)
    {
        return std::string("environment-") + scene.name;
    }

    std::string ReferencePath(const Scene& scene)
    {
        return RenderHarness::ReferenceDirectory() + "/" + Stem(scene) + ".png";
    }

    std::string ActualPath(const Scene& scene)
    {
        return std::string(CNAHOUSE_TEST_OUTPUT_DIR) + "/" + Stem(scene) + "-actual.png";
    }

    TEST(EnvironmentRenderTests, TheCompactSetHasTheRequiredViewpointsStatesAndBoundaries)
    {
        ASSERT_EQ(std::size(kScenes), 13U);

        std::set<std::string> names;
        std::set<std::string> weather;
        std::set<float> frontClearHours;
        std::set<float> upperClearHours;
        for (const Scene& scene : kScenes)
        {
            EXPECT_TRUE(names.emplace(scene.name).second) << scene.name;
            weather.emplace(scene.weather);
            const std::string name(scene.name);
            if (name.starts_with("front-clear-"))
            {
                frontClearHours.emplace(scene.hour);
            }
            if (name.starts_with("upper-window-clear-"))
            {
                upperClearHours.emplace(scene.hour);
            }
        }

        const std::set<std::string> requiredWeather{
            "W_CLEAR", "W_HEAVY_RAIN", "W_OVERCAST", "W_RAIN", "W_SNOW"};
        const std::set<float> requiredHours{12.0F, 17.0F, 22.0F};
        EXPECT_EQ(weather, requiredWeather);
        EXPECT_EQ(frontClearHours, requiredHours);
        EXPECT_EQ(upperClearHours, requiredHours);
        EXPECT_TRUE(names.contains("upper-window-heavy-rain"));
        EXPECT_TRUE(names.contains("under-porch-rain"));
        EXPECT_TRUE(names.contains("wet-driveway-rain"));
        EXPECT_TRUE(names.contains("front-snow-state-dawn"));
    }

    TEST(EnvironmentRenderTests, RepresentativeEnvironmentMatchesItsReferences)
    {
        if (!ContentIsBuilt())
        {
            GTEST_SKIP() << "no content/world/collision.bin";
        }

        for (const Scene& scene : kScenes)
        {
            SCOPED_TRACE(scene.name);
            const std::string actual = ActualPath(scene);
            if (RenderHarness::RenderingInSoftware())
            {
                const auto diff = RenderHarness::CompareWithReference(
                    OptionsFor(scene),
                    kWidth,
                    kHeight,
                    actual,
                    ReferencePath(scene),
                    2,
                    RenderHarness::NonDeterministicRegions(kWidth, kHeight));
                ASSERT_TRUE(diff.HasValue()) << diff.Error().ToString();
                EXPECT_FALSE(diff->sizeMismatch) << diff->ToString();
                EXPECT_LT(diff->DifferingFraction(), 0.002) << diff->ToString();
            }
            else
            {
                ASSERT_TRUE(RenderHarness::CaptureFrame(OptionsFor(scene), kWidth, kHeight, actual));
            }
        }
    }

    TEST(EnvironmentRenderTests, DISABLED_RegenerateEnvironmentReferences)
    {
        ASSERT_TRUE(ContentIsBuilt()) << "no content/world/collision.bin";
        ASSERT_TRUE(RenderHarness::RenderingInSoftware())
            << "the references must be generated through the CI software renderer";
        for (const Scene& scene : kScenes)
        {
            const std::string reference = ReferencePath(scene);
            ASSERT_TRUE(RenderHarness::CaptureFrame(OptionsFor(scene), kWidth, kHeight, reference))
                << scene.name;
            std::printf("  wrote %s\n", reference.c_str());
        }
    }
} // namespace
