// SPDX-License-Identifier: MIT
#pragma once

#include <span>

#include "Microsoft/Xna/Framework/Matrix.hpp"
#include "Microsoft/Xna/Framework/Vector3.hpp"

namespace cnahouse::visibility
{

    /// @brief The whole screen, in the units `NdcArea` returns.
    ///
    /// Normalised device coordinates run from -1 to +1 in x and y, so the visible square is 2 x 2.
    /// Every area below is a fraction of this.
    inline constexpr float kFullScreenNdcArea = 4.0F;

    /// @brief §25.2's cutoff: *"about 2 x 2 pixels at 1280 x 720. Below that the target cell
    ///        contributes nothing and the chain stops."*
    ///
    /// **This single number is what keeps a corridor of eight open doors from exploding.** Each
    /// portal in a chain admits less than the one before it (`ReduceFrustum` is monotone), so the
    /// chain terminates on its own -- but only eventually, and "eventually" down a hallway of
    /// doorways is six or seven rooms of traversal for a few pixels of picture.
    ///
    /// The arithmetic, because §25.2's *"about"* is doing some work: at 1280 x 720 one pixel is
    /// `4 / (1280 x 720)` = 4.34e-6 of the square, so 1.2e-5 is **2.8 pixels** -- 1.66 x 1.66,
    /// not quite the 2 x 2 the sentence rounds it to. The constant is §25.2's and is kept; what is
    /// recorded here is what it actually means, so that nobody re-derives 2 x 2 into 1.74e-5 and
    /// wonders why the chains got shorter.
    inline constexpr float kMinPortalNdcArea = 1.2e-5F;

    /// @brief The area a clipped portal covers on screen, where the whole screen is 4
    ///        (`HOUSE-00664`).
    ///
    /// **A vertex behind the eye makes this meaningless, and the answer then is "do not cull".**
    /// The perspective divide by a non-positive `w` puts the projected point somewhere it is not,
    /// and a portal reported as tiny because half of it is behind the camera is a room that
    /// vanishes when the player stands in its doorway. The caller has usually clipped against the
    /// near plane already (`HOUSE-00662`), which makes this unreachable; when it is reached, the
    /// whole screen is returned, which is the conservative answer.
    ///
    /// @param polygon the clipped portal in WORLD space, from `ClipRectToFrustum`.
    /// @param viewProjection `View() * Projection()` -- the same matrix `HOUSE-00630`'s frustum is
    ///        built from, so the two agree about where the screen is.
    [[nodiscard]] float NdcArea(std::span<const Microsoft::Xna::Framework::Vector3> polygon,
                                const Microsoft::Xna::Framework::Matrix& viewProjection);

    /// @brief §25.2's decision: is this portal worth walking through?
    [[nodiscard]] bool PortalContributes(std::span<const Microsoft::Xna::Framework::Vector3> polygon,
                                         const Microsoft::Xna::Framework::Matrix& viewProjection);

} // namespace cnahouse::visibility
