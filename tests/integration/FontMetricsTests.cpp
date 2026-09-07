// SPDX-License-Identifier: MIT
//
// `HOUSE-00200`'s acceptance, verified through the XNA surface rather than through the files:
// the five compiled fonts load, they carry the Latin-1 repertoire the descriptors asked for, and
// `MeasureString` answers sensibly about them.
//
// An INTEGRATION test rather than a unit test for the reason `CachesTests` gives: `Load<SpriteFont>`
// needs a real `GraphicsDevice` because a `SpriteFont` owns a `Texture2D`. There is nothing to stub
// that would still be the thing under test.
//
// **`tools/ci/check_fonts.py` checks the SOURCES; this checks the OUTPUT.** They are not the same
// claim. The gate reads the `.spritefont` and the `.ttf` and proves the pipeline is being asked for
// the right thing; this proves the pipeline delivered it. A processor that silently dropped a
// character region would pass the first and fail this one.
#include <algorithm>
#include <cstdint>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "Microsoft/Xna/Framework/Game.hpp"
#include "Microsoft/Xna/Framework/Graphics/SpriteFont.hpp"
#include "Microsoft/Xna/Framework/GraphicsDeviceManager.hpp"

namespace
{
    namespace Gfx = Microsoft::Xna::Framework::Graphics;
    using Microsoft::Xna::Framework::Vector2;

    /// The five content names `HOUSE-00200` produces. Written out rather than generated so that a
    /// deleted or renamed asset fails this list instead of silently shrinking the loop.
    constexpr const char* kUiFonts[] = {"Fonts/ui-16", "Fonts/ui-22", "Fonts/ui-30"};
    constexpr const char* kMonoFonts[] = {"Fonts/mono-13", "Fonts/mono-16"};

    /// U+00AD SOFT HYPHEN, excluded from the repertoire on purpose. Noto Sans Mono 2.014 has no
    /// glyph for it and the processor treats a requested-but-absent character as fatal, so both
    /// faces leave it out and stay comparable. See `tools/ci/check_fonts.py`.
    constexpr char16_t kSoftHyphen = 0x00AD;

    /// @brief The 190 code points every shipped face must carry.
    [[nodiscard]] std::vector<char16_t> RequiredCharacters()
    {
        std::vector<char16_t> required;
        for (char16_t code = 0x20; code <= 0x7E; ++code)
        {
            required.push_back(code);
        }
        for (char16_t code = 0xA0; code <= 0xFF; ++code)
        {
            if (code != kSoftHyphen)
            {
                required.push_back(code);
            }
        }
        return required;
    }

    /// The smallest `Game` that gives a live `ContentManager`. `Run()` is the only lifetime CNA
    /// offers on desktop (`HOUSE-00062`), so the assertions happen inside a frame.
    class FontHarness : public Microsoft::Xna::Framework::Game
    {
    public:
        using Body = std::function<void(Microsoft::Xna::Framework::Content::ContentManager&)>;

        explicit FontHarness(Body body)
            : gdm_(this)
            , body_(std::move(body))
        {
            gdm_.setPreferredBackBufferWidthProperty(320);
            gdm_.setPreferredBackBufferHeightProperty(240);
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

    protected:
        void Draw(const Microsoft::Xna::Framework::GameTime& gameTime) override
        {
            Game::Draw(gameTime);
            if (!ran_)
            {
                ran_ = true;
                try
                {
                    body_(getContentProperty());
                }
                catch (const std::exception& e)
                {
                    // Recorded, not rethrown: an exception leaving `Draw` unwinds through XNA's
                    // frame loop, and the message is what the test needs to report.
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
    };

    void WithContent(const FontHarness::Body& body)
    {
        FontHarness harness(body);
        harness.Run();
        ASSERT_TRUE(harness.Ran()) << "the harness never reached a frame";
        ASSERT_TRUE(harness.Failure().empty()) << harness.Failure();
    }

    TEST(FontMetricsTests, AllFiveCompiledFontsLoad)
    {
        WithContent(
            [](Microsoft::Xna::Framework::Content::ContentManager& content)
            {
                for (const char* name : kUiFonts)
                {
                    const Gfx::SpriteFont font = content.Load<Gfx::SpriteFont>(name);
                    EXPECT_FALSE(font.getCharactersProperty().empty()) << name;
                }
                for (const char* name : kMonoFonts)
                {
                    const Gfx::SpriteFont font = content.Load<Gfx::SpriteFont>(name);
                    EXPECT_FALSE(font.getCharactersProperty().empty()) << name;
                }
            });
    }

    TEST(FontMetricsTests, EveryFaceCarriesTheFullLatin1Repertoire)
    {
        WithContent(
            [](Microsoft::Xna::Framework::Content::ContentManager& content)
            {
                const std::vector<char16_t> required = RequiredCharacters();
                ASSERT_EQ(required.size(), 190u) << "95 printable ASCII + 96 Latin-1 - SOFT HYPHEN";

                for (const char* name :
                     {"Fonts/ui-16", "Fonts/ui-22", "Fonts/ui-30", "Fonts/mono-13", "Fonts/mono-16"})
                {
                    const Gfx::SpriteFont font = content.Load<Gfx::SpriteFont>(name);
                    const std::vector<char16_t>& present = font.getCharactersProperty();

                    std::vector<char16_t> missing;
                    for (const char16_t wanted : required)
                    {
                        if (std::find(present.begin(), present.end(), wanted) == present.end())
                        {
                            missing.push_back(wanted);
                        }
                    }
                    EXPECT_TRUE(missing.empty())
                        << name << " is missing " << missing.size() << " required character(s), first U+"
                        << std::hex << (missing.empty() ? 0 : static_cast<int>(missing.front()));

                    // The exclusion is asserted, not merely commented. If a future font swap made
                    // SOFT HYPHEN available and someone widened the regions, this fails and the
                    // decision gets revisited deliberately instead of drifting.
                    EXPECT_EQ(std::find(present.begin(), present.end(), kSoftHyphen), present.end())
                        << name << " unexpectedly carries U+00AD; see tools/ci/check_fonts.py";
                }
            });
    }

    TEST(FontMetricsTests, MeasureStringIsSane)
    {
        WithContent(
            [](Microsoft::Xna::Framework::Content::ContentManager& content)
            {
                const Gfx::SpriteFont font = content.Load<Gfx::SpriteFont>("Fonts/ui-16");

                const Vector2 empty = font.MeasureString(std::string(""));
                EXPECT_FLOAT_EQ(empty.X, 0.0f) << "an empty string occupies no width";

                const Vector2 one = font.MeasureString(std::string("M"));
                EXPECT_GT(one.X, 0.0f);
                EXPECT_GT(one.Y, 0.0f);

                // Height is a LINE height, so it does not grow with the text.
                const Vector2 many = font.MeasureString(std::string("MMMMMMMMMM"));
                EXPECT_GT(many.X, one.X) << "ten glyphs must be wider than one";
                EXPECT_FLOAT_EQ(many.Y, one.Y) << "measured height is the line height";

                // A proportional face must actually be proportional, or it is the wrong file.
                const Vector2 narrow = font.MeasureString(std::string("iiiiiiiiii"));
                EXPECT_LT(narrow.X, many.X) << "Noto Sans is proportional: 'i' is narrower than 'M'";

                EXPECT_GT(font.getLineSpacingProperty(), 0);
            });
    }

    /// @brief @p code as UTF-8, which is what `std::string` -> `System::String` expects.
    [[nodiscard]] std::string Utf8(char16_t code)
    {
        std::string text;
        if (code < 0x80)
        {
            text += static_cast<char>(code);
        }
        else
        {
            text += static_cast<char>(0xC0 | (code >> 6));
            text += static_cast<char>(0x80 | (code & 0x3F));
        }
        return text;
    }

    TEST(FontMetricsTests, MonoAdvancesAreUniformToWithinTheGridFittingError)
    {
        WithContent(
            [](Microsoft::Xna::Framework::Content::ContentManager& content)
            {
                // MEASURED (`HOUSE-00200`), and NOT what the obvious assertion would have been.
                //
                // Noto Sans Mono advances every one of these 190 characters by exactly 600/1000 em,
                // so "every character is the same width" looks certain. It is false at 13, and the
                // reason is in the pipeline rather than in the font:
                //
                //   * `<Size>` is POINTS AT 96 DPI, not pixels -- `FT_Set_Char_Size(..., 96, 96)`.
                //     So <Size>13</Size> is a 17.33 px em and <Size>16</Size> is a 21.33 px em.
                //   * the advance is taken as `slot->advance.x >> 6`, i.e. truncated to whole
                //     pixels AFTER hinting has grid-fitted it.
                //
                // 0.6 x 17.33 = 10.4 px, which cannot be a whole number of pixels: 176 characters
                // land on 10 and 14 land on 11. At <Size>16</Size> the advance is 0.6 x 21.33 =
                // 12.8 px, which grid-fits to 13 for all 190.
                //
                // So the debug overlay columns are exact at mono-16 and out by at most one pixel at
                // mono-13. That is the truth, and it is asserted rather than smoothed over with a
                // loose tolerance -- a test that allowed 2 px would also pass a genuinely
                // proportional face, which is the failure this is here to catch.
                struct Expectation
                {
                    const char* name;
                    int allowedSpread;
                };
                for (const Expectation expectation :
                     {Expectation{"Fonts/mono-13", 1}, Expectation{"Fonts/mono-16", 0}})
                {
                    const Gfx::SpriteFont font = content.Load<Gfx::SpriteFont>(expectation.name);

                    float narrowest = 0.0f;
                    float widest = 0.0f;
                    bool first = true;
                    for (const char16_t code : RequiredCharacters())
                    {
                        const float width = font.MeasureString(Utf8(code)).X;
                        EXPECT_GT(width, 0.0f)
                            << expectation.name << " U+" << std::hex << static_cast<int>(code);
                        narrowest = first ? width : std::min(narrowest, width);
                        widest = first ? width : std::max(widest, width);
                        first = false;
                    }
                    EXPECT_LE(widest - narrowest, static_cast<float>(expectation.allowedSpread))
                        << expectation.name << " advances span " << narrowest << ".." << widest
                        << " px; a monospace face must not be wider than that";

                    // The other half of "monospaced": widths compose linearly, with no kerning pair
                    // quietly narrowing a run. `SpriteBatch::DrawString` advances by these values,
                    // so a column computed as n * width is only correct if this holds.
                    const float single = font.MeasureString(std::string("M")).X;
                    const float ten = font.MeasureString(std::string("MMMMMMMMMM")).X;
                    EXPECT_FLOAT_EQ(ten, single * 10.0f)
                        << expectation.name << ": ten glyphs must measure ten single advances";
                }
            });
    }

    TEST(FontMetricsTests, TheUiFaceIsProportionalAndTheMonoFaceIsNot)
    {
        WithContent(
            [](Microsoft::Xna::Framework::Content::ContentManager& content)
            {
                // The two faces must not be the same file by accident -- a copy-paste in a
                // descriptor would otherwise leave the debug overlay silently proportional.
                const Gfx::SpriteFont ui = content.Load<Gfx::SpriteFont>("Fonts/ui-16");
                const Gfx::SpriteFont mono = content.Load<Gfx::SpriteFont>("Fonts/mono-16");

                EXPECT_GT(ui.MeasureString(std::string("M")).X, ui.MeasureString(std::string("i")).X)
                    << "the UI face must be proportional";
                EXPECT_FLOAT_EQ(mono.MeasureString(std::string("M")).X,
                                mono.MeasureString(std::string("i")).X)
                    << "the mono face must not be";
            });
    }

    TEST(FontMetricsTests, TheThreeUiSizesAreDistinctAndOrdered)
    {
        WithContent(
            [](Microsoft::Xna::Framework::Content::ContentManager& content)
            {
                // Three separately rasterised sizes, not one scaled: if a copy-paste ever made two
                // descriptors name the same <Size>, every glyph would still render and only this
                // would notice.
                const Gfx::SpriteFont small = content.Load<Gfx::SpriteFont>("Fonts/ui-16");
                const Gfx::SpriteFont medium = content.Load<Gfx::SpriteFont>("Fonts/ui-22");
                const Gfx::SpriteFont large = content.Load<Gfx::SpriteFont>("Fonts/ui-30");

                EXPECT_LT(small.getLineSpacingProperty(), medium.getLineSpacingProperty());
                EXPECT_LT(medium.getLineSpacingProperty(), large.getLineSpacingProperty());

                const std::string sample = "The quick brown fox";
                EXPECT_LT(small.MeasureString(sample).X, medium.MeasureString(sample).X);
                EXPECT_LT(medium.MeasureString(sample).X, large.MeasureString(sample).X);

                const Gfx::SpriteFont mono13 = content.Load<Gfx::SpriteFont>("Fonts/mono-13");
                const Gfx::SpriteFont mono16 = content.Load<Gfx::SpriteFont>("Fonts/mono-16");
                EXPECT_LT(mono13.getLineSpacingProperty(), mono16.getLineSpacingProperty());
            });
    }

    TEST(FontMetricsTests, TheDefaultCharacterSurvivedTheBuild)
    {
        WithContent(
            [](Microsoft::Xna::Framework::Content::ContentManager& content)
            {
                // Every descriptor sets <DefaultCharacter>?</DefaultCharacter>. Without one, drawing
                // a character outside the regions THROWS rather than substituting -- so this is the
                // difference between a stray glyph and a crash in front of a player.
                for (const char* name :
                     {"Fonts/ui-16", "Fonts/ui-22", "Fonts/ui-30", "Fonts/mono-13", "Fonts/mono-16"})
                {
                    const Gfx::SpriteFont font = content.Load<Gfx::SpriteFont>(name);
                    const std::optional<char16_t> fallback = font.getDefaultCharacterProperty();
                    ASSERT_TRUE(fallback.has_value()) << name;
                    EXPECT_EQ(*fallback, u'?') << name;
                }
            });
    }

} // namespace
