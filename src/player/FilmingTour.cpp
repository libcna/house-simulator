// SPDX-License-Identifier: MIT
#include "cnahouse/player/FilmingTour.hpp"

#include <algorithm>
#include <cmath>
#include <limits>

#include "cnahouse/physics/Move.hpp"
#include "cnahouse/util/ContentFile.hpp"
#include "cnahouse/util/Json.hpp"
#include "cnahouse/util/Log.hpp"

namespace cnahouse::player
{
    namespace
    {
        constexpr float kPi = 3.14159265359F;
        constexpr float kTurnSpeed = kPi / 4.0F;

        float Distance(const Microsoft::Xna::Framework::Vector3& a,
                       const Microsoft::Xna::Framework::Vector3& b)
        {
            return std::hypot(std::hypot(a.X - b.X, a.Z - b.Z), a.Y - b.Y);
        }
    } // namespace

    util::Result<void> FilmingTour::Load(std::string_view initialStatePath)
    {
        Stop();
        route_.clear();
        const auto text = util::ReadContentText(initialStatePath);
        if (!text)
        {
            return text.Error();
        }
        const auto document = util::JsonDocument::Parse(text.Value(), std::string(initialStatePath));
        if (!document)
        {
            return document.Error();
        }
        if (!document->Root().Has("filmingTour"))
        {
            return util::Ok(); // Old test worlds remain loadable, without a filming route.
        }
        const auto tour = document->Root().RequireObject("filmingTour");
        if (!tour)
        {
            return tour.Error();
        }
        const auto points = tour->RequireArray("route");
        if (!points)
        {
            return points.Error();
        }
        const auto rows = points->Elements();
        if (!rows)
        {
            return rows.Error();
        }
        std::vector<Point> loaded;
        for (const auto& row : *rows)
        {
            const auto feet = row.RequireVector3("feet");
            const auto cell = row.RequireString("cell");
            const auto crouched = row.RequireBool("crouched");
            const auto pause = row.RequireFloat("pauseSeconds");
            if (!feet)
            {
                return feet.Error();
            }
            if (!cell)
            {
                return cell.Error();
            }
            if (!crouched)
            {
                return crouched.Error();
            }
            if (!pause)
            {
                return pause.Error();
            }
            if (cell->empty() || !std::isfinite(feet->X) || !std::isfinite(feet->Y) ||
                !std::isfinite(feet->Z) || !std::isfinite(*pause) || *pause < 0.0F || *pause > 20.0F)
            {
                return util::Error(util::ErrorCode::InvalidData, "invalid filming point", row.Path());
            }
            loaded.push_back({*feet, *cell, *crouched, *pause});
        }
        if (loaded.size() < 2U || loaded.front().pauseSeconds == 0.0F)
        {
            return util::Error(util::ErrorCode::InvalidData,
                               "filming route needs an opening view and a walk");
        }
        route_ = std::move(loaded);
        return util::Ok();
    }

    bool FilmingTour::Start(const PlayerState& body,
                            std::string_view cell,
                            const physics::CollisionWorld& collision,
                            physics::BroadPhase& broad)
    {
        Stop();
        const auto* collisionCell = collision.Cell(cell);
        if (collisionCell == nullptr || body.noclip)
        {
            return false;
        }
        float nearest = std::numeric_limits<float>::max();
        for (std::size_t i = 0; i < route_.size(); ++i)
        {
            if (route_[i].cell != cell)
            {
                continue;
            }
            const float distance = Distance(route_[i].feet, body.Feet());
            if (distance < nearest)
            {
                // Join only a same-cell point reachable by the actual capsule sweep. Nearest
                // Euclidean distance alone can choose a point behind a bed/cupboard or return.
                const auto motion = route_[i].feet - body.Feet();
                const auto moved =
                    physics::CollideAndSlide(collision, *collisionCell, broad, body.Body(), motion);
                if (Distance(moved.position, body.position + motion) > 0.10F)
                {
                    continue;
                }
                nearest = distance;
                index_ = i;
            }
        }
        if (!std::isfinite(nearest) || nearest == std::numeric_limits<float>::max())
        {
            return false;
        }
        remaining_ = route_.size();
        active_ = true;
        closestDistance_ = nearest;
        util::Log::Info(
            util::LogCat::App, "filming tour starts in {}; C stops without moving the player", cell);
        return true;
    }

    void FilmingTour::Stop() noexcept
    {
        active_ = false;
        completed_ = false;
        stuck_ = false;
        remaining_ = 0U;
        reviewing_ = false;
        reviewTime_ = 0.0F;
        noProgressSeconds_ = 0.0F;
        visited_.clear();
    }

    InputState FilmingTour::Step(const PlayerState& body, const LookAngles& look, float seconds)
    {
        InputState input;
        if (!active_ || seconds <= 0.0F)
        {
            return input;
        }
        float wantedYaw = look.yaw;
        float wantedPitch = -kPi / 36.0F;
        if (reviewing_)
        {
            reviewTime_ += seconds;
            const float duration = route_[index_].pauseSeconds;
            wantedYaw = reviewYaw_ + 2.0F * kPi * std::min(reviewTime_ / duration, 1.0F);
            input.crouch = route_[index_].crouched;
            if (reviewTime_ >= duration)
            {
                reviewing_ = false;
                --remaining_;
                index_ = (index_ + 1U) % route_.size();
            }
        }
        else
        {
            while (remaining_ > 0U)
            {
                const auto& target = route_[index_];
                const auto feet = body.Feet();
                const float dx = target.feet.X - feet.X;
                const float dz = target.feet.Z - feet.Z;
                const float distance = std::hypot(dx, dz);
                const float dy = target.feet.Y - feet.Y;
                // A recorded contact point is not an exact navigation pin: the capsule may
                // slide along the same corner a few centimetres differently when yaw changes.
                // This advances intent only; actual collision/headroom stays untouched.
                const bool sameCellTransit =
                    target.pauseSeconds == 0.0F && body.cellId == target.cell && std::fabs(dy) < 0.30F;
                if (distance < 0.10F && (std::fabs(dy) < 0.16F || sameCellTransit))
                {
                    noProgressSeconds_ = 0.0F;
                    closestDistance_ = std::numeric_limits<float>::max();
                    if (target.pauseSeconds > 0.0F)
                    {
                        reviewing_ = true;
                        reviewTime_ = 0.0F;
                        reviewYaw_ = look.yaw;
                        visited_.push_back(target.cell);
                        util::Log::Info(
                            util::LogCat::App, "filming view {}: {}", visited_.size(), target.cell);
                        input.crouch = target.crouched;
                        break;
                    }
                    --remaining_;
                    index_ = (index_ + 1U) % route_.size();
                    continue;
                }
                const float distance3 = std::hypot(distance, dy);
                if (distance3 + 0.005F < closestDistance_)
                {
                    closestDistance_ = distance3;
                    noProgressSeconds_ = 0.0F;
                }
                else
                {
                    noProgressSeconds_ += seconds;
                }
                if (noProgressSeconds_ > 12.0F)
                {
                    active_ = false;
                    stuck_ = true;
                    util::Log::Warn(util::LogCat::App,
                                    "filming route stopped safely at point {} in {}",
                                    index_,
                                    target.cell);
                    return input;
                }
                // Look along the path, independently of world-space movement. Bounded angular
                // speed avoids instant turns; the real controller still decides every position.
                std::size_t ahead = index_;
                std::size_t lookPoints = 1U;
                float along = distance;
                while (along < 0.65F && lookPoints < remaining_)
                {
                    const std::size_t next = (ahead + 1U) % route_.size();
                    if (next == ahead || route_[ahead].pauseSeconds > 0.0F)
                    {
                        break;
                    }
                    along += Distance(route_[ahead].feet, route_[next].feet);
                    ahead = next;
                    ++lookPoints;
                }
                const auto& view = route_[ahead].feet;
                wantedYaw = std::atan2(view.X - feet.X, -(view.Z - feet.Z));
                wantedPitch = std::clamp(std::atan2(view.Y - feet.Y, std::max(along, 0.1F)), -0.25F, 0.25F);
                input.crouch = target.crouched;
                if (distance > 0.001F)
                {
                    // Slow towards each corner/stop without toggling the player's session run mode.
                    const float scale =
                        std::min(0.90F, distance * 5.0F) / (body.fastWalk ? kFastWalkSpeed : kWalkSpeed);
                    input.move = {(dx * std::cos(look.yaw) + dz * std::sin(look.yaw)) / distance * scale,
                                  (dx * std::sin(look.yaw) - dz * std::cos(look.yaw)) / distance * scale};
                }
                break;
            }
        }
        if (remaining_ == 0U)
        {
            active_ = false;
            completed_ = true;
            util::Log::Info(
                util::LogCat::App, "filming tour completed: {} room/grounds views", visited_.size());
        }
        input.look.X = std::clamp(
            std::remainder(wantedYaw - look.yaw, 2.0F * kPi), -kTurnSpeed * seconds, kTurnSpeed * seconds);
        // ApplyLook subtracts the source's screen-Y delta: pitch itself is positive UP.
        input.look.Y = std::clamp(look.pitch - wantedPitch, -0.3F * seconds, 0.3F * seconds);
        return input;
    }
} // namespace cnahouse::player
