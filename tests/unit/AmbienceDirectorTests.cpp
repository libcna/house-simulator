// SPDX-License-Identifier: MIT
#include <gtest/gtest.h>

#include <limits>

#include "cnahouse/audio/AmbienceDirector.hpp"

namespace
{
    using cnahouse::audio::AmbienceDirector;
    using cnahouse::world::CellKind;

    TEST(AmbienceDirectorTests, TheFirstListenerCellSelectsTheCorrectBedImmediately)
    {
        AmbienceDirector director;
        const auto indoors = director.Advance(CellKind::Room, 20.0, 0.0F);
        EXPECT_FLOAT_EQ(indoors.interior, 1.0F);
        EXPECT_FLOAT_EQ(indoors.exteriorDay, 0.0F);

        director.Reset();
        const auto outdoors = director.Advance(CellKind::Exterior, 20.0, 0.0F);
        EXPECT_FLOAT_EQ(outdoors.interior, 0.0F);
        EXPECT_FLOAT_EQ(outdoors.exteriorDay, 1.0F);
        EXPECT_FLOAT_EQ(outdoors.exteriorNight, 0.0F);
    }

    TEST(AmbienceDirectorTests, CrossingTheBoundaryUsesTheShortFadeInBothDirections)
    {
        AmbienceDirector director;
        (void)director.Advance(CellKind::Room, 20.0, 0.0F);

        const auto halfwayOut =
            director.Advance(CellKind::Exterior, 20.0, AmbienceDirector::kCellCrossfadeSeconds * 0.5F);
        EXPECT_FLOAT_EQ(halfwayOut.interior, 0.5F);
        EXPECT_FLOAT_EQ(halfwayOut.exteriorDay, 0.5F);
        EXPECT_FLOAT_EQ(halfwayOut.interior + halfwayOut.exteriorDay + halfwayOut.exteriorNight, 1.0F);

        const auto outside =
            director.Advance(CellKind::Exterior, 20.0, AmbienceDirector::kCellCrossfadeSeconds * 0.5F);
        EXPECT_FLOAT_EQ(outside.exteriorDay, 1.0F);

        const auto halfwayIn =
            director.Advance(CellKind::Garage, 20.0, AmbienceDirector::kCellCrossfadeSeconds * 0.5F);
        EXPECT_FLOAT_EQ(halfwayIn.interior, 0.5F) << "a garage is inside, not a special zone loop";
        EXPECT_FLOAT_EQ(halfwayIn.exteriorDay, 0.5F);
    }

    TEST(AmbienceDirectorTests, SunAltitudeCrossFadesOnlyTheTwoExteriorBeds)
    {
        AmbienceDirector director;
        const auto night = director.Advance(CellKind::Exterior, -12.0, 0.0F);
        EXPECT_FLOAT_EQ(night.exteriorNight, 1.0F);
        EXPECT_FLOAT_EQ(night.exteriorDay, 0.0F);

        const auto twilight = director.Advance(CellKind::Exterior, -1.5, 0.0F);
        EXPECT_FLOAT_EQ(twilight.exteriorDay, 0.5F);
        EXPECT_FLOAT_EQ(twilight.exteriorNight, 0.5F);

        const auto day = director.Advance(CellKind::Exterior, 20.0, 0.0F);
        EXPECT_FLOAT_EQ(day.exteriorDay, 1.0F);
        EXPECT_FLOAT_EQ(day.exteriorNight, 0.0F);
    }

    TEST(AmbienceDirectorTests, InvalidTimeDoesNotJumpTheCellFade)
    {
        AmbienceDirector director;
        (void)director.Advance(CellKind::Room, 20.0, 0.0F);
        const auto unchanged = director.Advance(CellKind::Exterior, 20.0, -1.0F);
        EXPECT_FLOAT_EQ(unchanged.interior, 1.0F);
        EXPECT_FLOAT_EQ(AmbienceDirector::DayMix(std::numeric_limits<double>::quiet_NaN()), 0.0F);
    }
} // namespace
