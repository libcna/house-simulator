// SPDX-License-Identifier: MIT
//
// `HOUSE-01255`. The per-fixture colour-temperature lookup and its integration into §28.1's
// per-room state.
#include <algorithm>
#include <cmath>
#include <cstdio>

#include <gtest/gtest.h>

#include "cnahouse/lighting/PlanckianLut.hpp"

namespace
{
    using cnahouse::lighting::kPlanckianHighestKelvin;
    using cnahouse::lighting::kPlanckianLowestKelvin;
    using cnahouse::lighting::kPlanckianLutSize;
    using cnahouse::lighting::kPlanckianLutStepKelvin;
    using cnahouse::lighting::PlanckianRgb;
    using cnahouse::lighting::PlanckianRgbTable;

} // namespace

TEST(PlanckianLutTests, TheAuthoredRangeHasOneEntryPerHundredKelvinIncludingItsEnds)
{
    const auto& table = PlanckianRgbTable();
    EXPECT_EQ(table.size(), 111U);
    EXPECT_EQ(kPlanckianLutSize, 111U);
    EXPECT_FLOAT_EQ(kPlanckianLowestKelvin, 1000.0F);
    EXPECT_FLOAT_EQ(kPlanckianHighestKelvin, 12000.0F);
    EXPECT_FLOAT_EQ(kPlanckianLutStepKelvin, 100.0F);
    EXPECT_FLOAT_EQ(kPlanckianLowestKelvin + static_cast<float>(table.size() - 1) * kPlanckianLutStepKelvin,
                    kPlanckianHighestKelvin);
}

TEST(PlanckianLutTests, CommonHouseLampsMatchIndependentDisplayRgbCheckpoints)
{
    // These checkpoints are the display-RGB values of the published approximation, rounded only
    // here. All five temperatures occur in layout.lights.json, so this is the range the game will
    // actually show rather than arbitrary samples chosen to suit the implementation.
    struct Checkpoint
    {
        float kelvin;
        float red;
        float green;
        float blue;
    };

    constexpr Checkpoint checkpoints[] = {
        {2400.0F, 1.0000F, 0.6079F, 0.2373F},
        {2700.0F, 1.0000F, 0.6538F, 0.3428F},
        {3000.0F, 1.0000F, 0.6949F, 0.4310F},
        {3500.0F, 1.0000F, 0.7550F, 0.5523F},
        {4000.0F, 1.0000F, 0.8071F, 0.6513F},
    };
    for (const Checkpoint& expected : checkpoints)
    {
        const auto actual = PlanckianRgb(expected.kelvin);
        EXPECT_NEAR(actual.X, expected.red, 0.0001F) << expected.kelvin;
        EXPECT_NEAR(actual.Y, expected.green, 0.0001F) << expected.kelvin;
        EXPECT_NEAR(actual.Z, expected.blue, 0.0001F) << expected.kelvin;
    }
}

TEST(PlanckianLutTests, WarmLampsAndCoolLampsDifferInTheDirectionTheirNamesSay)
{
    const auto bedroom = PlanckianRgb(2700.0F);
    const auto garage = PlanckianRgb(4000.0F);
    const auto cool = PlanckianRgb(12000.0F);

    EXPECT_GT(bedroom.X, bedroom.Y);
    EXPECT_GT(bedroom.Y, bedroom.Z) << "2700 K must be warm, not blue";
    EXPECT_GT(garage.Z, bedroom.Z) << "4000 K must be less warm than 2700 K";
    EXPECT_GT(cool.Z, cool.X) << "12000 K must be blue-white, not orange";
    std::printf("  2700 K RGB(%.3f, %.3f, %.3f); 4000 K RGB(%.3f, %.3f, %.3f)\n",
                static_cast<double>(bedroom.X),
                static_cast<double>(bedroom.Y),
                static_cast<double>(bedroom.Z),
                static_cast<double>(garage.X),
                static_cast<double>(garage.Y),
                static_cast<double>(garage.Z));
}

TEST(PlanckianLutTests, EveryEntryIsFiniteNormalisedAndTheLookupHasNoBucketStep)
{
    const auto& table = PlanckianRgbTable();
    for (const auto& color : table)
    {
        EXPECT_TRUE(std::isfinite(color.X));
        EXPECT_TRUE(std::isfinite(color.Y));
        EXPECT_TRUE(std::isfinite(color.Z));
        EXPECT_GE(color.X, 0.0F);
        EXPECT_GE(color.Y, 0.0F);
        EXPECT_GE(color.Z, 0.0F);
        EXPECT_LE(color.X, 1.0F);
        EXPECT_LE(color.Y, 1.0F);
        EXPECT_LE(color.Z, 1.0F);
    }

    // The curve changes quickly where blue first becomes visible around 1900 K, so the distance
    // between 100 K TABLE entries is not a discontinuity. The lookup is what the frame sees:
    // sampling it every kelvin bounds the real step and catches an accidental nearest-entry read.
    float worstOneKelvinStep = 0.0F;
    auto previous = PlanckianRgb(kPlanckianLowestKelvin);
    for (float kelvin = kPlanckianLowestKelvin + 1.0F; kelvin <= kPlanckianHighestKelvin; kelvin += 1.0F)
    {
        const auto current = PlanckianRgb(kelvin);
        const float step = std::abs(current.X - previous.X) + std::abs(current.Y - previous.Y) +
                           std::abs(current.Z - previous.Z);
        worstOneKelvinStep = std::max(worstOneKelvinStep, step);
        previous = current;
    }
    EXPECT_LT(worstOneKelvinStep, 0.001F);

    // There is no jump on either side of an exact bucket boundary either.
    for (float kelvin = kPlanckianLowestKelvin + kPlanckianLutStepKelvin; kelvin < kPlanckianHighestKelvin;
         kelvin += kPlanckianLutStepKelvin)
    {
        const auto below = PlanckianRgb(kelvin - 0.001F);
        const auto above = PlanckianRgb(kelvin + 0.001F);
        const float step =
            std::abs(above.X - below.X) + std::abs(above.Y - below.Y) + std::abs(above.Z - below.Z);
        EXPECT_LT(step, 0.00001F) << kelvin;
    }

    // Interpolation is real: halfway is the midpoint rather than one neighbouring bucket.
    const auto low = PlanckianRgb(2700.0F);
    const auto middle = PlanckianRgb(2750.0F);
    const auto high = PlanckianRgb(2800.0F);
    EXPECT_FLOAT_EQ(middle.X, (low.X + high.X) * 0.5F);
    EXPECT_FLOAT_EQ(middle.Y, (low.Y + high.Y) * 0.5F);
    EXPECT_FLOAT_EQ(middle.Z, (low.Z + high.Z) * 0.5F);
}

TEST(PlanckianLutTests, OutOfRangeAndNonFiniteInputsCannotEscapeIntoAnEffect)
{
    const auto low = PlanckianRgb(kPlanckianLowestKelvin);
    const auto high = PlanckianRgb(kPlanckianHighestKelvin);
    const auto neutral = PlanckianRgb(6500.0F);
    EXPECT_EQ(PlanckianRgb(-5000.0F), low);
    EXPECT_EQ(PlanckianRgb(90000.0F), high);
    EXPECT_EQ(PlanckianRgb(std::nanf("")), neutral);
    EXPECT_EQ(PlanckianRgb(INFINITY), neutral);
}
