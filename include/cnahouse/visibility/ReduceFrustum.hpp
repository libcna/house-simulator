// SPDX-License-Identifier: MIT
#pragma once

#include <span>

#include "Microsoft/Xna/Framework/Plane.hpp"
#include "Microsoft/Xna/Framework/Vector3.hpp"

#include "cnahouse/visibility/ClipFrustum.hpp"

namespace cnahouse::visibility
{

    /// @brief What `ReduceFrustum` made of a portal (`HOUSE-00663`).
    struct ReducedFrustum
    {
        ClipFrustum frustum;
        /// @brief True when every edge of the polygon became a side plane.
        ///
        /// False means the frustum is WIDER than the portal really allows -- an edge through the
        /// eye, or more edges than `ClipFrustum::kMaxPlanes` leaves room for. That over-draws and
        /// never over-culls, which is the only direction §25.4 permits, but a traversal that keeps
        /// seeing it is a traversal doing more work than it should and the caller can say so.
        bool complete = true;
        /// @brief Edges that produced no plane. Zero unless `complete` is false.
        std::size_t dropped = 0;
    };

    /// @brief §25.2's `ReduceFrustum`: *"build a new frustum whose side planes each contain the
    ///        camera position and one edge of the clipped polygon, keeping the original near and
    ///        far planes"*.
    ///
    /// This is the step that makes portal traversal worth doing. Without it, walking through a
    /// doorway hands the next room the whole camera frustum and the recursion tests everything;
    /// with it, the next room is tested against the cone the doorway actually admits, which is
    /// what makes a corridor of open doors cost anything less than the whole house.
    ///
    /// **The plane's sign comes from the polygon's own centroid, not from its winding.** A clipped
    /// polygon's order is whatever `ClipRectToFrustum` produced from whatever order the portal's
    /// corners were stored in, and one portal seen from its other side reverses it. Deciding
    /// "outward" by testing a point that is definitely INSIDE removes the question: `Cross` gives a
    /// normal, the centroid says which way it should face, and neither the winding nor which side
    /// of the wall the camera is on can get it wrong.
    ///
    /// @param eye the camera position -- §25.2's *"contain the camera position"*, and the apex of
    ///        every side plane.
    /// @param polygon the clipped portal, from `ClipRectToFrustum`. Fewer than three vertices is
    ///        not a portal and produces no frustum at all.
    /// @param nearPlane , farPlane the camera's own, kept unchanged: §25.2 reduces the SIDES.
    [[nodiscard]] ReducedFrustum ReduceFrustum(const Microsoft::Xna::Framework::Vector3& eye,
                                               std::span<const Microsoft::Xna::Framework::Vector3> polygon,
                                               const Microsoft::Xna::Framework::Plane& nearPlane,
                                               const Microsoft::Xna::Framework::Plane& farPlane);

} // namespace cnahouse::visibility
