// SPDX-License-Identifier: MIT
#pragma once

#include <optional>
#include <string>
#include <string_view>

#include "Microsoft/Xna/Framework/Color.hpp"
#include "Microsoft/Xna/Framework/Rectangle.hpp"
#include "Microsoft/Xna/Framework/Vector2.hpp"

namespace Microsoft::Xna::Framework::Graphics
{
    class GraphicsDevice;
    class SpriteBatch;
    class SpriteFont;
} // namespace Microsoft::Xna::Framework::Graphics

namespace cnahouse::ui
{

    /// @brief Where a string is anchored, so a caller says "top right" instead of doing the arithmetic.
    enum class Anchor
    {
        TopLeft,
        TopCentre,
        TopRight,
        BottomLeft,
        BottomRight,
        Centre,
    };

    /// @brief Draws text: measured, anchored, shadowed, and scaled in virtual units.
    ///
    /// **Virtual units are the point of this class.** The HUD is authored against a 1600×900 reference
    /// and the window can be anything, so a position given in pixels is right on one machine and wrong
    /// on every other. A caller gives virtual coordinates and this scales them, which means the layout
    /// is written once and a screenshot comparison at a different resolution still matches.
    ///
    /// **The drop shadow is not decoration.** White text on a bright window frame or a sunlit wall is
    /// unreadable, and the house has both. One offset copy in black at 60 % alpha makes every string
    /// legible against every background this game produces, for the cost of a second `DrawString`.
    ///
    /// `HOUSE-00066` measured that `MeasureString` is a usable layout oracle: the ink of a drawn string
    /// lands inside the measured box and `MeasureString` is exactly linear at `<Spacing>0</Spacing>`.
    /// Anchoring relies on that.
    class TextRenderer
    {
    public:
        /// @brief The reference resolution the HUD is authored against.
        static constexpr float kVirtualWidth = 1600.0f;
        static constexpr float kVirtualHeight = 900.0f;

        TextRenderer() = default;

        /// @brief Points the renderer at a font. Without one, every draw is a no-op.
        ///
        /// A no-op rather than a crash because a build whose content tree has not been generated must
        /// still run -- the same policy `CnaHouseGame::LoadContent` applies.
        void SetFont(const Microsoft::Xna::Framework::Graphics::SpriteFont* font) noexcept
        {
            font_ = font;
        }

        [[nodiscard]] bool HasFont() const noexcept
        {
            return font_ != nullptr;
        }

        /// @brief The face currently pointed at, so a caller that swaps one in can put it back.
        ///
        /// Added for `HOUSE-00201`: the smoke scene draws with its OWN face to prove that face
        /// loaded, and leaving the HUD pointed at a font the scene owns would dangle at teardown.
        [[nodiscard]] const Microsoft::Xna::Framework::Graphics::SpriteFont* Font() const noexcept
        {
            return font_;
        }

        /// @brief Tells the renderer the real back-buffer size, so virtual units can be scaled.
        ///
        /// The whole viewport is considered safe. Runtime code normally uses the overload below
        /// with XNA's `Viewport::TitleSafeArea`; this overload remains useful for tests and callers
        /// whose surface has no excluded edges.
        void SetViewport(int width, int height) noexcept;

        /// @brief Fits the 1600x900 virtual canvas inside @p safeArea without stretching it.
        ///
        /// Insets are expressed in physical back-buffer pixels, as XNA reports them. The authored
        /// canvas is uniformly scaled and centred in the safe rectangle, so 4:3 letterboxes
        /// vertically and a 20:9 phone letterboxes horizontally while every menu remains clear of
        /// a cutout or system gesture edge.
        void
        SetViewport(int width, int height, const Microsoft::Xna::Framework::Rectangle& safeArea) noexcept;

        /// @brief The uniform scale from virtual layout units to pixels.
        ///
        /// One scale for both axes, chosen as the SMALLER of the two ratios. It transforms layout
        /// positions; the SpriteFont stays at its authored pixel size so low-resolution targets do
        /// not turn the 16-pixel UI face into unreadable six-pixel text.
        [[nodiscard]] float Scale() const noexcept
        {
            return scale_;
        }

        /// @brief Physical-pixel rectangle occupied by the virtual canvas.
        [[nodiscard]] Microsoft::Xna::Framework::Rectangle LayoutBounds() const noexcept;

        /// @brief The size @p text would occupy, in virtual units.
        [[nodiscard]] Microsoft::Xna::Framework::Vector2 Measure(std::string_view text) const;

        /// @brief Draws @p text at @p virtualPosition, anchored per @p anchor.
        ///
        /// @param batch a `SpriteBatch` between `Begin` and `End`. Passed in rather than owned so one
        ///              batch covers the whole HUD -- `HOUSE-00106` measured a draw call at 8.15 µs of
        ///              CPU, and a batch per string would be a hundred of them.
        void Draw(Microsoft::Xna::Framework::Graphics::SpriteBatch& batch,
                  std::string_view text,
                  Microsoft::Xna::Framework::Vector2 virtualPosition,
                  Anchor anchor,
                  Microsoft::Xna::Framework::Color colour) const;

        /// @brief `Draw` with the drop shadow. What the HUD actually uses.
        void DrawShadowed(Microsoft::Xna::Framework::Graphics::SpriteBatch& batch,
                          std::string_view text,
                          Microsoft::Xna::Framework::Vector2 virtualPosition,
                          Anchor anchor,
                          Microsoft::Xna::Framework::Color colour) const;

    private:
        [[nodiscard]] Microsoft::Xna::Framework::Vector2 Resolve(
            std::string_view text, Microsoft::Xna::Framework::Vector2 virtualPosition, Anchor anchor) const;

        const Microsoft::Xna::Framework::Graphics::SpriteFont* font_ = nullptr;
        float scale_ = 1.0f;
        float originX_ = 0.0f;
        float originY_ = 0.0f;
        int viewportWidth_ = static_cast<int>(kVirtualWidth);
        int viewportHeight_ = static_cast<int>(kVirtualHeight);
    };

} // namespace cnahouse::ui
