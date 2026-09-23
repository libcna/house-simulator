// SPDX-License-Identifier: MIT
//
// `HOUSE-03226`: one deterministic walk from the street to every intended-accessible cell and
// back. The itinerary comes from the portal graph, the accessibility manifest and the eight
// authored stair flights. No room has a hand-written route. Every metre is advanced by §49.3's
// real controller against the deployed production collision.
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <iterator>
#include <limits>
#include <map>
#include <memory>
#include <numeric>
#include <optional>
#include <set>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include "System/IO/FileAccess.hpp"
#include "System/IO/FileMode.hpp"
#include "System/IO/FileStream.hpp"

#include "cnahouse/physics/BroadPhase.hpp"
#include "cnahouse/physics/CollisionLoader.hpp"
#include "cnahouse/physics/Sweep.hpp"
#include "cnahouse/physics/Terrain.hpp"
#include "cnahouse/player/BoundaryGuard.hpp"
#include "cnahouse/player/CellTracker.hpp"
#include "cnahouse/player/FixedStep.hpp"
#include "cnahouse/player/PlayerController.hpp"
#include "cnahouse/util/Ids.hpp"
#include "cnahouse/util/Json.hpp"
#include "cnahouse/world/SpatialIndex.hpp"
#include "cnahouse/world/WorldData.hpp"
#include "cnahouse/world/WorldLoader.hpp"

namespace
{
    namespace physics = cnahouse::physics;
    namespace player = cnahouse::player;
    namespace world = cnahouse::world;
    using cnahouse::util::Id;
    using cnahouse::util::IdRegistry;
    using Microsoft::Xna::Framework::Vector2;
    using Microsoft::Xna::Framework::Vector3;

    constexpr float kArrivalTolerance = 0.60F;
    constexpr float kWaypointTolerance = 0.14F;
    constexpr float kNarrowStairEndTolerance = 0.50F;
    constexpr float kStairEndTolerance = 0.90F;
    constexpr float kPortalInset = 0.48F;
    // The triangulated heightfield can report a sub-centimetre support overlap while grounded.
    constexpr float kTerrainPenetrationTolerance = 0.01F;
    constexpr unsigned int kMaxDetoursPerStop = 64U;
    constexpr std::uint64_t kStuckSteps = 720;
    constexpr std::uint64_t kStepLimit = 180000;

    struct StandingPoint
    {
        std::string cell;
        Vector3 feet;
        bool crouched = false;
    };

    struct Edge
    {
        std::string a;
        std::string b;
        const world::Portal* portal = nullptr;
        const world::StairFlight* flight = nullptr;
        float weight = 0.0F;
    };

    struct Stop
    {
        Vector3 feet;
        float tolerance = kWaypointTolerance;
        std::string expectedCell;
        bool arrival = false;
    };

    float HorizontalDistance(const Vector3& a, const Vector3& b)
    {
        return std::hypot(a.X - b.X, a.Z - b.Z);
    }

    float Distance(const Vector3& a, const Vector3& b)
    {
        const float dx = a.X - b.X;
        const float dy = a.Y - b.Y;
        const float dz = a.Z - b.Z;
        return std::sqrt(dx * dx + dy * dy + dz * dz);
    }

    float StopDistance(const Vector3& feet, const Stop& stop)
    {
        return stop.arrival ? Distance(feet, stop.feet) : HorizontalDistance(feet, stop.feet);
    }

    std::string Name(Id id)
    {
        return std::string(IdRegistry::NameOf(id));
    }

    std::pair<std::string, std::string> CellPair(std::string a, std::string b)
    {
        if (b < a)
        {
            std::swap(a, b);
        }
        return {std::move(a), std::move(b)};
    }

    std::map<std::string, StandingPoint> LoadStandingPoints()
    {
        std::map<std::string, StandingPoint> points;
        const auto document = cnahouse::util::JsonDocument::Load("docs/zones.json");
        if (!document)
        {
            return points;
        }
        const auto zones = document->Root().RequireArray("zones");
        if (!zones)
        {
            return points;
        }
        const auto zoneRows = zones->Elements();
        if (!zoneRows)
        {
            return points;
        }
        for (const cnahouse::util::JsonValue& zone : *zoneRows)
        {
            const auto cells = zone.RequireArray("cells");
            if (!cells)
            {
                return {};
            }
            const auto rows = cells->Elements();
            if (!rows)
            {
                return {};
            }
            for (const cnahouse::util::JsonValue& row : *rows)
            {
                const auto accessible = row.RequireBool("accessible");
                const auto cell = row.RequireString("id");
                if (!accessible || !cell)
                {
                    return {};
                }
                if (!*accessible)
                {
                    continue;
                }
                const auto feet = row.RequireVector3("standingPoint");
                const auto posture = row.OptionalString("standingPosture", "standing");
                if (!feet || !posture)
                {
                    return {};
                }
                points.emplace(*cell, StandingPoint{*cell, *feet, *posture == "crouched"});
            }
        }
        return points;
    }

    Vector3 PortalCentre(const world::Portal& portal, float feetY)
    {
        const float u = (portal.minU + portal.maxU) * 0.5F;
        switch (portal.axis)
        {
            case world::PlaneAxis::X:
                return Vector3(portal.planeValue, feetY, u);
            case world::PlaneAxis::Z:
                return Vector3(u, feetY, portal.planeValue);
            case world::PlaneAxis::Y:
                return Vector3(u, feetY, (portal.minV + portal.maxV) * 0.5F);
        }
        return {};
    }

    bool InFootprint(const world::Cell& cell, const Vector3& point)
    {
        return std::any_of(cell.boxes.begin(),
                           cell.boxes.end(),
                           [&point](const world::Footprint& box) { return box.Contains(point.X, point.Z); });
    }

    Vector3
    InsidePortal(const world::Cell& cell, const world::Portal& portal, float feetY, float u, float inset)
    {
        Vector3 point = PortalCentre(portal, feetY);
        if (portal.axis == world::PlaneAxis::Y)
        {
            return point;
        }
        if (portal.axis == world::PlaneAxis::X)
        {
            point.Z = u;
        }
        else
        {
            point.X = u;
        }
        Vector3 first = point;
        Vector3 second = point;
        if (portal.axis == world::PlaneAxis::X)
        {
            first.X -= inset;
            second.X += inset;
        }
        else
        {
            first.Z -= inset;
            second.Z += inset;
        }
        if (InFootprint(cell, first) && !InFootprint(cell, second))
        {
            return first;
        }
        if (InFootprint(cell, second))
        {
            return second;
        }

        return point;
    }

    float SegmentReach(const physics::CollisionWorld& collision,
                       const std::string& cellName,
                       const Vector3& fromFeet,
                       const Vector3& toFeet,
                       bool crouched)
    {
        const physics::CollisionCell* cell = collision.Cell(cellName);
        if (cell == nullptr)
        {
            return 0.0F;
        }
        const float halfHeight = crouched ? player::kPlayerCrouchHalfHeight : player::kPlayerHalfHeight;
        const float rise = halfHeight + player::kPlayerRadius;
        // A horizontal visibility sweep starts just above the support plane. At the authored
        // floor height the capsule is intentionally touching it, and a generic cell sweep may
        // report that support contact before it reaches the obstacle this query is asking about.
        const physics::Capsule body{
            fromFeet + Vector3(0.0F, rise + 0.002F, 0.0F), halfHeight, player::kPlayerRadius};
        physics::BroadPhase broad;
        const physics::CellSweepHit hit =
            physics::SweepCell(collision, *cell, broad, body, toFeet - fromFeet);
        return hit.hit ? hit.time : 1.0F;
    }

    std::optional<Vector3> FindDetour(const physics::CollisionWorld& collision,
                                      const world::Cell& cell,
                                      const Vector3& fromFeet,
                                      const Vector3& toFeet,
                                      bool crouched)
    {
        constexpr float spacing = 0.25F;
        constexpr float clearance = player::kPlayerRadius + 0.04F;
        const std::string cellName = Name(cell.id);
        const Vector3 flatTarget(toFeet.X, fromFeet.Y, toFeet.Z);
        std::optional<Vector3> best;
        float bestDistance = std::numeric_limits<float>::max();
        std::vector<Vector3> candidates;
        std::vector<bool> seenFromNodes;
        std::vector<bool> targetNodes;
        std::vector<std::pair<Vector3, float>> fromVisible;
        std::vector<std::pair<Vector3, float>> toVisible;
        for (const world::Footprint& box : cell.boxes)
        {
            for (float x = box.minX + clearance; x <= box.maxX - clearance; x += spacing)
            {
                for (float z = box.minZ + clearance; z <= box.maxZ - clearance; z += spacing)
                {
                    const Vector3 candidate(x, fromFeet.Y, z);
                    candidates.push_back(candidate);
                    const float fromDistance = HorizontalDistance(fromFeet, candidate);
                    const float toDistance = HorizontalDistance(candidate, flatTarget);
                    const bool seenFrom =
                        SegmentReach(collision, cellName, fromFeet, candidate, crouched) >= 0.995F;
                    const bool seesTarget =
                        SegmentReach(collision, cellName, candidate, flatTarget, crouched) >= 0.995F;
                    seenFromNodes.push_back(seenFrom);
                    targetNodes.push_back(seesTarget);
                    if (seenFrom)
                    {
                        fromVisible.emplace_back(candidate, fromDistance);
                    }
                    if (seesTarget)
                    {
                        toVisible.emplace_back(candidate, toDistance);
                    }
                    if (!seenFrom || !seesTarget || fromDistance + toDistance >= bestDistance)
                    {
                        continue;
                    }
                    best = candidate;
                    bestDistance = fromDistance + toDistance;
                }
            }
        }
        if (best)
        {
            return best;
        }

        const auto nearer = [](const auto& left, const auto& right) { return left.second < right.second; };
        std::sort(fromVisible.begin(), fromVisible.end(), nearer);
        std::sort(toVisible.begin(), toVisible.end(), nearer);
        constexpr std::size_t candidateLimit = 96U;
        fromVisible.resize(std::min(fromVisible.size(), candidateLimit));
        toVisible.resize(std::min(toVisible.size(), candidateLimit));
        for (const auto& [first, fromDistance] : fromVisible)
        {
            for (const auto& [second, toDistance] : toVisible)
            {
                const float distance = fromDistance + HorizontalDistance(first, second) + toDistance;
                if (distance >= bestDistance ||
                    SegmentReach(collision, cellName, first, second, crouched) < 0.995F)
                {
                    continue;
                }
                best = first;
                bestDistance = distance;
            }
        }
        if (best)
        {
            return best;
        }
        std::vector<int> parent(candidates.size(), -2);
        std::vector<std::size_t> queue;
        for (std::size_t candidate = 0; candidate < candidates.size(); ++candidate)
        {
            if (seenFromNodes[candidate])
            {
                parent[candidate] = -1;
                queue.push_back(candidate);
            }
        }
        for (std::size_t head = 0; head < queue.size(); ++head)
        {
            const std::size_t current = queue[head];
            if (targetNodes[current])
            {
                std::size_t first = current;
                while (parent[first] >= 0)
                {
                    first = static_cast<std::size_t>(parent[first]);
                }
                return candidates[first];
            }
            for (std::size_t next = 0; next < candidates.size(); ++next)
            {
                if (parent[next] != -2 || HorizontalDistance(candidates[current], candidates[next]) > 0.38F ||
                    SegmentReach(collision, cellName, candidates[current], candidates[next], crouched) <
                        0.995F)
                {
                    continue;
                }
                parent[next] = static_cast<int>(current);
                queue.push_back(next);
            }
        }
        return best;
    }

    std::optional<Vector3> TrialDetour(const world::Cell& cell,
                                       const Vector3& fromFeet,
                                       const Vector3& toFeet,
                                       unsigned int attempt,
                                       float baseDistance)
    {
        constexpr float angles[] = {90.0F, -90.0F, 60.0F, -60.0F, 35.0F, -35.0F};
        const float dx = toFeet.X - fromFeet.X;
        const float dz = toFeet.Z - fromFeet.Z;
        const float length = std::hypot(dx, dz);
        if (length < 0.01F)
        {
            return std::nullopt;
        }
        for (unsigned int offset = 0; offset < std::size(angles); ++offset)
        {
            const unsigned int index = (attempt + offset) % std::size(angles);
            const float radians = angles[index] * 3.14159265358979323846F / 180.0F;
            const float distance =
                baseDistance +
                baseDistance * 0.60F * static_cast<float>((attempt + offset) / std::size(angles));
            const float forwardX = dx / length;
            const float forwardZ = dz / length;
            const Vector3 candidate(
                fromFeet.X + (forwardX * std::cos(radians) - forwardZ * std::sin(radians)) * distance,
                fromFeet.Y,
                fromFeet.Z + (forwardX * std::sin(radians) + forwardZ * std::cos(radians)) * distance);
            if (InFootprint(cell, candidate))
            {
                return candidate;
            }
        }
        return std::nullopt;
    }

    bool SegmentInFootprint(const world::Cell& cell, const Vector3& from, const Vector3& to)
    {
        const float length = HorizontalDistance(from, to);
        const int samples = std::max(1, static_cast<int>(std::ceil(length / 0.10F)));
        for (int index = 0; index <= samples; ++index)
        {
            const float amount = static_cast<float>(index) / static_cast<float>(samples);
            const Vector3 point(from.X + (to.X - from.X) * amount, from.Y, from.Z + (to.Z - from.Z) * amount);
            if (!InFootprint(cell, point))
            {
                return false;
            }
        }
        return true;
    }

    std::optional<Vector3> FootprintDetour(const world::Cell& cell, const Vector3& from, const Vector3& to)
    {
        constexpr float inset = player::kPlayerRadius + 0.12F;
        std::vector<Vector3> candidates;
        for (const world::Footprint& box : cell.boxes)
        {
            const float minX = std::min(box.maxX, box.minX + inset);
            const float maxX = std::max(box.minX, box.maxX - inset);
            const float minZ = std::min(box.maxZ, box.minZ + inset);
            const float maxZ = std::max(box.minZ, box.maxZ - inset);
            const float midX = (box.minX + box.maxX) * 0.5F;
            const float midZ = (box.minZ + box.maxZ) * 0.5F;
            for (const float x : {minX, midX, maxX})
            {
                for (const float z : {minZ, midZ, maxZ})
                {
                    candidates.emplace_back(x, from.Y, z);
                }
            }
        }

        std::optional<Vector3> best;
        float bestDistance = std::numeric_limits<float>::max();
        std::vector<Vector3> fromVisible;
        std::vector<Vector3> toVisible;
        for (const Vector3& candidate : candidates)
        {
            const bool seenFrom = SegmentInFootprint(cell, from, candidate);
            const bool seesTarget = SegmentInFootprint(cell, candidate, to);
            if (seenFrom)
            {
                fromVisible.push_back(candidate);
            }
            if (seesTarget)
            {
                toVisible.push_back(candidate);
            }
            const float distance = HorizontalDistance(from, candidate) + HorizontalDistance(candidate, to);
            if (seenFrom && seesTarget && distance < bestDistance)
            {
                best = candidate;
                bestDistance = distance;
            }
        }
        if (best)
        {
            return best;
        }
        for (const Vector3& first : fromVisible)
        {
            for (const Vector3& second : toVisible)
            {
                const float distance = HorizontalDistance(from, first) + HorizontalDistance(first, second) +
                                       HorizontalDistance(second, to);
                if (distance >= bestDistance || !SegmentInFootprint(cell, first, second))
                {
                    continue;
                }
                best = first;
                bestDistance = distance;
            }
        }
        return best;
    }

    struct PortalCrossing
    {
        Vector3 before;
        Vector3 after;
        float score = 0.0F;
    };

    PortalCrossing PortalRoute(const world::Cell& fromCell,
                               const world::Cell& toCell,
                               const world::Portal& portal,
                               const StandingPoint& source,
                               const StandingPoint& destination,
                               const physics::CollisionWorld& collision)
    {
        const float centre = (portal.minU + portal.maxU) * 0.5F;
        const float low = portal.minU + player::kPlayerRadius + 0.03F;
        const float high = portal.maxU - player::kPlayerRadius - 0.03F;
        std::vector<float> candidates;
        if (low <= high)
        {
            for (int index = 0; index <= 12; ++index)
            {
                candidates.push_back(low + (high - low) * static_cast<float>(index) / 12.0F);
            }
            std::stable_sort(candidates.begin(),
                             candidates.end(),
                             [centre](float a, float b)
                             { return std::fabs(a - centre) < std::fabs(b - centre); });
        }
        else
        {
            candidates.push_back(centre);
        }

        std::optional<PortalCrossing> best;
        float bestScore = -1.0F;
        for (const float inset : {kPortalInset, 0.75F, 1.00F, 1.25F})
        {
            if ((fromCell.kind == world::CellKind::Stair || toCell.kind == world::CellKind::Stair) &&
                inset > kPortalInset)
            {
                continue;
            }
            for (const float u : candidates)
            {
                const Vector3 before = InsidePortal(fromCell, portal, source.feet.Y, u, inset);
                const Vector3 after = InsidePortal(toCell, portal, destination.feet.Y, u, inset);
                if (!InFootprint(fromCell, before) || !InFootprint(toCell, after))
                {
                    continue;
                }
                const float approach =
                    SegmentReach(collision, Name(fromCell.id), source.feet, before, source.crouched);
                const float throughFrom =
                    SegmentReach(collision, Name(fromCell.id), before, after, source.crouched);
                const float throughTo =
                    SegmentReach(collision, Name(toCell.id), before, after, destination.crouched);
                const float depart =
                    SegmentReach(collision, Name(toCell.id), after, destination.feet, destination.crouched);
                const float score = approach + 4.0F * throughFrom + 4.0F * throughTo + depart - inset;
                if (score <= bestScore)
                {
                    continue;
                }
                best = PortalCrossing{before, after, score};
                bestScore = score;
            }
        }
        return best.value_or(PortalCrossing{
            PortalCentre(portal, source.feet.Y), PortalCentre(portal, destination.feet.Y), 0.0F});
    }

    class DisjointSet
    {
    public:
        explicit DisjointSet(std::size_t size)
            : parent_(size)
        {
            std::iota(parent_.begin(), parent_.end(), 0U);
        }

        std::size_t Find(std::size_t value)
        {
            if (parent_[value] != value)
            {
                parent_[value] = Find(parent_[value]);
            }
            return parent_[value];
        }

        bool Join(std::size_t a, std::size_t b)
        {
            a = Find(a);
            b = Find(b);
            if (a == b)
            {
                return false;
            }
            parent_[b] = a;
            return true;
        }

    private:
        std::vector<std::size_t> parent_;
    };

    std::vector<Vector3> StairWaypoints(const physics::CollisionWorld& collision,
                                        const std::string& from,
                                        const std::string& to,
                                        const Vector3& destinationFeet,
                                        bool ascending)
    {
        const physics::CollisionCell* a = collision.Cell(from);
        const physics::CollisionCell* b = collision.Cell(to);
        if (a == nullptr || b == nullptr)
        {
            return {};
        }
        std::set<std::uint32_t> inA(a->shapes.begin(), a->shapes.end());
        std::vector<std::uint32_t> shared;
        std::copy_if(b->shapes.begin(),
                     b->shapes.end(),
                     std::back_inserter(shared),
                     [&inA](std::uint32_t shape) { return inA.contains(shape); });

        struct Segment
        {
            Vector3 low;
            Vector3 high;
        };

        std::vector<Segment> rampSegments;
        std::vector<Segment> boxSegments;
        for (const std::uint32_t shape : shared)
        {
            if (shape < collision.obbs.size())
            {
                const physics::CollisionObb& obb = collision.obbs[shape];
                if (obb.kind != physics::CollisionKind::Stair)
                {
                    continue;
                }
                const Vector3 top(obb.centre.X, obb.centre.Y + obb.halfExtents.Y, obb.centre.Z);
                boxSegments.push_back(Segment{top, top});
                continue;
            }
            const physics::CollisionMesh& mesh = collision.meshes[shape - collision.obbs.size()];
            if (mesh.kind != physics::CollisionKind::Stair)
            {
                continue;
            }
            std::vector<std::uint16_t> slopeVertices;
            for (std::size_t index = 0; index < mesh.indices.size(); index += 3)
            {
                const std::uint16_t ia = mesh.indices[index];
                const std::uint16_t ib = mesh.indices[index + 1];
                const std::uint16_t ic = mesh.indices[index + 2];
                const Vector3 ab = mesh.vertices[ib] - mesh.vertices[ia];
                const Vector3 ac = mesh.vertices[ic] - mesh.vertices[ia];
                const Vector3 normal = Vector3::Cross(ab, ac);
                const float length = normal.Length();
                if (length <= 1.0e-6F)
                {
                    continue;
                }
                const float vertical = std::fabs(normal.Y / length);
                if (vertical <= 0.20F || vertical >= 0.999F)
                {
                    continue;
                }
                for (const std::uint16_t vertex : {ia, ib, ic})
                {
                    if (std::find(slopeVertices.begin(), slopeVertices.end(), vertex) == slopeVertices.end())
                    {
                        slopeVertices.push_back(vertex);
                    }
                }
            }
            if (slopeVertices.empty())
            {
                continue;
            }
            float lowY = mesh.vertices[slopeVertices.front()].Y;
            float highY = lowY;
            for (const std::uint16_t vertex : slopeVertices)
            {
                lowY = std::min(lowY, mesh.vertices[vertex].Y);
                highY = std::max(highY, mesh.vertices[vertex].Y);
            }
            Vector3 low;
            Vector3 high;
            int lows = 0;
            int highs = 0;
            for (const std::uint16_t vertex : slopeVertices)
            {
                const Vector3& point = mesh.vertices[vertex];
                if (std::fabs(point.Y - lowY) < 1.0e-4F)
                {
                    low = Vector3(low.X + point.X, low.Y + point.Y, low.Z + point.Z);
                    ++lows;
                }
                if (std::fabs(point.Y - highY) < 1.0e-4F)
                {
                    high = Vector3(high.X + point.X, high.Y + point.Y, high.Z + point.Z);
                    ++highs;
                }
            }
            if (lows > 0 && highs > 0)
            {
                const float lowDivisor = static_cast<float>(lows);
                const float highDivisor = static_cast<float>(highs);
                low = Vector3(low.X / lowDivisor, low.Y / lowDivisor, low.Z / lowDivisor);
                high = Vector3(high.X / highDivisor, high.Y / highDivisor, high.Z / highDivisor);
                rampSegments.push_back(Segment{low, high});
            }
        }
        const bool boxesOnly = rampSegments.empty();
        std::vector<Segment> segments = boxesOnly ? std::move(boxSegments) : std::move(rampSegments);
        std::sort(segments.begin(),
                  segments.end(),
                  [](const Segment& left, const Segment& right)
                  {
                      return std::tie(left.low.Y, left.high.Y, left.low.X, left.low.Z) <
                             std::tie(right.low.Y, right.high.Y, right.low.X, right.low.Z);
                  });
        std::vector<Vector3> points;
        for (const Segment& segment : segments)
        {
            points.push_back(segment.low);
            if (Distance(segment.low, segment.high) > 0.01F)
            {
                points.push_back(segment.high);
            }
        }
        if (!ascending)
        {
            std::reverse(points.begin(), points.end());
        }
        if (!points.empty())
        {
            const float dx = destinationFeet.X - points.back().X;
            const float dz = destinationFeet.Z - points.back().Z;
            const float length = std::hypot(dx, dz);
            if (length > 0.01F)
            {
                constexpr float departure = player::kPlayerRadius + 0.30F;
                points.emplace_back(points.back().X + dx / length * departure,
                                    destinationFeet.Y,
                                    points.back().Z + dz / length * departure);
            }
        }
        return points;
    }

    void AppendTransition(std::vector<Stop>& route,
                          const Edge& edge,
                          const std::string& from,
                          const std::string& to,
                          const std::map<std::string, StandingPoint>& standing,
                          const world::WorldData& data,
                          const physics::CollisionWorld& collision)
    {
        const StandingPoint& source = standing.at(from);
        const StandingPoint& destination = standing.at(to);
        if (edge.flight != nullptr)
        {
            const bool ascending = destination.feet.Y > source.feet.Y;
            std::vector<Vector3> stair = StairWaypoints(collision, from, to, destination.feet, ascending);
            if (!ascending && stair.size() >= 3U)
            {
                const Vector3& low = stair[stair.size() - 2U];
                const Vector3& towardHigh = stair[stair.size() - 3U];
                Vector3& departure = stair.back();
                const float clearance = edge.flight->width * 0.5F + kPortalInset;
                if (std::fabs(towardHigh.Z - low.Z) >= std::fabs(towardHigh.X - low.X))
                {
                    if (destination.feet.X < low.X - edge.flight->width * 0.5F)
                    {
                        departure.X = low.X - clearance;
                    }
                    else if (destination.feet.X > low.X + edge.flight->width * 0.5F)
                    {
                        departure.X = low.X + clearance;
                    }
                }
                else if (destination.feet.Z < low.Z - edge.flight->width * 0.5F)
                {
                    departure.Z = low.Z - clearance;
                }
                else if (destination.feet.Z > low.Z + edge.flight->width * 0.5F)
                {
                    departure.Z = low.Z + clearance;
                }
            }
            if (!stair.empty() && edge.portal != nullptr && edge.portal->axis != world::PlaneAxis::Y)
            {
                const float safeLow = edge.portal->minU + kPortalInset;
                const float safeHigh = edge.portal->maxU - kPortalInset;
                if (safeLow <= safeHigh)
                {
                    const float reference =
                        edge.portal->axis == world::PlaneAxis::Z ? stair.back().X : stair.back().Z;
                    const float crossing = std::clamp(reference, safeLow, safeHigh);
                    for (Vector3& point : stair)
                    {
                        if (edge.portal->axis == world::PlaneAxis::Z)
                        {
                            point.X = crossing;
                        }
                        else
                        {
                            point.Z = crossing;
                        }
                    }
                }
            }
            else if (ascending && stair.size() >= 2U && edge.portal != nullptr)
            {
                Vector3 departure = stair[stair.size() - 2U];
                departure.Y = destination.feet.Y;
                bool adjusted = false;
                if (destination.feet.X < edge.portal->minU)
                {
                    departure.X = edge.portal->minU - kPortalInset;
                    adjusted = true;
                }
                else if (destination.feet.X > edge.portal->maxU)
                {
                    departure.X = edge.portal->maxU + kPortalInset;
                    adjusted = true;
                }
                if (destination.feet.Z < edge.portal->minV)
                {
                    departure.Z = edge.portal->minV - kPortalInset;
                    adjusted = true;
                }
                else if (destination.feet.Z > edge.portal->maxV)
                {
                    departure.Z = edge.portal->maxV + kPortalInset;
                    adjusted = true;
                }
                if (adjusted)
                {
                    stair.back() = departure;
                }
            }
            for (std::size_t index = 0; index < stair.size(); ++index)
            {
                const std::string expected = index == 0U ? from : (index + 1U == stair.size() ? to : "");
                const bool lowRampEnd =
                    stair.size() > 2U && (ascending ? index == 0U : index + 2U == stair.size());
                const bool finalRampEnd = stair.size() > 2U && index + 2U == stair.size();
                const float tolerance =
                    lowRampEnd ? (edge.flight->width < 1.0F ? kNarrowStairEndTolerance : kStairEndTolerance)
                    : finalRampEnd && ascending ? kNarrowStairEndTolerance
                                                : kWaypointTolerance;
                route.push_back(Stop{stair[index], tolerance, expected, false});
            }
            if (!stair.empty())
            {
                return;
            }
        }

        const world::Cell* fromCell = data.FindCell(cnahouse::util::Intern(from));
        const world::Cell* toCell = data.FindCell(cnahouse::util::Intern(to));
        if (fromCell == nullptr || toCell == nullptr || edge.portal == nullptr)
        {
            return;
        }
        const PortalCrossing crossing =
            PortalRoute(*fromCell, *toCell, *edge.portal, source, destination, collision);
        route.push_back(Stop{crossing.before, kWaypointTolerance, from, false});
        route.push_back(Stop{crossing.after, kWaypointTolerance, to, false});
    }

} // namespace

TEST(GrandTourTests, EveryAccessibleCellIsReachedOnFoot)
{
    const auto started = std::chrono::steady_clock::now();
    IdRegistry::ResetForTesting();
    const std::string directory = "content/world";
    const std::string collisionPath = directory + "/collision.bin";
    if (!std::filesystem::exists(directory + "/layout.cells.json") || !std::filesystem::exists(collisionPath))
    {
        GTEST_SKIP() << "no deployed world; run tools/ci/build_content.py --only world";
    }

    std::map<std::string, StandingPoint> standing = LoadStandingPoints();
    ASSERT_EQ(standing.size(), 90U) << "docs/zones.json did not yield the complete accessible set";
    ASSERT_TRUE(standing.contains("EXT_ROAD"));

    world::WorldData::Contents contents;
    ASSERT_TRUE(world::WorldLoader::LoadLevels(directory, contents));
    ASSERT_TRUE(world::WorldLoader::LoadCells(directory, contents));
    ASSERT_TRUE(world::WorldLoader::LoadPortals(directory, contents));
    ASSERT_TRUE(world::WorldLoader::LoadStairs(directory, contents));
    ASSERT_TRUE(world::WorldLoader::LoadInitialState(directory, contents));
    auto built = world::WorldData::Create(std::move(contents));
    ASSERT_TRUE(built) << built.Error().ToString();
    const world::WorldData& data = built.Value();
    const world::SpatialIndex index = world::SpatialIndex::Build(data);

    const std::unique_ptr<System::IO::FileStream> stream(
        new System::IO::FileStream(collisionPath, System::IO::FileMode::Open, System::IO::FileAccess::Read));
    auto collisionResult = physics::CollisionLoader::Read(*stream, collisionPath);
    ASSERT_TRUE(collisionResult) << collisionResult.Error().ToString();
    const physics::CollisionWorld& collision = collisionResult.Value();

    // The graph's vertices are precisely the manifest rows. Every passable authored portal whose
    // two ends are accessible is an edge. Stair flights are attached to their cell pair so the
    // spanning tree can deliberately retain all eight instead of accidentally bypassing a flight
    // through another room.
    std::map<std::pair<std::string, std::string>, const world::StairFlight*> flights;
    for (const world::StairFlight& flight : data.Stairs())
    {
        flights.emplace(CellPair(Name(flight.fromCell), Name(flight.toCell)), &flight);
    }
    ASSERT_EQ(flights.size(), 8U) << "the grand tour must exercise all eight authored flights";

    std::vector<Edge> edges;
    for (const world::Portal& portal : data.Portals())
    {
        const std::string a = Name(portal.cellA);
        const std::string b = Name(portal.cellB);
        if (!world::IsPassable(portal.kind) || !standing.contains(a) || !standing.contains(b))
        {
            continue;
        }
        const Vector3 centre = PortalCentre(portal, (standing.at(a).feet.Y + standing.at(b).feet.Y) * 0.5F);
        float weight =
            HorizontalDistance(standing.at(a).feet, centre) + HorizontalDistance(centre, standing.at(b).feet);
        const auto flight = flights.find(CellPair(a, b));
        if (flight == flights.end())
        {
            const world::Cell* cellA = data.FindCell(portal.cellA);
            const world::Cell* cellB = data.FindCell(portal.cellB);
            ASSERT_NE(cellA, nullptr);
            ASSERT_NE(cellB, nullptr);
            const PortalCrossing crossing =
                PortalRoute(*cellA, *cellB, portal, standing.at(a), standing.at(b), collision);
            weight += std::max(0.0F, 10.0F - crossing.score) * 100.0F;
        }
        edges.push_back(Edge{a, b, &portal, flight == flights.end() ? nullptr : flight->second, weight});
    }
    ASSERT_GE(edges.size(), standing.size() - 1U);

    // A deterministic minimum spanning tree, with the eight flights made mandatory first. Its
    // depth-first walk visits every manifest point and naturally returns to the street.
    std::vector<std::string> names;
    names.reserve(standing.size());
    for (const auto& [name, point] : standing)
    {
        (void)point;
        names.push_back(name);
    }
    std::map<std::string, std::size_t> number;
    for (std::size_t i = 0; i < names.size(); ++i)
    {
        number.emplace(names[i], i);
    }
    std::sort(edges.begin(),
              edges.end(),
              [](const Edge& left, const Edge& right)
              {
                  return std::tuple(left.flight == nullptr, left.weight, Name(left.portal->id)) <
                         std::tuple(right.flight == nullptr, right.weight, Name(right.portal->id));
              });
    DisjointSet components(names.size());
    std::vector<const Edge*> treeEdges;
    std::set<std::string> retainedFlights;
    for (const Edge& edge : edges)
    {
        if (!components.Join(number.at(edge.a), number.at(edge.b)))
        {
            continue;
        }
        treeEdges.push_back(&edge);
        if (edge.flight != nullptr)
        {
            retainedFlights.insert(Name(edge.flight->id));
        }
    }
    ASSERT_EQ(treeEdges.size(), standing.size() - 1U) << "the accessible portal graph is disconnected";
    ASSERT_EQ(retainedFlights.size(), flights.size()) << "the tree bypassed an authored flight";

    std::map<std::string, std::vector<std::pair<std::string, const Edge*>>> tree;
    for (const Edge* edge : treeEdges)
    {
        tree[edge->a].push_back({edge->b, edge});
        tree[edge->b].push_back({edge->a, edge});
    }
    for (auto& [cell, neighbours] : tree)
    {
        (void)cell;
        std::sort(neighbours.begin(), neighbours.end());
    }

    std::vector<Stop> route;
    std::set<std::string> scheduled;
    const auto visit = [&](const auto& self, const std::string& cell, const std::string& parent) -> void
    {
        scheduled.insert(cell);
        if (cell != "EXT_ROAD")
        {
            const world::Cell* manifestCell = data.FindCell(cnahouse::util::Intern(cell));
            ASSERT_NE(manifestCell, nullptr);
            float tolerance =
                manifestCell->kind == world::CellKind::Stair ? kArrivalTolerance : kWaypointTolerance;
            const world::Level* level = data.FindLevel(manifestCell->level);
            if (manifestCell->kind == world::CellKind::Stair && level != nullptr &&
                standing.at(cell).feet.Y < level->ffl - 0.01F)
            {
                tolerance = kStairEndTolerance;
            }
            route.push_back(Stop{standing.at(cell).feet, tolerance, cell, true});
        }
        for (const auto& [next, edge] : tree[cell])
        {
            if (next == parent)
            {
                continue;
            }
            AppendTransition(route, *edge, cell, next, standing, data, collision);
            self(self, next, cell);
            AppendTransition(route, *edge, next, cell, standing, data, collision);
            route.push_back(Stop{standing.at(cell).feet, kWaypointTolerance, cell, false});
        }
    };
    route.push_back(Stop{standing.at("EXT_ROAD").feet, kWaypointTolerance, "EXT_ROAD", true});
    visit(visit, "EXT_ROAD", {});
    route.push_back(Stop{data.GetInitialState().player.position, kArrivalTolerance, "EXT_ROAD", false});
    ASSERT_EQ(scheduled.size(), standing.size());

    player::PlayerState body;
    body.position = data.GetInitialState().player.position + Vector3(0.0F, body.Rise() + 0.02F, 0.0F);
    body.fastWalk = true;
    player::CellTracker tracker;
    tracker.Update(data, index, body.position);
    ASSERT_EQ(Name(tracker.Current()), "EXT_ROAD") << "the authored new-game spawn is not on the road";
    physics::BroadPhase broad;
    player::BoundaryGuard boundary;
    std::set<std::string> arrived;
    std::uint64_t steps = 0;
    std::uint64_t penetrations = 0;
    std::uint64_t floorLosses = 0;
    std::uint64_t detoursUsed = 0;
    float deepestPenetration = 0.0F;
    std::string deepestCell;
    std::size_t deepestStop = 0U;

    // Settle the 20 mm spawn clearance exactly as the game does before accepting the first point.
    const player::InputState still;
    for (int settle = 0; settle < 60; ++settle)
    {
        const physics::CollisionCell* cell = collision.Cell(Name(tracker.Current()));
        ASSERT_NE(cell, nullptr);
        body.cellId = cell->id;
        (void)player::PlayerStep(collision, *cell, broad, body, still, player::kFixedStepSeconds);
        tracker.Update(data, index, body.position);
    }

    for (std::size_t stopIndex = 0; stopIndex < route.size(); ++stopIndex)
    {
        const Stop& stop = route[stopIndex];
        float best = StopDistance(body.Feet(), stop);
        std::uint64_t bestAt = steps;
        std::optional<Vector3> detour;
        float detourBest = 0.0F;
        std::uint64_t detourBestAt = steps;
        unsigned int stopDetours = 0;
        while (StopDistance(body.Feet(), stop) > stop.tolerance ||
               (!stop.expectedCell.empty() && Name(tracker.Current()) != stop.expectedCell))
        {
            ASSERT_LT(steps, kStepLimit) << "tour exceeded its deterministic step budget at stop "
                                         << stopIndex << " for " << stop.expectedCell;
            const Vector3 feet = body.Feet();
            if (detour && HorizontalDistance(feet, *detour) <= kWaypointTolerance)
            {
                detour.reset();
            }
            if (detour)
            {
                const float remaining = HorizontalDistance(feet, *detour);
                if (remaining + 0.002F < detourBest)
                {
                    detourBest = remaining;
                    detourBestAt = steps;
                }
                else if (steps - detourBestAt > 120U)
                {
                    detour.reset();
                }
            }
            const physics::CollisionCell* steeringCell = collision.Cell(Name(tracker.Current()));
            if (!detour && steeringCell != nullptr && steeringCell->outdoors &&
                Name(tracker.Current()) == stop.expectedCell)
            {
                const world::Cell* routeCell = data.FindCell(tracker.Current());
                ASSERT_NE(routeCell, nullptr);
                if (!SegmentInFootprint(*routeCell, feet, stop.feet))
                {
                    detour = FootprintDetour(*routeCell, feet, stop.feet);
                    ASSERT_TRUE(detour.has_value())
                        << "no footprint route at stop " << stopIndex << " for " << stop.expectedCell;
                    ASSERT_LT(stopDetours, kMaxDetoursPerStop) << "footprint route did not converge at stop "
                                                               << stopIndex << " for " << stop.expectedCell;
                    ++stopDetours;
                    ++detoursUsed;
                    detourBest = HorizontalDistance(feet, *detour);
                    detourBestAt = steps;
                    bestAt = steps;
                }
            }
            if (!detour && steps - bestAt > 60U && steeringCell != nullptr && !stop.expectedCell.empty() &&
                Name(tracker.Current()) == stop.expectedCell &&
                HorizontalDistance(feet, stop.feet) > kWaypointTolerance &&
                (steeringCell->outdoors || std::fabs(feet.Y - stop.feet.Y) < 0.30F))
            {
                const world::Cell* routeCell = data.FindCell(tracker.Current());
                ASSERT_NE(routeCell, nullptr);
                if (steeringCell->outdoors)
                {
                    detour = FindDetour(collision, *routeCell, feet, stop.feet, body.crouched);
                    if (!detour)
                    {
                        detour = TrialDetour(*routeCell, feet, stop.feet, stopDetours, 2.5F);
                    }
                }
                else
                {
                    detour = FindDetour(collision, *routeCell, feet, stop.feet, body.crouched);
                    if (!detour)
                    {
                        detour = TrialDetour(*routeCell, feet, stop.feet, stopDetours, 0.75F);
                    }
                }
                ASSERT_TRUE(detour.has_value())
                    << "no collision-clear detour at stop " << stopIndex << " for " << stop.expectedCell
                    << " from (" << feet.X << ", " << feet.Y << ", " << feet.Z << ") toward (" << stop.feet.X
                    << ", " << stop.feet.Y << ", " << stop.feet.Z << ")";
                ASSERT_LT(stopDetours, kMaxDetoursPerStop)
                    << "detour did not converge at stop " << stopIndex << " for " << stop.expectedCell
                    << ", in " << Name(tracker.Current()) << " at (" << feet.X << ", " << feet.Y << ", "
                    << feet.Z << ") toward (" << stop.feet.X << ", " << stop.feet.Y << ", " << stop.feet.Z
                    << ")";
                ++stopDetours;
                ++detoursUsed;
                detourBest = HorizontalDistance(feet, *detour);
                detourBestAt = steps;
                bestAt = steps;
            }
            const Vector3 steering = detour.value_or(stop.feet);
            const float dx = steering.X - feet.X;
            const float dz = steering.Z - feet.Z;
            const float horizontal = std::hypot(dx, dz);
            player::InputState input;
            if (horizontal > 0.01F)
            {
                input.move = Vector2(dx / horizontal, -dz / horizontal); // yaw 0: right +X, forward -Z
            }
            body.yaw = 0.0F;
            const physics::CollisionCell* cell = collision.Cell(Name(tracker.Current()));
            ASSERT_NE(cell, nullptr) << "cell tracking left collision at stop " << stopIndex;
            body.cellId = cell->id;
            (void)player::PlayerStep(collision, *cell, broad, body, input, player::kFixedStepSeconds);
            tracker.Update(data, index, body.position);
            ++steps;

            ASSERT_TRUE(tracker.Current().IsValid()) << "cell tracking lost the body at stop " << stopIndex;
            const physics::CollisionCell* now = collision.Cell(Name(tracker.Current()));
            ASSERT_NE(now, nullptr);
            const physics::CellOverlap overlap = physics::OverlapCell(collision, *now, broad, body.Body());
            physics::CollisionKind overlapKind = physics::CollisionKind::Wall;
            const std::size_t overlapShape = overlap.shape;
            if (overlapShape < collision.obbs.size())
            {
                overlapKind = collision.obbs[overlapShape].kind;
            }
            else if (overlapShape - collision.obbs.size() < collision.meshes.size())
            {
                overlapKind = collision.meshes[overlapShape - collision.obbs.size()].kind;
            }
            const bool insideStatic = overlap.overlapped && overlapKind != physics::CollisionKind::Floor &&
                                      overlapKind != physics::CollisionKind::Stair &&
                                      overlap.depth > physics::kContactTolerance;
            penetrations += insideStatic ? 1U : 0U;
            if (insideStatic && overlap.depth > deepestPenetration)
            {
                deepestPenetration = overlap.depth;
                deepestCell = Name(tracker.Current());
                deepestStop = stopIndex;
            }
            if (now->outdoors)
            {
                const physics::Overlap terrain =
                    physics::OverlapCapsuleTerrain(collision.terrain, body.Body());
                const bool insideTerrain = terrain.overlapped && terrain.depth > kTerrainPenetrationTolerance;
                penetrations += insideTerrain ? 1U : 0U;
                if (insideTerrain && terrain.depth > deepestPenetration)
                {
                    deepestPenetration = terrain.depth;
                    deepestCell = Name(tracker.Current()) + ":terrain";
                    deepestStop = stopIndex;
                }
                const physics::TerrainSample ground =
                    physics::TerrainAt(collision.terrain, body.Feet().X, body.Feet().Z);
                floorLosses += ground.over && body.Feet().Y < ground.height - 0.05F ? 1U : 0U;
            }
            else if (const world::Cell* row = data.FindCell(tracker.Current());
                     row != nullptr && row->kind != world::CellKind::Stair)
            {
                const auto extent = data.ExtentOf(*row);
                if (extent)
                {
                    floorLosses += body.Feet().Y < extent->floorY - 0.05F ? 1U : 0U;
                }
            }
            Vector3 guarded = body.position;
            (void)boundary.Contain(guarded);

            const float distance = StopDistance(body.Feet(), stop);
            if (distance + 0.002F < best)
            {
                best = distance;
                bestAt = steps;
            }
            ASSERT_LE(steps - bestAt, kStuckSteps)
                << "controller made no progress for " << kStuckSteps << " steps at stop " << stopIndex
                << " for " << stop.expectedCell << ", in " << Name(tracker.Current()) << " at ("
                << body.Feet().X << ", " << body.Feet().Y << ", " << body.Feet().Z << ") toward ("
                << stop.feet.X << ", " << stop.feet.Y << ", " << stop.feet.Z << ")";
        }
        if (stop.arrival)
        {
            arrived.insert(stop.expectedCell);
            EXPECT_LE(Distance(body.Feet(), standing.at(stop.expectedCell).feet), kArrivalTolerance)
                << stop.expectedCell << " reached at (" << body.Feet().X << ", " << body.Feet().Y << ", "
                << body.Feet().Z << ") toward (" << standing.at(stop.expectedCell).feet.X << ", "
                << standing.at(stop.expectedCell).feet.Y << ", " << standing.at(stop.expectedCell).feet.Z
                << ")";
        }
    }

    const double seconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - started).count();
    std::printf("  grand tour: %zu manifest cells, %zu route stops, %llu controller steps, "
                "%llu collision detours, %.2f s\n",
                arrived.size(),
                route.size(),
                static_cast<unsigned long long>(steps),
                static_cast<unsigned long long>(detoursUsed),
                seconds);
    EXPECT_EQ(arrived.size(), standing.size());
    EXPECT_EQ(boundary.Escapes(), 0U) << "the §10.3 safety boundary caught the tour";
    EXPECT_EQ(penetrations, 0U) << "a controller step ended inside static collision; deepest "
                                << deepestPenetration << " m at stop " << deepestStop << " in "
                                << deepestCell;
    EXPECT_EQ(floorLosses, 0U) << "a controller step ended below its cell's floor";
    EXPECT_EQ(Name(tracker.Current()), "EXT_ROAD");
    EXPECT_LE(Distance(body.Feet(), data.GetInitialState().player.position), kArrivalTolerance);
    EXPECT_LT(seconds, 120.0) << "the grand tour must remain a CI test";
    IdRegistry::ResetForTesting();
}
