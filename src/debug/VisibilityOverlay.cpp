// SPDX-License-Identifier: MIT
#include "cnahouse/debug/VisibilityOverlay.hpp"

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
        constexpr float kRadiansToDegrees = 180.0F / std::numbers::pi_v<float>;

        /// A count that was never measured this frame prints as `-` rather than as 0. Zero is an
        /// answer -- "nothing was drawn" -- and a system that did not run has not given one.
        std::string Count(int value)
        {
            return value < 0 ? std::string("-") : std::to_string(value);
        }

        float Bearing(float yaw)
        {
            float degrees = std::fmod(yaw * kRadiansToDegrees, 360.0F);
            if (degrees < 0.0F)
            {
                degrees += 360.0F;
            }
            return degrees;
        }
    } // namespace

    std::vector<std::string> VisibilityOverlay::Lines(const VisibilitySnapshot& snapshot) const
    {
        std::vector<std::string> lines;

        // §25.8's first line, with its own numbers in its own order.
        lines.push_back(
            std::format("cells    visible {} / {}   maxdepth {}   frusta {}{}{}",
                        snapshot.visible.size(),
                        snapshot.cellsInWorld,
                        snapshot.traversal.maxDepth,
                        snapshot.traversal.portalsCrossed + 1,
                        snapshot.traversal.frustaDropped == 0
                            ? std::string()
                            : std::format("  ({} dropped)", snapshot.traversal.frustaDropped),
                        snapshot.traversal.cellsDropped == 0
                            ? std::string()
                            : std::format("  CELLS DROPPED {}", snapshot.traversal.cellsDropped)));

        // Every REASON a portal was not crossed, and not a total: a room that should be visible
        // and is not is one of these six numbers, and a total cannot say which.
        lines.push_back(std::format("portals  tested {}  crossed {}  |  closed {}  facing {}  "
                                    "deep {}  clipped {}  small {}  covered {}",
                                    snapshot.traversal.portalsTested,
                                    snapshot.traversal.portalsCrossed,
                                    snapshot.traversal.skippedClosed,
                                    snapshot.traversal.skippedFacing,
                                    snapshot.traversal.skippedDepth,
                                    snapshot.traversal.skippedClipped,
                                    snapshot.traversal.skippedArea,
                                    snapshot.traversal.skippedContained));

        lines.push_back(std::format("geometry chunks {} / {}   instances {} / {}   exterior {} / {} "
                                    "({} node)",
                                    Count(snapshot.chunksDrawn),
                                    Count(snapshot.chunksTested),
                                    Count(snapshot.instancesDrawn),
                                    Count(snapshot.instancesTested),
                                    Count(snapshot.exteriorDrawn),
                                    Count(snapshot.exteriorTested),
                                    Count(snapshot.exteriorNodes)));

        // §71.2 budgets 620 draw calls and 90 state changes. The budget is printed beside the
        // number, because a number a reader has to remember a budget for is a number they will
        // read wrong.
        lines.push_back(std::format("draws    {} / 620   state changes {} / 90   culling {}",
                                    Count(snapshot.drawCalls),
                                    Count(snapshot.stateChanges),
                                    snapshot.cullingApplied ? "ON" : "NOT APPLIED"));

        if (snapshot.frozen)
        {
            // First, and in capitals: a frozen overlay describes a frame that is not the one on
            // screen, and a reader who has forgotten they pressed `F5` will believe every number
            // above it.
            lines.push_back(std::format("FROZEN   the walk above is frame {}; the camera is at "
                                        "{:+.2f} {:+.2f} {:+.2f}",
                                        snapshot.walkFrame,
                                        snapshot.inspectionEye.X,
                                        snapshot.inspectionEye.Y,
                                        snapshot.inspectionEye.Z));
        }
        lines.push_back(std::format("camera   {}   pos {:+.2f} {:+.2f} {:+.2f}   yaw {:.0f}deg",
                                    snapshot.cell.empty() ? "-" : snapshot.cell,
                                    snapshot.eye.X,
                                    snapshot.eye.Y,
                                    snapshot.eye.Z,
                                    Bearing(snapshot.yaw)));

        const std::size_t listed = std::min(snapshot.visible.size(), kMaxListed);
        for (std::size_t i = 0; i < listed; ++i)
        {
            const VisibleCellLine& row = snapshot.visible[i];
            lines.push_back(std::format(
                "  {:<18} d{}  {} cone{}{}{}",
                row.cell,
                row.depth,
                row.cones,
                row.cones == 1 ? "" : "s",
                row.conesDropped == 0 ? std::string() : std::format(" (+{} dropped)", row.conesDropped),
                row.diffuse ? "  diffuse" : ""));
        }
        if (snapshot.visible.size() > listed)
        {
            lines.push_back(std::format("  ... and {} more", snapshot.visible.size() - listed));
        }
        if (snapshot.visible.empty())
        {
            // Not an empty space where the list would be: a walk that reached nothing is a real
            // and alarming answer, and it must not look like an overlay that has not been updated.
            lines.push_back("  (nothing visible, not even the camera's own cell)");
        }
        return lines;
    }

    void VisibilityOverlay::Draw(Microsoft::Xna::Framework::Graphics::SpriteBatch& batch,
                                 const ui::TextRenderer& text,
                                 const VisibilitySnapshot& snapshot) const
    {
        if (!visible_ || !text.HasFont())
        {
            return;
        }
        // The same units and margin as §69's `F2` and §71's `F1`: the overlays are read one after
        // the other and a reader should not have to find the text again.
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
