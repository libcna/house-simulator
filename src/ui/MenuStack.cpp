// SPDX-License-Identifier: MIT
#include "cnahouse/ui/MenuStack.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <format>

#include "Microsoft/Xna/Framework/Color.hpp"
#include "Microsoft/Xna/Framework/Vector2.hpp"

#include "cnahouse/player/IInputSource.hpp"
#include "cnahouse/ui/TextRenderer.hpp"

namespace cnahouse::ui
{

    std::string_view ScreenIdName(ScreenId id) noexcept
    {
        switch (id)
        {
            case ScreenId::Loading:
                return "loading";
            case ScreenId::MainMenu:
                return "main-menu";
            case ScreenId::PauseMenu:
                return "pause-menu";
            case ScreenId::SettingsMenu:
                return "settings";
            case ScreenId::CustomiseAvatar:
                return "customise-avatar";
            case ScreenId::Confirm:
                return "confirm";
            case ScreenId::Credits:
                return "credits";
        }
        return "?";
    }

    void MenuStack::Push(std::unique_ptr<IScreen> screen)
    {
        if (screen == nullptr)
        {
            return;
        }
        screens_.push_back(std::move(screen));
    }

    void MenuStack::Pop()
    {
        if (screens_.empty())
        {
            // A no-op rather than a crash. `Escape` arriving one frame after a screen closed itself
            // is an ordinary race between input and a transition, not a programming error.
            return;
        }
        screens_.pop_back();
    }

    void MenuStack::Clear()
    {
        screens_.clear();
    }

    void MenuStack::Replace(std::unique_ptr<IScreen> screen)
    {
        screens_.clear();
        Push(std::move(screen));
    }

    const IScreen* MenuStack::Top() const noexcept
    {
        return screens_.empty() ? nullptr : screens_.back().get();
    }

    bool MenuStack::Update(const player::InputState& input, float deltaSeconds)
    {
        if (screens_.empty())
        {
            return false;
        }

        // ONLY the top screen. A `Confirm` over a `PauseMenu` must be the only thing that hears the
        // keyboard, or `Escape` closes both at once -- which reads as the confirmation having been
        // answered when it was only dismissed.
        const ScreenAction action = screens_.back()->Update(input, deltaSeconds);
        switch (action)
        {
            case ScreenAction::None:
                break;
            case ScreenAction::Pop:
                Pop();
                break;
            case ScreenAction::PopAll:
                Clear();
                break;
            case ScreenAction::Quit:
                return true;
        }
        return false;
    }

    std::size_t MenuStack::FirstVisible() const noexcept
    {
        // Walk down from the top until an opaque screen is found; that one and everything above it
        // is drawn. Anything below an opaque screen is completely hidden and drawing it would cost
        // a full pass for pixels nobody sees.
        if (screens_.empty())
        {
            return 0;
        }
        std::size_t first = screens_.size() - 1;
        while (first > 0 && screens_[first]->IsTranslucent())
        {
            --first;
        }
        return first;
    }

    void MenuStack::Draw(Microsoft::Xna::Framework::Graphics::SpriteBatch& batch,
                         const TextRenderer& text) const
    {
        const std::size_t first = FirstVisible();
        for (std::size_t i = first; i < screens_.size(); ++i)
        {
            screens_[i]->Draw(batch, text);
        }
    }

    bool MenuStack::WorldIsPaused() const noexcept
    {
        // ANY screen in the stack, not just the top: a `Confirm` is translucent and does not itself
        // pause, but the `PauseMenu` under it may, and the world must not resume because a dialogue
        // opened on top of it.
        for (const auto& screen : screens_)
        {
            if (screen->PausesWorld())
            {
                return true;
            }
        }
        return false;
    }

    namespace
    {
        using Microsoft::Xna::Framework::Color;
        using Microsoft::Xna::Framework::Vector2;

        constexpr std::array<float, SettingsScreen::ControlCount()> kSettingsRowY = {
            0.244F, 0.294F, 0.344F, 0.394F, 0.539F, 0.589F, 0.639F};
        constexpr float kSettingsHitHalfHeight = 0.022F;
        constexpr float kSettingsSliderStart = 0.55F;
        constexpr float kSettingsSliderEnd = 0.80F;

        [[nodiscard]] std::size_t SettingsIndex(SettingsControl control) noexcept
        {
            return static_cast<std::size_t>(control);
        }

        [[nodiscard]] float StepSetting(float value, float amount, int direction, float low, float high)
        {
            const float changed = value + static_cast<float>(direction) * amount;
            return std::clamp(std::round(changed / amount) * amount, low, high);
        }

        template<typename T>
        [[nodiscard]] bool AssignChanged(T& value, T changed)
        {
            if (value == changed)
            {
                return false;
            }
            value = changed;
            return true;
        }
    } // namespace

    std::string_view SettingsControlName(SettingsControl control) noexcept
    {
        switch (control)
        {
            case SettingsControl::Master:
                return "Master";
            case SettingsControl::Footsteps:
                return "Footsteps";
            case SettingsControl::Ambience:
                return "Ambience";
            case SettingsControl::Weather:
                return "Weather";
            case SettingsControl::LookSensitivity:
                return "Look sensitivity";
            case SettingsControl::InvertY:
                return "Invert Y";
            case SettingsControl::WalkSpeed:
                return "Walk speed";
            case SettingsControl::Count:
                break;
        }
        return "?";
    }

    SettingsScreen::SettingsScreen(app::Settings& settings, Changed onChanged)
        : settings_(&settings)
        , onChanged_(std::move(onChanged))
    {
    }

    ScreenAction SettingsScreen::Update(const player::InputState& input, float deltaSeconds)
    {
        (void)deltaSeconds;
        if (input.cancelPressed)
        {
            return ScreenAction::Pop;
        }

        const auto count = static_cast<int>(ControlCount());
        int index = static_cast<int>(selected_);
        if (input.uiUpPressed)
        {
            index = (index + count - 1) % count;
            selected_ = static_cast<SettingsControl>(index);
        }
        if (input.uiDownPressed)
        {
            index = (index + 1) % count;
            selected_ = static_cast<SettingsControl>(index);
        }

        bool changed = false;
        if (input.pointerPressed)
        {
            changed = SelectPointer(input.pointerX, input.pointerY);
        }
        else if (input.uiLeftPressed)
        {
            changed = ChangeSelected(-1);
        }
        else if (input.uiRightPressed)
        {
            changed = ChangeSelected(1);
        }
        else if (input.uiAcceptPressed)
        {
            changed = ActivateSelected();
        }

        if (changed && onChanged_)
        {
            onChanged_();
        }
        return ScreenAction::None;
    }

    bool SettingsScreen::ChangeSelected(int direction)
    {
        switch (selected_)
        {
            case SettingsControl::Master:
                return AssignChanged(settings_->masterVolume,
                                     StepSetting(settings_->masterVolume, 0.05F, direction, 0.0F, 1.0F));
            case SettingsControl::Footsteps:
                return AssignChanged(settings_->footstepsVolume,
                                     StepSetting(settings_->footstepsVolume, 0.05F, direction, 0.0F, 1.0F));
            case SettingsControl::Ambience:
                return AssignChanged(settings_->ambienceVolume,
                                     StepSetting(settings_->ambienceVolume, 0.05F, direction, 0.0F, 1.0F));
            case SettingsControl::Weather:
                return AssignChanged(settings_->weatherVolume,
                                     StepSetting(settings_->weatherVolume, 0.05F, direction, 0.0F, 1.0F));
            case SettingsControl::LookSensitivity:
                return AssignChanged(settings_->mouseSensitivity,
                                     StepSetting(settings_->mouseSensitivity, 0.1F, direction, 0.2F, 4.0F));
            case SettingsControl::InvertY:
                return AssignChanged(settings_->invertY, direction > 0);
            case SettingsControl::WalkSpeed:
                return AssignChanged(settings_->fastWalk, direction > 0);
            case SettingsControl::Count:
                break;
        }
        return false;
    }

    bool SettingsScreen::ActivateSelected()
    {
        if (selected_ == SettingsControl::InvertY)
        {
            settings_->invertY = !settings_->invertY;
            return true;
        }
        if (selected_ == SettingsControl::WalkSpeed)
        {
            settings_->fastWalk = !settings_->fastWalk;
            return true;
        }
        return ChangeSelected(1);
    }

    bool SettingsScreen::SelectPointer(float x, float y)
    {
        if (!std::isfinite(x) || !std::isfinite(y) || x < 0.20F || x > 0.82F)
        {
            return false;
        }

        for (std::size_t i = 0; i < kSettingsRowY.size(); ++i)
        {
            if (std::abs(y - kSettingsRowY[i]) > kSettingsHitHalfHeight)
            {
                continue;
            }
            selected_ = static_cast<SettingsControl>(i);
            if (i <= SettingsIndex(SettingsControl::Weather) || selected_ == SettingsControl::LookSensitivity)
            {
                const float fraction = std::clamp(
                    (x - kSettingsSliderStart) / (kSettingsSliderEnd - kSettingsSliderStart), 0.0F, 1.0F);
                if (selected_ == SettingsControl::LookSensitivity)
                {
                    return AssignChanged(settings_->mouseSensitivity, 0.2F + fraction * 3.8F);
                }

                float* volume = &settings_->masterVolume;
                if (selected_ == SettingsControl::Footsteps)
                {
                    volume = &settings_->footstepsVolume;
                }
                else if (selected_ == SettingsControl::Ambience)
                {
                    volume = &settings_->ambienceVolume;
                }
                else if (selected_ == SettingsControl::Weather)
                {
                    volume = &settings_->weatherVolume;
                }
                return AssignChanged(*volume, fraction);
            }
            return ActivateSelected();
        }
        return false;
    }

    std::string SettingsScreen::ValueText(SettingsControl control) const
    {
        switch (control)
        {
            case SettingsControl::Master:
                return std::format("{:.0f}%", static_cast<double>(settings_->masterVolume) * 100.0);
            case SettingsControl::Footsteps:
                return std::format("{:.0f}%", static_cast<double>(settings_->footstepsVolume) * 100.0);
            case SettingsControl::Ambience:
                return std::format("{:.0f}%", static_cast<double>(settings_->ambienceVolume) * 100.0);
            case SettingsControl::Weather:
                return std::format("{:.0f}%", static_cast<double>(settings_->weatherVolume) * 100.0);
            case SettingsControl::LookSensitivity:
                return std::format("{:.1f}x", static_cast<double>(settings_->mouseSensitivity));
            case SettingsControl::InvertY:
                return settings_->invertY ? "On" : "Off";
            case SettingsControl::WalkSpeed:
                return settings_->fastWalk ? "Fast" : "Normal";
            case SettingsControl::Count:
                break;
        }
        return "";
    }

    void SettingsScreen::Draw(Microsoft::Xna::Framework::Graphics::SpriteBatch& batch,
                              const TextRenderer& text) const
    {
        text.DrawShadowed(batch, "Settings", Vector2(0.0F, 55.0F), Anchor::TopCentre, Color::White);
        text.DrawShadowed(batch, "Graphics", Vector2(0.0F, 125.0F), Anchor::TopCentre, Color::White);
        text.DrawShadowed(batch, "Audio", Vector2(0.0F, 170.0F), Anchor::TopCentre, Color::White);
        text.DrawShadowed(batch, "Controls", Vector2(0.0F, 435.0F), Anchor::TopCentre, Color::White);
        text.DrawShadowed(batch, "Environment", Vector2(0.0F, 625.0F), Anchor::TopCentre, Color::White);

        for (std::size_t i = 0; i < ControlCount(); ++i)
        {
            const auto control = static_cast<SettingsControl>(i);
            const bool selected = control == selected_;
            const std::string line = std::format(
                "{}{:18}  {}", selected ? "> " : "  ", SettingsControlName(control), ValueText(control));
            const int shade = selected ? 255 : 210;
            text.DrawShadowed(batch,
                              line,
                              Vector2(0.0F, kSettingsRowY[i] * TextRenderer::kVirtualHeight),
                              Anchor::TopCentre,
                              Color(shade, shade, shade, 255));
        }

        text.DrawShadowed(batch,
                          "Arrows/WASD change  Enter select  Esc back",
                          Vector2(0.0F, 790.0F),
                          Anchor::TopCentre,
                          Color(200, 200, 200, 255));
    }

} // namespace cnahouse::ui
