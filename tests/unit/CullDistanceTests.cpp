// SPDX-License-Identifier: MIT
//
// `HOUSE-00674`. §25.6's per-category distances, which are what stands between the frame and the
// ~4 100 instances of `EXT_WORLD` -- one cell, where portal traversal cannot help at all.
#include <cstdio>
#include <string>

#include <gtest/gtest.h>

#include "cnahouse/rendering/Quality.hpp"
#include "cnahouse/visibility/CullDistance.hpp"

namespace
{
    using cnahouse::visibility::CategoryName;
    using cnahouse::visibility::CullDistanceFor;
    using cnahouse::visibility::kCullDistances;
    using cnahouse::visibility::PropCategory;
    using cnahouse::visibility::WithinCullDistance;
} // namespace

TEST(CullDistanceTests, TheEightDistancesAreTheOnesSectionTwentyFiveStates)
{
    // *"small props 45 m, garden furniture 70 m, fences 120 m, trees 180 m, neighbourhood LOD0
    // 90 m, LOD1 160 m, LOD2 300 m, impostors 420 m"* -- transcribed, and asserted here because a
    // transcription is exactly the kind of thing that is right until somebody tidies it.
    EXPECT_FLOAT_EQ(CullDistanceFor(PropCategory::SmallProp), 45.0F);
    EXPECT_FLOAT_EQ(CullDistanceFor(PropCategory::GardenFurniture), 70.0F);
    EXPECT_FLOAT_EQ(CullDistanceFor(PropCategory::Fence), 120.0F);
    EXPECT_FLOAT_EQ(CullDistanceFor(PropCategory::Tree), 180.0F);
    EXPECT_FLOAT_EQ(CullDistanceFor(PropCategory::NeighbourhoodLod0), 90.0F);
    EXPECT_FLOAT_EQ(CullDistanceFor(PropCategory::NeighbourhoodLod1), 160.0F);
    EXPECT_FLOAT_EQ(CullDistanceFor(PropCategory::NeighbourhoodLod2), 300.0F);
    EXPECT_FLOAT_EQ(CullDistanceFor(PropCategory::Impostor), 420.0F);
    EXPECT_EQ(kCullDistances.size(), static_cast<std::size_t>(PropCategory::Count));
}

TEST(CullDistanceTests, TheOrderOfTheCategoriesIsTheOrderOfTheirDistancesExceptWhereItIsNot)
{
    // The three neighbourhood rows are a LOD ladder and grow; the four prop rows grow; and the two
    // ladders interleave -- a tree at 180 m outlives a neighbourhood house at LOD1 at 160 m. That
    // interleaving is the reason these are nine independent numbers rather than a sorted list,
    // and it is asserted so that a later "tidy up" that sorts them is noticed.
    EXPECT_LT(CullDistanceFor(PropCategory::SmallProp), CullDistanceFor(PropCategory::GardenFurniture));
    EXPECT_LT(CullDistanceFor(PropCategory::GardenFurniture), CullDistanceFor(PropCategory::Fence));
    EXPECT_LT(CullDistanceFor(PropCategory::Fence), CullDistanceFor(PropCategory::Tree));
    EXPECT_LT(CullDistanceFor(PropCategory::NeighbourhoodLod0),
              CullDistanceFor(PropCategory::NeighbourhoodLod1));
    EXPECT_LT(CullDistanceFor(PropCategory::NeighbourhoodLod1),
              CullDistanceFor(PropCategory::NeighbourhoodLod2));
    EXPECT_LT(CullDistanceFor(PropCategory::NeighbourhoodLod2), CullDistanceFor(PropCategory::Impostor));
    EXPECT_GT(CullDistanceFor(PropCategory::Tree), CullDistanceFor(PropCategory::NeighbourhoodLod1));
    EXPECT_LT(CullDistanceFor(PropCategory::Tree), CullDistanceFor(PropCategory::NeighbourhoodLod2));
}

TEST(CullDistanceTests, TheStatedDistanceIsTheLastOneAtWhichAThingIsDrawn)
{
    // The same boundary rule as §26.4's detail sets: at exactly 45 m a small prop is still there.
    EXPECT_TRUE(WithinCullDistance(PropCategory::SmallProp, 45.0F));
    EXPECT_FALSE(WithinCullDistance(PropCategory::SmallProp, 45.01F));
    EXPECT_TRUE(WithinCullDistance(PropCategory::SmallProp, 0.0F));
    EXPECT_TRUE(WithinCullDistance(PropCategory::Impostor, 420.0F));
    EXPECT_FALSE(WithinCullDistance(PropCategory::Impostor, 421.0F));
}

TEST(CullDistanceTests, SectionSixtyEightsViewDistanceMultipliesThemAll)
{
    // §68's setting *"multiplies the far plane and the residency radius"*, and these are the same
    // decision seen per category. §71.3 gives Low 0.7x and Ultra 1.3x.
    EXPECT_FLOAT_EQ(CullDistanceFor(PropCategory::Tree, 0.7F), 180.0F * 0.7F);
    EXPECT_FLOAT_EQ(CullDistanceFor(PropCategory::Tree, 1.3F), 180.0F * 1.3F);
    EXPECT_TRUE(WithinCullDistance(PropCategory::Tree, 200.0F, 1.3F));
    EXPECT_FALSE(WithinCullDistance(PropCategory::Tree, 200.0F, 0.7F));

    // ...and it is clamped to §68's own band: below 0.6 the garden empties as the player walks
    // down it, and above 1.4 the exterior costs more than the house for something nobody looks at.
    EXPECT_FLOAT_EQ(CullDistanceFor(PropCategory::Tree, 0.0F), 180.0F * 0.6F);
    EXPECT_FLOAT_EQ(CullDistanceFor(PropCategory::Tree, -5.0F), 180.0F * 0.6F);
    EXPECT_FLOAT_EQ(CullDistanceFor(PropCategory::Tree, 100.0F), 180.0F * 1.4F);
}

TEST(CullDistanceTests, EveryQualityTierKeepsTheImpostorsInsideTheFarPlane)
{
    // §10.3's far plane is 420 m and §25.6's impostors are drawn to 420. At §68's widest setting
    // that becomes 588 -- beyond the far plane, where nothing is drawn whatever this says. Worth
    // knowing rather than worth fixing: the two numbers meet at 1.0x, which is where §71.3's
    // High tier sits, and the excess is clipped by the projection rather than by this table.
    for (const auto preset : {cnahouse::app::QualityPreset::Low,
                              cnahouse::app::QualityPreset::Medium,
                              cnahouse::app::QualityPreset::High,
                              cnahouse::app::QualityPreset::Ultra})
    {
        const float scale = cnahouse::rendering::SettingsFor(preset).viewDistance;
        std::printf("  %-6s view distance %.2fx: trees to %.0f m, impostors to %.0f m\n",
                    std::string(cnahouse::app::QualityPresetName(preset)).c_str(),
                    static_cast<double>(scale),
                    static_cast<double>(CullDistanceFor(PropCategory::Tree, scale)),
                    static_cast<double>(CullDistanceFor(PropCategory::Impostor, scale)));
        EXPECT_GT(CullDistanceFor(PropCategory::Impostor, scale), CullDistanceFor(PropCategory::Tree, scale));
    }
}

TEST(CullDistanceTests, EveryCategoryHasAName)
{
    for (std::size_t i = 0; i < static_cast<std::size_t>(PropCategory::Count); ++i)
    {
        EXPECT_NE(CategoryName(static_cast<PropCategory>(i)), "?") << i;
    }
    EXPECT_EQ(CategoryName(PropCategory::Count), "?");
    EXPECT_FLOAT_EQ(CullDistanceFor(PropCategory::Count), 0.0F) << "a thing with no category is not drawn";
}
