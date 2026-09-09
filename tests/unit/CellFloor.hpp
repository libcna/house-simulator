// SPDX-License-Identifier: MIT
#pragma once

// Where a body stands in a cell, asked once (`HOUSE-00774`).
//
// Half a dozen guarantee tests walk every cell of the real house, and each one has to start by
// putting a body somewhere legal in it. They all did it the same way -- drop a 50 mm pebble down
// the middle and stand the body on what it lands on -- and they all asked only the cell's SHAPES.
//
// Outdoors that is the wrong floor. A yard's floor slab is at its cell's `yOverride`,
// `EXT_BACKYARD`'s is -0.90, and the lawn over it is at about zero: §11.5's height field is what a
// body outdoors stands on, and §49.2 says so. A body stood on the slab is 0.66 m inside the ground,
// which is a body in trouble before the test has done anything -- and, since `HOUSE-00774` gave the
// ground a say in §49.3's step 5, one the push-out cannot rescue.
//
// So the question is asked here, once, the way `GroundProbe` asks it: whichever is nearer, the
// cell's shapes or the ground, and the ground only in the cells the file says it belongs to.
#include <optional>

#include "Microsoft/Xna/Framework/Vector3.hpp"

#include "cnahouse/physics/BroadPhase.hpp"
#include "cnahouse/physics/CollisionData.hpp"
#include "cnahouse/physics/Sweep.hpp"
#include "cnahouse/physics/Terrain.hpp"

namespace cnahouse::tests
{
    /// A place to stand: the middle of the cell in plan, and the floor under it.
    struct StandingSpot
    {
        bool found = false;
        float x = 0.0F;
        float z = 0.0F;
        /// @brief The top of whatever is under the middle: a floor, a tread, or the ground.
        float floorY = 0.0F;
        /// @brief True when the ground is what it landed on rather than a shape.
        bool onGround = false;
    };

    /// @brief The floor under the middle of @p cell, or `found == false` when there is none.
    ///
    /// @param probe the radius of the pebble. Small on purpose: a body-sized probe started at head
    ///        height is inside the ceiling slab in most rooms, and what is wanted is the surface.
    [[nodiscard]] inline StandingSpot StandInTheMiddle(const physics::CollisionWorld& world,
                                                       const physics::CollisionCell& cell,
                                                       physics::BroadPhase& broad,
                                                       float probe = 0.05F)
    {
        StandingSpot spot;
        if (cell.shapes.empty() || cell.nx == 0u || cell.nz == 0u)
        {
            return spot;
        }
        spot.x = cell.originX + static_cast<float>(cell.nx) * 0.5F;
        spot.z = cell.originZ + static_cast<float>(cell.nz) * 0.5F;

        const float height = cell.bounds.Max.Y - cell.bounds.Min.Y;
        const Microsoft::Xna::Framework::Vector3 start(
            spot.x, (cell.bounds.Min.Y + cell.bounds.Max.Y) * 0.5F, spot.z);
        const physics::Capsule falling = physics::Sphere(start, probe);
        if (physics::OverlapCell(world, cell, broad, falling).overlapped)
        {
            return spot; // solid at mid-height down the middle: a chimney, a stack, a stair
        }

        const Microsoft::Xna::Framework::Vector3 down(0.0F, -height, 0.0F);
        const physics::CellSweepHit shapes = physics::SweepCell(world, cell, broad, falling, down);
        const physics::SweepHit ground =
            cell.outdoors ? physics::SweepCapsuleTerrain(world.terrain, falling, down) : physics::SweepHit{};
        if (shapes.hit && (!ground.hit || shapes.time <= ground.time))
        {
            spot.floorY = start.Y - height * shapes.time - probe;
        }
        else if (ground.hit)
        {
            spot.floorY = start.Y - height * ground.time - probe;
            spot.onGround = true;
        }
        else
        {
            return spot; // nothing under the middle of this cell to stand on
        }
        spot.found = true;
        return spot;
    }

} // namespace cnahouse::tests
