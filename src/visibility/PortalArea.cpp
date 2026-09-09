// SPDX-License-Identifier: MIT
#include "cnahouse/visibility/PortalArea.hpp"

#include <algorithm>
#include <array>
#include <cmath>

#include "cnahouse/visibility/ClipRect.hpp"

namespace cnahouse::visibility
{
    namespace
    {
        namespace Xna = Microsoft::Xna::Framework;

        /// Nearer to the eye than this and the perspective divide is meaningless. §10.3's near
        /// plane is 0.10 m, so a `w` this small cannot come from a point the camera can see.
        constexpr float kMinW = 1e-4F;
    } // namespace

    float NdcArea(std::span<const Xna::Vector3> polygon, const Xna::Matrix& viewProjection)
    {
        if (polygon.size() < 3 || polygon.size() > kMaxClippedVertices)
        {
            // A portal clipped to a line or a point covers nothing. Not the conservative answer,
            // and it does not need to be: an area of zero IS the truth here.
            return 0.0F;
        }

        std::array<float, kMaxClippedVertices> x{};
        std::array<float, kMaxClippedVertices> y{};
        for (std::size_t i = 0; i < polygon.size(); ++i)
        {
            const Xna::Vector3& p = polygon[i];
            const float w = p.X * viewProjection.M14 + p.Y * viewProjection.M24 + p.Z * viewProjection.M34 +
                            viewProjection.M44;
            if (w < kMinW)
            {
                // Behind the eye, or on it. Do NOT cull: a portal reported as tiny because half of
                // it is behind the camera is a room that vanishes when the player stands in its
                // doorway.
                return kFullScreenNdcArea;
            }
            x[i] = (p.X * viewProjection.M11 + p.Y * viewProjection.M21 + p.Z * viewProjection.M31 +
                    viewProjection.M41) /
                   w;
            y[i] = (p.X * viewProjection.M12 + p.Y * viewProjection.M22 + p.Z * viewProjection.M32 +
                    viewProjection.M42) /
                   w;
        }

        // The shoelace formula, absolute: the winding of a clipped polygon depends on which side of
        // the wall the camera is standing, and an area is an area either way.
        float twice = 0.0F;
        for (std::size_t i = 0; i < polygon.size(); ++i)
        {
            const std::size_t j = (i + 1) % polygon.size();
            twice += x[i] * y[j] - x[j] * y[i];
        }
        return std::fabs(twice) * 0.5F;
    }

    NdcRect NdcBounds(std::span<const Xna::Vector3> polygon, const Xna::Matrix& viewProjection)
    {
        NdcRect rect;
        if (polygon.empty() || polygon.size() > kMaxClippedVertices)
        {
            return rect;
        }
        for (const Xna::Vector3& p : polygon)
        {
            const float w = p.X * viewProjection.M14 + p.Y * viewProjection.M24 + p.Z * viewProjection.M34 +
                            viewProjection.M44;
            if (w < kMinW)
            {
                // The same rule as `NdcArea`: a portal the camera is standing in covers
                // everything, because the alternative is a room that vanishes in its own doorway.
                return NdcRect{-1.0F, -1.0F, 1.0F, 1.0F};
            }
            const float x = (p.X * viewProjection.M11 + p.Y * viewProjection.M21 + p.Z * viewProjection.M31 +
                             viewProjection.M41) /
                            w;
            const float y = (p.X * viewProjection.M12 + p.Y * viewProjection.M22 + p.Z * viewProjection.M32 +
                             viewProjection.M42) /
                            w;
            rect.minX = std::min(rect.minX, x);
            rect.minY = std::min(rect.minY, y);
            rect.maxX = std::max(rect.maxX, x);
            rect.maxY = std::max(rect.maxY, y);
        }
        return rect;
    }

    bool PortalContributes(std::span<const Xna::Vector3> polygon, const Xna::Matrix& viewProjection)
    {
        return NdcArea(polygon, viewProjection) >= kMinPortalNdcArea;
    }

} // namespace cnahouse::visibility
