// SPDX-License-Identifier: MIT
#pragma once

#include <array>
#include <cstdint>

#include "cnahouse/world/WorldTypes.hpp"

/// @file
/// `PortalRuntime` -- what the visibility walk needs to know about one portal, per frame
/// (`HOUSE-00665`, `cna-house.md` §25.3).

namespace cnahouse::visibility
{

    /// @brief One portal's mutable state: how far its leaf is open, and what that means.
    ///
    /// **The leaf does not shrink the doorway.** §25.3 settles the question the brief asks about
    /// partially open doors: the portal is the *hole in the wall*, and a sight line crossing that
    /// hole is never blocked by the leaf, because the leaf has swung out of the plane into one of
    /// the two cells. So above the latch threshold the aperture is the **whole** rectangle and the
    /// leaf becomes an ordinary opaque object standing in the target cell, hiding what is behind
    /// it exactly as a wardrobe would. A door open 10° reveals a thin slice of the room because the
    /// leaf blocks the rest, not because the portal was narrow.
    ///
    /// **The world rectangle is cached.** The visibility walk clips it against the frustum for
    /// every portal it reaches, several times a frame; deriving four corners from an axis, a plane
    /// value and a `(u, v)` rectangle each time is arithmetic nobody needs to repeat.
    ///
    /// **Vision and movement are different questions.** A closed glass door stops you walking
    /// through and does not stop you seeing through (§25.3); a closed opaque one stops both, and
    /// that is where the win is -- with the kitchen door shut, the kitchen's chunks, props, lights
    /// and further portals all leave consideration in one test.
    class PortalRuntime
    {
    public:
        /// @brief §25.3's latch. Treated as closed below this...
        static constexpr float kClosedBelow = 0.05F;
        /// @brief ...and as open above this, so a door settling shut does not flicker the portal
        /// on and off for a frame or two on the way.
        static constexpr float kOpenAbove = 0.08F;

        PortalRuntime() = default;

        /// @brief Caches @p portal's world rectangle and takes its opacity.
        ///
        /// An always-open portal -- a cased opening, a stair well -- starts open; anything with a
        /// leaf starts closed, which is the state §65.6 says the house begins in.
        explicit PortalRuntime(const world::Portal& portal) noexcept;

        /// @brief The four corners of the doorway, in world space, in a stable order.
        [[nodiscard]] const std::array<Microsoft::Xna::Framework::Vector3, 4>& WorldRect() const noexcept
        {
            return corners_;
        }

        [[nodiscard]] float Aperture() const noexcept
        {
            return aperture_;
        }

        /// @brief Moves the leaf. Returns true when the latched state changed.
        bool SetAperture(float fraction) noexcept;

        /// @brief The latched state: not `aperture > 0`, which flickers.
        [[nodiscard]] bool IsOpen() const noexcept
        {
            return open_;
        }

        /// @brief Can the visibility walk cross this portal?
        ///
        /// A portal with no leaf always; a glass one always, closed or not; anything opaque only
        /// while the latch says open.
        [[nodiscard]] bool PassesLight() const noexcept;

        /// @brief Can the player walk through it?
        ///
        /// Glass included: a closed glass door is a closed door to everything except vision.
        [[nodiscard]] bool PassesBodies() const noexcept;

        [[nodiscard]] world::PortalOpacity Opacity() const noexcept
        {
            return opacity_;
        }

    private:
        std::array<Microsoft::Xna::Framework::Vector3, 4> corners_{};
        world::PortalOpacity opacity_ = world::PortalOpacity::Open;
        float aperture_ = 0.0F;
        bool open_ = true;
        bool hasLeaf_ = false;
    };

} // namespace cnahouse::visibility
