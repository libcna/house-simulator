// SPDX-License-Identifier: MIT
//
// `HOUSE-00140`. The mouse behaviour here is written against what `HOUSE-00100` MEASURED rather
// than against what one would assume, and these are the tests that pin that down -- because the
// probe could not get real pointer motion, this is where the recentring logic is actually checked.
#include <gtest/gtest.h>

#include <cmath>

#include "Microsoft/Xna/Framework/Input/KeyboardState.hpp"
#include "Microsoft/Xna/Framework/Input/Keys.hpp"
#include "Microsoft/Xna/Framework/Input/MouseState.hpp"
#include "Microsoft/Xna/Framework/Input/Touch/TouchCollection.hpp"
#include "Microsoft/Xna/Framework/Input/Touch/TouchLocation.hpp"
#include "Microsoft/Xna/Framework/Input/Touch/TouchLocationState.hpp"

#include "cnahouse/player/KeyboardMouseSource.hpp"

namespace
{
    using cnahouse::player::InputConfig;
    using cnahouse::player::KeyboardMouseSource;
    using Microsoft::Xna::Framework::Input::ButtonState;
    using Microsoft::Xna::Framework::Input::KeyboardState;
    using Microsoft::Xna::Framework::Input::Keys;
    using Microsoft::Xna::Framework::Input::MouseState;
    using Microsoft::Xna::Framework::Input::Touch::TouchCollection;
    using Microsoft::Xna::Framework::Input::Touch::TouchLocation;
    using Microsoft::Xna::Framework::Input::Touch::TouchLocationState;

    // `KeyboardState`'s only plain-XNA constructor takes an `initializer_list`; the default and the
    // set-taking ones are `CNAEXT` in CNA, so `KeyboardState{...}` is what both these tests and the
    // runtime use.
    //
    // **An empty state is `KeyboardState({})`, NOT `KeyboardState({})`**, and this comment used to
    // claim the opposite. `T{}` prefers the DEFAULT constructor over an `initializer_list` one when
    // both exist, so `KeyboardState({})` silently selected the `CNAEXT` default and every "no keys
    // held" line in this file was an ADR-0001 violation -- with a comment above it asserting it was
    // not. `KeyboardState({})` passes an explicitly empty list and selects the XNA constructor.
    // Measured, and now enforced by `tools/ci/check_xna_strict.py` (`HOUSE-00168`); no
    // source-text lint can see the difference, because both spellings name the same type.

    MouseState At(int x, int y)
    {
        return MouseState(x,
                          y,
                          0,
                          ButtonState::Released,
                          ButtonState::Released,
                          ButtonState::Released,
                          ButtonState::Released,
                          ButtonState::Released);
    }

    MouseState PressedAt(int x, int y)
    {
        return MouseState(x,
                          y,
                          0,
                          ButtonState::Pressed,
                          ButtonState::Released,
                          ButtonState::Released,
                          ButtonState::Released,
                          ButtonState::Released);
    }

    TEST(InputTests, KeyboardUiNavigationIsReportedAsEdges)
    {
        KeyboardMouseSource source;
        source.Apply(KeyboardState{Keys::Down, Keys::Right, Keys::Enter}, At(0, 0), 0.016F);
        EXPECT_TRUE(source.Current().uiDownPressed);
        EXPECT_TRUE(source.Current().uiRightPressed);
        EXPECT_TRUE(source.Current().uiAcceptPressed);

        source.Apply(KeyboardState{Keys::Down, Keys::Right, Keys::Enter}, At(0, 0), 0.016F);
        EXPECT_FALSE(source.Current().uiDownPressed);
        EXPECT_FALSE(source.Current().uiRightPressed);
        EXPECT_FALSE(source.Current().uiAcceptPressed);
    }

    TEST(InputTests, MouseAndTouchBecomeTheSameNormalisedPointerPress)
    {
        InputConfig config;
        config.viewportWidth = 1600;
        config.viewportHeight = 900;
        KeyboardMouseSource source(config);

        source.Apply(KeyboardState({}), PressedAt(800, 450), 0.016F);
        EXPECT_EQ(source.Current().pointerKind, cnahouse::player::PointerKind::Mouse);
        EXPECT_TRUE(source.Current().pointerPressed);
        EXPECT_FLOAT_EQ(source.Current().pointerX, 0.5F);
        EXPECT_FLOAT_EQ(source.Current().pointerY, 0.5F);

        // Release the mouse before the independent touch gesture. `anyPressed` is deliberately one
        // edge across every device, so overlapping presses are one continuous user interaction.
        source.Apply(KeyboardState({}), At(800, 450), 0.016F);

        const std::vector<TouchLocation> locations{TouchLocation(
            7, TouchLocationState::Pressed, Microsoft::Xna::Framework::Vector2(400.0F, 225.0F))};
        const TouchCollection touches(locations);
        source.Apply(KeyboardState({}), At(0, 0), touches, 0.016F);
        EXPECT_EQ(source.Current().pointerKind, cnahouse::player::PointerKind::Touch);
        EXPECT_TRUE(source.Current().pointerPressed);
        EXPECT_FLOAT_EQ(source.Current().pointerX, 0.25F);
        EXPECT_FLOAT_EQ(source.Current().pointerY, 0.25F);
        EXPECT_TRUE(source.Current().anyPressed) << "a touch also opens the uniform audio gesture gate";
    }

    TEST(InputTests, MovementIsExpressedAsADirectionNotAsKeys)
    {
        KeyboardMouseSource source;
        source.Apply(KeyboardState{Keys::W}, At(0, 0), 0.016f);
        EXPECT_FLOAT_EQ(source.Current().move.Y, 1.0f);
        EXPECT_FLOAT_EQ(source.Current().move.X, 0.0f);

        source.Apply(KeyboardState{Keys::A}, At(0, 0), 0.016f);
        EXPECT_FLOAT_EQ(source.Current().move.X, -1.0f);
    }

    TEST(InputTests, ArrowMovementSharesTheWasdAxesWithoutDoubleCounting)
    {
        KeyboardMouseSource source;
        source.Apply(KeyboardState{Keys::Up, Keys::Right}, At(0, 0), 0.016F);
        EXPECT_NEAR(source.Current().move.X, std::sqrt(0.5F), 1e-5F);
        EXPECT_NEAR(source.Current().move.Y, std::sqrt(0.5F), 1e-5F);

        source.Apply(KeyboardState{Keys::W, Keys::Up, Keys::A, Keys::Left}, At(0, 0), 0.016F);
        EXPECT_NEAR(source.Current().move.X, -std::sqrt(0.5F), 1e-5F);
        EXPECT_NEAR(source.Current().move.Y, std::sqrt(0.5F), 1e-5F);

        source.Apply(KeyboardState{Keys::Down, Keys::Up, Keys::Left, Keys::Right}, At(0, 0), 0.016F);
        EXPECT_FLOAT_EQ(source.Current().move.X, 0.0F);
        EXPECT_FLOAT_EQ(source.Current().move.Y, 0.0F);
    }

    TEST(InputTests, DiagonalMovementIsNotFasterThanCardinal)
    {
        // The oldest bug in first-person movement, and one that only ever presents as "the player
        // feels fast on the diagonal".
        KeyboardMouseSource source;
        source.Apply(KeyboardState{Keys::W, Keys::D}, At(0, 0), 0.016f);
        const auto& move = source.Current().move;
        const float length = std::sqrt(move.X * move.X + move.Y * move.Y);
        EXPECT_NEAR(length, 1.0f, 1e-5f);
    }

    TEST(InputTests, OppositeKeysCancel)
    {
        KeyboardMouseSource source;
        source.Apply(KeyboardState{Keys::W, Keys::S, Keys::A, Keys::D}, At(0, 0), 0.016f);
        EXPECT_FLOAT_EQ(source.Current().move.X, 0.0f);
        EXPECT_FLOAT_EQ(source.Current().move.Y, 0.0f);
    }

    TEST(InputTests, PressesAreEdgesComputedOnceHere)
    {
        // Two systems each tracking "was it down last frame" is two chances to disagree about what
        // frame it is, so the edge is computed by the source and nowhere else.
        KeyboardMouseSource source;
        source.Apply(KeyboardState({}), At(0, 0), 0.016f);
        EXPECT_FALSE(source.Current().interactPressed);

        source.Apply(KeyboardState{Keys::E}, At(0, 0), 0.016f);
        EXPECT_TRUE(source.Current().interactPressed) << "the frame it goes down";

        source.Apply(KeyboardState{Keys::E}, At(0, 0), 0.016f);
        EXPECT_FALSE(source.Current().interactPressed) << "held is not pressed";

        source.Apply(KeyboardState({}), At(0, 0), 0.016f);
        source.Apply(KeyboardState{Keys::E}, At(0, 0), 0.016f);
        EXPECT_TRUE(source.Current().interactPressed) << "and again after a release";
    }

    TEST(InputTests, TheFirstMouseSampleProducesNoLook)
    {
        // The classic "camera snaps on startup": seeding the previous position from an assumed window
        // centre makes the first real sample an enormous bogus delta. `HOUSE-00100` measured that
        // `Mouse::GetState` is an event-driven snapshot, so where it starts is not something to guess.
        KeyboardMouseSource source;
        source.Apply(KeyboardState({}), At(4000, 3000), 0.016f);
        EXPECT_FLOAT_EQ(source.Current().look.X, 0.0f);
        EXPECT_FLOAT_EQ(source.Current().look.Y, 0.0f);
        EXPECT_FALSE(source.LookAvailable());
    }

    TEST(InputTests, LookIsADeltaInRadians)
    {
        KeyboardMouseSource source;
        source.Apply(KeyboardState({}), At(100, 100), 0.016f); // seeds
        source.Apply(KeyboardState({}), At(200, 100), 0.016f); // +100 px of yaw

        EXPECT_TRUE(source.LookAvailable());
        EXPECT_NEAR(source.Current().look.X, 100.0f * InputConfig::kRadiansPerPixel, 1e-6f)
            << "radians, not mouse counts -- sensitivity is applied once, here";
        EXPECT_FLOAT_EQ(source.Current().look.Y, 0.0f);
    }

    TEST(InputTests, SensitivityAndInversionAreAppliedOnce)
    {
        InputConfig config;
        config.sensitivity = 2.0f;
        config.invertY = true;
        KeyboardMouseSource source(config);
        source.Apply(KeyboardState({}), At(0, 0), 0.016f);
        source.Apply(KeyboardState({}), At(10, 10), 0.016f);

        EXPECT_NEAR(source.Current().look.X, 10.0f * InputConfig::kRadiansPerPixel * 2.0f, 1e-6f);
        EXPECT_NEAR(source.Current().look.Y, -10.0f * InputConfig::kRadiansPerPixel * 2.0f, 1e-6f)
            << "inverted, and inverted exactly once";
    }

    TEST(InputTests, AnUnchangedSampleContributesNoLookAndIsNotConsideredAvailable)
    {
        // `HOUSE-00100` found `Mouse::GetState` returning the same value on all 10 000 frames of an
        // unattended window, while `Game::IsActive` was true throughout. "The player held still" and
        // "no motion event arrived" are indistinguishable from here, so neither contributes look --
        // which is the safe reading of both.
        KeyboardMouseSource source;
        source.Apply(KeyboardState({}), At(400, 300), 0.016f);
        source.Apply(KeyboardState({}), At(400, 300), 0.016f);
        EXPECT_FALSE(source.LookAvailable());
        EXPECT_FLOAT_EQ(source.Current().look.X, 0.0f);
    }

    TEST(InputTests, CapturingOrReleasingTheMouseDiscardsThePositionHistory)
    {
        // The pointer may have gone anywhere while a menu was open. Without this the first delta after
        // re-capturing is the whole distance it travelled -- "the camera snaps when you close the
        // menu".
        KeyboardMouseSource source;
        source.Apply(KeyboardState({}), At(100, 100), 0.016f);
        source.Apply(KeyboardState({}), At(110, 100), 0.016f);
        ASSERT_TRUE(source.LookAvailable());

        source.SetMouseCaptured(true);
        EXPECT_FALSE(source.LookAvailable());
        source.Apply(KeyboardState({}), At(900, 700), 0.016f);
        EXPECT_FLOAT_EQ(source.Current().look.X, 0.0f)
            << "the first sample after a capture change re-seeds instead of producing a jump";
    }

    TEST(InputTests, DebugTogglesAreEdgesToo)
    {
        KeyboardMouseSource source;
        source.Apply(KeyboardState({}), At(0, 0), 0.016f);
        source.Apply(KeyboardState{Keys::F1}, At(0, 0), 0.016f);
        EXPECT_TRUE(source.Current().toggleOverlayPressed);
        source.Apply(KeyboardState{Keys::F1}, At(0, 0), 0.016f);
        EXPECT_FALSE(source.Current().toggleOverlayPressed);
    }

    TEST(InputTests, AltEnterTogglesFullscreenOncePerEnterPress)
    {
        KeyboardMouseSource source;
        source.Apply(KeyboardState{Keys::Enter}, At(0, 0), 0.016f);
        EXPECT_FALSE(source.Current().toggleFullscreenPressed);

        source.Apply(KeyboardState{Keys::LeftAlt, Keys::Enter}, At(0, 0), 0.016f);
        EXPECT_FALSE(source.Current().toggleFullscreenPressed)
            << "holding Enter and then pressing Alt must not produce an Enter edge";

        source.Apply(KeyboardState{Keys::LeftAlt}, At(0, 0), 0.016f);
        source.Apply(KeyboardState{Keys::LeftAlt, Keys::Enter}, At(0, 0), 0.016f);
        EXPECT_TRUE(source.Current().toggleFullscreenPressed);
        source.Apply(KeyboardState{Keys::LeftAlt, Keys::Enter}, At(0, 0), 0.016f);
        EXPECT_FALSE(source.Current().toggleFullscreenPressed) << "holding Enter must not toggle again";

        source.Apply(KeyboardState({}), At(0, 0), 0.016f);
        source.Apply(KeyboardState{Keys::RightAlt, Keys::Enter}, At(0, 0), 0.016f);
        EXPECT_TRUE(source.Current().toggleFullscreenPressed) << "either Alt key can request fullscreen";
    }

    TEST(InputTests, EachOverlayHasItsOwnFunctionKeyAndNobodyElsesEdge)
    {
        // §69's `F2`, §25.8's `F3`, §71's `F8` and `F9`, each to its own field. A scripted input source
        // sets these fields directly (`SetInputSourceForTesting`), so nothing downstream can catch
        // a key wired to the wrong one -- this is the only place the mapping itself is checked.
        struct Binding
        {
            Keys key;
            bool cnahouse::player::InputState::* field;
            const char* what;
        };

        const Binding bindings[] = {
            {Keys::F1, &cnahouse::player::InputState::toggleOverlayPressed, "F1 performance"},
            {Keys::F2, &cnahouse::player::InputState::toggleWorldOverlayPressed, "F2 world"},
            {Keys::F3, &cnahouse::player::InputState::toggleVisibilityOverlayPressed, "F3 visibility"},
            {Keys::F4,
             &cnahouse::player::InputState::toggleVisibilityGeometryPressed,
             "F4 visibility geometry"},
            {Keys::F5, &cnahouse::player::InputState::toggleFreezeVisibilityPressed, "F5 freeze"},
            {Keys::F8, &cnahouse::player::InputState::toggleEnvironmentOverlayPressed, "F8 environment"},
            {Keys::F9, &cnahouse::player::InputState::togglePhysicsOverlayPressed, "F9 physics"},
        };

        for (const Binding& binding : bindings)
        {
            KeyboardMouseSource source;
            source.Apply(KeyboardState({}), At(0, 0), 0.016f);
            source.Apply(KeyboardState{binding.key}, At(0, 0), 0.016f);
            EXPECT_TRUE(source.Current().*(binding.field)) << binding.what << " is not on its key";
            for (const Binding& other : bindings)
            {
                if (other.field != binding.field)
                {
                    EXPECT_FALSE(source.Current().*(other.field))
                        << binding.what << " also fired " << other.what;
                }
            }
        }

        // All of them at once, which is what catches two SHARING an edge slot: one key
        // pressed alone still looks right when its slot belongs to another, and only pressing both
        // in the same frame shows that the second one has already been consumed.
        KeyboardMouseSource together;
        together.Apply(KeyboardState({}), At(0, 0), 0.016f);
        together.Apply(KeyboardState{Keys::F1, Keys::F2, Keys::F3, Keys::F4, Keys::F5, Keys::F8, Keys::F9},
                       At(0, 0),
                       0.016f);
        for (const Binding& binding : bindings)
        {
            EXPECT_TRUE(together.Current().*(binding.field))
                << binding.what << " did not fire when the other overlays' keys were down too";
        }
        // ...and holding them repeats none of them, which is what makes each an edge of its own.
        together.Apply(KeyboardState{Keys::F1, Keys::F2, Keys::F3, Keys::F4, Keys::F5, Keys::F8, Keys::F9},
                       At(0, 0),
                       0.016f);
        for (const Binding& binding : bindings)
        {
            EXPECT_FALSE(together.Current().*(binding.field)) << binding.what << " repeated";
        }
    }

    TEST(InputTests, RunAndCrouchAreLevelsNotEdges)
    {
        // Held states, deliberately: a run that needed re-pressing every frame would be unusable.
        KeyboardMouseSource source;
        source.Apply(KeyboardState{Keys::LeftShift}, At(0, 0), 0.016f);
        EXPECT_TRUE(source.Current().run);
        source.Apply(KeyboardState{Keys::LeftShift}, At(0, 0), 0.016f);
        EXPECT_TRUE(source.Current().run) << "still held, still running";
        source.Apply(KeyboardState({}), At(0, 0), 0.016f);
        EXPECT_FALSE(source.Current().run);
    }

    TEST(InputTests, TheWalkModeKeyIsAnEdgeBesideThatLevel)
    {
        // §43.2: Shift toggles walking/running, rather than hold-to-sprint. So the
        // same key is reported twice -- as the level `run`, which a replay records and a gamepad
        // trigger may one day want, and as the edge `runPressed`, which is the one the toggle
        // reads. A consumer handed only the level would have to remember last frame's, and this
        // file's rule is that edges are computed here, once (`HOUSE-00556`).
        KeyboardMouseSource source;
        source.Apply(KeyboardState{Keys::LeftShift}, At(0, 0), 0.016f);
        EXPECT_TRUE(source.Current().runPressed) << "the press was not seen";
        source.Apply(KeyboardState{Keys::LeftShift}, At(0, 0), 0.016f);
        EXPECT_FALSE(source.Current().runPressed) << "holding it repeated the edge";
        EXPECT_TRUE(source.Current().run) << "...while the level is still held";
        source.Apply(KeyboardState({}), At(0, 0), 0.016f);
        EXPECT_FALSE(source.Current().runPressed);
        source.Apply(KeyboardState{Keys::LeftShift}, At(0, 0), 0.016f);
        EXPECT_TRUE(source.Current().runPressed) << "released and pressed again is a second edge";
    }

    TEST(InputTests, EitherShiftTogglesRunButHoldingBothDoesNotToggleTwice)
    {
        KeyboardMouseSource source;
        source.Apply(KeyboardState{Keys::RightShift}, At(0, 0), 0.016F);
        EXPECT_TRUE(source.Current().runPressed);
        source.Apply(KeyboardState{Keys::RightShift, Keys::LeftShift}, At(0, 0), 0.016F);
        EXPECT_FALSE(source.Current().runPressed);
        source.Apply(KeyboardState{Keys::LeftShift}, At(0, 0), 0.016F);
        EXPECT_FALSE(source.Current().runPressed);
        source.Apply(KeyboardState({}), At(0, 0), 0.016F);
        source.Apply(KeyboardState{Keys::LeftShift}, At(0, 0), 0.016F);
        EXPECT_TRUE(source.Current().runPressed);
    }

} // namespace

TEST(InputTests, CinemaIsOneEdgeAndDoesNotChangeMovementOrCrouchBindings)
{
    KeyboardMouseSource source;
    source.Apply(KeyboardState({Keys::C}), At(0, 0), 0.016F);
    EXPECT_TRUE(source.Current().cinemaPressed);
    EXPECT_FALSE(source.Current().crouch);
    EXPECT_FLOAT_EQ(source.Current().move.X, 0.0F);
    source.Apply(KeyboardState({Keys::C}), At(0, 0), 0.016F);
    EXPECT_FALSE(source.Current().cinemaPressed);
    source.Apply(KeyboardState({}), At(0, 0), 0.016F);
    source.Apply(KeyboardState({Keys::C, Keys::W, Keys::LeftControl}), At(0, 0), 0.016F);
    EXPECT_TRUE(source.Current().cinemaPressed);
    EXPECT_TRUE(source.Current().crouch);
    EXPECT_FLOAT_EQ(source.Current().move.Y, 1.0F);
}

TEST(InputTests, TwoFrameSmoothingIsOffByDefaultAndAveragesWhenItIsOn)
{
    // `HOUSE-00625`, §44: *"optional raw-ish smoothing over 2 frames, default off"*. Off by
    // default because smoothing IS latency -- it trades a millisecond of aim for a millisecond of
    // lag -- and two frames is the shortest average there is.
    KeyboardMouseSource raw;
    raw.Apply(KeyboardState({}), At(100, 100), 0.016f); // seeds the position
    raw.Apply(KeyboardState({}), At(110, 100), 0.016f);
    const float ten = raw.Current().look.X;
    EXPECT_GT(ten, 0.0f);
    raw.Apply(KeyboardState({}), At(110 + 20, 100), 0.016f);
    EXPECT_NEAR(raw.Current().look.X, 2.0f * ten, 1e-6f) << "the default smoothed something";

    InputConfig config;
    config.smoothing = true;
    KeyboardMouseSource smoothed(config);
    smoothed.Apply(KeyboardState({}), At(100, 100), 0.016f);
    smoothed.Apply(KeyboardState({}), At(110, 100), 0.016f);
    // The first smoothed frame averages 10 px with the nothing before it.
    EXPECT_NEAR(smoothed.Current().look.X, 0.5f * ten, 1e-6f);
    smoothed.Apply(KeyboardState({}), At(130, 100), 0.016f);
    // ...and the second averages 20 px with the 10 before it: 15.
    EXPECT_NEAR(smoothed.Current().look.X, 1.5f * ten, 1e-6f);
    // A frame of 20 again is the average of two twenties, which is a mouse that has settled.
    smoothed.Apply(KeyboardState({}), At(150, 100), 0.016f);
    EXPECT_NEAR(smoothed.Current().look.X, 2.0f * ten, 1e-6f);
}

TEST(InputTests, TheSmoothingHistoryIsDroppedWithThePositionHistory)
{
    // The delta from before a menu is not this frame's aim. `HOUSE-00140` drops the position
    // history across a capture change for that reason, and the averaged delta has to go with it or
    // the first frame back carries half of whatever the pointer did while the menu was open.
    InputConfig config;
    config.smoothing = true;
    KeyboardMouseSource source(config);
    source.Apply(KeyboardState({}), At(100, 100), 0.016f);
    source.Apply(KeyboardState({}), At(400, 100), 0.016f); // a big sweep
    ASSERT_GT(source.Current().look.X, 0.0f);

    source.SetMouseCaptured(true);
    source.SetMouseCaptured(false);
    source.Apply(KeyboardState({}), At(100, 100), 0.016f); // re-seeds
    source.Apply(KeyboardState({}), At(110, 100), 0.016f);

    KeyboardMouseSource fresh(config);
    fresh.Apply(KeyboardState({}), At(100, 100), 0.016f);
    fresh.Apply(KeyboardState({}), At(110, 100), 0.016f);
    EXPECT_FLOAT_EQ(source.Current().look.X, fresh.Current().look.X)
        << "the sweep before the capture change survived into the aim after it";
}

TEST(InputTests, APauseClearsTheAverageRatherThanCarryingItOver)
{
    // A frame with no motion is not a zero the average should include: `HOUSE-00100` says a still
    // hand and an unfocused window are indistinguishable from here, so a pause of unknown length
    // may have passed. Carrying the delta across it would let a sweep from before an alt-tab
    // arrive as half a sweep afterwards.
    InputConfig config;
    config.smoothing = true;
    KeyboardMouseSource source(config);
    source.Apply(KeyboardState({}), At(100, 100), 0.016f);
    source.Apply(KeyboardState({}), At(300, 100), 0.016f); // a 200 px sweep
    source.Apply(KeyboardState({}), At(300, 100), 0.016f); // ...and a still frame
    ASSERT_FALSE(source.LookAvailable());
    source.Apply(KeyboardState({}), At(310, 100), 0.016f); // 10 px

    KeyboardMouseSource fresh(config);
    fresh.Apply(KeyboardState({}), At(300, 100), 0.016f);
    fresh.Apply(KeyboardState({}), At(310, 100), 0.016f);
    EXPECT_FLOAT_EQ(source.Current().look.X, fresh.Current().look.X)
        << "the sweep from before the pause arrived after it";
}

TEST(InputTests, AltHeldIsWhatTheCursorPolicyReads)
{
    // §68's release, and a LEVEL rather than an edge: `HOUSE-00624`'s policy asks every frame.
    KeyboardMouseSource source;
    source.Apply(KeyboardState({}), At(0, 0), 0.016f);
    EXPECT_FALSE(source.Current().freeCursorHeld);
    source.Apply(KeyboardState{Keys::LeftAlt}, At(0, 0), 0.016f);
    EXPECT_TRUE(source.Current().freeCursorHeld);
    source.Apply(KeyboardState{Keys::RightAlt}, At(0, 0), 0.016f);
    EXPECT_TRUE(source.Current().freeCursorHeld) << "the other alt is not an alt";
    source.Apply(KeyboardState{Keys::W}, At(0, 0), 0.016f);
    EXPECT_FALSE(source.Current().freeCursorHeld);
}
