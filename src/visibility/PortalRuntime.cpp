// SPDX-License-Identifier: MIT
#include "cnahouse/visibility/PortalRuntime.hpp"

namespace cnahouse::visibility
{
    using Microsoft::Xna::Framework::Vector3;

    PortalRuntime::PortalRuntime(const world::Portal& portal) noexcept
    {
        // `u` and `v` mean different axes on a horizontal plane than on a vertical one
        // (`world::Portal`), and getting that wrong puts a doorway on its side.
        const float plane = portal.planeValue;
        switch (portal.axis)
        {
            case world::PlaneAxis::X:
                corners_ = {Vector3{plane, portal.minV, portal.minU},
                            Vector3{plane, portal.minV, portal.maxU},
                            Vector3{plane, portal.maxV, portal.maxU},
                            Vector3{plane, portal.maxV, portal.minU}};
                break;
            case world::PlaneAxis::Z:
                corners_ = {Vector3{portal.minU, portal.minV, plane},
                            Vector3{portal.maxU, portal.minV, plane},
                            Vector3{portal.maxU, portal.maxV, plane},
                            Vector3{portal.minU, portal.maxV, plane}};
                break;
            case world::PlaneAxis::Y:
                corners_ = {Vector3{portal.minU, plane, portal.minV},
                            Vector3{portal.maxU, plane, portal.minV},
                            Vector3{portal.maxU, plane, portal.maxV},
                            Vector3{portal.minU, plane, portal.maxV}};
                break;
        }

        opacity_ = portal.opacity;
        // A portal with no aperture has no leaf to open: it is a hole and it is always open. One
        // WITH a leaf starts shut, which is the state §65.6 begins the house in.
        hasLeaf_ = portal.aperture.IsValid();
        aperture_ = hasLeaf_ ? 0.0F : 1.0F;
        open_ = !hasLeaf_;
    }

    bool PortalRuntime::SetAperture(float fraction) noexcept
    {
        aperture_ = fraction < 0.0F ? 0.0F : (fraction > 1.0F ? 1.0F : fraction);
        const bool was = open_;
        // §25.3's latch: it takes `kOpenAbove` to open and a fall below `kClosedBelow` to close,
        // and between the two nothing changes -- which is what stops a door settling shut from
        // flickering the portal, and the whole cell behind it, on and off.
        if (!open_ && aperture_ > kOpenAbove)
        {
            open_ = true;
        }
        else if (open_ && aperture_ < kClosedBelow)
        {
            open_ = false;
        }
        return open_ != was;
    }

    bool PortalRuntime::PassesLight() const noexcept
    {
        if (!hasLeaf_ || opacity_ == world::PortalOpacity::Open)
        {
            return true;
        }
        // §25.3: "a closed glass door never closes its portal for vision, only for movement and
        // (partially) for sound".
        if (opacity_ == world::PortalOpacity::Glass || opacity_ == world::PortalOpacity::Translucent)
        {
            return true;
        }
        return open_;
    }

    bool PortalRuntime::PassesBodies() const noexcept
    {
        return !hasLeaf_ || open_;
    }
} // namespace cnahouse::visibility
