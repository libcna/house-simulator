// SPDX-License-Identifier: MIT
//
// `HOUSE-01604`. Analytic pixel-centre references for §33.3's continuous lunar mask.
#include <algorithm>
#include <cmath>
#include <limits>
#include <numbers>

#include <gtest/gtest.h>

#include "cnahouse/rendering/MoonMask.hpp"

namespace
{
    namespace Xna = Microsoft::Xna::Framework;
    using cnahouse::rendering::GenerateMoonMask;
    using cnahouse::rendering::kMoonMaskPhaseStep;
    using cnahouse::rendering::kMoonMaskSize;
    using cnahouse::rendering::MoonMask;
    using cnahouse::rendering::MoonMaskPixels;

    [[nodiscard]] bool SameColour(const Xna::Color& actual, int red, int green, int blue, int alpha)
    {
        return actual.getRProperty() == red && actual.getGProperty() == green &&
               actual.getBProperty() == blue && actual.getAProperty() == alpha;
    }

    [[nodiscard]] bool AnalyticallyLit(int x, int y, double phase)
    {
        const double u = (static_cast<double>(x) + 0.5) * (2.0 / kMoonMaskSize) - 1.0;
        const double v = 1.0 - (static_cast<double>(y) + 0.5) * (2.0 / kMoonMaskSize);
        if (u * u + v * v > 1.0)
        {
            return false;
        }
        const double k = std::cos(2.0 * std::numbers::pi * phase);
        const double halfChord = std::sqrt(1.0 - v * v);
        return phase < 0.5 ? u >= k * halfChord : u <= -k * halfChord;
    }
} // namespace

TEST(MoonMaskTests, SixteenPhasesMatchTheAnalyticEllipticalTerminator)
{
    for (int phaseIndex = 0; phaseIndex < 16; ++phaseIndex)
    {
        const double phase = static_cast<double>(phaseIndex) / 16.0;
        const MoonMaskPixels pixels = GenerateMoonMask(phase, 0.0);
        int discTexels = 0;
        int litTexels = 0;
        for (int y = 0; y < kMoonMaskSize; ++y)
        {
            for (int x = 0; x < kMoonMaskSize; ++x)
            {
                const double u = (static_cast<double>(x) + 0.5) * (2.0 / kMoonMaskSize) - 1.0;
                const double v = 1.0 - (static_cast<double>(y) + 0.5) * (2.0 / kMoonMaskSize);
                const bool onDisc = u * u + v * v <= 1.0;
                const bool lit = AnalyticallyLit(x, y, phase);
                const Xna::Color& pixel = pixels[static_cast<std::size_t>(y * kMoonMaskSize + x)];
                if (!onDisc)
                {
                    EXPECT_TRUE(SameColour(pixel, 0, 0, 0, 0))
                        << "phase " << phase << " at " << x << ',' << y;
                }
                else if (lit)
                {
                    EXPECT_TRUE(SameColour(pixel, 255, 255, 255, 255))
                        << "phase " << phase << " at " << x << ',' << y;
                }
                else
                {
                    EXPECT_TRUE(SameColour(pixel, 8, 9, 10, 255))
                        << "phase " << phase << " at " << x << ',' << y;
                }
                discTexels += onDisc ? 1 : 0;
                litTexels += onDisc && lit ? 1 : 0;
            }
        }

        // An independent integral of the ellipse gives the illuminated area `(1-k)/2`.
        // Pixel-centre sampling converges to it; one per cent is much wider than the 128² error.
        const double analyticFraction = 0.5 * (1.0 - std::cos(2.0 * std::numbers::pi * phase));
        EXPECT_NEAR(static_cast<double>(litTexels) / static_cast<double>(discTexels), analyticFraction, 0.01)
            << "phase " << phase;
    }
}

TEST(MoonMaskTests, PositionAngleRotatesTheMaskWithoutChangingItsCoverage)
{
    const MoonMaskPixels unrotated = GenerateMoonMask(0.25, 0.0);
    const MoonMaskPixels quarterTurn = GenerateMoonMask(0.25, std::numbers::pi / 2.0);
    int unrotatedLit = 0;
    int rotatedLit = 0;
    for (std::size_t index = 0; index < unrotated.size(); ++index)
    {
        unrotatedLit += unrotated[index].getRProperty() == 255 ? 1 : 0;
        rotatedLit += quarterTurn[index].getRProperty() == 255 ? 1 : 0;
    }
    EXPECT_EQ(unrotatedLit, rotatedLit);

    const auto at = [](const MoonMaskPixels& pixels, int x, int y) -> const Xna::Color&
    { return pixels[static_cast<std::size_t>(y * kMoonMaskSize + x)]; };
    EXPECT_TRUE(SameColour(at(unrotated, 96, 64), 255, 255, 255, 255));
    EXPECT_TRUE(SameColour(at(unrotated, 32, 32), 8, 9, 10, 255));
    EXPECT_TRUE(SameColour(at(quarterTurn, 96, 64), 8, 9, 10, 255));
    EXPECT_TRUE(SameColour(at(quarterTurn, 32, 32), 255, 255, 255, 255));
}

TEST(MoonMaskTests, CacheRegeneratesOnlyAfterCircularPhaseMovementExceedsOneTexel)
{
    MoonMask mask;
    EXPECT_TRUE(mask.Update(0.998, 0.0));
    EXPECT_EQ(mask.GenerationCount(), 1U);

    // Distance is measured from the last generation, not accumulated frame-to-frame.
    EXPECT_FALSE(mask.Update(0.001, 1.0));
    EXPECT_FALSE(mask.Update(0.998 + kMoonMaskPhaseStep, 1.0));
    EXPECT_EQ(mask.GenerationCount(), 1U);
    EXPECT_TRUE(mask.Update(0.998 + kMoonMaskPhaseStep + 1e-9, 1.0));
    EXPECT_EQ(mask.GenerationCount(), 2U);
    EXPECT_TRUE(mask.HasPixels());
    EXPECT_EQ(mask.Pixels().size(), 128U * 128U);
}

TEST(MoonMaskTests, CircularInputsWrapAndInvalidInputsFailClosed)
{
    const MoonMaskPixels wrapped = GenerateMoonMask(1.25, 0.0);
    const MoonMaskPixels canonical = GenerateMoonMask(0.25, 0.0);
    const MoonMaskPixels invalid =
        GenerateMoonMask(std::numeric_limits<double>::infinity(), std::numeric_limits<double>::quiet_NaN());
    const MoonMaskPixels newMoon = GenerateMoonMask(0.0, 0.0);
    for (std::size_t index = 0; index < wrapped.size(); ++index)
    {
        EXPECT_EQ(wrapped[index].getPackedValueProperty(), canonical[index].getPackedValueProperty());
        EXPECT_EQ(invalid[index].getPackedValueProperty(), newMoon[index].getPackedValueProperty());
    }
}
