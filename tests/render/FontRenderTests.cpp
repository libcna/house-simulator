// SPDX-License-Identifier: MIT
//
// `HOUSE-00200`'s last claim, and the only one the other tests cannot make: the vendored faces
// actually put ink on a surface.
//
// `FontMetricsTests` proves the five fonts load and that `MeasureString` answers sensibly about
// them. Every one of those assertions reads METADATA. A `SpriteFont` whose glyph atlas was blank --
// a texture that failed to upload, a premultiply that zeroed every channel, a pipeline that wrote
// correct kerning beside an empty bitmap -- would pass all of them. This draws the text and reads
// the pixels back.
//
// A RENDER test, so it needs a real device and runs in the nightly job under `LIBGL_ALWAYS_SOFTWARE`
// (`HOUSE-00138`). It compares against no reference image on purpose: the assertions are properties
// -- ink where the text is, background where it is not, more ink at a larger size -- which survive a
// Mesa upgrade that would invalidate a committed PNG for no real reason.
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "Microsoft/Xna/Framework/Color.hpp"
#include "Microsoft/Xna/Framework/Game.hpp"
#include "Microsoft/Xna/Framework/Graphics/GraphicsDevice.hpp"
#include "Microsoft/Xna/Framework/Graphics/RenderTarget2D.hpp"
#include "Microsoft/Xna/Framework/Graphics/SpriteBatch.hpp"
#include "Microsoft/Xna/Framework/Graphics/SpriteFont.hpp"
#include "Microsoft/Xna/Framework/GraphicsDeviceManager.hpp"
#include "Microsoft/Xna/Framework/Vector2.hpp"

namespace
{
    namespace Gfx = Microsoft::Xna::Framework::Graphics;
    using Microsoft::Xna::Framework::Color;
    using Microsoft::Xna::Framework::Vector2;

    constexpr int kWidth = 256;
    constexpr int kHeight = 64;

    /// @brief What was drawn, read back from the render target.
    struct Capture
    {
        std::vector<Color> pixels;

        /// @brief Pixels that are not the (black, opaque) background.
        [[nodiscard]] std::size_t InkPixels() const
        {
            std::size_t ink = 0;
            for (const Color& pixel : pixels)
            {
                // Any channel lit at all. Antialiased glyph edges are faint, and a threshold high
                // enough to ignore them would also ignore a font rendered at 10 % opacity.
                if (pixel.getRProperty() > 8 || pixel.getGProperty() > 8 || pixel.getBProperty() > 8)
                {
                    ++ink;
                }
            }
            return ink;
        }

        /// @brief Ink pixels within a horizontal band, used to check WHERE the text landed.
        [[nodiscard]] std::size_t InkInColumns(int fromX, int toX) const
        {
            std::size_t ink = 0;
            for (int y = 0; y < kHeight; ++y)
            {
                for (int x = fromX; x < toX && x < kWidth; ++x)
                {
                    const Color& pixel =
                        pixels[static_cast<std::size_t>(y) * static_cast<std::size_t>(kWidth) +
                               static_cast<std::size_t>(x)];
                    if (pixel.getRProperty() > 8 || pixel.getGProperty() > 8 || pixel.getBProperty() > 8)
                    {
                        ++ink;
                    }
                }
            }
            return ink;
        }
    };

    /// A `Game` that draws once into an offscreen target and hands back the pixels.
    class DrawHarness : public Microsoft::Xna::Framework::Game
    {
    public:
        using Body = std::function<void(Gfx::GraphicsDevice&,
                                        Gfx::SpriteBatch&,
                                        Microsoft::Xna::Framework::Content::ContentManager&,
                                        Capture&)>;

        explicit DrawHarness(Body body)
            : gdm_(this)
            , body_(std::move(body))
        {
            gdm_.setPreferredBackBufferWidthProperty(kWidth);
            gdm_.setPreferredBackBufferHeightProperty(kHeight);
            gdm_.setSynchronizeWithVerticalRetraceProperty(false);
            setIsFixedTimeStepProperty(false);
            getContentProperty().setRootDirectoryProperty(CNAHOUSE_TEST_CONTENT_ROOT);
        }

        [[nodiscard]] bool Ran() const noexcept
        {
            return ran_;
        }

        [[nodiscard]] const std::string& Failure() const noexcept
        {
            return failure_;
        }

        [[nodiscard]] const Capture& Result() const noexcept
        {
            return capture_;
        }

    protected:
        void Draw(const Microsoft::Xna::Framework::GameTime& gameTime) override
        {
            Game::Draw(gameTime);
            if (!ran_)
            {
                ran_ = true;
                try
                {
                    Gfx::GraphicsDevice& device = getGraphicsDeviceProperty();
                    Gfx::SpriteBatch batch(device);
                    body_(device, batch, getContentProperty(), capture_);
                }
                catch (const std::exception& e)
                {
                    failure_ = e.what();
                }
            }
            Exit();
        }

    private:
        Microsoft::Xna::Framework::GraphicsDeviceManager gdm_;
        Body body_;
        bool ran_ = false;
        std::string failure_;
        Capture capture_;
    };

    /// @brief Draws @p text with the font named @p fontName at @p position and reads the frame back.
    [[nodiscard]] Capture DrawText(const char* fontName, const std::string& text, Vector2 position)
    {
        Capture result;
        DrawHarness harness(
            [&](Gfx::GraphicsDevice& device,
                Gfx::SpriteBatch& batch,
                Microsoft::Xna::Framework::Content::ContentManager& content,
                Capture& capture)
            {
                const Gfx::SpriteFont font = content.Load<Gfx::SpriteFont>(fontName);

                Gfx::RenderTarget2D target(
                    device, kWidth, kHeight, false, Gfx::SurfaceFormat::Color, Gfx::DepthFormat::None);
                device.SetRenderTarget(&target);
                device.Clear(Color::Black);

                batch.Begin();
                // White on black: the largest possible separation between ink and background, so
                // the ink threshold never has to be tuned.
                batch.DrawString(font, text, position, Color::White);
                batch.End();

                device.SetRenderTarget(nullptr);
                capture.pixels.resize(static_cast<std::size_t>(kWidth) * kHeight);
                target.GetData(capture.pixels.data(), static_cast<int>(capture.pixels.size()));
            });
        harness.Run();
        EXPECT_TRUE(harness.Ran()) << "the harness never reached a frame";
        EXPECT_TRUE(harness.Failure().empty()) << harness.Failure();
        result = harness.Result();
        return result;
    }

    TEST(FontRenderTests, DrawingTextPutsInkOnTheTarget)
    {
        const Capture drawn = DrawText("Fonts/ui-16", "Hamburgefonstiv", Vector2(8.0f, 8.0f));
        ASSERT_EQ(drawn.pixels.size(), static_cast<std::size_t>(kWidth) * kHeight);

        // The number is deliberately loose at the bottom and absent at the top: what is being
        // caught is a BLANK atlas, and fifteen glyphs of a 16-point face cannot produce fewer than
        // a hundred lit pixels under any rasteriser.
        EXPECT_GT(drawn.InkPixels(), 100u)
            << "the glyph atlas produced no visible pixels, so the SpriteFont is blank";
    }

    TEST(FontRenderTests, NothingIsDrawnWhereThereIsNoText)
    {
        // The other half of the previous test. Ink somewhere is not evidence of correct rendering
        // if the whole target is lit -- that would be a cleared-to-white surface passing as text.
        const Capture drawn = DrawText("Fonts/ui-16", "Hi", Vector2(4.0f, 4.0f));

        EXPECT_GT(drawn.InkInColumns(0, 64), 0u) << "the string should be near the left edge";
        EXPECT_EQ(drawn.InkInColumns(160, kWidth), 0u)
            << "the right of the target should be untouched background";
    }

    TEST(FontRenderTests, AnEmptyStringDrawsNothingAtAll)
    {
        const Capture drawn = DrawText("Fonts/ui-16", "", Vector2(8.0f, 8.0f));
        EXPECT_EQ(drawn.InkPixels(), 0u) << "an empty string must leave the target as it was";
    }

    TEST(FontRenderTests, ALargerSizeDrawsMoreInkThanASmallerOne)
    {
        // Proves the three UI sizes are three distinct rasterisations rather than one asset loaded
        // three times -- the same claim `FontMetricsTests` makes from the metrics, made here from
        // the pixels, because a correct `LineSpacing` beside a wrong atlas would satisfy only one.
        const std::string sample = "Wg";
        const Capture small = DrawText("Fonts/ui-16", sample, Vector2(4.0f, 4.0f));
        const Capture large = DrawText("Fonts/ui-30", sample, Vector2(4.0f, 4.0f));

        EXPECT_GT(small.InkPixels(), 0u);
        EXPECT_GT(large.InkPixels(), small.InkPixels())
            << "ui-30 must cover more pixels than ui-16 for the same string";
    }

    TEST(FontRenderTests, ALatin1CharacterOutsideAsciiRenders)
    {
        // The whole point of carrying U+00A0..U+00FF. If the pipeline had silently dropped the
        // supplement, this would draw the '?' default character -- which still produces ink, so the
        // assertion is a COMPARISON against the same glyph's ASCII neighbour rather than a count.
        const Capture accented = DrawText("Fonts/ui-30", "\xC3\x89", Vector2(8.0f, 8.0f)); // U+00C9
        const Capture plain = DrawText("Fonts/ui-30", "E", Vector2(8.0f, 8.0f));
        const Capture fallback = DrawText("Fonts/ui-30", "?", Vector2(8.0f, 8.0f));

        EXPECT_GT(accented.InkPixels(), 0u);
        // E-acute is an E plus an accent, so it must be strictly inkier than a bare E...
        EXPECT_GT(accented.InkPixels(), plain.InkPixels()) << "U+00C9 should carry more ink than plain 'E'";
        // ...and it must not simply be the default character.
        EXPECT_NE(accented.InkPixels(), fallback.InkPixels())
            << "U+00C9 rendered as the '?' default character, so the Latin-1 supplement is missing";
    }

} // namespace
