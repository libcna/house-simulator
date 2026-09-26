// SPDX-License-Identifier: MIT
#include "cnahouse/player/TouchSource.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <vector>

#include "Microsoft/Xna/Framework/Input/ButtonState.hpp"
#include "Microsoft/Xna/Framework/Input/Mouse.hpp"
#include "Microsoft/Xna/Framework/Input/Touch/TouchLocationState.hpp"
#include "Microsoft/Xna/Framework/Input/Touch/TouchPanel.hpp"

namespace cnahouse::player
{
    namespace
    {
        using Microsoft::Xna::Framework::Vector2;
        using Microsoft::Xna::Framework::Input::Touch::TouchCollection;
        using Microsoft::Xna::Framework::Input::Touch::TouchLocation;
        using Microsoft::Xna::Framework::Input::Touch::TouchLocationState;

        const TouchLocation* ActiveFinger(const TouchCollection& touches, int id)
        {
            for (int index = 0; index < touches.getCountProperty(); ++index)
            {
                const auto& touch = touches[static_cast<std::size_t>(index)];
                if (touch.getIdProperty() == id && touch.getStateProperty() != TouchLocationState::Released &&
                    touch.getStateProperty() != TouchLocationState::Invalid)
                {
                    return &touch;
                }
            }
            return nullptr;
        }

        bool NewFinger(const TouchLocation& touch)
        {
            if (touch.getStateProperty() == TouchLocationState::Pressed)
            {
                return true;
            }
            // CNA advances Pressed to Moved at the input-frame boundary. The first game read
            // may therefore see Moved with a previous Pressed location, not Pressed itself.
            TouchLocation previous(touch.getIdProperty(), TouchLocationState::Invalid, Vector2(0.0F, 0.0F));
            return touch.getStateProperty() == TouchLocationState::Moved &&
                   touch.TryGetPreviousLocation(previous) &&
                   previous.getStateProperty() == TouchLocationState::Pressed;
        }
    } // namespace

    TouchSource::TouchSource(TouchConfig config)
        : config_(config)
    {
    }

    bool TouchSource::HasActiveTouch() const
    {
        const auto touches = Microsoft::Xna::Framework::Input::Touch::TouchPanel::GetState();
        for (int index = 0; index < touches.getCountProperty(); ++index)
        {
            const auto state = touches[static_cast<std::size_t>(index)].getStateProperty();
            if (state == TouchLocationState::Pressed || state == TouchLocationState::Moved)
            {
                return true;
            }
        }
        return false;
    }

    void TouchSource::Update(float deltaSeconds)
    {
        if (emulateMouse_)
        {
            const auto mouse = Microsoft::Xna::Framework::Input::Mouse::GetState();
            const bool down =
                mouse.getLeftButtonProperty() == Microsoft::Xna::Framework::Input::ButtonState::Pressed;
            const TouchCollection touches(
                down ? std::vector<TouchLocation>{TouchLocation(
                           1,
                           mouseWasDown_ ? TouchLocationState::Moved : TouchLocationState::Pressed,
                           Vector2(static_cast<float>(mouse.getXProperty()),
                                   static_cast<float>(mouse.getYProperty())))}
                     : std::vector<TouchLocation>{});
            mouseWasDown_ = down;
            Apply(touches, deltaSeconds);
            return;
        }
        Apply(Microsoft::Xna::Framework::Input::Touch::TouchPanel::GetState(), deltaSeconds);
    }

    std::optional<Microsoft::Xna::Framework::Vector2> TouchSource::StickOrigin() const noexcept
    {
        return stick_ ? std::optional<Vector2>(stick_->origin) : std::nullopt;
    }

    std::optional<Microsoft::Xna::Framework::Vector2> TouchSource::StickPosition() const noexcept
    {
        return stick_ ? std::optional<Vector2>(stick_->previous) : std::nullopt;
    }

    void TouchSource::Apply(const TouchCollection& touches, float deltaSeconds)
    {
        (void)deltaSeconds;
        state_ = InputState{};
        lookAvailable_ = false;

        if (stick_ && ActiveFinger(touches, stick_->id) == nullptr)
        {
            stick_.reset();
        }
        if (look_ && ActiveFinger(touches, look_->id) == nullptr)
        {
            look_.reset();
        }
        if (menuFinger_ && ActiveFinger(touches, *menuFinger_) == nullptr)
        {
            menuFinger_.reset();
        }
        if (speedFinger_ && ActiveFinger(touches, *speedFinger_) == nullptr)
        {
            speedFinger_.reset();
        }

        const float left = static_cast<float>(config_.layoutX);
        const float top = static_cast<float>(config_.layoutY);
        const float layoutWidth = static_cast<float>(
            std::max(config_.layoutWidth > 0 ? config_.layoutWidth : config_.viewportWidth, 1));
        const float layoutHeight = static_cast<float>(
            std::max(config_.layoutHeight > 0 ? config_.layoutHeight : config_.viewportHeight, 1));
        const float right = left + layoutWidth;
        const float bottom = top + layoutHeight;
        const float scale = std::min(layoutWidth / 1600.0F, layoutHeight / 900.0F);
        const float buttonInset = TouchConfig::kButtonInsetVirtual * scale;
        for (int index = 0; index < touches.getCountProperty(); ++index)
        {
            const auto& touch = touches[static_cast<std::size_t>(index)];
            const auto phase = touch.getStateProperty();
            if (phase != TouchLocationState::Pressed && phase != TouchLocationState::Moved)
            {
                continue;
            }
            const Vector2 position = touch.getPositionProperty();
            const bool newFinger = NewFinger(touch);
            if (newFinger)
            {
                state_.anyPressed = true;
                // A new finger is a menu tap even while the other two fingers continue walking
                // and looking. Menus consume the normalised primary pointer edge.
                if (!state_.pointerPressed)
                {
                    state_.pointerKind = PointerKind::Touch;
                    state_.pointerX = std::clamp((position.X - left) / layoutWidth, 0.0F, 1.0F);
                    state_.pointerY = std::clamp((position.Y - top) / layoutHeight, 0.0F, 1.0F);
                    state_.pointerPressed = true;
                }
            }

            const int id = touch.getIdProperty();
            if ((menuFinger_ && *menuFinger_ == id) || (speedFinger_ && *speedFinger_ == id))
            {
                continue;
            }
            const bool rightButton = position.X >= right - buttonInset && position.X < right;
            const bool menuButton = rightButton && position.Y >= top && position.Y < top + buttonInset;
            const bool speedButton = rightButton && position.Y >= bottom - buttonInset && position.Y < bottom;
            if (newFinger && buttonsEnabled_ && (menuButton || speedButton))
            {
                state_.menuPressed |= menuButton;
                state_.runPressed |= speedButton;
                if (menuButton)
                {
                    menuFinger_ = id;
                }
                else
                {
                    speedFinger_ = id;
                }
                continue;
            }
            if (stick_ && stick_->id == id)
            {
                continue;
            }
            if (look_ && look_->id == id)
            {
                continue;
            }
            // Assign once, by the first position, then keep the ID even when a drag crosses
            // the centre line. This permits an independent look drag while the stick is held.
            if (!stick_ && position.X >= left && position.X < left + layoutWidth * 0.5F &&
                position.Y >= top + layoutHeight * 0.5F && position.Y < bottom)
            {
                stick_ = Finger{id, position, position};
            }
            else if (!look_ && position.X >= left + layoutWidth * 0.5F && position.X < right &&
                     position.Y >= top && position.Y < bottom && !(menuButton || speedButton))
            {
                look_ = Finger{id, position, position};
            }
        }

        if (stick_)
        {
            const auto* touch = ActiveFinger(touches, stick_->id);
            const Vector2 position = touch->getPositionProperty();
            // The 180-virtual-unit radius scales with the 1600x900 virtual viewport.
            const float radius = std::max(TouchConfig::kStickRadiusVirtual * scale, 1.0F);
            const float dx = (position.X - stick_->origin.X) / radius;
            const float dy = (stick_->origin.Y - position.Y) / radius;
            const float length = std::sqrt(dx * dx + dy * dy);
            const float divisor = std::max(length, 1.0F);
            state_.move = Vector2(dx / divisor, dy / divisor);
            stick_->previous = position;
        }

        if (look_)
        {
            const auto* touch = ActiveFinger(touches, look_->id);
            const Vector2 position = touch->getPositionProperty();
            const float dx = position.X - look_->previous.X;
            const float dy = position.Y - look_->previous.Y;
            look_->previous = position;
            if (dx != 0.0F || dy != 0.0F)
            {
                constexpr float kRadiansPerPixel = 0.0022F;
                const float lookScale = kRadiansPerPixel * config_.lookSensitivity;
                state_.look = Vector2(dx * lookScale, dy * lookScale * (config_.invertY ? -1.0F : 1.0F));
                lookAvailable_ = true;
            }
        }
    }

} // namespace cnahouse::player
