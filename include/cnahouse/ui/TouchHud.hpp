// SPDX-License-Identifier: MIT
#pragma once

#include <optional>

#include "Microsoft/Xna/Framework/Vector2.hpp"

namespace Microsoft::Xna::Framework::Graphics
{
    class SpriteBatch;
    class Texture2D;
} // namespace Microsoft::Xna::Framework::Graphics

namespace cnahouse::ui
{
    class TextRenderer;

    /// @brief Only the retained walk controls, drawn in the existing HUD SpriteBatch.
    void DrawTouchHud(Microsoft::Xna::Framework::Graphics::SpriteBatch& batch,
                      const Microsoft::Xna::Framework::Graphics::Texture2D& whiteTexel,
                      const TextRenderer& text,
                      bool fastWalk,
                      std::optional<Microsoft::Xna::Framework::Vector2> stickOrigin,
                      std::optional<Microsoft::Xna::Framework::Vector2> stickPosition);
} // namespace cnahouse::ui
