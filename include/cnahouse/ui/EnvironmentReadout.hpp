// SPDX-License-Identifier: MIT
#pragma once

#include <string>

#include "cnahouse/environment/SimClock.hpp"

namespace Microsoft::Xna::Framework::Graphics
{
    class SpriteBatch;
}

namespace cnahouse::ui
{
    class TextRenderer;

    /// @brief §67's compact, player-facing time/season/temperature readout (`HOUSE-01546`).
    ///
    /// Deliberately not a debug overlay: there is no key, diagnostic label, epoch, rate or
    /// implementation status. `Line` and `Draw` share the same presentation so unit tests and the
    /// rendered HUD cannot disagree about what a player sees.
    class EnvironmentReadout
    {
    public:
        [[nodiscard]] std::string Line(const environment::SimClock& clock) const;

        void Draw(Microsoft::Xna::Framework::Graphics::SpriteBatch& batch,
                  const TextRenderer& text,
                  const environment::SimClock& clock) const;
    };

} // namespace cnahouse::ui
