// SPDX-License-Identifier: MIT
//
// `HOUSE-00156`. Everything the stack decides -- who updates, who draws, whether the world runs --
// is decided without a `GraphicsDevice`, so it is tested without one. `Draw` is the single method
// that needs a batch, and it is exercised for real by the integration tests instead of being mocked.
#include <array>
#include <string>
#include <string_view>
#include <vector>

#include <gtest/gtest.h>

#include "cnahouse/app/Settings.hpp"
#include "cnahouse/audio/AudioSystem.hpp"
#include "cnahouse/player/IInputSource.hpp"
#include "cnahouse/rendering/RenderTier.hpp"
#include "cnahouse/ui/LoadingScreen.hpp"
#include "cnahouse/ui/MenuStack.hpp"

namespace
{
    using cnahouse::app::Settings;
    using cnahouse::app::WeatherMode;
    using cnahouse::audio::AudioSystem;
    using cnahouse::audio::Category;
    using cnahouse::player::InputState;
    using cnahouse::player::PointerKind;
    using cnahouse::ui::ControlScheme;
    using cnahouse::ui::ControlsHint;
    using cnahouse::ui::CreditsScreen;
    using cnahouse::ui::IScreen;
    using cnahouse::ui::LoadingScreen;
    using cnahouse::ui::MainMenuScreen;
    using cnahouse::ui::MenuCommand;
    using cnahouse::ui::MenuStack;
    using cnahouse::ui::PauseMenuScreen;
    using cnahouse::ui::ScreenAction;
    using cnahouse::ui::ScreenId;
    using cnahouse::ui::SettingsControl;
    using cnahouse::ui::SettingsControlName;
    using cnahouse::ui::SettingsFeatures;
    using cnahouse::ui::SettingsScreen;

    /// Records that it was updated, and answers whatever the test told it to.
    class FakeScreen final : public IScreen
    {
    public:
        FakeScreen(ScreenId id, std::vector<ScreenId>& updates)
            : id_(id)
            , updates_(&updates)
        {
        }

        [[nodiscard]] ScreenId Id() const override
        {
            return id_;
        }

        ScreenAction Update(const InputState&, float) override
        {
            updates_->push_back(id_);
            return action_;
        }

        void Draw(Microsoft::Xna::Framework::Graphics::SpriteBatch&,
                  const cnahouse::ui::TextRenderer&) const override
        {
        }

        [[nodiscard]] bool IsTranslucent() const override
        {
            return translucent_;
        }

        [[nodiscard]] bool PausesWorld() const override
        {
            return pauses_;
        }

        void SetAction(ScreenAction action) noexcept
        {
            action_ = action;
        }

        void SetTranslucent(bool translucent) noexcept
        {
            translucent_ = translucent;
        }

        void SetPauses(bool pauses) noexcept
        {
            pauses_ = pauses;
        }

    private:
        ScreenId id_;
        std::vector<ScreenId>* updates_;
        ScreenAction action_ = ScreenAction::None;
        bool translucent_ = false;
        bool pauses_ = false;
    };

    TEST(MenuStackTests, AnEmptyStackUpdatesNothingAndDoesNotCaptureInput)
    {
        MenuStack stack;
        std::vector<ScreenId> updates;
        EXPECT_TRUE(stack.Empty());
        EXPECT_EQ(stack.Top(), nullptr);
        EXPECT_FALSE(stack.CapturesInput());
        EXPECT_FALSE(stack.WorldIsPaused());
        EXPECT_FALSE(stack.Update(InputState{}, 1.0f / 60.0f));
        EXPECT_TRUE(updates.empty());
    }

    TEST(MenuStackTests, PoppingAnEmptyStackIsANoOpAndNotACrash)
    {
        // `Escape` arriving one frame after a screen closed itself is an ordinary race between input
        // and a transition, not a programming error.
        MenuStack stack;
        stack.Pop();
        stack.Pop();
        EXPECT_TRUE(stack.Empty());
    }

    TEST(MenuStackTests, OnlyTheTopScreenIsUpdated)
    {
        // The case this protects: a `Confirm` over a `PauseMenu`. If both heard the keyboard,
        // `Escape` would close both at once, which reads as the confirmation having been ANSWERED
        // when it was only dismissed.
        MenuStack stack;
        std::vector<ScreenId> updates;
        stack.Push(std::make_unique<FakeScreen>(ScreenId::PauseMenu, updates));
        stack.Push(std::make_unique<FakeScreen>(ScreenId::Confirm, updates));

        stack.Update(InputState{}, 1.0f / 60.0f);

        EXPECT_EQ(updates, std::vector<ScreenId>{ScreenId::Confirm});
        EXPECT_EQ(stack.Depth(), 2u);
        ASSERT_NE(stack.Top(), nullptr);
        EXPECT_EQ(stack.Top()->Id(), ScreenId::Confirm);
    }

    TEST(MenuStackTests, PopClosesOneScreenAndPopAllClosesEveryOne)
    {
        MenuStack stack;
        std::vector<ScreenId> updates;
        stack.Push(std::make_unique<FakeScreen>(ScreenId::MainMenu, updates));
        auto settingsOwned = std::make_unique<FakeScreen>(ScreenId::SettingsMenu, updates);
        FakeScreen* settings = settingsOwned.get();
        stack.Push(std::move(settingsOwned));

        settings->SetAction(ScreenAction::Pop);
        EXPECT_FALSE(stack.Update(InputState{}, 0.016f));
        EXPECT_EQ(stack.Depth(), 1u);
        ASSERT_NE(stack.Top(), nullptr);
        EXPECT_EQ(stack.Top()->Id(), ScreenId::MainMenu);

        auto again = std::make_unique<FakeScreen>(ScreenId::Confirm, updates);
        again->SetAction(ScreenAction::PopAll);
        stack.Push(std::move(again));
        EXPECT_FALSE(stack.Update(InputState{}, 0.016f));
        EXPECT_TRUE(stack.Empty()) << "returning to play from three levels of menu is one action";
    }

    TEST(MenuStackTests, QuitIsReportedRatherThanActedOn)
    {
        // The stack does not call `Exit()`. Ending the session is the `Game`'s business, and a UI
        // widget that could stop the process is one that can stop it by accident.
        MenuStack stack;
        std::vector<ScreenId> updates;
        auto owned = std::make_unique<FakeScreen>(ScreenId::MainMenu, updates);
        owned->SetAction(ScreenAction::Quit);
        stack.Push(std::move(owned));

        EXPECT_TRUE(stack.Update(InputState{}, 0.016f));
        EXPECT_EQ(stack.Depth(), 1u) << "quitting does not first tidy the stack";
    }

    TEST(MenuStackTests, ReplaceClearsEverythingBelow)
    {
        MenuStack stack;
        std::vector<ScreenId> updates;
        stack.Push(std::make_unique<FakeScreen>(ScreenId::MainMenu, updates));
        stack.Push(std::make_unique<FakeScreen>(ScreenId::SettingsMenu, updates));
        stack.Replace(std::make_unique<FakeScreen>(ScreenId::Credits, updates));

        EXPECT_EQ(stack.Depth(), 1u);
        ASSERT_NE(stack.Top(), nullptr);
        EXPECT_EQ(stack.Top()->Id(), ScreenId::Credits);
    }

    TEST(MenuStackTests, AnyScreenInTheStackCanPauseTheWorldNotJustTheTop)
    {
        // A `Confirm` is translucent and does not itself pause, but the `PauseMenu` under it may --
        // and the world must not resume just because a dialogue opened on top of it.
        MenuStack stack;
        std::vector<ScreenId> updates;
        auto pauseOwned = std::make_unique<FakeScreen>(ScreenId::PauseMenu, updates);
        pauseOwned->SetPauses(true);
        stack.Push(std::move(pauseOwned));
        stack.Push(std::make_unique<FakeScreen>(ScreenId::Confirm, updates));

        EXPECT_TRUE(stack.WorldIsPaused());
        stack.Clear();
        EXPECT_FALSE(stack.WorldIsPaused());
    }

    TEST(MenuStackTests, TheDefaultIsThatTheWorldKeepsRunning)
    {
        // §67.3's decision, not an omission: the pause menu does not freeze the world, because a
        // house that stops when you look away is less convincing.
        MenuStack stack;
        std::vector<ScreenId> updates;
        stack.Push(std::make_unique<FakeScreen>(ScreenId::PauseMenu, updates));
        EXPECT_FALSE(stack.WorldIsPaused());
        EXPECT_TRUE(stack.CapturesInput()) << "but the pointer still belongs to the UI";
    }

    TEST(MenuStackTests, PushingNullptrIsIgnoredRatherThanStoredAndDereferenced)
    {
        MenuStack stack;
        stack.Push(nullptr);
        EXPECT_TRUE(stack.Empty());
        EXPECT_FALSE(stack.Update(InputState{}, 0.016f));
    }

    // --- the loading screen ------------------------------------------------------------------------

    TEST(LoadingScreenTests, ItWaitsForBothTheGestureAndTheContent)
    {
        int gestures = 0;
        LoadingScreen screen("cna-house test", [&] { ++gestures; });

        InputState quiet;
        EXPECT_EQ(screen.Update(quiet, 0.016f), cnahouse::ui::ScreenAction::None);
        EXPECT_EQ(gestures, 0);

        // A gesture with content still loading must NOT dismiss -- there would be nothing behind it.
        InputState pressed;
        pressed.anyPressed = true;
        EXPECT_EQ(screen.Update(pressed, 0.016f), cnahouse::ui::ScreenAction::None);
        EXPECT_TRUE(screen.GestureSeen());
        EXPECT_EQ(gestures, 1);

        // Content ready but no further press: the gesture already happened, so it goes.
        screen.SetReady(true);
        EXPECT_EQ(screen.Update(quiet, 0.016f), cnahouse::ui::ScreenAction::Pop);
    }

    TEST(LoadingScreenTests, ContentReadyAloneDoesNotDismissIt)
    {
        // If it did, the audio gate would never be satisfied on the Web build -- the screen would
        // vanish before the player had touched anything and the device could never open.
        int gestures = 0;
        LoadingScreen screen("cna-house test", [&] { ++gestures; });
        screen.SetReady(true);

        for (int frame = 0; frame < 60; ++frame)
        {
            EXPECT_EQ(screen.Update(InputState{}, 0.016f), cnahouse::ui::ScreenAction::None);
        }
        EXPECT_EQ(gestures, 0);
    }

    TEST(LoadingScreenTests, TheGestureCallbackFiresExactlyOnce)
    {
        // It opens the audio device. Firing again would re-probe the mixer every frame the player
        // held a key down.
        int gestures = 0;
        LoadingScreen screen("cna-house test", [&] { ++gestures; });

        InputState pressed;
        pressed.anyPressed = true;
        for (int frame = 0; frame < 10; ++frame)
        {
            screen.Update(pressed, 0.016f);
        }
        EXPECT_EQ(gestures, 1);
    }

    TEST(LoadingScreenTests, ItPausesTheWorldAndIsOpaque)
    {
        // Unlike the pause menu, there is nothing to keep running: the house has not been built yet.
        const LoadingScreen screen("cna-house test", nullptr);
        EXPECT_TRUE(screen.PausesWorld());
        EXPECT_FALSE(screen.IsTranslucent());
        EXPECT_EQ(screen.Id(), ScreenId::Loading);
    }

    TEST(LoadingScreenTests, ANullCallbackIsSafe)
    {
        LoadingScreen screen("cna-house test", nullptr);
        InputState pressed;
        pressed.anyPressed = true;
        EXPECT_NO_THROW(screen.Update(pressed, 0.016f));
        EXPECT_TRUE(screen.GestureSeen());
    }

    TEST(LoadingScreenTests, ElapsedAdvancesSoThePromptCanPulse)
    {
        LoadingScreen screen("cna-house test", nullptr);
        screen.Update(InputState{}, 0.5f);
        screen.Update(InputState{}, 0.25f);
        EXPECT_FLOAT_EQ(screen.Elapsed(), 0.75f);
    }

    // --- retained application menu flow ----------------------------------------------------------

    TEST(ApplicationMenuTests, MainMenuOffersStartSettingsCreditsAndQuitWithoutContinue)
    {
        std::vector<MenuCommand> requested;
        MainMenuScreen screen([&](MenuCommand command, ControlScheme) { requested.push_back(command); });
        InputState accept;
        accept.uiAcceptPressed = true;
        InputState down;
        down.uiDownPressed = true;

        EXPECT_EQ(screen.Update(accept, 0.016F), ScreenAction::None);
        ASSERT_EQ(requested, std::vector<MenuCommand>{MenuCommand::Start});
        screen.Update(down, 0.016F);
        screen.Update(accept, 0.016F);
        screen.Update(down, 0.016F);
        screen.Update(accept, 0.016F);
        EXPECT_EQ(
            requested,
            (std::vector<MenuCommand>{MenuCommand::Start, MenuCommand::Settings, MenuCommand::Credits}));
        screen.Update(down, 0.016F);
        EXPECT_EQ(screen.Update(accept, 0.016F), ScreenAction::Quit);
        EXPECT_TRUE(screen.PausesWorld());
    }

    TEST(ApplicationMenuTests, MainMenuPointerRecordsTheActiveTouchScheme)
    {
        MenuCommand command = MenuCommand::None;
        ControlScheme scheme = ControlScheme::KeyboardMouse;
        MainMenuScreen screen(
            [&](MenuCommand value, ControlScheme active)
            {
                command = value;
                scheme = active;
            });
        InputState touch;
        touch.pointerKind = PointerKind::Touch;
        touch.pointerPressed = true;
        touch.pointerX = 0.5F;
        touch.pointerY = 0.48F;

        EXPECT_EQ(screen.Update(touch, 0.016F), ScreenAction::None);
        EXPECT_EQ(command, MenuCommand::Settings);
        EXPECT_EQ(scheme, ControlScheme::Touch);
    }

    TEST(ApplicationMenuTests, PauseMenuCanResumeOpenSettingsReturnHomeAndQuit)
    {
        std::vector<MenuCommand> requested;
        PauseMenuScreen screen([&](MenuCommand command, ControlScheme) { requested.push_back(command); });
        InputState accept;
        accept.uiAcceptPressed = true;
        InputState down;
        down.uiDownPressed = true;

        EXPECT_EQ(screen.Update(accept, 0.016F), ScreenAction::PopAll);
        screen.Update(down, 0.016F);
        EXPECT_EQ(screen.Update(accept, 0.016F), ScreenAction::None);
        screen.Update(down, 0.016F);
        EXPECT_EQ(screen.Update(accept, 0.016F), ScreenAction::None);
        EXPECT_EQ(requested, (std::vector<MenuCommand>{MenuCommand::Settings, MenuCommand::MainMenu}));
        screen.Update(down, 0.016F);
        EXPECT_EQ(screen.Update(accept, 0.016F), ScreenAction::Quit);

        InputState cancel;
        cancel.cancelPressed = true;
        EXPECT_EQ(screen.Update(cancel, 0.016F), ScreenAction::PopAll);
    }

    TEST(ApplicationMenuTests, CreditsPresentTheGeneratedDocumentAndScroll)
    {
        std::string document;
        for (int line = 0; line < 30; ++line)
        {
            document += "asset entry\n";
        }
        CreditsScreen screen(document);
        EXPECT_EQ(screen.LineCount(), 31U);

        InputState down;
        down.uiDownPressed = true;
        EXPECT_EQ(screen.Update(down, 0.016F), ScreenAction::None);
        EXPECT_EQ(screen.ScrollOffset(), 1U);

        InputState cancel;
        cancel.cancelPressed = true;
        EXPECT_EQ(screen.Update(cancel, 0.016F), ScreenAction::Pop);
    }

    TEST(ApplicationMenuTests, FirstRunHintNamesTheActiveSchemeAndFadesAfterTwelveSeconds)
    {
        ControlsHint hint;
        EXPECT_FALSE(hint.Visible());
        hint.Start(ControlScheme::KeyboardMouse);
        EXPECT_TRUE(hint.Visible());
        EXPECT_NE(hint.Text().find("WASD"), std::string_view::npos);
        EXPECT_NE(hint.Text().find("mouse"), std::string_view::npos);
        EXPECT_NE(hint.Text().find("Esc"), std::string_view::npos);
        hint.Update(9.0F);
        EXPECT_FLOAT_EQ(hint.Alpha(), 1.0F);
        hint.Update(1.5F);
        EXPECT_FLOAT_EQ(hint.Alpha(), 0.5F);
        hint.Update(1.5F);
        EXPECT_FALSE(hint.Visible());
        EXPECT_FLOAT_EQ(hint.Alpha(), 0.0F);

        hint.Start(ControlScheme::Touch);
        EXPECT_NE(hint.Text().find("Touch"), std::string_view::npos);
        EXPECT_NE(hint.Text().find("pause"), std::string_view::npos);
    }

    TEST(SettingsScreenTests, KeyboardNavigationReachesEveryControlAndWraps)
    {
        Settings settings = Settings::Defaults();
        SettingsScreen screen(settings);
        constexpr std::array expected = {SettingsControl::Quality,
                                         SettingsControl::DisplaySize,
                                         SettingsControl::Fullscreen,
                                         SettingsControl::VerticalSync,
                                         SettingsControl::FieldOfView,
                                         SettingsControl::Master,
                                         SettingsControl::Footsteps,
                                         SettingsControl::Ambience,
                                         SettingsControl::Weather,
                                         SettingsControl::LookSensitivity,
                                         SettingsControl::InvertY,
                                         SettingsControl::WalkSpeed,
                                         SettingsControl::TimeOfDay,
                                         SettingsControl::TimeSpeed,
                                         SettingsControl::EnvironmentWeather};

        for (const SettingsControl control : expected)
        {
            EXPECT_EQ(screen.Selected(), control);
            EXPECT_NE(SettingsControlName(control), std::string_view{"?"});
            InputState down;
            down.uiDownPressed = true;
            EXPECT_EQ(screen.Update(down, 0.016F), ScreenAction::None);
        }
        EXPECT_EQ(screen.Selected(), SettingsControl::Quality);

        InputState up;
        up.uiUpPressed = true;
        screen.Update(up, 0.016F);
        EXPECT_EQ(screen.Selected(), SettingsControl::EnvironmentWeather);
    }

    TEST(SettingsScreenTests, GraphicsRowsAreFilteredByTheBuildProfileWithoutCapabilityQueries)
    {
        Settings settings = Settings::Defaults();
        cnahouse::app::Platform platform;
        platform.displaySizes = {{1280, 720}, {1600, 900}, {1920, 1080}};
        const cnahouse::rendering::RenderTier tier(cnahouse::app::RenderTier::S);

        platform.target = cnahouse::app::BuildTarget::Desktop;
        SettingsFeatures desktop = SettingsFeatures::Resolve(platform, tier, settings);
        SettingsScreen desktopScreen(settings, {}, desktop);
        EXPECT_TRUE(desktopScreen.Shows(SettingsControl::DisplaySize));
        EXPECT_TRUE(desktopScreen.Shows(SettingsControl::Fullscreen));
        EXPECT_TRUE(desktopScreen.Shows(SettingsControl::VerticalSync));
        EXPECT_EQ(desktopScreen.VisibleControlCount(), 15U);

        platform.target = cnahouse::app::BuildTarget::Web;
        SettingsFeatures web = SettingsFeatures::Resolve(platform, tier, settings);
        SettingsScreen webScreen(settings, {}, web);
        EXPECT_TRUE(web.canvasSize);
        EXPECT_TRUE(webScreen.Shows(SettingsControl::DisplaySize));
        EXPECT_TRUE(webScreen.Shows(SettingsControl::Fullscreen))
            << "HOUSE-03721 owns the browser fullscreen path from this row";
        EXPECT_FALSE(webScreen.Shows(SettingsControl::VerticalSync));

        platform.target = cnahouse::app::BuildTarget::Android;
        SettingsFeatures android = SettingsFeatures::Resolve(platform, tier, settings);
        SettingsScreen androidScreen(settings, {}, android);
        EXPECT_FALSE(androidScreen.Shows(SettingsControl::DisplaySize));
        EXPECT_FALSE(androidScreen.Shows(SettingsControl::Fullscreen));
        EXPECT_FALSE(androidScreen.Shows(SettingsControl::VerticalSync));
        EXPECT_TRUE(androidScreen.Shows(SettingsControl::Quality));
        EXPECT_TRUE(androidScreen.Shows(SettingsControl::FieldOfView));
    }

    TEST(SettingsScreenTests, GraphicsRowsUseOnlyHighWebAndAndroidProfiles)
    {
        Settings settings = Settings::Defaults();
        cnahouse::app::Platform platform;
        platform.displaySizes = {{1280, 720}, {1600, 900}, {1920, 1080}};
        const cnahouse::rendering::RenderTier tier(cnahouse::app::RenderTier::S);
        std::vector<SettingsControl> changed;
        SettingsScreen screen(
            settings,
            [&](SettingsControl control) { changed.push_back(control); },
            SettingsFeatures::Resolve(platform, tier, settings));

        InputState right;
        right.uiRightPressed = true;
        screen.Update(right, 0.016F);
        EXPECT_EQ(settings.quality, cnahouse::app::QualityPreset::Low)
            << "High wraps to the retained Android profile; Ultra is not offered";

        InputState left;
        left.uiLeftPressed = true;
        screen.Update(left, 0.016F);
        EXPECT_EQ(settings.quality, cnahouse::app::QualityPreset::High);

        InputState down;
        down.uiDownPressed = true;
        screen.Update(down, 0.016F);
        ASSERT_EQ(screen.Selected(), SettingsControl::DisplaySize);
        screen.Update(right, 0.016F);
        EXPECT_EQ(settings.backBufferWidth, 1920);
        EXPECT_EQ(settings.backBufferHeight, 1080);
        EXPECT_EQ(
            changed,
            (std::vector{SettingsControl::Quality, SettingsControl::Quality, SettingsControl::DisplaySize}));
    }

    TEST(SettingsScreenTests, KeyboardEditsAudioAndControlsAndAppliesEachChangeLive)
    {
        Settings settings = Settings::Defaults();
        AudioSystem audio(false);
        int applied = 0;
        SettingsScreen screen(settings,
                              [&](SettingsControl)
                              {
                                  ++applied;
                                  audio.SetMasterVolume(settings.masterVolume);
                                  audio.SetCategoryVolume(Category::Footsteps, settings.footstepsVolume);
                                  audio.SetCategoryVolume(Category::Ambience, settings.ambienceVolume);
                                  audio.SetCategoryVolume(Category::Weather, settings.weatherVolume);
                              });

        InputState down;
        down.uiDownPressed = true;
        for (int row = 0; row < 5; ++row)
        {
            screen.Update(down, 0.016F);
        }
        ASSERT_EQ(screen.Selected(), SettingsControl::Master);

        InputState right;
        right.uiRightPressed = true;
        screen.Update(right, 0.016F);
        EXPECT_FLOAT_EQ(settings.masterVolume, 0.85F);
        EXPECT_FLOAT_EQ(audio.MasterVolume(), 0.85F);

        screen.Update(down, 0.016F);
        screen.Update(right, 0.016F);
        EXPECT_FLOAT_EQ(settings.footstepsVolume, 0.90F);
        EXPECT_FLOAT_EQ(audio.CategoryVolume(Category::Footsteps), 0.90F);

        for (int row = 0; row < 4; ++row)
        {
            screen.Update(down, 0.016F);
        }
        ASSERT_EQ(screen.Selected(), SettingsControl::InvertY);
        InputState accept;
        accept.uiAcceptPressed = true;
        screen.Update(accept, 0.016F);
        EXPECT_TRUE(settings.invertY);

        screen.Update(down, 0.016F);
        screen.Update(accept, 0.016F);
        EXPECT_TRUE(settings.fastWalk);
        EXPECT_EQ(applied, 4);
    }

    TEST(SettingsScreenTests, MouseAndTouchUseTheSameBoundedHitTargets)
    {
        for (const PointerKind kind : {PointerKind::Mouse, PointerKind::Touch})
        {
            Settings settings = Settings::Defaults();
            int applied = 0;
            SettingsScreen screen(settings, [&](SettingsControl) { ++applied; });

            InputState pointer;
            pointer.pointerKind = kind;
            pointer.pointerPressed = true;
            pointer.pointerX = 0.6125F; // one quarter of the 0.55..0.80 slider track
            pointer.pointerY = 0.382F;  // footsteps row
            screen.Update(pointer, 0.016F);

            EXPECT_EQ(screen.Selected(), SettingsControl::Footsteps);
            EXPECT_NEAR(settings.footstepsVolume, 0.25F, 1e-6F);
            EXPECT_EQ(applied, 1);
        }
    }

    TEST(SettingsScreenTests, EnvironmentOffersOnlyTheRetainedTimeAndWeatherChoices)
    {
        Settings settings = Settings::Defaults();
        std::vector<SettingsControl> changed;
        SettingsScreen screen(settings, [&](SettingsControl control) { changed.push_back(control); });

        InputState down;
        down.uiDownPressed = true;
        for (int row = 0; row < 12; ++row)
        {
            screen.Update(down, 0.016F);
        }
        ASSERT_EQ(screen.Selected(), SettingsControl::TimeOfDay);

        InputState right;
        right.uiRightPressed = true;
        screen.Update(right, 0.016F);
        EXPECT_FLOAT_EQ(settings.fixedTimeOfDayHours, 6.0F);

        screen.Update(down, 0.016F);
        screen.Update(right, 0.016F);
        EXPECT_FLOAT_EQ(settings.dayLengthRealMinutes, 48.0F);

        screen.Update(down, 0.016F);
        ASSERT_EQ(screen.Selected(), SettingsControl::EnvironmentWeather);
        for (const std::string_view expected : {"W_CLEAR", "W_OVERCAST", "W_RAIN"})
        {
            screen.Update(right, 0.016F);
            EXPECT_EQ(settings.weatherMode, WeatherMode::Fixed);
            EXPECT_EQ(settings.fixedWeatherArchetype, expected);
        }
        screen.Update(right, 0.016F);
        EXPECT_EQ(settings.weatherMode, WeatherMode::On);
        EXPECT_EQ(changed.size(), 6U);
    }

    TEST(SettingsScreenTests, AClickOutsideThePageChangesNothingAndEscapeClosesIt)
    {
        Settings settings = Settings::Defaults();
        int applied = 0;
        SettingsScreen screen(settings, [&](SettingsControl) { ++applied; });

        InputState outside;
        outside.pointerPressed = true;
        outside.pointerX = 0.99F;
        outside.pointerY = 0.99F;
        EXPECT_EQ(screen.Update(outside, 0.016F), ScreenAction::None);
        EXPECT_EQ(applied, 0);

        InputState cancel;
        cancel.cancelPressed = true;
        EXPECT_EQ(screen.Update(cancel, 0.016F), ScreenAction::Pop);
    }
} // namespace
