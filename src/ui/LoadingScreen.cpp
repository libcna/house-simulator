// SPDX-License-Identifier: MIT
#include "cnahouse/ui/LoadingScreen.hpp"

#include <cmath>
#include <cstdint>

#include "Microsoft/Xna/Framework/Color.hpp"
#include "Microsoft/Xna/Framework/Vector2.hpp"

#include "cnahouse/player/IInputSource.hpp"
#include "cnahouse/ui/TextRenderer.hpp"

namespace cnahouse::ui
{
    namespace
    {
        using Microsoft::Xna::Framework::Color;
        using Microsoft::Xna::Framework::Vector2;
    } // namespace

    LoadingScreen::LoadingScreen(std::string versionLine, std::function<void()> onGesture)
        : versionLine_(std::move(versionLine))
        , onGesture_(std::move(onGesture))
    {
    }

    ScreenAction LoadingScreen::Update(const player::InputState& input, float deltaSeconds)
    {
        elapsed_ += deltaSeconds;

        if (input.anyPressed && !gestureSeen_)
        {
            // ONCE, and on the FIRST press rather than on the dismissing one. The gesture is what
            // permits audio, and permitting it early means the device is already open by the time
            // the first sound is asked for -- so a player who presses a key while content is still
            // loading does not then wait again for the mixer.
            gestureSeen_ = true;
            if (onGesture_)
            {
                onGesture_();
            }
        }

        // Both conditions, in this order: the gesture may arrive long before loading finishes, and
        // dismissing before the house exists would show an empty world.
        if (gestureSeen_ && ready_)
        {
            return ScreenAction::Pop;
        }
        return ScreenAction::None;
    }

    void LoadingScreen::Draw(Microsoft::Xna::Framework::Graphics::SpriteBatch& batch,
                             const TextRenderer& text) const
    {
        text.DrawShadowed(batch, versionLine_, Vector2(0.0f, -80.0f), Anchor::Centre, Color::White);

        if (!ready_)
        {
            text.DrawShadowed(batch, "Loading…", Vector2(0.0f, 0.0f), Anchor::Centre, Color::White);
        }

        // The prompt is shown even before loading finishes, because the gesture is useful early:
        // pressing now opens the audio device now.
        if (!gestureSeen_)
        {
            // A slow pulse rather than a blink. 0.6 Hz, and never below 40 % alpha, so it reads as
            // "waiting for you" rather than as a flashing warning -- and so it is still legible in
            // a screenshot taken at any moment.
            const float pulse = 0.7f + 0.3f * std::sin(elapsed_ * 3.77f);
            // `int`, not `std::uint8_t`. MEASURED: `Color(bytecs, bytecs, bytecs, bytecs)` is
            // marked `CNAEXT`; the plain XNA 4.0 constructors take `intcs` or `float`. A byte alpha
            // here compiles into an ADR-0001 violation that `check_xna_only.py` cannot see.
            const auto alpha = static_cast<int>(pulse * 255.0f);
            text.DrawShadowed(batch,
                              "Press any key to begin",
                              Vector2(0.0f, 60.0f),
                              Anchor::Centre,
                              Color(255, 255, 255, alpha));
        }
        else if (!ready_)
        {
            text.DrawShadowed(batch,
                              "Ready when the house is",
                              Vector2(0.0f, 60.0f),
                              Anchor::Centre,
                              Color(200, 200, 200, 255));
        }
    }

} // namespace cnahouse::ui
