// SPDX-License-Identifier: MIT
#include "cnahouse/rendering/Camera.hpp"

#include "Microsoft/Xna/Framework/MathHelper.hpp"

namespace cnahouse::rendering
{
    namespace Xna = Microsoft::Xna::Framework;

    Xna::Matrix Camera::View() const
    {
        return Xna::Matrix::CreateLookAt(eye, target, Xna::Vector3::Up);
    }

    Xna::Matrix Camera::Projection(float aspect) const
    {
        // A zero or negative aspect is a viewport that has not been sized yet -- during a resize,
        // or before the first frame. `CreatePerspectiveFieldOfView` throws on it, and a frame that
        // threw is worse than a frame drawn square.
        const float safe = aspect > 0.0f ? aspect : 1.0f;
        return Xna::Matrix::CreatePerspectiveFieldOfView(
            Xna::MathHelper::ToRadians(fieldOfViewDegrees), safe, nearPlane, farPlane);
    }

} // namespace cnahouse::rendering
