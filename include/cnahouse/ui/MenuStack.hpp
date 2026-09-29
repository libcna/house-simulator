// SPDX-License-Identifier: MIT
#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "cnahouse/app/Platform.hpp"
#include "cnahouse/app/Settings.hpp"

namespace cnahouse::rendering
{
    class RenderTier;
}

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

    enum class MenuCommand : std::uint8_t
    {
        None,
        Start,
        Settings,
        Credits,
        MainMenu,
    };

    enum class ControlScheme : std::uint8_t
    {
        KeyboardMouse,
        Touch,
        KeyboardMouseFilming,
    };

    using MenuRequested = std::function<void(MenuCommand, ControlScheme)>;

    class MainMenuScreen final : public IScreen
    {
    public:
        explicit MainMenuScreen(MenuRequested requested, bool touchLayout = false);

        [[nodiscard]] ScreenId Id() const override
        {
            return ScreenId::MainMenu;
        }

        ScreenAction Update(const player::InputState& input, float deltaSeconds) override;
        void Draw(Microsoft::Xna::Framework::Graphics::SpriteBatch& batch,
                  const TextRenderer& text) const override;

        [[nodiscard]] std::size_t SelectedIndex() const noexcept
        {
            return selected_;
        }

        [[nodiscard]] bool PausesWorld() const override
        {
            return true;
        }

    private:
        MenuRequested requested_;
        std::size_t selected_ = 0;
        bool touchLayout_ = false;
    };

    class PauseMenuScreen final : public IScreen
    {
    public:
        explicit PauseMenuScreen(MenuRequested requested, bool touchLayout = false);

        [[nodiscard]] ScreenId Id() const override
        {
            return ScreenId::PauseMenu;
        }

        ScreenAction Update(const player::InputState& input, float deltaSeconds) override;
        void Draw(Microsoft::Xna::Framework::Graphics::SpriteBatch& batch,
                  const TextRenderer& text) const override;

        [[nodiscard]] std::size_t SelectedIndex() const noexcept
        {
            return selected_;
        }

    private:
        MenuRequested requested_;
        std::size_t selected_ = 0;
        bool touchLayout_ = false;
    };

    /// @brief Scrollable view of the generated third-party asset document.
    class CreditsScreen final : public IScreen
    {
    public:
        explicit CreditsScreen(std::string document);

        [[nodiscard]] ScreenId Id() const override
        {
            return ScreenId::Credits;
        }

        ScreenAction Update(const player::InputState& input, float deltaSeconds) override;
        void Draw(Microsoft::Xna::Framework::Graphics::SpriteBatch& batch,
                  const TextRenderer& text) const override;

        [[nodiscard]] std::size_t LineCount() const noexcept
        {
            return lines_.size();
        }

        [[nodiscard]] std::size_t ScrollOffset() const noexcept
        {
            return scroll_;
        }

    private:
        std::vector<std::string> lines_;
        std::size_t scroll_ = 0;
    };

    /// @brief The one first-run line retained by the reduced roadmap.
    class ControlsHint
    {
    public:
        static constexpr float kDurationSeconds = 12.0F;
        static constexpr float kFadeSeconds = 3.0F;

        void Start(ControlScheme scheme) noexcept;
        void Update(float deltaSeconds) noexcept;
        void Draw(Microsoft::Xna::Framework::Graphics::SpriteBatch& batch, const TextRenderer& text) const;

        [[nodiscard]] bool Visible() const noexcept
        {
            return started_ && elapsed_ < kDurationSeconds;
        }

        [[nodiscard]] float Alpha() const noexcept;
        [[nodiscard]] std::string_view Text() const noexcept;

    private:
        ControlScheme scheme_ = ControlScheme::KeyboardMouse;
        float elapsed_ = 0.0F;
        bool started_ = false;
    };

    /// @brief The controls populated by M9 on the one retained settings page.
    enum class SettingsControl : std::uint8_t
    {
        Quality,
        DisplaySize,
        Fullscreen,
        VerticalSync,
        FieldOfView,
        Master,
        Footsteps,
        Ambience,
        Weather,
        LookSensitivity,
        InvertY,
        TimeOfDay,
        TimeSpeed,
        EnvironmentWeather,
        Count,
    };

    [[nodiscard]] std::string_view SettingsControlName(SettingsControl control) noexcept;

    /// @brief The project-owned effective feature set as the settings page needs it.
    ///
    /// It contains build/profile facts and standard-XNA display sizes only. The UI therefore
    /// filters rows without asking CNA or the graphics device what it supports.
    struct SettingsFeatures
    {
        bool canvasSize = false;
        bool displaySize = true;
        bool fullscreen = true;
        bool verticalSync = true;
        bool tierE = false;
        /// Look sensitivity is the touch drag's, not the mouse's: the only pointer is a finger.
        bool touchLook = false;
        std::vector<app::DisplaySize> displaySizes{{1600, 900}};

        [[nodiscard]] static SettingsFeatures Resolve(const app::Platform& platform,
                                                      const rendering::RenderTier& tier,
                                                      const app::Settings& settings);
        [[nodiscard]] bool Shows(SettingsControl control) const noexcept;
    };

    /// @brief One settings page, using the existing screen stack and device-independent input.
    ///
    /// Every row is applied through @p onChanged after an edit; the screen neither owns a second
    /// settings copy nor invents a widget system.
    class SettingsScreen final : public IScreen
    {
    public:
        using Changed = std::function<void(SettingsControl)>;

        explicit SettingsScreen(app::Settings& settings,
                                Changed onChanged = {},
                                SettingsFeatures features = {});

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

        [[nodiscard]] std::size_t VisibleControlCount() const noexcept
        {
            return controls_.size();
        }

        [[nodiscard]] bool Shows(SettingsControl control) const noexcept;

        static constexpr std::size_t ControlCount() noexcept
        {
            return static_cast<std::size_t>(SettingsControl::Count);
        }

    private:
        [[nodiscard]] bool ChangeSelected(int direction);
        [[nodiscard]] bool ActivateSelected();
        [[nodiscard]] bool SelectPointer(float x, float y);
        [[nodiscard]] std::string ValueText(SettingsControl control) const;
        [[nodiscard]] float& LookSensitivity() const noexcept;

        app::Settings* settings_;
        Changed onChanged_;
        SettingsFeatures features_;
        std::vector<SettingsControl> controls_;
        SettingsControl selected_ = SettingsControl::Quality;
    };

} // namespace cnahouse::ui
