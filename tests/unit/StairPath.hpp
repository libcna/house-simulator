// SPDX-License-Identifier: MIT
#pragma once

// The path a body walks up a flight, from the collision RAMP itself.
//
// Extracted from `HOUSE-00615`'s guarantee test when `HOUSE-00632`'s tune pass needed to walk the
// same eight flights and measure the camera over them. Two copies of "where the waypoints up a
// staircase are" would be two chances for a tune pass to be measuring a path the guarantee is not
// walking -- and every awkward detail in here was learnt by walking into something.
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

#include "Microsoft/Xna/Framework/Vector3.hpp"

#include "cnahouse/physics/CollisionData.hpp"

namespace cnahouse::tests
{
    using Microsoft::Xna::Framework::Vector3;

    /// Close enough to a waypoint to move on to the next. A landing is 1.1 x 2.3 m and a body
    /// 0.60 m from its centre is standing on it; the flights are 1.1 m wide, so this is half a
    /// width. Tighter than this and a body stopped by the balustrade a hand's breadth short of a
    /// waypoint's exact centre reads as one that could not get there.
    inline constexpr float kStairArrived = 0.60F;

    /// Horizontal distance, which is the only kind that decides whether a body has arrived: the
    /// vertical part of "how far away is that waypoint" is what the flight is for.
    [[nodiscard]] inline float Flat(const Vector3& a, const Vector3& b)
    {
        const float dx = a.X - b.X;
        const float dz = a.Z - b.Z;
        return std::sqrt(dx * dx + dz * dz);
    }

    /// One piece of a flight, as the collision holds it: a wedge or a landing box.
    struct StairSegment
    {
        float lowY = 0.0F;
        float highY = 0.0F;
        Vector3 low;  ///< the middle of its bottom edge
        Vector3 high; ///< and of its top one; the same point for a landing
        bool landing = false;
    };

    /// Every stair segment of @p cell whose height is inside [@p from, @p to], in climbing order.
    [[nodiscard]] inline std::vector<StairSegment>
    SegmentsOf(const physics::CollisionWorld& world, const physics::CollisionCell& cell, float from, float to)
    {
        std::vector<StairSegment> segments;
        for (const std::uint32_t shape : cell.shapes)
        {
            if (shape < world.obbs.size())
            {
                const physics::CollisionObb& obb = world.obbs[shape];
                if (obb.kind != physics::CollisionKind::Stair)
                {
                    continue;
                }
                const float top = obb.centre.Y + obb.halfExtents.Y;
                if (top < from - 0.25F || top > to + 0.25F)
                {
                    continue;
                }
                StairSegment segment;
                segment.landing = true;
                segment.lowY = top;
                segment.highY = top;
                segment.low = Vector3(obb.centre.X, top, obb.centre.Z);
                segment.high = segment.low;
                segments.push_back(segment);
                continue;
            }
            const std::size_t index = shape - world.obbs.size();
            if (index >= world.meshes.size() || world.meshes[index].kind != physics::CollisionKind::Stair)
            {
                continue;
            }
            const physics::CollisionMesh& mesh = world.meshes[index];
            // Select only the upward-facing walking slope. The main switchback now has a
            // raked underside one riser below it; using the mesh AABB's minimum Y as the foot
            // would put the pad after the returning ramp in the route and mistake its underside
            // for the walking surface.
            std::vector<std::uint16_t> walking;
            for (std::size_t triangle = 0; triangle < mesh.indices.size(); triangle += 3U)
            {
                const auto ia = mesh.indices[triangle];
                const auto ib = mesh.indices[triangle + 1U];
                const auto ic = mesh.indices[triangle + 2U];
                const Vector3 normal = Vector3::Cross(mesh.vertices[ib] - mesh.vertices[ia],
                                                      mesh.vertices[ic] - mesh.vertices[ia]);
                const float length = normal.Length();
                if (length < 1.0e-6F || normal.Y / length <= 0.20F || normal.Y / length >= 0.999F)
                {
                    continue;
                }
                for (const auto vertex : {ia, ib, ic})
                {
                    if (std::find(walking.begin(), walking.end(), vertex) == walking.end())
                    {
                        walking.push_back(vertex);
                    }
                }
            }
            if (walking.empty())
            {
                continue;
            }
            float lowY = mesh.vertices[walking.front()].Y;
            float highY = lowY;
            for (const auto vertex : walking)
            {
                lowY = std::min(lowY, mesh.vertices[vertex].Y);
                highY = std::max(highY, mesh.vertices[vertex].Y);
            }
            if (lowY < from - 0.25F || highY > to + 0.25F)
            {
                continue;
            }
            const auto centreAt = [&](float height)
            {
                Vector3 sum;
                int count = 0;
                for (const auto vertex : walking)
                {
                    if (std::fabs(mesh.vertices[vertex].Y - height) < 1.0e-4F)
                    {
                        sum = sum + mesh.vertices[vertex];
                        ++count;
                    }
                }
                return sum / static_cast<float>(count);
            };
            StairSegment segment;
            segment.lowY = lowY;
            segment.highY = highY;
            segment.high = centreAt(highY);
            segment.low = centreAt(lowY);
            segments.push_back(segment);
        }
        std::sort(segments.begin(),
                  segments.end(),
                  [](const StairSegment& a, const StairSegment& b) { return a.lowY < b.lowY; });
        return segments;
    }

    /// The waypoints up @p segments, in climbing order, ending a stride past the top.
    [[nodiscard]] inline std::vector<Vector3> PathUp(const std::vector<StairSegment>& segments)
    {
        std::vector<Vector3> up;
        for (std::size_t i = 0; i < segments.size(); ++i)
        {
            const StairSegment& segment = segments[i];
            if (segment.landing)
            {
                if (i > 0 && i + 1 < segments.size() && !segments[i - 1].landing && !segments[i + 1].landing)
                {
                    // Cross a switchback on the *interior* of its full-width pad. Cutting
                    // diagonally to either toe meets a wedge cheek at the pad edge; the old
                    // waypoint at the returning toe did exactly that when walked downhill.
                    const Vector3& before = segments[i - 1].high;
                    const Vector3& after = segments[i + 1].low;
                    if (std::fabs(before.X - after.X) > std::fabs(before.Z - after.Z))
                    {
                        up.push_back(Vector3(before.X, segment.low.Y, segment.low.Z));
                        up.push_back(segment.low);
                        up.push_back(Vector3(after.X, segment.low.Y, segment.low.Z));
                    }
                    else
                    {
                        up.push_back(Vector3(segment.low.X, segment.low.Y, before.Z));
                        up.push_back(segment.low);
                        up.push_back(Vector3(segment.low.X, segment.low.Y, after.Z));
                    }
                }
                else
                {
                    up.push_back(segment.low);
                }
                continue;
            }
            if (i > 0)
            {
                // The foot of every run but the first: the body starts on that one.
                up.push_back(segment.low);
            }
            // Waypoints ALONG the run, not just at its ends. A body aimed two metres up a flight
            // walks the straight line to that point, which on a U-shaped stair cuts across the
            // well and into the next run's cheek; aimed a third of a metre ahead it follows the
            // lane it is standing in. Four steps up each run is enough for the 2.5 m ones here.
            for (int part = 1; part <= 4; ++part)
            {
                const float t = static_cast<float>(part) / 4.0F;
                up.push_back(Vector3(segment.low.X + (segment.high.X - segment.low.X) * t,
                                     segment.low.Y + (segment.high.Y - segment.low.Y) * t,
                                     segment.low.Z + (segment.high.Z - segment.low.Z) * t));
            }
        }
        // One stride past the top, and a stride is 0.40 m -- not a fraction of the run. A quarter
        // of the basement flight is 1.1 m, which walks the body past the head of the stairs and
        // into the corner of the well beyond it, where it spends the descent facing a wall.
        const StairSegment& last = segments.back();
        const float runX = last.high.X - last.low.X;
        const float runZ = last.high.Z - last.low.Z;
        const float runLength = std::max(1e-3F, std::sqrt(runX * runX + runZ * runZ));
        up.push_back(Vector3(
            last.high.X + runX / runLength * 0.40F, last.high.Y, last.high.Z + runZ / runLength * 0.40F));
        return up;
    }

    /// Where a body starts a climb: ON the first run, a seventh of the way up it.
    ///
    /// Not on the floor in front of it: the foot of a flight is where the NEXT flight down comes
    /// up, and a body backed off from the main stair's bottom tread is standing over the basement
    /// stairwell. Walking into a flight from the room is `HOUSE-00617`'s tour; this is about the
    /// flight.
    [[nodiscard]] inline Vector3 FootStart(const std::vector<StairSegment>& segments)
    {
        const StairSegment& first = segments.front();
        return Vector3(first.low.X + (first.high.X - first.low.X) * 0.15F,
                       first.low.Y + (first.high.Y - first.low.Y) * 0.15F,
                       first.low.Z + (first.high.Z - first.low.Z) * 0.15F);
    }

} // namespace cnahouse::tests
