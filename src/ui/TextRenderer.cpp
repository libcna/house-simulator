// SPDX-License-Identifier: MIT
#include "cnahouse/ui/TextRenderer.hpp"

#include <algorithm>
#include <cmath>

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
        /// 60 %. An `int`, not a `std::uint8_t`, and that is not a style choice: `Color` has both
        /// `Color(intcs, intcs, intcs, intcs)` — the XNA 4.0 constructor — and a CNAEXT
        /// `Color(bytecs, bytecs, bytecs, bytecs)` convenience overload. Byte arguments select the
        /// CNAEXT one, which ADR-0001 forbids, and no forbidden identifier appears at the call site
        /// for `check_xna_only.py` to find. `tools/ci/check_xna_strict.py` catches it by asking the
        /// compiler which overload it actually chose (`HOUSE-00168`).
        constexpr int kShadowAlpha = 153;

    } // namespace

    void TextRenderer::SetViewport(int width, int height) noexcept
    {
        SetViewport(width, height, Microsoft::Xna::Framework::Rectangle(0, 0, width, height));
    }

    void TextRenderer::SetViewport(int width,
                                   int height,
                                   const Microsoft::Xna::Framework::Rectangle& safeArea) noexcept
    {
        viewportWidth_ = std::max(1, width);
        viewportHeight_ = std::max(1, height);

        // Clamp an OS-provided rectangle to the real surface. A transient empty rectangle during a
        // resize falls back to the surface rather than collapsing every label to a point.
        const int left = std::clamp(safeArea.X, 0, viewportWidth_);
        const int top = std::clamp(safeArea.Y, 0, viewportHeight_);
        const int right = std::clamp(safeArea.X + std::max(0, safeArea.Width), left, viewportWidth_);
        const int bottom = std::clamp(safeArea.Y + std::max(0, safeArea.Height), top, viewportHeight_);
        const int safeWidth = right - left;
        const int safeHeight = bottom - top;
        const bool usable = safeWidth > 0 && safeHeight > 0;
        const float availableWidth = static_cast<float>(usable ? safeWidth : viewportWidth_);
        const float availableHeight = static_cast<float>(usable ? safeHeight : viewportHeight_);
        const float safeLeft = static_cast<float>(usable ? left : 0);
        const float safeTop = static_cast<float>(usable ? top : 0);

        // The SMALLER ratio preserves the authored aspect. The remaining strip is letterbox space,
        // shared equally on both sides of the safe rectangle.
        scale_ = std::min(availableWidth / kVirtualWidth, availableHeight / kVirtualHeight);
        originX_ = safeLeft + (availableWidth - kVirtualWidth * scale_) * 0.5F;
        originY_ = safeTop + (availableHeight - kVirtualHeight * scale_) * 0.5F;
    }

    Microsoft::Xna::Framework::Rectangle TextRenderer::LayoutBounds() const noexcept
    {
        return Microsoft::Xna::Framework::Rectangle(static_cast<int>(std::lround(originX_)),
                                                    static_cast<int>(std::lround(originY_)),
                                                    static_cast<int>(std::lround(kVirtualWidth * scale_)),
                                                    static_cast<int>(std::lround(kVirtualHeight * scale_)));
    }

    Vector2 TextRenderer::Measure(std::string_view text) const
    {
        if (font_ == nullptr)
        {
            return Vector2(0.0f, 0.0f);
        }
        // The face deliberately retains its authored pixel size at every viewport. Its measured box
        // is therefore already in the physical coordinate system used to finish anchor placement.
        const Vector2 measured = font_->MeasureString(std::string(text));
        return Vector2(measured.X, measured.Y);
    }

    Vector2 TextRenderer::Resolve(std::string_view text, Vector2 virtualPosition, Anchor anchor) const
    {
        const Vector2 size = Measure(text);
        float x = originX_ + virtualPosition.X * scale_;
        float y = originY_ + virtualPosition.Y * scale_;
        const float right = originX_ + kVirtualWidth * scale_;
        const float bottom = originY_ + kVirtualHeight * scale_;
        const float centreX = originX_ + kVirtualWidth * scale_ * 0.5F;
        const float centreY = originY_ + kVirtualHeight * scale_ * 0.5F;

        switch (anchor)
        {
            case Anchor::TopLeft:
                break;
            case Anchor::TopCentre:
                x = centreX - size.X * 0.5F + virtualPosition.X * scale_;
                break;
            case Anchor::TopRight:
                x = right - virtualPosition.X * scale_ - size.X;
                break;
            case Anchor::BottomLeft:
                y = bottom - virtualPosition.Y * scale_ - size.Y;
                break;
            case Anchor::BottomRight:
                x = right - virtualPosition.X * scale_ - size.X;
                y = bottom - virtualPosition.Y * scale_ - size.Y;
                break;
            case Anchor::Centre:
                x = centreX - size.X * 0.5F + virtualPosition.X * scale_;
                y = centreY - size.Y * 0.5F + virtualPosition.Y * scale_;
                break;
        }
        return Vector2(x, y);
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
        batch.DrawString(*font_, std::string(text), shadow, Color(0, 0, 0, kShadowAlpha));
        batch.DrawString(*font_, std::string(text), position, colour);
    }

} // namespace cnahouse::ui
