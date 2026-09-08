// SPDX-License-Identifier: MIT
#include "cnahouse/physics/Ground.hpp"

#include <algorithm>

#include "cnahouse/physics/BroadPhase.hpp"

namespace cnahouse::physics
{
    namespace
    {
        namespace Xna = Microsoft::Xna::Framework;
    }

    GroundProbeResult GroundProbe(const CollisionWorld& world,
                                  const CollisionCell& cell,
                                  BroadPhase& broad,
                                  const Capsule& capsule,
                                  float reach)
    {
        GroundProbeResult result;
        result.cellId = cell.id;
        if (reach <= 0.0F)
        {
            return result;
        }

        const Xna::Vector3 down(0.0F, -reach, 0.0F);
        const CellSweepHit shapes = SweepCell(world, cell, broad, capsule, down);
        const SweepHit ground = SweepCapsuleTerrain(world.terrain, capsule, down);

        // Whichever is nearer. A body on a terrace has the slab under it and the lawn under that,
        // and the one it is standing on is the slab.
        bool useTerrain = false;
        SweepHit best;
        if (shapes.hit)
        {
            best = static_cast<const SweepHit&>(shapes);
        }
        if (ground.hit && (!best.hit || ground.time < best.time))
        {
            best = ground;
            useTerrain = true;
        }
        if (!best.hit)
        {
            return result; // over nothing within reach: §43.1's gravity is the caller's next move
        }

        result.normal = best.normal;
        // `startedInside` comes with `time` 0 -- already touching -- so this is 0 for a body that
        // is resting, and the distance it still has to fall for one that is not quite.
        result.distance = best.startedInside ? 0.0F : reach * best.time;
        result.height = capsule.Bottom() - result.distance;
        result.terrain = useTerrain;

        if (useTerrain)
        {
            // The material under the CONTACT, not under the centre: a body standing on the edge of
            // the drive with its middle over the lawn is walking on concrete.
            const TerrainSample sample = TerrainAt(world.terrain, capsule.centre.X, capsule.centre.Z);
            result.surface = sample.surface;
            result.kind = CollisionKind::Exterior;
        }
        else
        {
            const std::size_t obbCount = world.obbs.size();
            if (shapes.shape < obbCount)
            {
                result.surface = world.obbs[shapes.shape].surface;
                result.kind = world.obbs[shapes.shape].kind;
            }
            else if (shapes.shape != CellSweepHit::kNothing &&
                     (shapes.shape - obbCount) < world.meshes.size())
            {
                const CollisionMesh& mesh = world.meshes[shapes.shape - obbCount];
                result.surface = mesh.surface;
                result.kind = mesh.kind;
            }
        }

        if (IsWalkable(result.normal))
        {
            result.onGround = true;
        }
        else
        {
            // §43.1's 46°. There IS something down there, and it is not a floor -- which is a
            // different answer from "over nothing", and the two lead to different behaviour:
            // gravity either way, but a slide along the slope rather than a free fall.
            result.steep = true;
        }
        return result;
    }

} // namespace cnahouse::physics
