// SPDX-License-Identifier: MIT
#pragma once

#include <cstdint>
#include <memory>
#include <vector>

#include "Microsoft/Xna/Framework/BoundingBox.hpp"
#include "Microsoft/Xna/Framework/Color.hpp"
#include "Microsoft/Xna/Framework/Matrix.hpp"
#include "Microsoft/Xna/Framework/Vector3.hpp"

namespace Microsoft::Xna::Framework
{
    class BoundingFrustum;
    struct BoundingSphere;
} // namespace Microsoft::Xna::Framework

namespace Microsoft::Xna::Framework::Graphics
{
    class BasicEffect;
    class GraphicsDevice;
} // namespace Microsoft::Xna::Framework::Graphics

namespace cnahouse::debug
{

    /// @brief Immediate-mode wireframe drawing, on `BasicEffect` and nothing else.
    ///
    /// **This is the tool that makes the portal system debuggable at all.** A visibility bug is
    /// invisible by construction -- the symptom is geometry that is *not* there -- so the only way to
    /// see one is to draw the frustum, the cell bounds and the portal rectangles that produced the
    /// decision. Phases 9, 7 and 41 are effectively undebuggable without it, which is why it is built
    /// in phase 2 rather than when it is first missed.
    ///
    /// **Everything is batched into one dynamic vertex buffer and drawn in ONE call per primitive
    /// type.** `HOUSE-00106` measured a draw call at 8.15 µs of CPU, so a `DrawUserPrimitives` per
    /// wire box -- the obvious implementation -- would cost more than the scene it is annotating and
    /// would change the very timings the overlay reports. `HOUSE-00092` measured 2 000 dynamic quads
    /// at 0.171 ms, so the batched form is genuinely cheap.
    ///
    /// Compiled out entirely when `CNAHOUSE_DEBUG_TOOLS` is off: `HOUSE-00147`'s acceptance.
    class DebugDraw
    {
    public:
        /// @brief The most line vertices one frame may queue.
        ///
        /// Sized for a full portal-traversal visualisation of a large floor -- 78 cells, their bounds,
        /// their portals and the frusta -- and bounded so a runaway caller costs a dropped line rather
        /// than an allocation storm inside a frame.
        static constexpr std::size_t kMaxLineVertices = 65536;

        explicit DebugDraw(Microsoft::Xna::Framework::Graphics::GraphicsDevice& device);
        ~DebugDraw();

        DebugDraw(const DebugDraw&) = delete;
        DebugDraw& operator=(const DebugDraw&) = delete;

        /// @brief Discards last frame's geometry. Called once per frame before anything queues.
        void Begin(const Microsoft::Xna::Framework::Matrix& view,
                   const Microsoft::Xna::Framework::Matrix& projection) noexcept;

        void Line(const Microsoft::Xna::Framework::Vector3& from,
                  const Microsoft::Xna::Framework::Vector3& to,
                  Microsoft::Xna::Framework::Color colour);

        void Box(const Microsoft::Xna::Framework::BoundingBox& box, Microsoft::Xna::Framework::Color colour);

        /// @brief A wire sphere as three orthogonal circles.
        ///
        /// Three circles rather than a mesh: a mesh reads as a solid at a glance and hides what is
        /// behind it, which defeats the purpose of a debug overlay.
        void Sphere(const Microsoft::Xna::Framework::BoundingSphere& sphere,
                    Microsoft::Xna::Framework::Color colour,
                    int segments = 24);

        /// @brief The eight corners of a frustum, wired as near quad, far quad and four edges.
        ///
        /// `HOUSE-00104` verified `BoundingFrustum::GetCorners` against analytic answers, so what this
        /// draws is the frustum the visibility system is actually using and not an approximation of it.
        void Frustum(const Microsoft::Xna::Framework::BoundingFrustum& frustum,
                     Microsoft::Xna::Framework::Color colour);

        /// @brief A filled quad from four corners in winding order. For portal rectangles.
        void Quad(const Microsoft::Xna::Framework::Vector3& a,
                  const Microsoft::Xna::Framework::Vector3& b,
                  const Microsoft::Xna::Framework::Vector3& c,
                  const Microsoft::Xna::Framework::Vector3& d,
                  Microsoft::Xna::Framework::Color colour);

        /// @brief Submits everything queued. One draw per primitive type.
        void Flush();

        [[nodiscard]] std::size_t LineVertexCount() const noexcept
        {
            return lines_.size();
        }

        [[nodiscard]] std::size_t TriangleVertexCount() const noexcept
        {
            return triangles_.size();
        }

        /// @brief How many vertices were dropped because the buffer was full, since `Begin`.
        [[nodiscard]] std::size_t DroppedVertices() const noexcept
        {
            return dropped_;
        }

    private:
        struct Vertex
        {
            Microsoft::Xna::Framework::Vector3 position;
            /// Packed RGBA, because `Color` is not trivially copyable and so cannot go in a vertex
            /// struct passed to `SetData<T>` -- measured while writing the phase-1 probes.
            std::uint32_t colour;
        };

        void Push(std::vector<Vertex>& target,
                  const Microsoft::Xna::Framework::Vector3& position,
                  Microsoft::Xna::Framework::Color colour);

        class Impl;
        std::unique_ptr<Impl> impl_;
        std::vector<Vertex> lines_;
        std::vector<Vertex> triangles_;
        std::size_t dropped_ = 0;
    };

} // namespace cnahouse::debug
