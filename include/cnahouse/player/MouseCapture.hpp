// SPDX-License-Identifier: MIT
#pragma once

namespace cnahouse::player
{

    /// @brief What the game knows when it decides whether to hold the pointer.
    struct CaptureRequest
    {
        /// @brief `Game::IsActive`. **`HOUSE-00100` measured it true on all 9 999 frames of a
        ///        probe on this platform**, including while another window had focus, so it is
        ///        consulted and not relied on: what actually frees the cursor here is a menu or
        ///        the `Alt` key, and this field is what makes the policy right on a platform whose
        ///        `IsActive` works.
        bool windowActive = true;
        /// @brief Anything the player has to point at: §67's menus, and the loading screen.
        bool menuOpen = false;
        /// @brief §68's `Alt`, held. A player who wants their cursor back mid-game -- to reach a
        ///        second monitor, or the window's own close button -- should not have to open a
        ///        menu to get it.
        bool freeCursorHeld = false;
    };

    /// @brief §44's cursor: hidden and held during play, given back the moment it is wanted
    ///        (`HOUSE-00624`).
    ///
    /// **A policy rather than a flag, because three things ask for the cursor and one of them is
    /// a key being held.** Scattered `SetMouseCaptured` calls at each of the three sites gave
    /// whichever ran last the final say -- and the one that runs last is whichever the frame
    /// happened to reach, which is how a cursor ends up invisible over an open menu.
    class MouseCapturePolicy
    {
    public:
        /// @brief Decides this frame. @return true when the answer CHANGED, which is the only
        ///        time the caller has to tell XNA about it.
        bool Update(const CaptureRequest& request) noexcept;

        [[nodiscard]] bool Captured() const noexcept
        {
            return captured_;
        }

        /// @brief What `Game::setIsMouseVisibleProperty` should be given: the inverse, named for
        ///        the property it feeds so the call site reads as what it does.
        [[nodiscard]] bool CursorVisible() const noexcept
        {
            return !captured_;
        }

    private:
        bool captured_ = false;
    };

} // namespace cnahouse::player
