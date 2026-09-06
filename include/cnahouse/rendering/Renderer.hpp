// SPDX-License-Identifier: MIT
#pragma once

#include <array>
#include <cstdint>
#include <memory>
#include <string_view>

namespace Microsoft::Xna::Framework::Graphics
{
    class GraphicsDevice;
}

namespace cnahouse::debug
{
    class Counters;
}

namespace cnahouse::rendering
{

    class RenderTier;
    class StateTracker;

    /// @brief The draw passes of `cna-house.md` §7.5, in the order they run.
    ///
    /// **The enum order IS the frame order.** `Renderer` walks it from `Shadow` to `Hud` and offers
    /// no way to reorder, because the order is an architectural decision -- sky before opaque so the
    /// dome is overdrawn rather than overdrawing, alpha-test before transparent so cut-outs write
    /// depth the sorted pass can test against -- and a runtime-sortable list would let a caller
    /// discover that decision by getting it wrong.
    enum class Pass : std::uint8_t
    {
        Shadow = 0,    ///< (Tier E) light frustum fit, `ShadowDepth.fx` into a render target
        Sky,           ///< dome, stars, moon, sun, clouds -- depth write off
        OpaqueStatic,  ///< per visible cell, per chunk, `DualTexture`/room-lit
        OpaqueDynamic, ///< props, pets, avatar, doors
        AlphaTest,     ///< foliage, fences, grilles
        Transparent,   ///< glass, water, curtains, particles -- back to front
        GlareQueries,  ///< `OcclusionQuery` grid at the sun and moon
        Composite,     ///< (Tier E) tonemap and glare, full-screen quad
        Hud,           ///< `SpriteBatch` prompt, clock, debug overlays
        Count,
    };

    [[nodiscard]] std::string_view PassName(Pass pass) noexcept;

    /// @brief Whether @p pass exists only in Tier E.
    ///
    /// ADR-0003 promises that Tier S is COMPLETE, which is only true because the two passes that
    /// need compiled effects are the two whose absence changes how the frame looks and not what it
    /// contains: no shadow map, and no tonemap-plus-glare composite.
    [[nodiscard]] bool PassIsTierEOnly(Pass pass) noexcept;

    /// @brief What a pass is handed. Deliberately small: a pass that needs more state should be
    ///        given it at construction, not found through a context that grows into a service
    ///        locator.
    struct PassContext
    {
        Microsoft::Xna::Framework::Graphics::GraphicsDevice& device;
        StateTracker& states;
        debug::Counters& counters;
        float deltaSeconds = 0.0f;
    };

    /// @brief One draw pass.
    class IRenderPass
    {
    public:
        virtual ~IRenderPass() = default;

        virtual void Draw(PassContext& context) = 0;

        /// @brief Whether the pass has anything to draw this frame.
        ///
        /// Asked once per frame instead of leaving each pass to return early, so that "ran" and
        /// "had nothing to do" are different numbers in the overlay. A pass that quietly did
        /// nothing for a hundred frames is a bug that looks exactly like a pass that is working.
        [[nodiscard]] virtual bool IsActive() const
        {
            return true;
        }

        /// @brief Whether this pass leaves the device in a state `StateTracker` cannot predict.
        ///
        /// `SpriteBatch::Begin`/`End` sets and restores several states at once, and a render-target
        /// change resets others. `Renderer` invalidates the tracker after any pass that says yes,
        /// which is the difference between a redundant state set and a frame drawn with the sprite
        /// batch's blend state.
        [[nodiscard]] virtual bool DisturbsDeviceState() const
        {
            return false;
        }
    };

    /// @brief Runs the passes of §7.5 in order, times each one, and skips the ones that cannot run.
    ///
    /// **This type holds no passes of its own.** Phases 12 onwards install them; until then a pass
    /// that is not installed is skipped and counted, which is exactly what an empty frame should
    /// report. What is real here from the start is the ordering, the tier gating, the state
    /// invalidation between passes and the per-pass timing -- the parts that every later pass
    /// depends on and that are painful to retrofit.
    class Renderer
    {
    public:
        /// @brief Four seconds at 60 Hz, matching `debug::Timing::kWindow` so the columns agree.
        static constexpr std::size_t kWindow = 240;

        explicit Renderer(const RenderTier& tier) noexcept
            : tier_(&tier)
        {
        }

        /// @brief Installs @p impl as @p pass, replacing whatever was there.
        void Install(Pass pass, std::unique_ptr<IRenderPass> impl);

        [[nodiscard]] bool IsInstalled(Pass pass) const noexcept;

        /// @brief Turns a pass off for debugging. Cannot turn on a pass that is not installed or
        ///        that this tier does not have.
        void SetEnabled(Pass pass, bool enabled) noexcept;
        [[nodiscard]] bool IsEnabled(Pass pass) const noexcept;

        /// @brief Whether @p pass would run this frame: installed, enabled, and allowed by the tier.
        [[nodiscard]] bool WillRun(Pass pass) const noexcept;

        /// @brief Runs the frame.
        void Draw(PassContext& context);

        [[nodiscard]] double LastMilliseconds(Pass pass) const noexcept;
        [[nodiscard]] double AverageMilliseconds(Pass pass) const noexcept;
        /// @brief The sum of every pass's average. What the draw side costs on the CPU.
        [[nodiscard]] double TotalAverageMilliseconds() const noexcept;

        /// @brief Passes that ran in the last completed frame, and passes that did not.
        [[nodiscard]] std::uint32_t PassesRun() const noexcept
        {
            return passesRun_;
        }

        [[nodiscard]] std::uint32_t PassesSkipped() const noexcept
        {
            return passesSkipped_;
        }

    private:
        static constexpr std::size_t kPassCount = static_cast<std::size_t>(Pass::Count);

        struct Slot
        {
            std::unique_ptr<IRenderPass> impl;
            bool enabled = true;
            double last = 0.0;
            std::array<double, kWindow> window{};
            std::size_t next = 0;
            std::size_t count = 0;
        };

        const RenderTier* tier_;
        std::array<Slot, kPassCount> slots_{};
        std::uint32_t passesRun_ = 0;
        std::uint32_t passesSkipped_ = 0;
    };

} // namespace cnahouse::rendering
