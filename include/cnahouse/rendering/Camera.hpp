// SPDX-License-Identifier: MIT
#pragma once

#include "Microsoft/Xna/Framework/Matrix.hpp"
#include "Microsoft/Xna/Framework/Vector3.hpp"

namespace cnahouse::rendering
{

    /// @brief Where the frame is drawn from: an eye, a target, and a lens.
    ///
    /// A plain aggregate rather than a class with a controller inside it. `HOUSE-00476`'s free-fly
    /// camera moves one of these; `cna-house.md` §26's player camera will set one from the avatar;
    /// a render test fixes one at a stated place. All three want the same two matrices and none of
    /// them wants the others' input handling.
    ///
    /// `Vector3::Up` and a right-handed look-at, which is what `Matrix::CreateLookAt` gives and what
    /// §14 says the world is: Y up, −Z north.
    struct Camera
    {
        Microsoft::Xna::Framework::Vector3 eye{0.0f, 1.7f, 0.0f};
        Microsoft::Xna::Framework::Vector3 target{0.0f, 1.7f, -1.0f};
        /// @brief Vertical field of view in degrees. §67.1's default.
        float fieldOfViewDegrees = 60.0f;
        /// @brief §70.2: 0.10 m, close enough to put the camera against a wall without clipping
        ///        through it, far enough that the depth buffer keeps its precision at 60 m.
        float nearPlane = 0.10f;
        /// @brief Far enough for the whole plot: `EXT_WORLD` is 400 m across.
        float farPlane = 400.0f;

        [[nodiscard]] Microsoft::Xna::Framework::Matrix View() const;
        /// @brief The projection for a viewport of @p aspect (width / height).
        [[nodiscard]] Microsoft::Xna::Framework::Matrix Projection(float aspect) const;
    };

} // namespace cnahouse::rendering
