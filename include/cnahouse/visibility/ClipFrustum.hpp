// SPDX-License-Identifier: MIT
#pragma once

#include <array>
#include <cstddef>

#include "Microsoft/Xna/Framework/ContainmentType.hpp"
#include "Microsoft/Xna/Framework/Plane.hpp"

namespace Microsoft::Xna::Framework
{
    struct BoundingBox;
    struct BoundingSphere;
    class BoundingFrustum;
} // namespace Microsoft::Xna::Framework

namespace cnahouse::visibility
{

    /// @brief §25.2's reduced frustum: up to ten planes, tested the way XNA tests six
    ///        (`HOUSE-00661`).
    ///
    /// **Why this exists at all.** `BoundingFrustum` is fixed at six planes and offers no way to
    /// add one, and §25.2's traversal builds a frustum whose sides come from the edges of a
    /// portal's clipped polygon -- *"a convex polygon of ≤ 8 vertices"*, so up to eight sides plus
    /// the near and far ones the camera started with. Ten is that bound, and it is a bound rather
    /// than a guess: `ClipRectToFrustum` (`HOUSE-00662`) clips an axis-aligned rectangle against
    /// four side planes, and four cuts of a four-sided polygon cannot produce more than eight
    /// edges.
    ///
    /// **It is built on `Plane` and nothing else** -- a Tier-A construction, no CNAEXT (§25.2).
    ///
    /// **The plane convention is XNA's, exactly.** A frustum's normals point OUTWARD: a box
    /// entirely on a plane's `Front` side is outside the frustum. That is what
    /// `BoundingFrustum::Contains` means by `PlaneIntersectionType::Front` -> `Disjoint`, and
    /// matching it is what lets `HOUSE-00630`'s frustum and this one be compared box for box --
    /// which is how this type is tested.
    class ClipFrustum
    {
    public:
        /// @brief §25.2's bound: eight sides from the clipped polygon plus a near and a far plane.
        static constexpr std::size_t kMaxPlanes = 10;

        ClipFrustum() = default;

        /// @brief The six planes of an XNA frustum, in XNA's own order.
        explicit ClipFrustum(const Microsoft::Xna::Framework::BoundingFrustum& frustum);

        /// @brief Adds one plane. Returns false when there is no room, and changes nothing.
        ///
        /// A `bool` and not an exception or a silent drop: a traversal that produced a
        /// nine-vertex polygon has found something this design did not expect, and the caller --
        /// which can still fall back to the unreduced frustum -- is the only thing that can decide
        /// what to do about it. Dropping the plane silently would make the frustum too WIDE, which
        /// over-draws rather than over-culls, but it would do it invisibly.
        [[nodiscard]] bool Add(const Microsoft::Xna::Framework::Plane& plane) noexcept;

        [[nodiscard]] std::size_t PlaneCount() const noexcept
        {
            return count_;
        }

        [[nodiscard]] const Microsoft::Xna::Framework::Plane& operator[](std::size_t index) const
        {
            return planes_[index];
        }

        /// @brief §25.4's per-cell and per-chunk test.
        ///
        /// A frustum with NO planes contains everything, which is the right answer and a useful
        /// one: it is the identity the traversal starts from and what a caller falls back to when
        /// a reduction fails.
        [[nodiscard]] Microsoft::Xna::Framework::ContainmentType
        Contains(const Microsoft::Xna::Framework::BoundingBox& box) const;

        [[nodiscard]] Microsoft::Xna::Framework::ContainmentType
        Contains(const Microsoft::Xna::Framework::BoundingSphere& sphere) const;

        [[nodiscard]] Microsoft::Xna::Framework::ContainmentType
        Contains(const Microsoft::Xna::Framework::Vector3& point) const;

        [[nodiscard]] bool Intersects(const Microsoft::Xna::Framework::BoundingBox& box) const;

        [[nodiscard]] bool Intersects(const Microsoft::Xna::Framework::BoundingSphere& sphere) const;

    private:
        std::array<Microsoft::Xna::Framework::Plane, kMaxPlanes> planes_{};
        std::size_t count_ = 0;
    };

} // namespace cnahouse::visibility
