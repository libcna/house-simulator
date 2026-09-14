// SPDX-License-Identifier: MIT
#pragma once

namespace Microsoft::Xna::Framework::Graphics
{
    class DepthStencilState;
    class RasterizerState;
} // namespace Microsoft::Xna::Framework::Graphics

namespace cnahouse::rendering
{

    /// @brief The three cull states the project uses, and which geometry each belongs to.
    ///
    /// **`CullClockwise` is the default, and that is measured, not assumed.** glTF declares a
    /// counter-clockwise triangle front-facing; XNA's default `CullCounterClockwise` removes exactly
    /// that winding. `HOUSE-00071` measured it end to end on an open single-sided quad: `CullClockwise`
    /// drew 8 960 pixels and `CullCounterClockwise` drew **0**.
    ///
    /// That probe also produced the warning attached to `ProceduralFront` below, which cost a whole
    /// probe run to learn.
    enum class CullPolicy
    {
        /// @brief Single-sided geometry imported from glTF. The default for everything the pipeline
        ///        produced.
        ImportedFront,

        /// @brief The same geometry under a **mirrored** placement, where the world matrix has a
        ///        negative determinant and the winding is therefore reversed.
        ///
        /// A mirrored prop drawn with `ImportedFront` is inside-out, which reads as a hole rather than
        /// as a backwards object -- so this is not a nicety.
        Mirrored,

        /// @brief Foliage cards, the sky dome and anything else meant to be seen from both sides.
        ///
        /// `HOUSE-00080` measured that a back-facing alpha-test card is invisible under
        /// `ImportedFront`, appears under `Mirrored`, and is drawn from both sides under this one with
        /// identical coverage.
        TwoSided,

        /// @brief Geometry this project GENERATES rather than imports.
        ///
        /// The same state as `ImportedFront` -- and a separate name, because
        /// **procedural geometry does not inherit the glTF winding convention** and forgetting that is
        /// silent. `HOUSE-00080`'s first fixture was wound top-left → top-right → bottom-right, which
        /// in normalised device coordinates (+Y is **up**) is *clockwise*, and the entire quad vanished:
        /// ten checks failed for one fixture reason. Every generator in phases 6, 10, 25 and 27 must
        /// wind counter-clockwise to match the imported assets, and naming this policy separately is
        /// where a reader is told so.
        ProceduralFront,
    };

    /// @brief The shared `RasterizerState` objects, created once.
    ///
    /// Created once because XNA state objects are immutable after first use and because a new one per
    /// draw is an allocation on the hottest path in the renderer -- `HOUSE-00106` measured a draw call
    /// at 8.15 µs of CPU, and there is no room in that for anything avoidable.
    [[nodiscard]] const Microsoft::Xna::Framework::Graphics::RasterizerState&
    StateFor(CullPolicy policy) noexcept;

    /// @brief The policy for geometry drawn under @p worldDeterminant.
    ///
    /// A negative determinant means the world matrix mirrors, which reverses the winding -- so the cull
    /// state has to reverse with it. Computing this in one place is what stops every draw site making
    /// the same judgement independently and one of them getting it wrong.
    [[nodiscard]] CullPolicy PolicyForDeterminant(float worldDeterminant, bool twoSided) noexcept;

    /// @brief Depth test used after Tier S's opaque static-light pass.
    ///
    /// The same world-space geometry and matrices are submitted again, so `Equal` keeps every
    /// covered pixel while disabled writes preserve the first pass's depth for later overlays.
    /// `HOUSE-00079` measured this on a depth-tilted quad: no rejected pixels and exact addition.
    [[nodiscard]] const Microsoft::Xna::Framework::Graphics::DepthStencilState& DepthEqualReadOnly() noexcept;

} // namespace cnahouse::rendering
