// SPDX-License-Identifier: MIT
//
// `HOUSE-00624`. §44 hides the cursor and holds it during play; §67's menus and §68's `Alt` want
// it back. Three reasons to release and one to hold, which is why this is a policy and not a flag.
#include <gtest/gtest.h>

#include "cnahouse/player/MouseCapture.hpp"

namespace
{
    using cnahouse::player::CaptureRequest;
    using cnahouse::player::MouseCapturePolicy;

    CaptureRequest Playing()
    {
        CaptureRequest request;
        request.windowActive = true;
        request.menuOpen = false;
        request.freeCursorHeld = false;
        return request;
    }

} // namespace

TEST(MouseCaptureTests, EveryReasonToReleaseWinsOverTheOneToHold)
{
    // The whole truth table, because the interesting property is that it is an OR of releases and
    // not an AND: a policy that needed all three reasons at once to give the pointer up would hide
    // the cursor over an open menu whenever the player also happened to be holding Alt.
    MouseCapturePolicy policy;
    for (const bool active : {false, true})
    {
        for (const bool menu : {false, true})
        {
            for (const bool alt : {false, true})
            {
                CaptureRequest request;
                request.windowActive = active;
                request.menuOpen = menu;
                request.freeCursorHeld = alt;
                policy.Update(request);
                const bool expected = active && !menu && !alt;
                EXPECT_EQ(policy.Captured(), expected)
                    << "active " << active << " menu " << menu << " alt " << alt;
                EXPECT_EQ(policy.CursorVisible(), !expected) << "the cursor and the capture disagree";
            }
        }
    }
}

TEST(MouseCaptureTests, ItStartsReleasedAndSaysWhenItChanges)
{
    // The game only talks to XNA when the answer changes -- `Mouse::SetPosition` every frame while
    // a menu is open would fight the pointer the player is trying to use -- so "did it change" is
    // as much of the interface as the state is.
    MouseCapturePolicy policy;
    EXPECT_FALSE(policy.Captured()) << "the cursor was taken before the first frame";
    EXPECT_TRUE(policy.CursorVisible());

    EXPECT_TRUE(policy.Update(Playing())) << "taking the cursor was not reported";
    EXPECT_TRUE(policy.Captured());
    EXPECT_FALSE(policy.Update(Playing())) << "an unchanged frame reported a change";
    EXPECT_FALSE(policy.Update(Playing()));

    CaptureRequest menu = Playing();
    menu.menuOpen = true;
    EXPECT_TRUE(policy.Update(menu));
    EXPECT_FALSE(policy.Captured());
    EXPECT_FALSE(policy.Update(menu));

    // ...and back, when the menu closes.
    EXPECT_TRUE(policy.Update(Playing()));
    EXPECT_TRUE(policy.Captured());
}

TEST(MouseCaptureTests, AltGivesItBackWhileItIsHeldAndTakesItAfterwards)
{
    // §68's release is a LEVEL and not a toggle: hold it to reach the second monitor, let go to
    // carry on playing. A toggle would leave a player who alt-tabbed away with a cursor they did
    // not ask for on the way back.
    MouseCapturePolicy policy;
    policy.Update(Playing());
    ASSERT_TRUE(policy.Captured());

    CaptureRequest alt = Playing();
    alt.freeCursorHeld = true;
    EXPECT_TRUE(policy.Update(alt));
    EXPECT_FALSE(policy.Captured());

    EXPECT_TRUE(policy.Update(Playing()));
    EXPECT_TRUE(policy.Captured());
}
