// SPDX-License-Identifier: MIT
#pragma once

#include <cstdint>

namespace Microsoft::Xna::Framework::Graphics
{
    class BlendState;
    class DepthStencilState;
    class GraphicsDevice;
    class RasterizerState;
} // namespace Microsoft::Xna::Framework::Graphics

namespace cnahouse::rendering
{

    /// @brief Skips redundant state sets, and counts the ones it could not skip.
    ///
    /// **Why bother, when `HOUSE-00106` measured `EffectPass::Apply()` at 0.184 µs and the draw call
    /// at 8.15 µs?** Precisely *because* of that ratio. State changes are cheap and draw calls are not,
    /// which means the useful thing this class does is not the saving — it is the **counting**. A
    /// renderer that changes blend state 900 times to issue 300 draws is sorting its render list
    /// wrongly, and that is invisible without a number. The skip is a small bonus; the counter is the
    /// diagnostic.
    ///
    /// So the counters are not debug-only. They are compiled always, exactly like `debug::Counters`,
    /// because a perf test that measured a different program would be measuring nothing.
    ///
    /// Comparison is by **pointer identity**, not by value. The project's states are the shared
    /// singletons of `RenderStates.hpp` and XNA's own `BlendState::Opaque` and friends, so identity is
    /// the right question and a member-by-member comparison would cost more than the set it avoids.
    class StateTracker
    {
    public:
        explicit StateTracker(Microsoft::Xna::Framework::Graphics::GraphicsDevice& device) noexcept
            : device_(&device)
        {
        }

        void SetBlend(const Microsoft::Xna::Framework::Graphics::BlendState& state);
        void SetDepthStencil(const Microsoft::Xna::Framework::Graphics::DepthStencilState& state);
        void SetRasterizer(const Microsoft::Xna::Framework::Graphics::RasterizerState& state);

        /// @brief Forgets what it believes is bound.
        ///
        /// **Required after anything else touches the device** -- `SpriteBatch::End`, a render-target
        /// change, a `DebugDraw::Flush`. A tracker that kept believing a stale binding would skip the
        /// set that was actually needed, which produces a frame drawn with someone else's blend state
        /// and is far worse than a redundant set. Called from `BeginFrame` too.
        void Invalidate() noexcept;

        void BeginFrame() noexcept;

        struct Counts
        {
            std::uint32_t blendApplied = 0;
            std::uint32_t blendSkipped = 0;
            std::uint32_t depthApplied = 0;
            std::uint32_t depthSkipped = 0;
            std::uint32_t rasterApplied = 0;
            std::uint32_t rasterSkipped = 0;

            [[nodiscard]] std::uint32_t TotalApplied() const noexcept
            {
                return blendApplied + depthApplied + rasterApplied;
            }

            [[nodiscard]] std::uint32_t TotalSkipped() const noexcept
            {
                return blendSkipped + depthSkipped + rasterSkipped;
            }
        };

        [[nodiscard]] const Counts& LastFrame() const noexcept
        {
            return lastFrame_;
        }

        [[nodiscard]] const Counts& Current() const noexcept
        {
            return current_;
        }

    private:
        Microsoft::Xna::Framework::Graphics::GraphicsDevice* device_ = nullptr;
        const Microsoft::Xna::Framework::Graphics::BlendState* blend_ = nullptr;
        const Microsoft::Xna::Framework::Graphics::DepthStencilState* depth_ = nullptr;
        const Microsoft::Xna::Framework::Graphics::RasterizerState* raster_ = nullptr;
        Counts current_;
        Counts lastFrame_;
    };

} // namespace cnahouse::rendering
