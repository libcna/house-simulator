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

    /// The centroid of every vertex within a centimetre of @p wanted, optionally only those
    /// further than @p beyond from @p away.
    [[nodiscard]] inline Vector3 EdgeCentre(const physics::CollisionMesh& mesh,
                                            float wanted,
                                            const Vector3* away = nullptr,
                                            float beyond = 0.0F)
    {
        Vector3 sum;
        int count = 0;
        for (const Vector3& vertex : mesh.vertices)
        {
            if (std::fabs(vertex.Y - wanted) >= 0.01F)
            {
                continue;
            }
            if (away != nullptr && Flat(vertex, *away) < beyond)
            {
                continue;
            }
            sum = Vector3(sum.X + vertex.X, sum.Y + vertex.Y, sum.Z + vertex.Z);
            ++count;
        }
        if (count == 0)
        {
            return sum;
        }
        const auto n = static_cast<float>(count);
        return Vector3(sum.X / n, sum.Y / n, sum.Z / n);
    }

    /// The foot of a ramp's WALKING surface -- which is not the middle of its underside.
    ///
    /// `build_collision.py` builds a flight as a solid wedge: a triangular prism whose bottom face
    /// is flat at the base height and whose top face is the slope. Four of its six vertices are at
    /// the bottom, two at each end, so the centroid of "everything at the minimum height" is the
    /// middle of the run's footprint and not the bottom of the slope -- which is a waypoint half a
    /// flight away from where a body walking up would be, and how `HOUSE-00615` spent its first
    /// run walking backwards into the stairwell.
    [[nodiscard]] inline Vector3 FootOfTheSlope(const physics::CollisionMesh& mesh, const Vector3& top)
    {
        float furthest = 0.0F;
        for (const Vector3& vertex : mesh.vertices)
        {
            if (std::fabs(vertex.Y - mesh.bounds.Min.Y) < 0.01F)
            {
                furthest = std::max(furthest, Flat(vertex, top));
            }
        }
        return EdgeCentre(mesh, mesh.bounds.Min.Y, &top, furthest - 0.10F);
    }

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
            if (mesh.bounds.Min.Y < from - 0.25F || mesh.bounds.Max.Y > to + 0.25F)
            {
                continue;
            }
            StairSegment segment;
            segment.lowY = mesh.bounds.Min.Y;
            segment.highY = mesh.bounds.Max.Y;
            // The wedge's bottom and top EDGES, from the vertices themselves: a bounding box
            // cannot say which end of a ramp is the low one, and the whole path depends on it.
            segment.high = EdgeCentre(mesh, mesh.bounds.Max.Y);
            segment.low = FootOfTheSlope(mesh, segment.high);
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
                up.push_back(segment.low);
                continue;
            }
            if (i > 0)
            {
                // The foot of every run but the first: the body starts on that one.
                //
                // A U-shaped flight turns on its landing, and the next run's wedge SITS on that
                // landing -- at `STAIR_MAIN_L0_L1` the whole eastern half of the landing is under
                // run 2, 0.71 m thick at its deep end. The only way onto it is at its toe, so the
                // path along the landing is an L: to the run's own end of the landing first, and
                // across to its lane second. Walked as one diagonal, a body meets the wedge's
                // cheek two metres before the toe and stands there for ever.
                if (segments[i - 1].landing)
                {
                    const StairSegment& landing = segments[i - 1];
                    up.push_back(Vector3(landing.low.X, landing.low.Y, segment.low.Z));
                }
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
