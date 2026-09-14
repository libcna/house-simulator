// SPDX-License-Identifier: MIT
#include <cmath>

#include <gtest/gtest.h>

#include "cnahouse/lighting/ExposureModel.hpp"

namespace
{
    using cnahouse::lighting::ExposureAdapter;
    using cnahouse::lighting::ExposureTargetFor;
    using cnahouse::lighting::kAmbientFloor;
    using cnahouse::lighting::kExteriorExposure;
    using cnahouse::lighting::kMaximumInteriorExposure;
    using cnahouse::lighting::RoomLightState;

    TEST(ExposureModelTests, ExteriorPreservesHeadroomAndInteriorTargetTracksActualRoomLight)
    {
        RoomLightState room;
        EXPECT_FLOAT_EQ(ExposureTargetFor(room, true), kExteriorExposure);
        EXPECT_FLOAT_EQ(room.Level(), kAmbientFloor);
        EXPECT_GT(ExposureTargetFor(room, false), 5.8F);
        EXPECT_LE(ExposureTargetFor(room, false), kMaximumInteriorExposure);

        room.artificial = 0.5F;
        const float halfLit = ExposureTargetFor(room, false);
        EXPECT_GT(halfLit, 1.0F);
        EXPECT_LT(halfLit, kMaximumInteriorExposure);

        room.artificial = 1.0F;
        EXPECT_FLOAT_EQ(ExposureTargetFor(room, false), 1.0F);
    }

    TEST(ExposureModelTests, FirstCellSnapsAndDarkAdaptationIsSlowerThanBrightAdaptation)
    {
        ExposureAdapter darkeningView;
        darkeningView.Advance(kExteriorExposure, 1.0F / 60.0F);
        EXPECT_FLOAT_EQ(darkeningView.Scale(), kExteriorExposure) << "spawn has no prior eye state";
        darkeningView.Advance(kMaximumInteriorExposure, 0.9F);

        ExposureAdapter brighteningView;
        brighteningView.Advance(kMaximumInteriorExposure, 1.0F / 60.0F);
        brighteningView.Advance(kExteriorExposure, 0.9F);

        const float darkFraction =
            (darkeningView.Scale() - kExteriorExposure) / (kMaximumInteriorExposure - kExteriorExposure);
        const float brightFraction = (kMaximumInteriorExposure - brighteningView.Scale()) /
                                     (kMaximumInteriorExposure - kExteriorExposure);
        EXPECT_NEAR(brightFraction, 1.0F - std::exp(-1.0F), 1.0e-5F);
        EXPECT_LT(darkFraction, brightFraction);
    }

    TEST(ExposureModelTests, TierSUsesEffectsForLiftAndATintQuadForConstriction)
    {
        ExposureAdapter exposure;
        exposure.Snap(kMaximumInteriorExposure);
        EXPECT_FLOAT_EQ(exposure.EffectScale(), kMaximumInteriorExposure);
        EXPECT_FLOAT_EQ(exposure.TintAlpha(), 0.0F);

        exposure.Snap(kExteriorExposure);
        EXPECT_FLOAT_EQ(exposure.EffectScale(), 1.0F);
        EXPECT_NEAR(exposure.TintAlpha(), 1.0F - kExteriorExposure, 1.0e-6F);
    }

    TEST(ExposureModelTests, InvalidInputCannotPoisonTheFrame)
    {
        ExposureAdapter exposure;
        exposure.Advance(std::nanf(""), 1.0F);
        EXPECT_FLOAT_EQ(exposure.Scale(), 1.0F);
        exposure.Advance(kMaximumInteriorExposure, std::nanf(""));
        EXPECT_FLOAT_EQ(exposure.Scale(), 1.0F);
        exposure.Snap(1000.0F);
        EXPECT_FLOAT_EQ(exposure.Scale(), kMaximumInteriorExposure);
    }
} // namespace
