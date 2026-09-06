// SPDX-License-Identifier: MIT
//
// `HOUSE-00145`. What can be tested without a `GraphicsDevice` is tested here; anchoring needs
// `SpriteFont::MeasureString` and therefore a device, so it belongs to the render regression
// harness of `HOUSE-00164` rather than being faked with a stub that would prove nothing.
#include <gtest/gtest.h>

#include <cmath>

#include "cnahouse/ui/TextRenderer.hpp"

namespace
{
    using cnahouse::ui::TextRenderer;

    TEST(TextRendererTests, TheReferenceResolutionScalesToOne)
    {
        TextRenderer text;
        text.SetViewport(1600, 900);
        EXPECT_FLOAT_EQ(text.Scale(), 1.0f) << "the HUD is authored against 1600x900";
    }

    TEST(TextRendererTests, ScaleIsUniformAndTakesTheTighterAxis)
    {
        TextRenderer text;

        // Twice as wide as the reference but the same height: scaling by the WIDTH ratio would push
        // text off the bottom. The smaller ratio is the one that fits on both axes.
        text.SetViewport(3200, 900);
        EXPECT_FLOAT_EQ(text.Scale(), 1.0f);

        text.SetViewport(1600, 1800);
        EXPECT_FLOAT_EQ(text.Scale(), 1.0f);

        text.SetViewport(800, 450);
        EXPECT_FLOAT_EQ(text.Scale(), 0.5f) << "half the reference on both axes is half the scale";

        text.SetViewport(3200, 1800);
        EXPECT_FLOAT_EQ(text.Scale(), 2.0f);
    }

    TEST(TextRendererTests, ADegenerateViewportDoesNotDivideByZero)
    {
        // A window can genuinely report zero during a resize, and a scale of infinity would put every
        // glyph at a coordinate the rasteriser rejects -- which presents as "the HUD vanished".
        TextRenderer text;
        text.SetViewport(0, 0);
        EXPECT_GT(text.Scale(), 0.0f);
        EXPECT_TRUE(std::isfinite(text.Scale()));
    }

    TEST(TextRendererTests, WithoutAFontEverythingIsANoOpRatherThanACrash)
    {
        // A build whose content tree has not been generated must still run. This is the same policy
        // `CnaHouseGame::LoadContent` applies, and it has to hold here too or the first frame after a
        // failed font load is a null dereference.
        TextRenderer text;
        EXPECT_FALSE(text.HasFont());
        const auto size = text.Measure("anything");
        EXPECT_FLOAT_EQ(size.X, 0.0f);
        EXPECT_FLOAT_EQ(size.Y, 0.0f);
    }

    TEST(TextRendererTests, ClearingTheFontIsAllowedAndTakesEffect)
    {
        // `UnloadContent` clears the font BEFORE destroying it. A renderer holding a dangling font is
        // a use-after-free at shutdown, which is the hardest kind to reproduce.
        TextRenderer text;
        text.SetFont(nullptr);
        EXPECT_FALSE(text.HasFont());
    }

} // namespace
