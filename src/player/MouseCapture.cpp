// SPDX-License-Identifier: MIT
#include "cnahouse/player/MouseCapture.hpp"

namespace cnahouse::player
{
    bool MouseCapturePolicy::Update(const CaptureRequest& request) noexcept
    {
        // Every reason to release wins over the one reason to hold. A player looking at a menu, a
        // player holding Alt and a window that has lost focus all want the pointer back, and a
        // policy that ANDed them would need all three to be false at once to give it up.
        const bool wanted = request.windowActive && !request.menuOpen && !request.freeCursorHeld;
        if (wanted == captured_)
        {
            return false;
        }
        captured_ = wanted;
        return true;
    }

} // namespace cnahouse::player
