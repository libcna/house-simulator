// SPDX-License-Identifier: MIT
#include "cnahouse/player/KeyboardMouseSource.hpp"

#include <algorithm>
#include <cmath>

#include "Microsoft/Xna/Framework/Input/ButtonState.hpp"
#include "Microsoft/Xna/Framework/Input/Keyboard.hpp"
#include "Microsoft/Xna/Framework/Input/Keys.hpp"
#include "Microsoft/Xna/Framework/Input/Mouse.hpp"
#include "Microsoft/Xna/Framework/Input/Touch/TouchLocationState.hpp"
#include "Microsoft/Xna/Framework/Input/Touch/TouchPanel.hpp"

namespace cnahouse::player
{
    namespace
    {
        using Microsoft::Xna::Framework::Input::Keys;
        using KeyboardState = Microsoft::Xna::Framework::Input::KeyboardState;

        float Axis(const KeyboardState& keyboard,
                   Keys positive,
                   Keys alternatePositive,
                   Keys negative,
                   Keys alternateNegative)
        {
            const float plus =
                keyboard.IsKeyDown(positive) || keyboard.IsKeyDown(alternatePositive) ? 1.0f : 0.0f;
            const float minus =
                keyboard.IsKeyDown(negative) || keyboard.IsKeyDown(alternateNegative) ? 1.0f : 0.0f;
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
              Microsoft::Xna::Framework::Input::Touch::TouchPanel::GetState(),
              deltaSeconds);

        // A browser owns the locked pointer. Warping it to a nominal centre there is ignored or
        // reported as ordinary motion, so the next frame would consume a bogus large delta.
        // The normal sample-to-sample path below already handles Web pointer motion and seeds
        // itself again after each capture transition. Preserve desktop's measured recenter path.
#if !defined(__EMSCRIPTEN__)
        if (captured_)
        {
            // Recentre AFTER sampling, so the sample just taken is the player's motion and the write
            // is what the next frame will recognise and discount.
            Microsoft::Xna::Framework::Input::Mouse::SetPosition(config_.recentreX, config_.recentreY);
            previousMouseX_ = config_.recentreX;
            previousMouseY_ = config_.recentreY;
            hasPreviousMouse_ = true;
        }
#endif
    }

    void KeyboardMouseSource::Apply(const KeyboardState& keyboard,
                                    const Microsoft::Xna::Framework::Input::MouseState& mouse,
                                    float deltaSeconds)
    {
        ApplyDevices(keyboard, mouse, nullptr, deltaSeconds);
    }

    void KeyboardMouseSource::Apply(const KeyboardState& keyboard,
                                    const Microsoft::Xna::Framework::Input::MouseState& mouse,
                                    const Microsoft::Xna::Framework::Input::Touch::TouchCollection& touches,
                                    float deltaSeconds)
    {
        ApplyDevices(keyboard, mouse, &touches, deltaSeconds);
    }

    void
    KeyboardMouseSource::ApplyDevices(const KeyboardState& keyboard,
                                      const Microsoft::Xna::Framework::Input::MouseState& mouse,
                                      const Microsoft::Xna::Framework::Input::Touch::TouchCollection* touches,
                                      float deltaSeconds)
    {
        (void)deltaSeconds;
        state_ = InputState{};

        // --- movement, in game terms ---------------------------------------------------------------
        state_.move.X = Axis(keyboard, Keys::D, Keys::Right, Keys::A, Keys::Left);
        state_.move.Y = Axis(keyboard, Keys::W, Keys::Up, Keys::S, Keys::Down);
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
        auto edge = [&](Edge slot, bool down)
        {
            const auto index = static_cast<std::size_t>(slot);
            const bool pressed = down && !previousEdges_[index];
            previousEdges_[index] = down;
            return pressed;
        };
        state_.interactPressed = edge(Edge::Interact, keyboard.IsKeyDown(Keys::E));
        // One combined edge for either Shift; holding both cannot toggle twice.
        state_.runPressed = edge(Edge::WalkMode, state_.run);
        state_.cancelPressed = edge(Edge::Cancel, keyboard.IsKeyDown(Keys::Escape));
        state_.menuPressed = edge(Edge::Menu, keyboard.IsKeyDown(Keys::Tab));
        state_.cinemaPressed = edge(Edge::Cinema, keyboard.IsKeyDown(Keys::C));
        state_.toggleFullscreenPressed =
            edge(Edge::ToggleFullscreen, keyboard.IsKeyDown(Keys::Enter)) && state_.freeCursorHeld;
        state_.toggleOverlayPressed = edge(Edge::ToggleOverlay, keyboard.IsKeyDown(Keys::F1));
        state_.toggleWorldOverlayPressed = edge(Edge::ToggleWorldOverlay, keyboard.IsKeyDown(Keys::F2));
        state_.toggleVisibilityOverlayPressed =
            edge(Edge::ToggleVisibilityOverlay, keyboard.IsKeyDown(Keys::F3));
        state_.toggleVisibilityGeometryPressed =
            edge(Edge::ToggleVisibilityGeometry, keyboard.IsKeyDown(Keys::F4));
        state_.toggleFreezeVisibilityPressed =
            edge(Edge::ToggleFreezeVisibility, keyboard.IsKeyDown(Keys::F5));
        state_.toggleEnvironmentOverlayPressed =
            edge(Edge::ToggleEnvironmentOverlay, keyboard.IsKeyDown(Keys::F8));
        state_.togglePhysicsOverlayPressed = edge(Edge::TogglePhysicsOverlay, keyboard.IsKeyDown(Keys::F9));
        state_.screenshotPressed = edge(Edge::Screenshot, keyboard.IsKeyDown(Keys::F12));

        state_.uiUpPressed = edge(Edge::UiUp, keyboard.IsKeyDown(Keys::Up) || keyboard.IsKeyDown(Keys::W));
        state_.uiDownPressed =
            edge(Edge::UiDown, keyboard.IsKeyDown(Keys::Down) || keyboard.IsKeyDown(Keys::S));
        state_.uiLeftPressed =
            edge(Edge::UiLeft, keyboard.IsKeyDown(Keys::Left) || keyboard.IsKeyDown(Keys::A));
        state_.uiRightPressed =
            edge(Edge::UiRight, keyboard.IsKeyDown(Keys::Right) || keyboard.IsKeyDown(Keys::D));
        state_.uiAcceptPressed =
            edge(Edge::UiAccept, keyboard.IsKeyDown(Keys::Enter) || keyboard.IsKeyDown(Keys::Space));

        const float width = static_cast<float>(std::max(config_.viewportWidth, 1));
        const float height = static_cast<float>(std::max(config_.viewportHeight, 1));
        const bool mousePrimary =
            mouse.getLeftButtonProperty() == Microsoft::Xna::Framework::Input::ButtonState::Pressed;
        state_.pointerKind = PointerKind::Mouse;
        state_.pointerX = std::clamp(static_cast<float>(mouse.getXProperty()) / width, 0.0F, 1.0F);
        state_.pointerY = std::clamp(static_cast<float>(mouse.getYProperty()) / height, 0.0F, 1.0F);
        state_.pointerPressed = mousePrimary && !primaryDownPreviously_;
        primaryDownPreviously_ = mousePrimary;

        bool touchDown = false;
        if (touches != nullptr && touches->getCountProperty() > 0)
        {
            const auto& touch = (*touches)[0];
            const auto position = touch.getPositionProperty();
            const auto touchState = touch.getStateProperty();
            touchDown = touchState == Microsoft::Xna::Framework::Input::Touch::TouchLocationState::Pressed ||
                        touchState == Microsoft::Xna::Framework::Input::Touch::TouchLocationState::Moved;
            state_.pointerKind = PointerKind::Touch;
            state_.pointerX = std::clamp(position.X / width, 0.0F, 1.0F);
            state_.pointerY = std::clamp(position.Y / height, 0.0F, 1.0F);
            state_.pointerPressed =
                touchState == Microsoft::Xna::Framework::Input::Touch::TouchLocationState::Pressed;
        }

        // ANY input, as one edge. `GetPressedKeys()` is plain XNA 4.0 -- the CNAEXT markings on
        // `KeyboardState` are on its default and set constructors and on `ToString`, not on this.
        // Mouse buttons count too: "click to start" is what a browser actually waits for.
        const bool anyDown =
            !keyboard.GetPressedKeys().empty() ||
            mouse.getLeftButtonProperty() == Microsoft::Xna::Framework::Input::ButtonState::Pressed ||
            mouse.getRightButtonProperty() == Microsoft::Xna::Framework::Input::ButtonState::Pressed ||
            mouse.getMiddleButtonProperty() == Microsoft::Xna::Framework::Input::ButtonState::Pressed ||
            touchDown;
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
