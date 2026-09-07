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

#include "cnahouse/player/KeyboardMouseSource.hpp"

namespace
{
    using cnahouse::player::InputConfig;
    using cnahouse::player::KeyboardMouseSource;
    using Microsoft::Xna::Framework::Input::ButtonState;
    using Microsoft::Xna::Framework::Input::KeyboardState;
    using Microsoft::Xna::Framework::Input::Keys;
    using Microsoft::Xna::Framework::Input::MouseState;

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

    TEST(InputTests, MovementIsExpressedAsADirectionNotAsKeys)
    {
        KeyboardMouseSource source;
        source.Apply(KeyboardState{Keys::W}, At(0, 0), 0.016f);
        EXPECT_FLOAT_EQ(source.Current().move.Y, 1.0f);
        EXPECT_FLOAT_EQ(source.Current().move.X, 0.0f);

        source.Apply(KeyboardState{Keys::A}, At(0, 0), 0.016f);
        EXPECT_FLOAT_EQ(source.Current().move.X, -1.0f);
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

} // namespace
