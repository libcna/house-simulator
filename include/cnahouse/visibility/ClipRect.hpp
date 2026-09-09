// SPDX-License-Identifier: MIT
#pragma once

#include <array>
#include <cstddef>
#include <span>

#include "Microsoft/Xna/Framework/Plane.hpp"
#include "Microsoft/Xna/Framework/Vector3.hpp"

namespace cnahouse::visibility
{

    /// @brief The most vertices a clipped portal polygon can have.
    ///
    /// Sutherland-Hodgman adds at most one vertex per clipping plane, so a four-sided doorway
    /// against §25.2's reduced frustum -- eight side planes at the very most -- cannot exceed
    /// twelve. §25.2 quotes eight, which is the same arithmetic for the camera's own four side
    /// planes; twelve is that bound followed all the way down the traversal.
    inline constexpr std::size_t kMaxClippedVertices = 12;

    /// @brief What is left of a portal's rectangle after the frustum has cut it (`HOUSE-00662`).
    struct ClippedPolygon
    {
        std::array<Microsoft::Xna::Framework::Vector3, kMaxClippedVertices> points{};
        std::size_t count = 0;
        /// @brief The clip needed more room than `kMaxClippedVertices`, so it stopped early.
        ///
        /// Cannot happen for any frustum §25.2 builds -- the arithmetic above is a bound, not a
        /// hope -- and is reported rather than asserted because the honest response is to use the
        /// polygon as it stands: one clipped by fewer planes is LARGER than the true answer, which
        /// over-draws rather than culling something visible.
        bool overflowed = false;

        [[nodiscard]] bool Empty() const noexcept
        {
            return count == 0;
        }

        [[nodiscard]] std::span<const Microsoft::Xna::Framework::Vector3> Points() const noexcept
        {
            return std::span(points.data(), count);
        }
    };

    /// @brief §25.2's `ClipRectToFrustum`: the doorway rectangle, cut down to what can be seen
    ///        through it.
    ///
    /// **Sutherland-Hodgman against half-spaces, in world space.** §25.2 calls it a 2-D clip
    /// because a portal is *"an axis-aligned rectangle on an axis-aligned plane"*, and it is --
    /// but the planes it is cut by are not axis-aligned, and projecting into some 2-D frame first
    /// would cost a basis, two transforms and a special case for a portal seen edge-on. Clipping
    /// the four world-space corners against the planes directly is the same algorithm with none of
    /// that, and it is exact.
    ///
    /// @param rect the portal's four corners in world space, in order round the rectangle --
    ///        `PortalRuntime::WorldRect()` (`HOUSE-00665`).
    /// @param planes the SIDE planes to cut against, with §25.2's outward normals: a point is
    ///        outside where `DotCoordinate > 0`. The near and far planes are deliberately not the
    ///        caller's obligation to leave out -- clipping against them is harmless and cutting a
    ///        portal that crosses the near plane is necessary -- but the polygon this produces is
    ///        what `ReduceFrustum` builds side planes from, and a side plane through the camera
    ///        and an edge lying IN the near plane is a plane through the camera's own eye.
    [[nodiscard]] ClippedPolygon ClipRectToFrustum(std::span<const Microsoft::Xna::Framework::Vector3> rect,
                                                   std::span<const Microsoft::Xna::Framework::Plane> planes);

    /// @brief The area of a planar convex polygon, in square metres. Zero for fewer than three
    ///        vertices, which is what a portal clipped to a line or a point is.
    [[nodiscard]] float PolygonArea(std::span<const Microsoft::Xna::Framework::Vector3> points);

} // namespace cnahouse::visibility
