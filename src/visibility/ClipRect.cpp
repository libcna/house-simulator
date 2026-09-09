// SPDX-License-Identifier: MIT
#include "cnahouse/visibility/ClipRect.hpp"

#include <cmath>

namespace cnahouse::visibility
{
    namespace
    {
        namespace Xna = Microsoft::Xna::Framework;

        /// A point this close to a plane counts as ON it, and on it counts as inside.
        ///
        /// A doorway lying exactly IN one of the frustum's planes is not a hypothetical: the
        /// camera's near plane is 0.10 m in front of an eye that can stand 0.30 m from a wall, and
        /// a body in a doorway is standing in the plane of the portal it is looking through.
        /// Without the tolerance those cases lose vertices to rounding, and a portal that loses a
        /// vertex is a portal that culls half the room behind it.
        constexpr float kOnPlane = 1e-5F;

        Xna::Vector3 Lerp(const Xna::Vector3& a, const Xna::Vector3& b, float t)
        {
            return Xna::Vector3(a.X + (b.X - a.X) * t, a.Y + (b.Y - a.Y) * t, a.Z + (b.Z - a.Z) * t);
        }
    } // namespace

    ClippedPolygon ClipRectToFrustum(std::span<const Xna::Vector3> rect, std::span<const Xna::Plane> planes)
    {
        ClippedPolygon polygon;
        if (rect.size() < 3 || rect.size() > kMaxClippedVertices)
        {
            return polygon;
        }
        for (const Xna::Vector3& corner : rect)
        {
            polygon.points[polygon.count] = corner;
            ++polygon.count;
        }

        for (const Xna::Plane& plane : planes)
        {
            if (polygon.count == 0)
            {
                // Everything was cut away by an earlier plane. Nothing later can bring it back,
                // and the portal is not visible.
                break;
            }

            ClippedPolygon next;
            for (std::size_t i = 0; i < polygon.count; ++i)
            {
                const Xna::Vector3& from = polygon.points[i];
                const Xna::Vector3& to = polygon.points[(i + 1) % polygon.count];
                const float fromSide = plane.DotCoordinate(from);
                const float toSide = plane.DotCoordinate(to);
                const bool fromInside = fromSide <= kOnPlane;
                const bool toInside = toSide <= kOnPlane;

                const auto emit = [&next, &polygon](const Xna::Vector3& point)
                {
                    if (next.count >= kMaxClippedVertices)
                    {
                        polygon.overflowed = true;
                        return;
                    }
                    next.points[next.count] = point;
                    ++next.count;
                };

                if (fromInside)
                {
                    emit(from);
                }
                if (fromInside != toInside)
                {
                    // Where the edge crosses the plane. The denominator cannot be zero: the two
                    // ends are on opposite sides, so their signed distances differ.
                    emit(Lerp(from, to, fromSide / (fromSide - toSide)));
                }
            }

            if (polygon.overflowed)
            {
                // Keep the polygon as it was before this plane. It is LARGER than the true clip,
                // which over-draws; a truncated one is not a polygon at all.
                break;
            }
            next.overflowed = polygon.overflowed;
            polygon = next;
        }
        return polygon;
    }

    float PolygonArea(std::span<const Xna::Vector3> points)
    {
        if (points.size() < 3)
        {
            return 0.0F;
        }
        // The magnitude of half the summed cross products of the fan from vertex 0. Planar and
        // convex is all this needs, which is what Sutherland-Hodgman of a rectangle produces.
        Xna::Vector3 sum(0.0F, 0.0F, 0.0F);
        for (std::size_t i = 1; i + 1 < points.size(); ++i)
        {
            const Xna::Vector3 a(
                points[i].X - points[0].X, points[i].Y - points[0].Y, points[i].Z - points[0].Z);
            const Xna::Vector3 b(
                points[i + 1].X - points[0].X, points[i + 1].Y - points[0].Y, points[i + 1].Z - points[0].Z);
            sum = Xna::Vector3(sum.X + (a.Y * b.Z - a.Z * b.Y),
                               sum.Y + (a.Z * b.X - a.X * b.Z),
                               sum.Z + (a.X * b.Y - a.Y * b.X));
        }
        return 0.5F * std::sqrt(sum.X * sum.X + sum.Y * sum.Y + sum.Z * sum.Z);
    }

} // namespace cnahouse::visibility
