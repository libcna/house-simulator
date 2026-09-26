// SPDX-License-Identifier: MIT
#include "cnahouse/ui/MenuStack.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <format>

#include "Microsoft/Xna/Framework/Color.hpp"
#include "Microsoft/Xna/Framework/Vector2.hpp"

#include "cnahouse/player/FirstPersonCamera.hpp"
#include "cnahouse/player/IInputSource.hpp"
#include "cnahouse/rendering/RenderTier.hpp"
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

        constexpr std::array<float, SettingsScreen::ControlCount()> kSettingsRowY = {0.111F,
                                                                                     0.149F,
                                                                                     0.187F,
                                                                                     0.224F,
                                                                                     0.262F,
                                                                                     0.344F,
                                                                                     0.382F,
                                                                                     0.420F,
                                                                                     0.458F,
                                                                                     0.540F,
                                                                                     0.578F,
                                                                                     0.616F,
                                                                                     0.698F,
                                                                                     0.736F,
                                                                                     0.774F};
        constexpr float kSettingsHitHalfHeight = 0.017F;
        constexpr float kSettingsSliderStart = 0.55F;
        constexpr float kSettingsSliderEnd = 0.80F;
        constexpr std::array kVisibleQualityPresets = {
            app::QualityPreset::Low, app::QualityPreset::Medium, app::QualityPreset::High};
        constexpr std::array kFixedHours = {-1.0F, 6.0F, 12.0F, 18.0F, 22.0F};
        constexpr std::array kTimeSpeeds = {24.0F, 48.0F, 96.0F};
        constexpr std::array<std::string_view, 3> kFixedWeather = {"W_CLEAR", "W_OVERCAST", "W_RAIN"};

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

        [[nodiscard]] std::size_t QualityIndex(app::QualityPreset preset) noexcept
        {
            if (preset == app::QualityPreset::Low)
            {
                return 0;
            }
            if (preset == app::QualityPreset::Medium)
            {
                return 1;
            }
            return 2;
        }

        template<typename T, std::size_t Size>
        [[nodiscard]] std::size_t ChoiceIndex(const std::array<T, Size>& choices, const T& value) noexcept
        {
            const auto found = std::find(choices.begin(), choices.end(), value);
            return found == choices.end() ? 0U : static_cast<std::size_t>(found - choices.begin());
        }

        constexpr std::array<std::string_view, 4> kMainMenuItems = {"Start", "Settings", "Credits", "Quit"};
        constexpr std::array<std::string_view, 4> kPauseMenuItems = {
            "Resume", "Settings", "Main menu", "Quit"};
        constexpr std::array<float, 4> kMenuRowY = {0.40F, 0.48F, 0.56F, 0.64F};

        [[nodiscard]] ControlScheme SchemeFor(const player::InputState& input) noexcept
        {
            return input.pointerPressed && input.pointerKind == player::PointerKind::Touch
                       ? ControlScheme::Touch
                       : ControlScheme::KeyboardMouse;
        }

        [[nodiscard]] bool SelectMenuPointer(const player::InputState& input, std::size_t& selected)
        {
            if (!input.pointerPressed || input.pointerX < 0.30F || input.pointerX > 0.70F)
            {
                return false;
            }
            for (std::size_t i = 0; i < kMenuRowY.size(); ++i)
            {
                if (std::abs(input.pointerY - kMenuRowY[i]) <= 0.035F)
                {
                    selected = i;
                    return true;
                }
            }
            return false;
        }

        template<std::size_t Size>
        void DrawMenu(Microsoft::Xna::Framework::Graphics::SpriteBatch& batch,
                      const TextRenderer& text,
                      std::string_view title,
                      const std::array<std::string_view, Size>& items,
                      std::size_t selected)
        {
            text.DrawShadowed(batch, title, Vector2(0.0F, 220.0F), Anchor::TopCentre, Color::White);
            for (std::size_t i = 0; i < items.size(); ++i)
            {
                const std::string line = std::format("{}{}", i == selected ? "> " : "  ", items[i]);
                const int shade = i == selected ? 255 : 210;
                text.DrawShadowed(batch,
                                  line,
                                  Vector2(0.0F, kMenuRowY[i] * TextRenderer::kVirtualHeight),
                                  Anchor::TopCentre,
                                  Color(shade, shade, shade, 255));
            }
        }
    } // namespace

    MainMenuScreen::MainMenuScreen(MenuRequested requested)
        : requested_(std::move(requested))
    {
    }

    ScreenAction MainMenuScreen::Update(const player::InputState& input, float deltaSeconds)
    {
        (void)deltaSeconds;
        if (input.uiUpPressed)
        {
            selected_ = (selected_ + kMainMenuItems.size() - 1U) % kMainMenuItems.size();
        }
        if (input.uiDownPressed)
        {
            selected_ = (selected_ + 1U) % kMainMenuItems.size();
        }
        const bool pointerSelected = SelectMenuPointer(input, selected_);
        if (!input.uiAcceptPressed && !pointerSelected)
        {
            return ScreenAction::None;
        }
        if (selected_ == 3U)
        {
            return ScreenAction::Quit;
        }
        if (requested_)
        {
            constexpr std::array commands = {MenuCommand::Start, MenuCommand::Settings, MenuCommand::Credits};
            requested_(commands[selected_], SchemeFor(input));
        }
        return ScreenAction::None;
    }

    void MainMenuScreen::Draw(Microsoft::Xna::Framework::Graphics::SpriteBatch& batch,
                              const TextRenderer& text) const
    {
        DrawMenu(batch, text, "House Simulator", kMainMenuItems, selected_);
    }

    PauseMenuScreen::PauseMenuScreen(MenuRequested requested)
        : requested_(std::move(requested))
    {
    }

    ScreenAction PauseMenuScreen::Update(const player::InputState& input, float deltaSeconds)
    {
        (void)deltaSeconds;
        if (input.cancelPressed)
        {
            return ScreenAction::PopAll;
        }
        if (input.uiUpPressed)
        {
            selected_ = (selected_ + kPauseMenuItems.size() - 1U) % kPauseMenuItems.size();
        }
        if (input.uiDownPressed)
        {
            selected_ = (selected_ + 1U) % kPauseMenuItems.size();
        }
        const bool pointerSelected = SelectMenuPointer(input, selected_);
        if (!input.uiAcceptPressed && !pointerSelected)
        {
            return ScreenAction::None;
        }
        if (selected_ == 0U)
        {
            return ScreenAction::PopAll;
        }
        if (selected_ == 3U)
        {
            return ScreenAction::Quit;
        }
        if (requested_)
        {
            requested_(selected_ == 1U ? MenuCommand::Settings : MenuCommand::MainMenu, SchemeFor(input));
        }
        return ScreenAction::None;
    }

    void PauseMenuScreen::Draw(Microsoft::Xna::Framework::Graphics::SpriteBatch& batch,
                               const TextRenderer& text) const
    {
        DrawMenu(batch, text, "Paused", kPauseMenuItems, selected_);
    }

    CreditsScreen::CreditsScreen(std::string document)
    {
        std::size_t begin = 0;
        while (begin <= document.size())
        {
            const std::size_t end = document.find('\n', begin);
            lines_.push_back(document.substr(begin, end == std::string::npos ? end : end - begin));
            if (end == std::string::npos)
            {
                break;
            }
            begin = end + 1U;
        }
    }

    ScreenAction CreditsScreen::Update(const player::InputState& input, float deltaSeconds)
    {
        (void)deltaSeconds;
        if (input.cancelPressed || (input.pointerPressed && input.pointerY >= 0.84F))
        {
            return ScreenAction::Pop;
        }
        constexpr std::size_t page = 25U;
        if (input.uiUpPressed || (input.pointerPressed && input.pointerY < 0.5F))
        {
            scroll_ = scroll_ == 0U ? 0U : scroll_ - 1U;
        }
        if (input.uiDownPressed || (input.pointerPressed && input.pointerY >= 0.5F))
        {
            const std::size_t maximum = lines_.size() > page ? lines_.size() - page : 0U;
            scroll_ = std::min(scroll_ + 1U, maximum);
        }
        return ScreenAction::None;
    }

    void CreditsScreen::Draw(Microsoft::Xna::Framework::Graphics::SpriteBatch& batch,
                             const TextRenderer& text) const
    {
        text.DrawShadowed(batch, "Credits", Vector2(0.0F, 45.0F), Anchor::TopCentre, Color::White);
        constexpr std::size_t page = 25U;
        const std::size_t end = std::min(lines_.size(), scroll_ + page);
        for (std::size_t i = scroll_; i < end; ++i)
        {
            text.DrawShadowed(batch,
                              lines_[i],
                              Vector2(60.0F, 90.0F + static_cast<float>(i - scroll_) * 28.0F),
                              Anchor::TopLeft,
                              Color(225, 225, 225, 255));
        }
        text.DrawShadowed(batch,
                          "Up/Down scroll  Esc back  Tap bottom to back",
                          Vector2(0.0F, 835.0F),
                          Anchor::TopCentre,
                          Color::White);
    }

    void ControlsHint::Start(ControlScheme scheme) noexcept
    {
        scheme_ = scheme;
        elapsed_ = 0.0F;
        started_ = true;
    }

    void ControlsHint::Update(float deltaSeconds) noexcept
    {
        if (started_ && std::isfinite(deltaSeconds) && deltaSeconds > 0.0F)
        {
            elapsed_ = std::min(kDurationSeconds, elapsed_ + deltaSeconds);
        }
    }

    float ControlsHint::Alpha() const noexcept
    {
        if (!Visible())
        {
            return 0.0F;
        }
        return std::clamp((kDurationSeconds - elapsed_) / kFadeSeconds, 0.0F, 1.0F);
    }

    std::string_view ControlsHint::Text() const noexcept
    {
        return scheme_ == ControlScheme::Touch ? "Touch: move stick · drag to look · pause button"
                                               : "WASD move · mouse look · Shift walk speed · Esc pause";
    }

    void ControlsHint::Draw(Microsoft::Xna::Framework::Graphics::SpriteBatch& batch,
                            const TextRenderer& text) const
    {
        const int alpha = static_cast<int>(Alpha() * 255.0F);
        if (alpha == 0)
        {
            return;
        }
        text.DrawShadowed(
            batch, Text(), Vector2(0.0F, 835.0F), Anchor::TopCentre, Color(alpha, alpha, alpha, alpha));
    }

    std::string_view SettingsControlName(SettingsControl control) noexcept
    {
        switch (control)
        {
            case SettingsControl::Quality:
                return "Quality preset";
            case SettingsControl::DisplaySize:
                return "Resolution";
            case SettingsControl::Fullscreen:
                return "Fullscreen";
            case SettingsControl::VerticalSync:
                return "V-sync";
            case SettingsControl::FieldOfView:
                return "Field of view";
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
            case SettingsControl::TimeOfDay:
                return "Time of day";
            case SettingsControl::TimeSpeed:
                return "Time speed";
            case SettingsControl::EnvironmentWeather:
                return "Weather";
            case SettingsControl::Count:
                break;
        }
        return "?";
    }

    SettingsFeatures SettingsFeatures::Resolve(const app::Platform& platform,
                                               const rendering::RenderTier& tier,
                                               const app::Settings& settings)
    {
        SettingsFeatures features;
        features.tierE = tier.IsTierE();
        features.canvasSize = platform.target == app::BuildTarget::Web;
        features.displaySize = platform.target != app::BuildTarget::Android;
        features.fullscreen = platform.target != app::BuildTarget::Android;
        features.verticalSync = platform.target == app::BuildTarget::Desktop;
        features.displaySizes.clear();

        if (features.canvasSize)
        {
            // Web has no meaningful monitor-mode list. Keep the two 16:9 canvas sizes that can
            // show the complete settings page with the project's existing 16-pixel UI font;
            // the browser-reported 800x600 mode compresses rows until labels overlap.
            features.displaySizes = {{1280, 720}, {1600, 900}};
            if (settings.backBufferWidth >= 1280 && settings.backBufferHeight >= 720 &&
                settings.backBufferWidth * 9 == settings.backBufferHeight * 16)
            {
                features.displaySizes.push_back({settings.backBufferWidth, settings.backBufferHeight});
            }
        }
        else if (features.displaySize)
        {
            for (const app::DisplaySize size : platform.displaySizes)
            {
                if (size.width >= 640 && size.width <= 7680 && size.height >= 480 && size.height <= 4320)
                {
                    features.displaySizes.push_back(size);
                }
            }
            features.displaySizes.push_back({settings.backBufferWidth, settings.backBufferHeight});
        }
        std::sort(features.displaySizes.begin(), features.displaySizes.end());
        features.displaySizes.erase(std::unique(features.displaySizes.begin(), features.displaySizes.end()),
                                    features.displaySizes.end());
        return features;
    }

    bool SettingsFeatures::Shows(SettingsControl control) const noexcept
    {
        switch (control)
        {
            case SettingsControl::DisplaySize:
                return displaySize && !displaySizes.empty();
            case SettingsControl::Fullscreen:
                return fullscreen;
            case SettingsControl::VerticalSync:
                return verticalSync;
            case SettingsControl::Quality:
            case SettingsControl::FieldOfView:
            case SettingsControl::Master:
            case SettingsControl::Footsteps:
            case SettingsControl::Ambience:
            case SettingsControl::Weather:
            case SettingsControl::LookSensitivity:
            case SettingsControl::InvertY:
            case SettingsControl::WalkSpeed:
            case SettingsControl::TimeOfDay:
            case SettingsControl::TimeSpeed:
            case SettingsControl::EnvironmentWeather:
                return true;
            case SettingsControl::Count:
                break;
        }
        return false;
    }

    SettingsScreen::SettingsScreen(app::Settings& settings, Changed onChanged, SettingsFeatures features)
        : settings_(&settings)
        , onChanged_(std::move(onChanged))
        , features_(std::move(features))
    {
        for (std::size_t i = 0; i < ControlCount(); ++i)
        {
            const auto control = static_cast<SettingsControl>(i);
            if (features_.Shows(control))
            {
                controls_.push_back(control);
            }
        }
        selected_ = controls_.front();
    }

    bool SettingsScreen::Shows(SettingsControl control) const noexcept
    {
        return std::find(controls_.begin(), controls_.end(), control) != controls_.end();
    }

    ScreenAction SettingsScreen::Update(const player::InputState& input, float deltaSeconds)
    {
        (void)deltaSeconds;
        if (input.cancelPressed)
        {
            return ScreenAction::Pop;
        }

        const auto count = static_cast<int>(controls_.size());
        auto selected = std::find(controls_.begin(), controls_.end(), selected_);
        int index = static_cast<int>(std::distance(controls_.begin(), selected));
        if (input.uiUpPressed)
        {
            index = (index + count - 1) % count;
            selected_ = controls_[static_cast<std::size_t>(index)];
        }
        if (input.uiDownPressed)
        {
            index = (index + 1) % count;
            selected_ = controls_[static_cast<std::size_t>(index)];
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
            onChanged_(selected_);
        }
        return ScreenAction::None;
    }

    bool SettingsScreen::ChangeSelected(int direction)
    {
        switch (selected_)
        {
            case SettingsControl::Quality:
            {
                const auto count = static_cast<int>(kVisibleQualityPresets.size());
                int index = static_cast<int>(QualityIndex(settings_->quality));
                index = (index + count + direction) % count;
                return AssignChanged(settings_->quality,
                                     kVisibleQualityPresets[static_cast<std::size_t>(index)]);
            }
            case SettingsControl::DisplaySize:
            {
                const app::DisplaySize current{settings_->backBufferWidth, settings_->backBufferHeight};
                auto found = std::find(features_.displaySizes.begin(), features_.displaySizes.end(), current);
                std::size_t index =
                    static_cast<std::size_t>(std::distance(features_.displaySizes.begin(), found));
                const auto count = static_cast<int>(features_.displaySizes.size());
                index = static_cast<std::size_t>((static_cast<int>(index) + count + direction) % count);
                const app::DisplaySize changed = features_.displaySizes[index];
                const bool widthChanged = AssignChanged(settings_->backBufferWidth, changed.width);
                const bool heightChanged = AssignChanged(settings_->backBufferHeight, changed.height);
                return widthChanged || heightChanged;
            }
            case SettingsControl::Fullscreen:
                return AssignChanged(settings_->fullscreen, direction > 0);
            case SettingsControl::VerticalSync:
                return AssignChanged(settings_->verticalSync, direction > 0);
            case SettingsControl::FieldOfView:
                return AssignChanged(settings_->fieldOfView,
                                     StepSetting(settings_->fieldOfView,
                                                 5.0F,
                                                 direction,
                                                 player::kMinFovDegrees,
                                                 player::kMaxFovDegrees));
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
            case SettingsControl::TimeOfDay:
            {
                const auto count = static_cast<int>(kFixedHours.size());
                int index = static_cast<int>(ChoiceIndex(kFixedHours, settings_->fixedTimeOfDayHours));
                index = (index + count + direction) % count;
                return AssignChanged(settings_->fixedTimeOfDayHours,
                                     kFixedHours[static_cast<std::size_t>(index)]);
            }
            case SettingsControl::TimeSpeed:
            {
                const auto count = static_cast<int>(kTimeSpeeds.size());
                int index = static_cast<int>(ChoiceIndex(kTimeSpeeds, settings_->dayLengthRealMinutes));
                index = (index + count + direction) % count;
                return AssignChanged(settings_->dayLengthRealMinutes,
                                     kTimeSpeeds[static_cast<std::size_t>(index)]);
            }
            case SettingsControl::EnvironmentWeather:
            {
                int index = 0;
                if (settings_->weatherMode == app::WeatherMode::Fixed)
                {
                    index = 1 + static_cast<int>(ChoiceIndex(
                                    kFixedWeather, std::string_view{settings_->fixedWeatherArchetype}));
                }
                constexpr int count = 4;
                index = (index + count + direction) % count;
                if (index == 0)
                {
                    return AssignChanged(settings_->weatherMode, app::WeatherMode::On);
                }
                const bool modeChanged = AssignChanged(settings_->weatherMode, app::WeatherMode::Fixed);
                const bool stateChanged =
                    AssignChanged(settings_->fixedWeatherArchetype,
                                  std::string{kFixedWeather[static_cast<std::size_t>(index - 1)]});
                return modeChanged || stateChanged;
            }
            case SettingsControl::Count:
                break;
        }
        return false;
    }

    bool SettingsScreen::ActivateSelected()
    {
        if (selected_ == SettingsControl::Fullscreen)
        {
            settings_->fullscreen = !settings_->fullscreen;
            return true;
        }
        if (selected_ == SettingsControl::VerticalSync)
        {
            settings_->verticalSync = !settings_->verticalSync;
            return true;
        }
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

        for (const SettingsControl control : controls_)
        {
            if (std::abs(y - kSettingsRowY[SettingsIndex(control)]) > kSettingsHitHalfHeight)
            {
                continue;
            }
            selected_ = control;
            const bool volumeControl =
                control == SettingsControl::Master || control == SettingsControl::Footsteps ||
                control == SettingsControl::Ambience || control == SettingsControl::Weather;
            if (volumeControl || control == SettingsControl::LookSensitivity ||
                control == SettingsControl::FieldOfView)
            {
                const float fraction = std::clamp(
                    (x - kSettingsSliderStart) / (kSettingsSliderEnd - kSettingsSliderStart), 0.0F, 1.0F);
                if (control == SettingsControl::FieldOfView)
                {
                    return AssignChanged(settings_->fieldOfView,
                                         player::kMinFovDegrees +
                                             fraction * (player::kMaxFovDegrees - player::kMinFovDegrees));
                }
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
            case SettingsControl::Quality:
                if (settings_->quality == app::QualityPreset::Low)
                {
                    return "Android";
                }
                if (settings_->quality == app::QualityPreset::Medium)
                {
                    return "Web";
                }
                return features_.tierE ? "High" : "High (Tier S)";
            case SettingsControl::DisplaySize:
                return std::format("{} x {}", settings_->backBufferWidth, settings_->backBufferHeight);
            case SettingsControl::Fullscreen:
                return settings_->fullscreen ? "On" : "Off";
            case SettingsControl::VerticalSync:
                return settings_->verticalSync ? "On" : "Off";
            case SettingsControl::FieldOfView:
                return std::format("{:.0f} deg", static_cast<double>(settings_->fieldOfView));
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
            case SettingsControl::TimeOfDay:
                return settings_->fixedTimeOfDayHours < 0.0F
                           ? "Automatic"
                           : std::format("{:02.0f}:00", static_cast<double>(settings_->fixedTimeOfDayHours));
            case SettingsControl::TimeSpeed:
                return std::format("{:.0f}x", 1440.0 / static_cast<double>(settings_->dayLengthRealMinutes));
            case SettingsControl::EnvironmentWeather:
                if (settings_->weatherMode != app::WeatherMode::Fixed)
                {
                    return "Automatic";
                }
                if (settings_->fixedWeatherArchetype == "W_OVERCAST")
                {
                    return "Overcast";
                }
                if (settings_->fixedWeatherArchetype == "W_RAIN")
                {
                    return "Rain";
                }
                return "Clear";
            case SettingsControl::Count:
                break;
        }
        return "";
    }

    void SettingsScreen::Draw(Microsoft::Xna::Framework::Graphics::SpriteBatch& batch,
                              const TextRenderer& text) const
    {
        text.DrawShadowed(batch, "Settings", Vector2(0.0F, 30.0F), Anchor::TopCentre, Color::White);
        text.DrawShadowed(batch, "Graphics", Vector2(0.0F, 72.0F), Anchor::TopCentre, Color::White);
        text.DrawShadowed(batch, "Audio", Vector2(0.0F, 280.0F), Anchor::TopCentre, Color::White);
        text.DrawShadowed(batch, "Controls", Vector2(0.0F, 456.0F), Anchor::TopCentre, Color::White);
        text.DrawShadowed(batch, "Environment", Vector2(0.0F, 598.0F), Anchor::TopCentre, Color::White);

        for (const SettingsControl control : controls_)
        {
            const bool selected = control == selected_;
            const std::string_view name = control == SettingsControl::DisplaySize && features_.canvasSize
                                              ? std::string_view{"Canvas size"}
                                              : SettingsControlName(control);
            const std::string line =
                std::format("{}{:18}  {}", selected ? "> " : "  ", name, ValueText(control));
            const int shade = selected ? 255 : 210;
            text.DrawShadowed(
                batch,
                line,
                Vector2(0.0F, kSettingsRowY[SettingsIndex(control)] * TextRenderer::kVirtualHeight),
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
