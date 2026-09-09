// SPDX-License-Identifier: MIT
//
// `HOUSE-00669`. §15.4's `translucent` portal and §26.4's detail sets: *"a room seen through a
// frosted door renders its shell and furniture but none of its 60 small dressing props."*
//
// Two tables, and the interesting part is where they meet. §26.4 lists three drop conditions for
// `dressing` and two for `micro` as if they were independent; they are not, and this is where that
// gets decided.
#include <gtest/gtest.h>

#include "cnahouse/visibility/DetailSets.hpp"

namespace
{
    using cnahouse::app::QualityPreset;
    using cnahouse::visibility::CellDetail;
    using cnahouse::visibility::ConeFlags;
    using cnahouse::visibility::DetailFor;
    using cnahouse::visibility::DetailSet;
    using cnahouse::visibility::kDressingDistance;
    using cnahouse::visibility::kMicroDistance;
} // namespace

TEST(DetailSetTests, TheDistancesAreTheOnesSectionTwentySixStates)
{
    EXPECT_FLOAT_EQ(kDressingDistance, 18.0F);
    EXPECT_FLOAT_EQ(kMicroDistance, 8.0F);
}

TEST(DetailSetTests, TheEssentialSetIsNeverDropped)
{
    // §26.4's first row is one word: *"never"*. A room with no shell, no doors and no lights is
    // not a cheaper room -- it is a hole in the house.
    for (const auto quality :
         {QualityPreset::Low, QualityPreset::Medium, QualityPreset::High, QualityPreset::Ultra})
    {
        for (const float distance : {0.0F, 10.0F, 100.0F, 400.0F})
        {
            for (const auto flags : {ConeFlags::None, ConeFlags::Diffuse})
            {
                EXPECT_TRUE(DetailFor(flags, quality, distance, 0).Draws(DetailSet::Essential))
                    << static_cast<int>(quality) << " at " << distance;
            }
        }
    }
}

TEST(DetailSetTests, AFrostedDoorCostsTheRoomItsDressing)
{
    // §15.4: *"passes, but the reduced frustum is marked diffuse so the target cell renders at
    // LOD+1 and no small props"* -- the whole point of the translucent opacity, and the reason the
    // flag is carried down the chain at all.
    const CellDetail clear = DetailFor(ConeFlags::None, QualityPreset::Ultra, 2.0F, 0);
    EXPECT_TRUE(clear.dressing);
    EXPECT_TRUE(clear.micro);
    EXPECT_EQ(clear.lodBias, 0);

    const CellDetail frosted = DetailFor(ConeFlags::Diffuse, QualityPreset::Ultra, 2.0F, 0);
    EXPECT_FALSE(frosted.dressing) << "the frosted door cost nothing";
    EXPECT_FALSE(frosted.micro);
    EXPECT_EQ(frosted.lodBias, 1) << "§15.4's LOD+1";

    // ...and the bias is added to §68's, not replacing it.
    EXPECT_EQ(DetailFor(ConeFlags::Diffuse, QualityPreset::Ultra, 2.0F, 2).lodBias, 3);
    EXPECT_EQ(DetailFor(ConeFlags::None, QualityPreset::Ultra, 2.0F, 2).lodBias, 2);
}

TEST(DetailSetTests, TheQualityRowsAreWhatTheTableSays)
{
    // `dressing` goes at quality `low`; `micro` goes at anything *below* `high`.
    EXPECT_FALSE(DetailFor(ConeFlags::None, QualityPreset::Low, 1.0F, 0).dressing);
    EXPECT_TRUE(DetailFor(ConeFlags::None, QualityPreset::Medium, 1.0F, 0).dressing);
    EXPECT_TRUE(DetailFor(ConeFlags::None, QualityPreset::High, 1.0F, 0).dressing);
    EXPECT_TRUE(DetailFor(ConeFlags::None, QualityPreset::Ultra, 1.0F, 0).dressing);

    EXPECT_FALSE(DetailFor(ConeFlags::None, QualityPreset::Low, 1.0F, 0).micro);
    EXPECT_FALSE(DetailFor(ConeFlags::None, QualityPreset::Medium, 1.0F, 0).micro);
    EXPECT_TRUE(DetailFor(ConeFlags::None, QualityPreset::High, 1.0F, 0).micro);
    EXPECT_TRUE(DetailFor(ConeFlags::None, QualityPreset::Ultra, 1.0F, 0).micro);
}

TEST(DetailSetTests, TheDistanceRowsAreTheOnesTheTableStatesAndTheBoundaryIsInside)
{
    // At exactly the stated distance the set is still drawn: §26.4 says *"beyond 18 m"*, and 18 is
    // not beyond 18. A boundary that goes the other way makes a prop pop as a player walks a
    // millimetre.
    EXPECT_TRUE(DetailFor(ConeFlags::None, QualityPreset::Ultra, kDressingDistance, 0).dressing);
    EXPECT_FALSE(DetailFor(ConeFlags::None, QualityPreset::Ultra, kDressingDistance + 0.01F, 0).dressing);
    EXPECT_TRUE(DetailFor(ConeFlags::None, QualityPreset::Ultra, kMicroDistance, 0).micro);
    EXPECT_FALSE(DetailFor(ConeFlags::None, QualityPreset::Ultra, kMicroDistance + 0.01F, 0).micro);

    // Between the two distances the room keeps its books and loses its crumbs, which is the
    // gradient the two numbers exist to make.
    const CellDetail between = DetailFor(ConeFlags::None, QualityPreset::Ultra, 12.0F, 0);
    EXPECT_TRUE(between.dressing);
    EXPECT_FALSE(between.micro);
}

TEST(DetailSetTests, TheMicroSetNeverOutlivesTheDressingItSitsOn)
{
    // §26.4 lists the two rows independently, and taken literally they allow a room 20 m away at
    // `ultra` to draw individual pens on a desk whose books have gone. That is a worse picture
    // than either rule intends, so `micro` is a subset of `dressing` -- stated here because it is
    // the one thing in this function the table does not say.
    for (const auto quality :
         {QualityPreset::Low, QualityPreset::Medium, QualityPreset::High, QualityPreset::Ultra})
    {
        for (const float distance : {0.0F, 4.0F, 8.0F, 12.0F, 18.0F, 25.0F, 100.0F})
        {
            for (const auto flags : {ConeFlags::None, ConeFlags::Diffuse})
            {
                const CellDetail detail = DetailFor(flags, quality, distance, 0);
                EXPECT_TRUE(detail.dressing || !detail.micro)
                    << "crumbs without books at " << distance << " m";
            }
        }
    }
}
