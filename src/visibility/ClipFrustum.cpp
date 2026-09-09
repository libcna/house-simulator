// SPDX-License-Identifier: MIT
#include "cnahouse/visibility/ClipFrustum.hpp"

#include "Microsoft/Xna/Framework/BoundingBox.hpp"
#include "Microsoft/Xna/Framework/BoundingFrustum.hpp"
#include "Microsoft/Xna/Framework/BoundingSphere.hpp"
#include "Microsoft/Xna/Framework/PlaneIntersectionType.hpp"

namespace cnahouse::visibility
{
    namespace
    {
        namespace Xna = Microsoft::Xna::Framework;

        /// The loop `BoundingFrustum::Contains` runs, over however many planes there are.
        ///
        /// `Front` is outside -- one plane with the whole shape in front of it is enough to be
        /// done -- `Intersecting` means the answer cannot be `Contains` any more, and `Back` is
        /// the ordinary inside case. Writing it once over a template keeps the box and the sphere
        /// from drifting apart, which is exactly what happened to the two copies this replaced.
        template<typename Shape, typename Planes>
        Xna::ContainmentType ContainsShape(const Planes& planes, std::size_t count, const Shape& shape)
        {
            bool intersects = false;
            for (std::size_t i = 0; i < count; ++i)
            {
                Xna::PlaneIntersectionType type{};
                shape.Intersects(planes[i], type);
                if (type == Xna::PlaneIntersectionType::Front)
                {
                    return Xna::ContainmentType::Disjoint;
                }
                if (type == Xna::PlaneIntersectionType::Intersecting)
                {
                    intersects = true;
                }
            }
            return intersects ? Xna::ContainmentType::Intersects : Xna::ContainmentType::Contains;
        }
    } // namespace

    ClipFrustum::ClipFrustum(const Xna::BoundingFrustum& frustum)
    {
        // XNA's own order: near, far, left, right, top, bottom. The order does not change any
        // answer -- every plane is tested -- but keeping it makes a frustum built from an XNA one
        // print the same as the one it came from, which is what a failing test is read against.
        for (const Xna::Plane& plane : {frustum.getNearProperty(),
                                        frustum.getFarProperty(),
                                        frustum.getLeftProperty(),
                                        frustum.getRightProperty(),
                                        frustum.getTopProperty(),
                                        frustum.getBottomProperty()})
        {
            (void)Add(plane);
        }
    }

    bool ClipFrustum::Add(const Xna::Plane& plane) noexcept
    {
        if (count_ >= kMaxPlanes)
        {
            return false;
        }
        planes_[count_] = plane;
        ++count_;
        return true;
    }

    Xna::ContainmentType ClipFrustum::Contains(const Xna::BoundingBox& box) const
    {
        return ContainsShape(planes_, count_, box);
    }

    Xna::ContainmentType ClipFrustum::Contains(const Xna::BoundingSphere& sphere) const
    {
        return ContainsShape(planes_, count_, sphere);
    }

    Xna::ContainmentType ClipFrustum::Contains(const Xna::Vector3& point) const
    {
        // A point is never `Intersects`: it is on one side of every plane or exactly on one, and
        // XNA counts "exactly on" as inside for a box, so it does here too.
        for (std::size_t i = 0; i < count_; ++i)
        {
            if (planes_[i].DotCoordinate(point) > 0.0F)
            {
                return Xna::ContainmentType::Disjoint;
            }
        }
        return Xna::ContainmentType::Contains;
    }

    bool ClipFrustum::Intersects(const Xna::BoundingBox& box) const
    {
        return Contains(box) != Xna::ContainmentType::Disjoint;
    }

    bool ClipFrustum::Intersects(const Xna::BoundingSphere& sphere) const
    {
        return Contains(sphere) != Xna::ContainmentType::Disjoint;
    }

} // namespace cnahouse::visibility
