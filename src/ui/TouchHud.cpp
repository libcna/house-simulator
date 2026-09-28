// SPDX-License-Identifier: MIT
#include "cnahouse/ui/TouchHud.hpp"

#include <algorithm>
#include <cmath>

#include "Microsoft/Xna/Framework/Color.hpp"
#include "Microsoft/Xna/Framework/Graphics/SpriteBatch.hpp"
#include "Microsoft/Xna/Framework/Graphics/SpriteEffects.hpp"
#include "Microsoft/Xna/Framework/Graphics/Texture2D.hpp"

#include "cnahouse/player/TouchSource.hpp"
#include "cnahouse/ui/TextRenderer.hpp"

namespace cnahouse::ui
{
    namespace
    {
        using Microsoft::Xna::Framework::Color;
        using Microsoft::Xna::Framework::Vector2;
        using Microsoft::Xna::Framework::Graphics::SpriteBatch;
        using Microsoft::Xna::Framework::Graphics::SpriteEffects;
        using Microsoft::Xna::Framework::Graphics::Texture2D;

        void DrawRing(SpriteBatch& batch, const Texture2D& texel, Vector2 centre, float radius, Color colour)
        {
            constexpr int kSegments = 24;
            constexpr float kTau = 6.28318530718F;
            for (int segment = 0; segment < kSegments; ++segment)
            {
                const float first = kTau * static_cast<float>(segment) / static_cast<float>(kSegments);
                const float second = kTau * static_cast<float>(segment + 1) / static_cast<float>(kSegments);
                const Vector2 start(centre.X + radius * std::cos(first), centre.Y + radius * std::sin(first));
                const Vector2 end(centre.X + radius * std::cos(second), centre.Y + radius * std::sin(second));
                const float dx = end.X - start.X;
                const float dy = end.Y - start.Y;
                batch.Draw(texel,
                           start,
                           std::nullopt,
                           colour,
                           std::atan2(dy, dx),
                           Vector2(0.0F, 0.5F),
                           Vector2(std::sqrt(dx * dx + dy * dy), 3.0F),
                           SpriteEffects::None,
                           0.0F);
            }
        }
    } // namespace

    void DrawTouchHud(SpriteBatch& batch,
                      const Texture2D& whiteTexel,
                      const TextRenderer& text,
                      bool fastWalk,
                      std::optional<Vector2> stickOrigin,
                      std::optional<Vector2> stickPosition)
    {
        const auto bounds = text.LayoutBounds();
        const float scale = text.Scale();
        const float right = static_cast<float>(bounds.X + bounds.Width);
        const float bottom = static_cast<float>(bounds.Y + bounds.Height);
        const Vector2 idleStick(static_cast<float>(bounds.X) + 180.0F * scale, bottom - 180.0F * scale);
        const Vector2 base = stickOrigin.value_or(idleStick);
        const float radius = stickOrigin ? player::TouchConfig::kStickRadiusVirtual * scale : 66.0F * scale;
        DrawRing(batch, whiteTexel, base, radius, Color(0.25F, 0.75F, 0.9F, 0.45F));
        if (stickOrigin && stickPosition)
        {
            const float dx = stickPosition->X - stickOrigin->X;
            const float dy = stickPosition->Y - stickOrigin->Y;
            const float length = std::sqrt(dx * dx + dy * dy);
            const float factor = length > radius ? radius / length : 1.0F;
            DrawRing(batch,
                     whiteTexel,
                     Vector2(base.X + dx * factor, base.Y + dy * factor),
                     28.0F * scale,
                     Color(0.45F, 0.95F, 1.0F, 0.85F));
        }
        else
        {
            text.DrawShadowed(batch, "MOVE", Vector2(159.0F, 710.0F), Anchor::TopLeft, Color::White);
        }

        const float buttonX = right - 80.0F * scale;
        DrawRing(batch,
                 whiteTexel,
                 Vector2(buttonX, static_cast<float>(bounds.Y) + 80.0F * scale),
                 60.0F * scale,
                 Color(0.25F, 0.75F, 0.9F, 0.7F));
        DrawRing(batch,
                 whiteTexel,
                 Vector2(buttonX, bottom - 80.0F * scale),
                 60.0F * scale,
                 fastWalk ? Color(0.6F, 0.9F, 0.3F, 0.85F) : Color(0.25F, 0.75F, 0.9F, 0.7F));
        text.DrawShadowed(batch, "MENU", Vector2(1496.0F, 70.0F), Anchor::TopLeft, Color::White);
        text.DrawShadowed(
            batch, fastWalk ? "RUN" : "WALK", Vector2(1496.0F, 810.0F), Anchor::TopLeft, Color::White);
    }
} // namespace cnahouse::ui
