// SPDX-License-Identifier: MIT
#include "cnahouse/player/KeyboardMouseSource.hpp"

#include <algorithm>
#include <cmath>

#include "Microsoft/Xna/Framework/Input/ButtonState.hpp"
#include "Microsoft/Xna/Framework/Input/Keyboard.hpp"
#include "Microsoft/Xna/Framework/Input/Keys.hpp"
#include "Microsoft/Xna/Framework/Input/Mouse.hpp"

namespace cnahouse::player
{
    namespace
    {
        using Microsoft::Xna::Framework::Input::Keys;
        using KeyboardState = Microsoft::Xna::Framework::Input::KeyboardState;

        float Axis(const KeyboardState& keyboard, Keys positive, Keys negative)
        {
            const float plus = keyboard.IsKeyDown(positive) ? 1.0f : 0.0f;
            const float minus = keyboard.IsKeyDown(negative) ? 1.0f : 0.0f;
            return plus - minus;
        }

    } // namespace

    KeyboardMouseSource::KeyboardMouseSource(InputConfig config)
        : config_(config)
    {
    }

    void KeyboardMouseSource::SetMouseCaptured(bool captured) noexcept
    {
        if (captured_ == captured)
        {
            return;
        }
        captured_ = captured;
        // Discard the position history across a capture change. The pointer may have moved anywhere
        // while the menu was open, and the first delta after re-capturing would otherwise be the whole
        // distance it travelled -- the "camera snaps when you close the menu" bug.
        hasPreviousMouse_ = false;
        lookAvailable_ = false;
        // The two-frame average's history goes with it -- and does so in `Apply`'s seeding branch
        // rather than here, because dropping `hasPreviousMouse_` is what sends the next frame
        // down that branch. Zeroing it in both places is one place too many: a bug in the one
        // that matters would be hidden by the one that does not.
    }

    void KeyboardMouseSource::Update(float deltaSeconds)
    {
        Apply(Microsoft::Xna::Framework::Input::Keyboard::GetState(),
              Microsoft::Xna::Framework::Input::Mouse::GetState(),
              deltaSeconds);

        if (captured_)
        {
            // Recentre AFTER sampling, so the sample just taken is the player's motion and the write
            // is what the next frame will recognise and discount.
            Microsoft::Xna::Framework::Input::Mouse::SetPosition(config_.recentreX, config_.recentreY);
            previousMouseX_ = config_.recentreX;
            previousMouseY_ = config_.recentreY;
            hasPreviousMouse_ = true;
        }
    }

    void KeyboardMouseSource::Apply(const KeyboardState& keyboard,
                                    const Microsoft::Xna::Framework::Input::MouseState& mouse,
                                    float deltaSeconds)
    {
        (void)deltaSeconds;
        state_ = InputState{};

        // --- movement, in game terms ---------------------------------------------------------------
        state_.move.X = Axis(keyboard, Keys::D, Keys::A);
        state_.move.Y = Axis(keyboard, Keys::W, Keys::S);
        // Normalised so diagonal movement is not faster than cardinal -- the oldest bug in
        // first-person movement, and one that only shows up as "the player feels fast on the diagonal".
        const float length = std::sqrt(state_.move.X * state_.move.X + state_.move.Y * state_.move.Y);
        if (length > 1.0f)
        {
            state_.move.X /= length;
            state_.move.Y /= length;
        }

        state_.run = keyboard.IsKeyDown(Keys::LeftShift) || keyboard.IsKeyDown(Keys::RightShift);
        state_.crouch = keyboard.IsKeyDown(Keys::LeftControl) || keyboard.IsKeyDown(Keys::RightControl);
        state_.jump = keyboard.IsKeyDown(Keys::Space);
        // §68's cursor release. Both alts, because a keyboard has two and a player uses whichever
        // hand is free.
        state_.freeCursorHeld = keyboard.IsKeyDown(Keys::LeftAlt) || keyboard.IsKeyDown(Keys::RightAlt);

        // An edge: down now and up before. Computed here so no consumer keeps its own history --
        // two systems each tracking "was it down last frame" is two chances to disagree about the frame.
        auto edge = [&](Edge slot, Keys key)
        {
            const auto index = static_cast<std::size_t>(slot);
            const bool down = keyboard.IsKeyDown(key);
            const bool pressed = down && !previousEdges_[index];
            previousEdges_[index] = down;
            return pressed;
        };
        state_.interactPressed = edge(Edge::Interact, Keys::E);
        // §43.2's toggle. `LeftShift` is the one that toggles; the right one is the level's
        // second binding and repeating it here would make the two shifts fight over the edge.
        state_.runPressed = edge(Edge::WalkMode, Keys::LeftShift);
        state_.cancelPressed = edge(Edge::Cancel, Keys::Escape);
        state_.menuPressed = edge(Edge::Menu, Keys::Tab);
        state_.toggleOverlayPressed = edge(Edge::ToggleOverlay, Keys::F1);
        state_.toggleWorldOverlayPressed = edge(Edge::ToggleWorldOverlay, Keys::F2);
        state_.screenshotPressed = edge(Edge::Screenshot, Keys::F12);

        // ANY input, as one edge. `GetPressedKeys()` is plain XNA 4.0 -- the CNAEXT markings on
        // `KeyboardState` are on its default and set constructors and on `ToString`, not on this.
        // Mouse buttons count too: "click to start" is what a browser actually waits for.
        const bool anyDown =
            !keyboard.GetPressedKeys().empty() ||
            mouse.getLeftButtonProperty() == Microsoft::Xna::Framework::Input::ButtonState::Pressed ||
            mouse.getRightButtonProperty() == Microsoft::Xna::Framework::Input::ButtonState::Pressed ||
            mouse.getMiddleButtonProperty() == Microsoft::Xna::Framework::Input::ButtonState::Pressed;
        state_.anyPressed = anyDown && !anyDownPreviously_;
        anyDownPreviously_ = anyDown;

        // --- look ------------------------------------------------------------------------------------
        const int x = mouse.getXProperty();
        const int y = mouse.getYProperty();

        if (!hasPreviousMouse_)
        {
            // Seeded from the first REAL sample rather than assumed to be the window centre. Assuming
            // the centre produces one enormous bogus delta on the first frame, which is the classic
            // "camera snaps on startup". `HOUSE-00100` measured that `Mouse::GetState` returns an
            // event-driven snapshot, so where it starts is not something to guess at.
            previousMouseX_ = x;
            previousMouseY_ = y;
            hasPreviousMouse_ = true;
            lookAvailable_ = false;
            previousLookX_ = 0.0f;
            previousLookY_ = 0.0f;
            return;
        }

        const int dx = x - previousMouseX_;
        const int dy = y - previousMouseY_;
        previousMouseX_ = x;
        previousMouseY_ = y;

        if (dx == 0 && dy == 0)
        {
            // No motion event arrived. Not the same as "the player held still": on an unfocused window
            // the snapshot simply does not advance, and the two are indistinguishable from here -- so
            // neither contributes look, which is the safe reading of both.
            lookAvailable_ = false;
            previousLookX_ = 0.0f;
            previousLookY_ = 0.0f;
            return;
        }

        lookAvailable_ = true;
        const float scale = InputConfig::kRadiansPerPixel * config_.sensitivity;
        const float lookX = static_cast<float>(dx) * scale;
        const float lookY = static_cast<float>(dy) * scale * (config_.invertY ? -1.0f : 1.0f);

        if (config_.smoothing)
        {
            // §44's two-frame average, applied AFTER the sensitivity so the setting is still the
            // multiplier the player set and not a number the smoothing has been through.
            state_.look.X = 0.5f * (lookX + previousLookX_);
            state_.look.Y = 0.5f * (lookY + previousLookY_);
        }
        else
        {
            state_.look.X = lookX;
            state_.look.Y = lookY;
        }
        previousLookX_ = lookX;
        previousLookY_ = lookY;
    }

} // namespace cnahouse::player
