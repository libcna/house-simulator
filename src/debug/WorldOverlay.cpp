// SPDX-License-Identifier: MIT
#include "cnahouse/debug/WorldOverlay.hpp"

#include <cmath>
#include <format>
#include <numbers>

#include "Microsoft/Xna/Framework/Color.hpp"
#include "Microsoft/Xna/Framework/Graphics/SpriteBatch.hpp"
#include "Microsoft/Xna/Framework/Vector2.hpp"

#include "cnahouse/ui/TextRenderer.hpp"

namespace cnahouse::debug
{
    namespace
    {
        constexpr float kPi = std::numbers::pi_v<float>;
        constexpr float kRadiansToDegrees = 180.0F / kPi;

        std::string_view StepName(world::SpatialIndex::Step step)
        {
            switch (step)
            {
                case world::SpatialIndex::Step::Incremental:
                    return "incremental";
                case world::SpatialIndex::Step::Neighbour:
                    return "neighbour";
                case world::SpatialIndex::Step::Grid:
                    return "grid";
                case world::SpatialIndex::Step::None:
                    return "NOT FOUND";
            }
            return "?";
        }

        std::string_view KindName(physics::CollisionKind kind)
        {
            switch (kind)
            {
                case physics::CollisionKind::Floor:
                    return "floor";
                case physics::CollisionKind::Ceiling:
                    return "ceiling";
                case physics::CollisionKind::Wall:
                    return "wall";
                case physics::CollisionKind::Stair:
                    return "stair";
                case physics::CollisionKind::Prop:
                    return "prop";
                case physics::CollisionKind::Exterior:
                    return "exterior";
            }
            return "?";
        }
    } // namespace

    float WorldOverlay::BearingDegrees(float yaw)
    {
        // §14: yaw 0 looks north and positive turns east, which is exactly a compass bearing --
        // so this is a wrap and a unit conversion and nothing else.
        float degrees = std::fmod(yaw * kRadiansToDegrees, 360.0F);
        if (degrees < 0.0F)
        {
            degrees += 360.0F;
        }
        return degrees;
    }

    std::string WorldOverlay::Compass(float yaw)
    {
        static constexpr std::string_view kPoints[] = {"N", "NE", "E", "SE", "S", "SW", "W", "NW"};
        // Eight points, so each covers 45° and the boundary is at 22.5°: rounding to the nearest
        // point is what makes "N" mean "within 22.5° of north" rather than "exactly north".
        const int index = static_cast<int>(std::lround(BearingDegrees(yaw) / 45.0F)) % 8;
        return std::string(kPoints[static_cast<std::size_t>(index)]);
    }

    std::vector<std::string> WorldOverlay::Lines(const WorldSnapshot& snapshot) const
    {
        std::vector<std::string> lines;

        // The cell first, because it is the answer to the question this overlay is opened for --
        // and WHICH of §16.4's four steps found it, because "grid" every frame means the
        // incremental test is failing and the 5 cm hysteresis is doing nothing.
        lines.push_back(std::format("cell     {}  ({})  level {}",
                                    snapshot.cell.empty() ? "-" : snapshot.cell,
                                    StepName(snapshot.cellFoundBy),
                                    snapshot.level.empty() ? "-" : snapshot.level));

        // Millimetres. A metre-precision position cannot be typed back into `teleport`, which is
        // most of what a position on a debug overlay is FOR.
        lines.push_back(std::format("pos      {:+.3f} {:+.3f} {:+.3f}",
                                    snapshot.position.X,
                                    snapshot.position.Y,
                                    snapshot.position.Z));

        lines.push_back(std::format("look     yaw {:.1f}deg {}  pitch {:+.1f}deg",
                                    BearingDegrees(snapshot.yaw),
                                    Compass(snapshot.yaw),
                                    snapshot.pitch * kRadiansToDegrees));

        lines.push_back(std::format("move     {:.2f} m/s  {}{}{}",
                                    snapshot.speed,
                                    snapshot.fastWalk ? "run" : "walk",
                                    snapshot.crouched ? "  crouched" : "",
                                    snapshot.onGround ? "" : "  AIRBORNE"));

        if (snapshot.onGround)
        {
            lines.push_back(std::format("ground   {} '{}'  gap {:.0f} mm",
                                        KindName(snapshot.ground),
                                        snapshot.surface.empty() ? "-" : snapshot.surface,
                                        snapshot.groundGap * 1000.0F));
        }
        else
        {
            // Not "floor '-' gap 0 mm": nothing is under the body, and saying so in the same shape
            // as an answer is how a debug overlay tells its reader something that is not true.
            lines.push_back("ground   nothing under the body");
        }

        // §54 and §50 arrive in phase 14. The lines are here and say so rather than being absent,
        // because an overlay that grows lines as features land is one whose layout moves under a
        // reader who has learnt where to look.
        lines.push_back(std::format("held     {}", snapshot.heldItem.value_or("-")));
        lines.push_back(std::format("target   {}{}",
                                    snapshot.target.value_or("-"),
                                    snapshot.targetState.empty() ? "" : "  " + snapshot.targetState));
        return lines;
    }

    void WorldOverlay::Draw(Microsoft::Xna::Framework::Graphics::SpriteBatch& batch,
                            const ui::TextRenderer& text,
                            const WorldSnapshot& snapshot) const
    {
        if (!visible_ || !text.HasFont())
        {
            return;
        }
        // The same virtual units as §71's `F1`, and the same left margin: the two overlays are
        // read one after the other and a reader should not have to find the text again.
        constexpr float kLineHeight = 18.0F;
        constexpr float kLeft = 12.0F;
        constexpr float kTop = 40.0F;

        const std::vector<std::string> lines = Lines(snapshot);
        for (std::size_t i = 0; i < lines.size(); ++i)
        {
            text.DrawShadowed(
                batch,
                lines[i],
                Microsoft::Xna::Framework::Vector2(kLeft, kTop + static_cast<float>(i) * kLineHeight),
                ui::Anchor::TopLeft,
                Microsoft::Xna::Framework::Color::White);
        }
    }

} // namespace cnahouse::debug
