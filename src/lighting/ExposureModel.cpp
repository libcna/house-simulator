// SPDX-License-Identifier: MIT
#include "cnahouse/lighting/ExposureModel.hpp"

#include <algorithm>
#include <cmath>

namespace cnahouse::lighting
{
    namespace
    {
        constexpr float kMinimumExposure = kExteriorExposure;

        float Sanitized(float value) noexcept
        {
            return std::isfinite(value) ? std::clamp(value, kMinimumExposure, kMaximumInteriorExposure)
                                        : 1.0F;
        }
    } // namespace

    float ExposureTargetFor(const RoomLightState& room, bool skyOpenExterior) noexcept
    {
        if (skyOpenExterior)
        {
            return kExteriorExposure;
        }
        const float level = std::clamp(room.Level(), kAmbientFloor, 1.0F);
        return 1.0F + (kMaximumInteriorExposure - 1.0F) * (1.0F - level);
    }

    void ExposureAdapter::Advance(float target, float deltaSeconds) noexcept
    {
        const float safeTarget = Sanitized(target);
        if (!initialized_)
        {
            Snap(safeTarget);
            return;
        }
        if (!std::isfinite(deltaSeconds) || deltaSeconds <= 0.0F || safeTarget == scale_)
        {
            return;
        }

        // A larger display multiplier means the camera entered a DARKER scene. That is the slow
        // 2.2 s branch; constricting after entering a bright scene is the faster 0.9 s branch.
        const float timeConstant =
            safeTarget > scale_ ? kDarkSceneAdaptationSeconds : kBrightSceneAdaptationSeconds;
        const float fraction = 1.0F - std::exp(-deltaSeconds / timeConstant);
        scale_ += (safeTarget - scale_) * fraction;
        if (std::abs(safeTarget - scale_) < 1.0e-5F)
        {
            scale_ = safeTarget;
        }
    }

    void ExposureAdapter::Snap(float target) noexcept
    {
        scale_ = Sanitized(target);
        initialized_ = true;
    }

    float ExposureAdapter::EffectScale() const noexcept
    {
        return std::max(scale_, 1.0F);
    }

    float ExposureAdapter::TintAlpha() const noexcept
    {
        return 1.0F - std::min(scale_, 1.0F);
    }

} // namespace cnahouse::lighting
