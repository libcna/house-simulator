// SPDX-License-Identifier: MIT
#include "cnahouse/ui/MenuStack.hpp"

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

} // namespace cnahouse::ui
