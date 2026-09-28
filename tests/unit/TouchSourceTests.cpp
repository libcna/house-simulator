// SPDX-License-Identifier: MIT
#include <gtest/gtest.h>

#include <cmath>
#include <initializer_list>
#include <vector>

#include "Microsoft/Xna/Framework/Input/Touch/TouchCollection.hpp"
#include "Microsoft/Xna/Framework/Input/Touch/TouchLocation.hpp"
#include "Microsoft/Xna/Framework/Input/Touch/TouchLocationState.hpp"
#include "Microsoft/Xna/Framework/Vector2.hpp"

#include "cnahouse/app/Platform.hpp"
#include "cnahouse/player/TouchSource.hpp"

namespace
{
    using cnahouse::player::IInputSource;
    using cnahouse::player::PointerKind;
    using cnahouse::player::TouchConfig;
    using cnahouse::player::TouchSource;
    using Microsoft::Xna::Framework::Vector2;
    using Microsoft::Xna::Framework::Input::Touch::TouchCollection;
    using Microsoft::Xna::Framework::Input::Touch::TouchLocation;
    using Microsoft::Xna::Framework::Input::Touch::TouchLocationState;

    TouchLocation Finger(int id, TouchLocationState state, float x, float y)
    {
        return TouchLocation(id, state, Vector2(x, y));
    }

    TouchCollection Frame(std::initializer_list<TouchLocation> touches)
    {
        return TouchCollection(std::vector<TouchLocation>(touches));
    }

    TEST(TouchSourceTests, FocusLossDropsHeldFingersBeforeTheNextGesture)
    {
        TouchSource source;
        source.Apply(Frame({Finger(10, TouchLocationState::Pressed, 200.0F, 700.0F),
                            Finger(20, TouchLocationState::Pressed, 1200.0F, 450.0F)}),
                     0.016F);
        source.Apply(Frame({Finger(10, TouchLocationState::Moved, 290.0F, 610.0F),
                            Finger(20, TouchLocationState::Moved, 1250.0F, 450.0F)}),
                     0.016F);
        ASSERT_TRUE(source.StickOrigin().has_value());
        ASSERT_TRUE(source.LookAvailable());

        source.Reset();
        EXPECT_FALSE(source.StickOrigin().has_value());
        EXPECT_FALSE(source.StickPosition().has_value());
        EXPECT_FALSE(source.LookAvailable());
        EXPECT_FLOAT_EQ(source.Current().move.X, 0.0F);
        EXPECT_FLOAT_EQ(source.Current().look.X, 0.0F);

        source.Apply(Frame({}), 0.016F);
        source.Apply(Frame({Finger(30, TouchLocationState::Pressed, 210.0F, 690.0F)}), 0.016F);
        EXPECT_TRUE(source.StickOrigin().has_value());
        EXPECT_TRUE(source.Current().pointerPressed);
        EXPECT_FLOAT_EQ(source.Current().move.X, 0.0F);
    }

    TEST(TouchSourceTests, IndependentStickAndLookFollowIdsWhenCollectionOrderChanges)
    {
        TouchSource source;
        IInputSource& input = source;
        source.Apply(Frame({Finger(10, TouchLocationState::Pressed, 200.0F, 700.0F),
                            Finger(20, TouchLocationState::Pressed, 1200.0F, 450.0F)}),
                     0.016F);
        EXPECT_TRUE(input.Current().anyPressed);
        EXPECT_TRUE(input.Current().pointerPressed);
        EXPECT_EQ(input.Current().pointerKind, PointerKind::Touch);
        EXPECT_FLOAT_EQ(input.Current().move.X, 0.0F);
        EXPECT_FALSE(input.LookAvailable());

        source.Apply(Frame({Finger(20, TouchLocationState::Moved, 1300.0F, 450.0F),
                            Finger(10, TouchLocationState::Moved, 290.0F, 610.0F)}),
                     0.016F);
        EXPECT_FLOAT_EQ(input.Current().move.X, 0.5F);
        EXPECT_FLOAT_EQ(input.Current().move.Y, 0.5F);
        EXPECT_NEAR(input.Current().look.X, 0.22F, 1e-6F);
        EXPECT_FLOAT_EQ(input.Current().look.Y, 0.0F);
        EXPECT_TRUE(input.LookAvailable());
        EXPECT_FALSE(input.Current().pointerPressed);
        EXPECT_FALSE(input.Current().anyPressed);

        source.Apply(Frame({Finger(10, TouchLocationState::Moved, 380.0F, 520.0F),
                            Finger(20, TouchLocationState::Moved, 1300.0F, 450.0F)}),
                     0.016F);
        EXPECT_NEAR(input.Current().move.X, std::sqrt(0.5F), 1e-6F);
        EXPECT_NEAR(input.Current().move.Y, std::sqrt(0.5F), 1e-6F);
        EXPECT_FALSE(input.LookAvailable());

        source.Apply(Frame({Finger(10, TouchLocationState::Released, 380.0F, 520.0F),
                            Finger(20, TouchLocationState::Moved, 1400.0F, 400.0F)}),
                     0.016F);
        EXPECT_FLOAT_EQ(input.Current().move.X, 0.0F);
        EXPECT_FLOAT_EQ(input.Current().move.Y, 0.0F);
        EXPECT_NEAR(input.Current().look.X, 0.22F, 1e-6F);
        EXPECT_NEAR(input.Current().look.Y, -0.11F, 1e-6F);
    }

    TEST(TouchSourceTests, ThirdFingerTapDoesNotStealEitherActiveGesture)
    {
        TouchSource source;
        source.Apply(Frame({Finger(1, TouchLocationState::Pressed, 200.0F, 700.0F),
                            Finger(2, TouchLocationState::Pressed, 1200.0F, 450.0F)}),
                     0.016F);
        source.Apply(Frame({Finger(1, TouchLocationState::Moved, 290.0F, 700.0F),
                            Finger(2, TouchLocationState::Moved, 1250.0F, 450.0F),
                            Finger(3, TouchLocationState::Pressed, 1500.0F, 100.0F)}),
                     0.016F);
        EXPECT_FLOAT_EQ(source.Current().move.X, 0.5F);
        EXPECT_NEAR(source.Current().look.X, 0.11F, 1e-6F);
        EXPECT_TRUE(source.Current().pointerPressed);
        EXPECT_EQ(source.Current().pointerKind, PointerKind::Touch);
        EXPECT_FLOAT_EQ(source.Current().pointerX, 0.9375F);
        EXPECT_NEAR(source.Current().pointerY, 1.0F / 9.0F, 1e-6F);
        EXPECT_TRUE(source.Current().anyPressed);

        source.Apply(Frame({Finger(3, TouchLocationState::Released, 1500.0F, 100.0F),
                            Finger(2, TouchLocationState::Moved, 1250.0F, 450.0F),
                            Finger(1, TouchLocationState::Moved, 290.0F, 700.0F)}),
                     0.016F);
        EXPECT_FALSE(source.Current().pointerPressed);
        EXPECT_FALSE(source.Current().anyPressed);
        EXPECT_FLOAT_EQ(source.Current().move.X, 0.5F);
    }

    TEST(TouchSourceTests, AdvancedFirstFrameStillProducesOneTapEdge)
    {
        TouchSource source;
        const TouchLocation first(7,
                                  TouchLocationState::Moved,
                                  Vector2(800.0F, 450.0F),
                                  TouchLocationState::Pressed,
                                  Vector2(800.0F, 450.0F));
        source.Apply(Frame({first}), 0.016F);
        EXPECT_TRUE(source.Current().anyPressed);
        EXPECT_TRUE(source.Current().pointerPressed);
        EXPECT_EQ(source.Current().pointerKind, PointerKind::Touch);
        source.Apply(Frame({TouchLocation(7,
                                          TouchLocationState::Moved,
                                          Vector2(800.0F, 450.0F),
                                          TouchLocationState::Moved,
                                          Vector2(800.0F, 450.0F))}),
                     0.016F);
        EXPECT_FALSE(source.Current().anyPressed);
        EXPECT_FALSE(source.Current().pointerPressed);
    }

    TEST(TouchSourceTests, CrossingTheCentreDoesNotSwapMovementAndLook)
    {
        TouchSource source;
        source.Apply(Frame({Finger(1, TouchLocationState::Pressed, 700.0F, 700.0F),
                            Finger(2, TouchLocationState::Pressed, 900.0F, 300.0F)}),
                     0.016F);
        source.Apply(Frame({Finger(2, TouchLocationState::Moved, 750.0F, 300.0F),
                            Finger(1, TouchLocationState::Moved, 900.0F, 700.0F)}),
                     0.016F);
        EXPECT_FLOAT_EQ(source.Current().move.X, 1.0F);
        EXPECT_NEAR(source.Current().look.X, -0.33F, 1e-6F);
    }

    TEST(TouchSourceTests, VirtualRadiusAndSeparateLookSensitivityApplyOnce)
    {
        TouchConfig config;
        config.viewportWidth = 800;
        config.viewportHeight = 450;
        config.lookSensitivity = 2.0F;
        config.invertY = true;
        TouchSource source(config);
        source.Apply(Frame({Finger(1, TouchLocationState::Pressed, 100.0F, 350.0F),
                            Finger(2, TouchLocationState::Pressed, 500.0F, 100.0F)}),
                     0.016F);
        source.Apply(Frame({Finger(1, TouchLocationState::Moved, 190.0F, 350.0F),
                            Finger(2, TouchLocationState::Moved, 550.0F, 150.0F)}),
                     0.016F);
        EXPECT_FLOAT_EQ(source.Current().move.X, 1.0F);
        EXPECT_NEAR(source.Current().look.X, 0.22F, 1e-6F);
        EXPECT_NEAR(source.Current().look.Y, -0.22F, 1e-6F);
    }

    TEST(TouchSourceTests, MissingFingerReleasesItsRoleAndNewLookHasNoJump)
    {
        TouchSource source;
        source.Apply(Frame({Finger(4, TouchLocationState::Pressed, 1200.0F, 300.0F)}), 0.016F);
        source.Apply(Frame({Finger(4, TouchLocationState::Moved, 1300.0F, 300.0F)}), 0.016F);
        ASSERT_TRUE(source.LookAvailable());
        source.Apply(Frame({}), 0.016F);
        EXPECT_FALSE(source.LookAvailable());
        source.Apply(Frame({Finger(5, TouchLocationState::Pressed, 1300.0F, 100.0F)}), 0.016F);
        EXPECT_FALSE(source.LookAvailable());
        EXPECT_FLOAT_EQ(source.Current().look.X, 0.0F);
        source.Apply(Frame({Finger(5, TouchLocationState::Moved, 1310.0F, 100.0F)}), 0.016F);
        EXPECT_NEAR(source.Current().look.X, 0.022F, 1e-6F);
    }

    TEST(TouchSourceTests, FloatingStickExposesActualOriginAndThumb)
    {
        TouchSource source;
        EXPECT_FALSE(source.StickOrigin());
        source.Apply(Frame({Finger(8, TouchLocationState::Pressed, 300.0F, 700.0F)}), 0.016F);
        ASSERT_TRUE(source.StickOrigin());
        EXPECT_FLOAT_EQ(source.StickOrigin()->X, 300.0F);
        EXPECT_FLOAT_EQ(source.StickOrigin()->Y, 700.0F);
        source.Apply(Frame({Finger(8, TouchLocationState::Moved, 390.0F, 700.0F)}), 0.016F);
        EXPECT_FLOAT_EQ(source.StickPosition()->X, 390.0F);
        EXPECT_FLOAT_EQ(source.Current().move.X, 0.5F);
        source.Apply(Frame({}), 0.016F);
        EXPECT_FALSE(source.StickOrigin());
    }

    TEST(TouchSourceTests, ButtonCornersAreNotLookGestures)
    {
        TouchSource source;
        source.Apply(Frame({Finger(1, TouchLocationState::Pressed, 1500.0F, 100.0F),
                            Finger(2, TouchLocationState::Pressed, 1500.0F, 800.0F)}),
                     0.016F);
        source.Apply(Frame({Finger(1, TouchLocationState::Moved, 1450.0F, 100.0F),
                            Finger(2, TouchLocationState::Moved, 1450.0F, 800.0F)}),
                     0.016F);
        EXPECT_FALSE(source.LookAvailable());
        source.Apply(Frame({Finger(3, TouchLocationState::Pressed, 1200.0F, 450.0F)}), 0.016F);
        source.Apply(Frame({Finger(3, TouchLocationState::Moved, 1300.0F, 450.0F)}), 0.016F);
        EXPECT_TRUE(source.LookAvailable());
        EXPECT_NEAR(source.Current().look.X, 0.22F, 1e-6F);
    }

    TEST(TouchSourceTests, ButtonsEmitIndependentEdgesWithoutStealingHeldGestures)
    {
        TouchSource source;
        source.SetButtonsEnabled(true);
        source.Apply(Frame({Finger(1, TouchLocationState::Pressed, 200.0F, 700.0F),
                            Finger(2, TouchLocationState::Pressed, 1200.0F, 450.0F)}),
                     0.016F);
        source.Apply(Frame({Finger(1, TouchLocationState::Moved, 290.0F, 700.0F),
                            Finger(2, TouchLocationState::Moved, 1250.0F, 450.0F),
                            Finger(3, TouchLocationState::Pressed, 1500.0F, 100.0F),
                            Finger(4, TouchLocationState::Pressed, 1500.0F, 800.0F)}),
                     0.016F);
        EXPECT_TRUE(source.Current().menuPressed);
        EXPECT_TRUE(source.Current().runPressed);
        EXPECT_FLOAT_EQ(source.Current().move.X, 0.5F);
        EXPECT_NEAR(source.Current().look.X, 0.11F, 1e-6F);

        source.Apply(Frame({Finger(3, TouchLocationState::Moved, 1200.0F, 450.0F),
                            Finger(4, TouchLocationState::Moved, 1200.0F, 450.0F),
                            Finger(1, TouchLocationState::Moved, 290.0F, 700.0F),
                            Finger(2, TouchLocationState::Moved, 1250.0F, 450.0F)}),
                     0.016F);
        EXPECT_FALSE(source.Current().menuPressed);
        EXPECT_FALSE(source.Current().runPressed);
        EXPECT_FLOAT_EQ(source.Current().look.X, 0.0F);
        EXPECT_FLOAT_EQ(source.Current().move.X, 0.5F);
    }

    TEST(TouchSourceTests, ButtonHitBoxesFollowSafeVirtualCanvasOnWideDisplay)
    {
        TouchConfig config;
        config.viewportWidth = 2000;
        config.viewportHeight = 900;
        config.layoutX = 280;
        config.layoutY = 45;
        config.layoutWidth = 1440;
        config.layoutHeight = 810;
        TouchSource source(config);
        source.SetButtonsEnabled(true);
        source.Apply(Frame({Finger(1, TouchLocationState::Pressed, 1650.0F, 115.0F),
                            Finger(2, TouchLocationState::Pressed, 1650.0F, 785.0F)}),
                     0.016F);
        EXPECT_NEAR(source.Current().pointerX, 1370.0F / 1440.0F, 1e-6F);
        EXPECT_NEAR(source.Current().pointerY, 70.0F / 810.0F, 1e-6F);
        EXPECT_TRUE(source.Current().menuPressed);
        EXPECT_TRUE(source.Current().runPressed);
        EXPECT_FALSE(source.LookAvailable());
        source.Apply(Frame({}), 0.016F);
        source.Apply(Frame({Finger(3, TouchLocationState::Pressed, 1200.0F, 450.0F)}), 0.016F);
        source.Apply(Frame({Finger(3, TouchLocationState::Moved, 1250.0F, 450.0F)}), 0.016F);
        EXPECT_FALSE(source.Current().menuPressed);
        EXPECT_FALSE(source.Current().runPressed);
        EXPECT_NEAR(source.Current().look.X, 0.11F, 1e-6F);
    }

    TEST(TouchSourceTests, MenuPointerUsesPhysicalFontOffsetAndRejectsLetterboxPadding)
    {
        TouchConfig config;
        config.viewportWidth = 800;
        config.viewportHeight = 600;
        config.layoutY = 75;
        config.layoutWidth = 800;
        config.layoutHeight = 450;
        config.pointerOffsetY = 20.0F;
        TouchSource source(config);
        source.Apply(Frame({Finger(1, TouchLocationState::Pressed, 400.0F, 320.0F)}), 0.016F);
        ASSERT_TRUE(source.Current().pointerPressed);
        EXPECT_FLOAT_EQ(source.Current().pointerY, 0.5F);
        source.Apply(Frame({}), 0.016F);
        source.Apply(Frame({Finger(2, TouchLocationState::Pressed, 400.0F, 590.0F)}), 0.016F);
        EXPECT_FALSE(source.Current().pointerPressed);
        EXPECT_TRUE(source.Current().anyPressed);
    }

    TEST(TouchSourceTests, BuildProfileDefaultsKeepTouchHudOffDesktop)
    {
        const auto platform = cnahouse::app::Platform::FromBuild();
        if (platform.target == cnahouse::app::BuildTarget::Android)
        {
            EXPECT_TRUE(platform.hasTouch);
            EXPECT_FALSE(platform.hasKeyboard);
        }
        else
        {
            EXPECT_FALSE(platform.hasTouch);
            EXPECT_TRUE(platform.hasKeyboard);
        }
    }

    TEST(TouchSourceTests, WebProfileSelectsTouchOnlyAfterReportedTouch)
    {
        cnahouse::app::Platform web;
        web.target = cnahouse::app::BuildTarget::Web;
        EXPECT_FALSE(web.SelectWebTouchInput(false));
        EXPECT_FALSE(web.hasTouch);
        EXPECT_TRUE(web.hasKeyboard);
        EXPECT_TRUE(web.SelectWebTouchInput(true));
        EXPECT_TRUE(web.hasTouch);
        EXPECT_FALSE(web.hasKeyboard);
        EXPECT_FALSE(web.SelectWebTouchInput(true));

        cnahouse::app::Platform desktop;
        EXPECT_FALSE(desktop.SelectWebTouchInput(true));
        EXPECT_FALSE(desktop.hasTouch);
        EXPECT_TRUE(desktop.hasKeyboard);
    }
} // namespace
