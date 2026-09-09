// SPDX-License-Identifier: MIT
#include "cnahouse/visibility/VisibilitySystem.hpp"

#include "cnahouse/world/WorldData.hpp"

namespace cnahouse::visibility
{

    VisibilitySystem::VisibilitySystem(const world::WorldData& world)
        : world_(&world)
    {
        runtimes_.reserve(world.Portals().size());
        for (const world::Portal& portal : world.Portals())
        {
            runtimes_.emplace_back(portal);
        }
    }

    void VisibilitySystem::SetCamera(const CameraView& camera)
    {
        camera_ = camera;
        hasCamera_ = true;
    }

    bool VisibilitySystem::SetAperture(util::Id portal, float fraction)
    {
        if (world_ == nullptr)
        {
            return false;
        }
        const std::span<const world::Portal> portals = world_->Portals();
        for (std::size_t i = 0; i < portals.size() && i < runtimes_.size(); ++i)
        {
            if (portals[i].id == portal)
            {
                return runtimes_[i].SetAperture(fraction);
            }
        }
        return false;
    }

    float VisibilitySystem::Aperture(util::Id portal) const
    {
        if (world_ == nullptr)
        {
            return 0.0F;
        }
        const std::span<const world::Portal> portals = world_->Portals();
        for (std::size_t i = 0; i < portals.size() && i < runtimes_.size(); ++i)
        {
            if (portals[i].id == portal)
            {
                return runtimes_[i].Aperture();
            }
        }
        return 0.0F;
    }

    void VisibilitySystem::Update(const app::FrameContext& frame)
    {
        frame_ = frame.frameIndex;
        if (world_ == nullptr || !hasCamera_)
        {
            // A frame before anyone has said where the camera is. The set is left as it was and
            // the frame index still moves, so a consumer comparing the two can tell.
            return;
        }

        PortalTraversal::Input input;
        input.world = world_;
        input.portals = runtimes_;
        input.cameraCell = camera_.cell;
        input.eye = camera_.eye;
        input.viewProjection = camera_.viewProjection;
        input.cameraFrustum = camera_.frustum;
        input.nearPlane = camera_.nearPlane;
        input.farPlane = camera_.farPlane;
        // §25.2's depth table asks whether the camera is inside or out, and the cell it is in is
        // the only thing that knows: `EXT_*` cells are `CellKind::Exterior` (§15.3).
        const world::Cell* cell = world_->FindCell(camera_.cell);
        input.side = cell != nullptr && cell->kind == world::CellKind::Exterior ? CameraSide::Exterior
                                                                                : CameraSide::Interior;
        traversal_.Run(input);
    }

} // namespace cnahouse::visibility
