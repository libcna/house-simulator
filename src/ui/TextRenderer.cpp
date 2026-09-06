// SPDX-License-Identifier: MIT
#include "cnahouse/ui/TextRenderer.hpp"

#include <algorithm>

#include "Microsoft/Xna/Framework/Graphics/SpriteBatch.hpp"
#include "Microsoft/Xna/Framework/Graphics/SpriteFont.hpp"

namespace cnahouse::ui
{
    namespace
    {
        using Microsoft::Xna::Framework::Color;
        using Microsoft::Xna::Framework::Vector2;

        /// One virtual unit of offset. Small enough to read as a shadow rather than as a second string,
        /// large enough to separate the glyph from its background at every scale this game runs at.
        constexpr float kShadowOffset = 1.5f;
        constexpr std::uint8_t kShadowAlpha = 153; // 60 %

    } // namespace

    void TextRenderer::SetViewport(int width, int height) noexcept
    {
        viewportWidth_ = std::max(1, width);
        viewportHeight_ = std::max(1, height);
        // The SMALLER of the two ratios, so a window that is wider than the reference does not push
        // text off the bottom and one that is taller does not push it off the side. A per-axis scale
        // would stretch the glyphs, which is worse than either.
        scale_ = std::min(static_cast<float>(viewportWidth_) / kVirtualWidth,
                          static_cast<float>(viewportHeight_) / kVirtualHeight);
    }

    Vector2 TextRenderer::Measure(std::string_view text) const
    {
        if (font_ == nullptr)
        {
            return Vector2(0.0f, 0.0f);
        }
        // `MeasureString` answers in FONT pixels; the caller thinks in virtual units, and the two are
        // the same thing only when the scale is 1. Dividing here keeps every caller in one coordinate
        // system.
        const Vector2 measured = font_->MeasureString(std::string(text));
        return Vector2(measured.X, measured.Y);
    }

    Vector2 TextRenderer::Resolve(std::string_view text, Vector2 virtualPosition, Anchor anchor) const
    {
        const Vector2 size = Measure(text);
        float x = virtualPosition.X;
        float y = virtualPosition.Y;

        switch (anchor)
        {
            case Anchor::TopLeft:
                break;
            case Anchor::TopRight:
                x = kVirtualWidth - virtualPosition.X - size.X;
                break;
            case Anchor::BottomLeft:
                y = kVirtualHeight - virtualPosition.Y - size.Y;
                break;
            case Anchor::BottomRight:
                x = kVirtualWidth - virtualPosition.X - size.X;
                y = kVirtualHeight - virtualPosition.Y - size.Y;
                break;
            case Anchor::Centre:
                x = (kVirtualWidth - size.X) * 0.5f + virtualPosition.X;
                y = (kVirtualHeight - size.Y) * 0.5f + virtualPosition.Y;
                break;
        }
        return Vector2(x * scale_, y * scale_);
    }

    void TextRenderer::Draw(Microsoft::Xna::Framework::Graphics::SpriteBatch& batch,
                            std::string_view text,
                            Vector2 virtualPosition,
                            Anchor anchor,
                            Color colour) const
    {
        if (font_ == nullptr || text.empty())
        {
            return;
        }
        batch.DrawString(*font_, std::string(text), Resolve(text, virtualPosition, anchor), colour);
    }

    void TextRenderer::DrawShadowed(Microsoft::Xna::Framework::Graphics::SpriteBatch& batch,
                                    std::string_view text,
                                    Vector2 virtualPosition,
                                    Anchor anchor,
                                    Color colour) const
    {
        if (font_ == nullptr || text.empty())
        {
            return;
        }
        const Vector2 position = Resolve(text, virtualPosition, anchor);
        const Vector2 shadow(position.X + kShadowOffset * scale_, position.Y + kShadowOffset * scale_);

        // MEASURED (`HOUSE-00065`): the content pipeline premultiplies alpha by default and
        // `SpriteBatch::Begin()` selects `BlendState::AlphaBlend`, which is the premultiplied blend --
        // so a 60 % shadow is `(0, 0, 0, 153)` with the colour channels already multiplied down, which
        // for black is zero either way. Written out because the next translucent colour drawn here will
        // NOT be black and will need the multiplication.
        batch.DrawString(*font_,
                         std::string(text),
                         shadow,
                         Color(static_cast<std::uint8_t>(0),
                               static_cast<std::uint8_t>(0),
                               static_cast<std::uint8_t>(0),
                               kShadowAlpha));
        batch.DrawString(*font_, std::string(text), position, colour);
    }

} // namespace cnahouse::ui
