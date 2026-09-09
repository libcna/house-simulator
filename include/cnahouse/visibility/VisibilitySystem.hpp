// SPDX-License-Identifier: MIT
#pragma once

#include <cstdint>
#include <span>
#include <string_view>
#include <vector>

#include "Microsoft/Xna/Framework/Matrix.hpp"
#include "Microsoft/Xna/Framework/Plane.hpp"
#include "Microsoft/Xna/Framework/Vector3.hpp"

#include "cnahouse/app/ISystem.hpp"
#include "cnahouse/util/Ids.hpp"
#include "cnahouse/visibility/ClipFrustum.hpp"
#include "cnahouse/visibility/PortalRuntime.hpp"
#include "cnahouse/visibility/PortalTraversal.hpp"

namespace cnahouse::world
{
    class WorldData;
}

namespace cnahouse::visibility
{

    /// @brief Where the camera is this frame, as the visibility stage needs it.
    ///
    /// Assembled by whoever owns the camera and handed over BEFORE the stage runs. The system does
    /// not reach for a camera: §7.5's systems *"read and write state held by the objects the
    /// service container owns"* and do not call each other, and a visibility system that pulled a
    /// pose out of a renderer would be an ordering constraint that `UpdateStage` cannot express.
    struct CameraView
    {
        /// @brief §16.4's answer for the eye, from `player::CellTracker` (`HOUSE-00559`).
        util::Id cell;
        Microsoft::Xna::Framework::Vector3 eye;
        Microsoft::Xna::Framework::Matrix viewProjection;
        ClipFrustum frustum;
        Microsoft::Xna::Framework::Plane nearPlane;
        Microsoft::Xna::Framework::Plane farPlane;
    };

    /// @brief §25's step 2, once a frame: camera cell in, visible set out (`HOUSE-00670`).
    ///
    /// **One walk per frame, and everything downstream reads the same answer.** §25.1's pipeline
    /// runs chunk culling, instance culling, lighting, audio and residency off this set; if any of
    /// them re-ran the traversal they would get a slightly different answer for the same frame the
    /// moment the camera moved between the two calls, and a chunk drawn against a frustum the
    /// lighting did not agree with is the kind of bug that takes a week.
    ///
    /// The frame it was computed for is published beside it, so a consumer can assert it is
    /// reading this frame's set rather than the last one's -- which is what happens when a stage is
    /// accidentally ordered before `UpdateStage::Visibility`.
    class VisibilitySystem final : public app::ISystem
    {
    public:
        /// @brief Takes the world and builds one `PortalRuntime` per portal (`HOUSE-00665`).
        ///
        /// The world outlives the system; §15's data is loaded once and is const for the session.
        explicit VisibilitySystem(const world::WorldData& world);

        [[nodiscard]] app::UpdateStage Stage() const noexcept override
        {
            return app::UpdateStage::Visibility;
        }

        [[nodiscard]] std::string_view Name() const noexcept override
        {
            return "visibility";
        }

        void Update(const app::FrameContext& frame) override;

        /// @brief The camera to walk from, next time the stage runs.
        void SetCamera(const CameraView& camera);

        /// @brief §25.3's aperture, from the door that moved.
        ///
        /// Returns true when the latch changed -- which is when the visible set can change without
        /// the camera moving, and is what §65's doors publish.
        bool SetAperture(util::Id portal, float fraction);

        [[nodiscard]] float Aperture(util::Id portal) const;

        [[nodiscard]] std::span<const VisibleCell> Visible() const noexcept
        {
            return traversal_.Visible();
        }

        [[nodiscard]] bool IsVisible(util::Id cell) const noexcept
        {
            return traversal_.IsVisible(cell);
        }

        [[nodiscard]] const VisibleCell* Find(util::Id cell) const noexcept
        {
            return traversal_.Find(cell);
        }

        [[nodiscard]] const TraversalStats& Stats() const noexcept
        {
            return traversal_.Stats();
        }

        /// @brief The frame index the current set was computed for. Zero before the first update.
        [[nodiscard]] std::uint64_t Frame() const noexcept
        {
            return frame_;
        }

        [[nodiscard]] std::span<const PortalRuntime> Portals() const noexcept
        {
            return runtimes_;
        }

    private:
        const world::WorldData* world_ = nullptr;
        std::vector<PortalRuntime> runtimes_;
        PortalTraversal traversal_;
        CameraView camera_;
        std::uint64_t frame_ = 0;
        bool hasCamera_ = false;
    };

} // namespace cnahouse::visibility
