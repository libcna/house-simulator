// SPDX-License-Identifier: MIT
#include "cnahouse/visibility/ReduceFrustum.hpp"

#include <cmath>

namespace cnahouse::visibility
{
    namespace
    {
        namespace Xna = Microsoft::Xna::Framework;

        /// An edge whose plane normal is shorter than this passes through the eye, or is a
        /// vertex repeated. Either way there is no plane to build, and the edge is skipped --
        /// which widens the frustum rather than narrowing it wrongly.
        constexpr float kDegenerate = 1e-9F;

        Xna::Vector3 Cross(const Xna::Vector3& a, const Xna::Vector3& b)
        {
            return Xna::Vector3(a.Y * b.Z - a.Z * b.Y, a.Z * b.X - a.X * b.Z, a.X * b.Y - a.Y * b.X);
        }

        Xna::Vector3 Centroid(std::span<const Xna::Vector3> points)
        {
            Xna::Vector3 sum(0.0F, 0.0F, 0.0F);
            for (const Xna::Vector3& point : points)
            {
                sum = Xna::Vector3(sum.X + point.X, sum.Y + point.Y, sum.Z + point.Z);
            }
            const auto n = static_cast<float>(points.size());
            return Xna::Vector3(sum.X / n, sum.Y / n, sum.Z / n);
        }
    } // namespace

    ReducedFrustum ReduceFrustum(const Xna::Vector3& eye,
                                 std::span<const Xna::Vector3> polygon,
                                 const Xna::Plane& nearPlane,
                                 const Xna::Plane& farPlane)
    {
        ReducedFrustum reduced;
        if (polygon.size() < 3)
        {
            // Not a portal: a line or a point admits no cone. The caller skips it, which is what
            // `kMinPortalNdcArea` (`HOUSE-00664`) would have decided a moment later anyway.
            reduced.complete = false;
            reduced.dropped = polygon.size();
            return reduced;
        }

        // §25.2 keeps the camera's own near and far planes, and they go in first so that a
        // polygon with more edges than there is room for loses SIDES rather than losing the far
        // plane -- a frustum without its far plane is unbounded, and the traversal would test the
        // whole world against it.
        (void)reduced.frustum.Add(nearPlane);
        (void)reduced.frustum.Add(farPlane);

        const Xna::Vector3 middle = Centroid(polygon);
        for (std::size_t i = 0; i < polygon.size(); ++i)
        {
            const Xna::Vector3& from = polygon[i];
            const Xna::Vector3& to = polygon[(i + 1) % polygon.size()];
            const Xna::Vector3 a(from.X - eye.X, from.Y - eye.Y, from.Z - eye.Z);
            const Xna::Vector3 b(to.X - eye.X, to.Y - eye.Y, to.Z - eye.Z);

            Xna::Vector3 normal = Cross(a, b);
            const float length = std::sqrt(normal.X * normal.X + normal.Y * normal.Y + normal.Z * normal.Z);
            if (length < kDegenerate)
            {
                reduced.complete = false;
                ++reduced.dropped;
                continue;
            }
            normal = Xna::Vector3(normal.X / length, normal.Y / length, normal.Z / length);

            Xna::Plane plane(normal, -(normal.X * eye.X + normal.Y * eye.Y + normal.Z * eye.Z));
            if (plane.DotCoordinate(middle) > 0.0F)
            {
                // The normal came out pointing INTO the polygon. Which way round that lands
                // depends on the winding, and the winding depends on which side of the wall the
                // camera is standing -- so it is decided here, by a point that is known to be
                // inside, rather than assumed.
                plane = Xna::Plane(Xna::Vector3(-normal.X, -normal.Y, -normal.Z), -plane.D);
            }

            if (!reduced.frustum.Add(plane))
            {
                reduced.complete = false;
                ++reduced.dropped;
            }
        }
        return reduced;
    }

} // namespace cnahouse::visibility
