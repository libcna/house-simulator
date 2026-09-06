// SPDX-License-Identifier: MIT
#pragma once

#include <string>
#include <vector>

#include "cnahouse/debug/Counters.hpp"
#include "cnahouse/debug/Timing.hpp"

namespace Microsoft::Xna::Framework::Graphics
{
    class SpriteBatch;
}

namespace cnahouse::ui
{
    class TextRenderer;
}

namespace cnahouse::app
{
    struct Platform;
}

namespace cnahouse::debug
{

    /// @brief The `F1` performance overlay: frame time, a frame graph, per-stage times and counters.
    ///
    /// **Why the frame graph and not just the numbers.** A number tells you the average; a graph tells
    /// you the *shape*, and the shape is what distinguishes "this scene is heavy" from "something
    /// hitches every two seconds". The second is the bug that actually gets reported, and it is
    /// invisible in an average.
    ///
    /// The overlay is a *presenter*: it owns no measurement, only the layout of measurements taken
    /// elsewhere. That is what lets a perf test assert against `Counters` and `Timing` directly rather
    /// than parsing a screen.
    class Overlay
    {
    public:
        /// @brief How many frames the graph shows. Two seconds at 60 Hz.
        static constexpr std::size_t kGraphFrames = 120;
        /// @brief The frame time the graph's top edge represents: 33.3 ms, i.e. 30 fps.
        ///
        /// Not 16.6: a graph whose ceiling is the target clips exactly when something goes wrong, which
        /// is the moment the shape matters most. The 16.6 ms line is drawn *inside* the graph instead.
        static constexpr float kGraphCeilingMilliseconds = 33.3f;

        void SetVisible(bool visible) noexcept
        {
            visible_ = visible;
        }

        void Toggle() noexcept
        {
            visible_ = !visible_;
        }

        [[nodiscard]] bool Visible() const noexcept
        {
            return visible_;
        }

        /// @brief Records one frame's total time for the graph.
        void PushFrameTime(float milliseconds);

        /// @brief The lines the overlay would draw, top to bottom.
        ///
        /// Returned as text rather than drawn directly so the content is unit-testable without a
        /// device -- which is the difference between an overlay that is verified and one that is merely
        /// looked at.
        [[nodiscard]] std::vector<std::string>
        Lines(const app::Platform& platform, const Timing& timing, const Counters& counters) const;

        /// @brief The graph as one row of characters, oldest frame first.
        ///
        /// A text graph, deliberately: it needs no vertex buffer, no second effect and no render
        /// target, it works identically under `HEADLESS`, and it can be asserted in a test. A pixel
        /// graph is prettier and would have to wait for a render harness to be worth anything.
        [[nodiscard]] std::string GraphRow() const;

        void Draw(Microsoft::Xna::Framework::Graphics::SpriteBatch& batch,
                  const ui::TextRenderer& text,
                  const app::Platform& platform,
                  const Timing& timing,
                  const Counters& counters) const;

    private:
        bool visible_ = false;
        std::vector<float> frameTimes_;
    };

} // namespace cnahouse::debug
