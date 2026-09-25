// SPDX-License-Identifier: MIT
#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "cnahouse/app/Settings.hpp"

namespace Microsoft::Xna::Framework::Graphics
{
    class SpriteBatch;
}

namespace cnahouse::player
{
    struct InputState;
}

namespace cnahouse::ui
{

    class TextRenderer;

    /// @brief The screens of `cna-house.md` §67.3, plus the loading/title screen of §7.4.
    enum class ScreenId
    {
        Loading,
        MainMenu,
        PauseMenu,
        SettingsMenu,
        CustomiseAvatar,
        Confirm,
        Credits,
    };

    [[nodiscard]] std::string_view ScreenIdName(ScreenId id) noexcept;

    /// @brief What a screen asks the stack to do after its update.
    enum class ScreenAction
    {
        /// @brief Stay. The overwhelmingly common answer.
        None,
        /// @brief Close this screen. `Confirm` answering, `PauseMenu` resuming.
        Pop,
        /// @brief Close every screen. Returning to play from three levels of menu.
        PopAll,
        /// @brief End the session.
        Quit,
    };

    /// @brief One screen.
    class IScreen
    {
    public:
        virtual ~IScreen() = default;

        [[nodiscard]] virtual ScreenId Id() const = 0;

        virtual ScreenAction Update(const player::InputState& input, float deltaSeconds) = 0;

        virtual void Draw(Microsoft::Xna::Framework::Graphics::SpriteBatch& batch,
                          const TextRenderer& text) const = 0;

        /// @brief Whether the screen underneath is still drawn.
        ///
        /// `Confirm` over `PauseMenu` is the case this exists for. The default is opaque, because a
        /// screen that forgot to say draws over nothing and looks like a bug in the screen below.
        [[nodiscard]] virtual bool IsTranslucent() const
        {
            return false;
        }

        /// @brief Whether the world stops simulating while this screen is anywhere in the stack.
        ///
        /// **Default false, and that is §67.3's decision, not an omission.** The pause menu does not
        /// freeze the world -- the clock keeps running and the backdrop is a live frame -- because a
        /// house that stops when you look away is less convincing. A settings option pauses it for
        /// players who prefer that, which is why this is a per-screen question at all.
        [[nodiscard]] virtual bool PausesWorld() const
        {
            return false;
        }
    };

    /// @brief The screen stack. Bottom is the oldest; only the top updates.
    ///
    /// **Only the top screen updates, but more than one may draw.** That asymmetry is the whole
    /// design: a `Confirm` over a `PauseMenu` must show the menu behind it and must be the only
    /// thing that hears the keyboard, or `Escape` closes both at once.
    class MenuStack
    {
    public:
        void Push(std::unique_ptr<IScreen> screen);

        /// @brief Closes the top screen. Doing this on an empty stack is a no-op, not a crash.
        void Pop();

        /// @brief Closes every screen.
        void Clear();

        /// @brief Replaces the whole stack with @p screen. The loading screen handing over to play.
        void Replace(std::unique_ptr<IScreen> screen);

        [[nodiscard]] bool Empty() const noexcept
        {
            return screens_.empty();
        }

        [[nodiscard]] std::size_t Depth() const noexcept
        {
            return screens_.size();
        }

        [[nodiscard]] const IScreen* Top() const noexcept;

        /// @brief Updates the top screen and applies whatever it asked for.
        /// @return true if the session should end.
        bool Update(const player::InputState& input, float deltaSeconds);

        /// @brief Draws from the deepest screen that is actually visible, upward.
        void Draw(Microsoft::Xna::Framework::Graphics::SpriteBatch& batch, const TextRenderer& text) const;

        /// @brief Whether any screen in the stack wants the world stopped.
        [[nodiscard]] bool WorldIsPaused() const noexcept;

        /// @brief Whether the player is looking at a menu rather than at the house.
        ///
        /// What the mouse capture and the crosshair key off: a non-empty stack means the pointer
        /// belongs to the UI.
        [[nodiscard]] bool CapturesInput() const noexcept
        {
            return !screens_.empty();
        }

    private:
        /// @brief The index of the deepest screen that must be drawn.
        [[nodiscard]] std::size_t FirstVisible() const noexcept;

        std::vector<std::unique_ptr<IScreen>> screens_;
    };

    /// @brief The controls populated by HOUSE-02516 on the one retained settings page.
    enum class SettingsControl : std::uint8_t
    {
        Master,
        Footsteps,
        Ambience,
        Weather,
        LookSensitivity,
        InvertY,
        WalkSpeed,
        Count,
    };

    [[nodiscard]] std::string_view SettingsControlName(SettingsControl control) noexcept;

    /// @brief One settings page, using the existing screen stack and device-independent input.
    ///
    /// Graphics and Environment deliberately have headings only here: their exact rows belong to
    /// HOUSE-02518 and HOUSE-02521. Audio and Controls are complete and applied through @p onChanged
    /// after every edit; the screen neither owns a second settings copy nor invents a widget system.
    class SettingsScreen final : public IScreen
    {
    public:
        using Changed = std::function<void()>;

        explicit SettingsScreen(app::Settings& settings, Changed onChanged = {});

        [[nodiscard]] ScreenId Id() const override
        {
            return ScreenId::SettingsMenu;
        }

        ScreenAction Update(const player::InputState& input, float deltaSeconds) override;

        void Draw(Microsoft::Xna::Framework::Graphics::SpriteBatch& batch,
                  const TextRenderer& text) const override;

        [[nodiscard]] SettingsControl Selected() const noexcept
        {
            return selected_;
        }

        static constexpr std::size_t ControlCount() noexcept
        {
            return static_cast<std::size_t>(SettingsControl::Count);
        }

    private:
        [[nodiscard]] bool ChangeSelected(int direction);
        [[nodiscard]] bool ActivateSelected();
        [[nodiscard]] bool SelectPointer(float x, float y);
        [[nodiscard]] std::string ValueText(SettingsControl control) const;

        app::Settings* settings_;
        Changed onChanged_;
        SettingsControl selected_ = SettingsControl::Master;
    };

} // namespace cnahouse::ui
