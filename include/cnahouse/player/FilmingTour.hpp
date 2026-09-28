// SPDX-License-Identifier: MIT
#pragma once

#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

#include "cnahouse/player/MouseLook.hpp"
#include "cnahouse/player/PlayerController.hpp"
#include "cnahouse/util/Result.hpp"

namespace cnahouse::player
{
    /// One owner-requested filming walk, authored in initialstate.json. This only produces
    /// controller intent; it never writes the body, bypasses collision or owns another camera.
    class FilmingTour
    {
    public:
        struct Point
        {
            Microsoft::Xna::Framework::Vector3 feet;
            std::string cell;
            bool crouched = false;
            float pauseSeconds = 0.0F;
        };

        [[nodiscard]] util::Result<void> Load(std::string_view initialStatePath);
        [[nodiscard]] bool Start(const PlayerState& body,
                                 std::string_view cell,
                                 const physics::CollisionWorld& collision,
                                 physics::BroadPhase& broad);
        void Stop() noexcept;
        [[nodiscard]] InputState Step(const PlayerState& body, const LookAngles& look, float seconds);

        [[nodiscard]] bool Active() const noexcept
        {
            return active_;
        }

        [[nodiscard]] bool Completed() const noexcept
        {
            return completed_;
        }

        [[nodiscard]] bool Stuck() const noexcept
        {
            return stuck_;
        }

        [[nodiscard]] std::size_t Index() const noexcept
        {
            return index_;
        }

        [[nodiscard]] const std::vector<Point>& Route() const noexcept
        {
            return route_;
        }

        [[nodiscard]] const std::vector<std::string>& Visited() const noexcept
        {
            return visited_;
        }

    private:
        std::vector<Point> route_;
        std::vector<std::string> visited_;
        std::size_t index_ = 0;
        std::size_t remaining_ = 0;
        bool active_ = false;
        bool completed_ = false;
        bool stuck_ = false;
        bool reviewing_ = false;
        float reviewTime_ = 0.0F;
        float reviewYaw_ = 0.0F;
        float noProgressSeconds_ = 0.0F;
        float closestDistance_ = 0.0F;
    };
} // namespace cnahouse::player
