// SPDX-License-Identifier: MIT
#include "cnahouse/debug/DebugDraw.hpp"

#include <cmath>

#include "Microsoft/Xna/Framework/BoundingFrustum.hpp"
#include "Microsoft/Xna/Framework/BoundingSphere.hpp"
#include "Microsoft/Xna/Framework/Graphics/BasicEffect.hpp"
#include "Microsoft/Xna/Framework/Graphics/BlendState.hpp"
#include "Microsoft/Xna/Framework/Graphics/BufferUsage.hpp"
#include "Microsoft/Xna/Framework/Graphics/DepthStencilState.hpp"
#include "Microsoft/Xna/Framework/Graphics/DynamicVertexBuffer.hpp"
#include "Microsoft/Xna/Framework/Graphics/EffectPass.hpp"
#include "Microsoft/Xna/Framework/Graphics/EffectPassCollection.hpp"
#include "Microsoft/Xna/Framework/Graphics/EffectTechnique.hpp"
#include "Microsoft/Xna/Framework/Graphics/GraphicsDevice.hpp"
#include "Microsoft/Xna/Framework/Graphics/PrimitiveType.hpp"
#include "Microsoft/Xna/Framework/Graphics/RasterizerState.hpp"
#include "Microsoft/Xna/Framework/Graphics/SetDataOptions.hpp"
#include "Microsoft/Xna/Framework/Graphics/VertexDeclaration.hpp"
#include "Microsoft/Xna/Framework/Graphics/VertexElement.hpp"
#include "Microsoft/Xna/Framework/MathHelper.hpp"

namespace cnahouse::debug
{
    namespace
    {
        using Microsoft::Xna::Framework::Color;
        using Microsoft::Xna::Framework::Matrix;
        using Microsoft::Xna::Framework::Vector3;
        namespace Gfx = Microsoft::Xna::Framework::Graphics;

        /// XNA's `VertexElementFormat::Color` reads a packed 0xAABBGGRR word. `Color` itself is not
        /// trivially copyable, so it cannot be a member of a vertex struct handed to `SetData<T>` --
        /// measured while writing the phase-1 probes, and the reason this conversion exists.
        std::uint32_t Pack(Color colour)
        {
            return static_cast<std::uint32_t>(colour.getRProperty()) |
                   (static_cast<std::uint32_t>(colour.getGProperty()) << 8) |
                   (static_cast<std::uint32_t>(colour.getBProperty()) << 16) |
                   (static_cast<std::uint32_t>(colour.getAProperty()) << 24);
        }

    } // namespace

    class DebugDraw::Impl
    {
    public:
        explicit Impl(Gfx::GraphicsDevice& graphicsDevice)
            : device(graphicsDevice)
            , declaration({Gfx::VertexElement(
                               0, Gfx::VertexElementFormat::Vector3, Gfx::VertexElementUsage::Position, 0),
                           Gfx::VertexElement(
                               12, Gfx::VertexElementFormat::Color, Gfx::VertexElementUsage::Color, 0)})
            , buffer(graphicsDevice,
                     declaration,
                     static_cast<int>(DebugDraw::kMaxLineVertices),
                     Gfx::BufferUsage::WriteOnly)
            , effect(graphicsDevice)
        {
            effect.setLightingEnabledProperty(false);
            effect.setTextureEnabledProperty(false);
            effect.setVertexColorEnabledProperty(true);
            effect.setWorldProperty(Matrix::getIdentityProperty());
        }

        Gfx::GraphicsDevice& device;
        Gfx::VertexDeclaration declaration;
        Gfx::DynamicVertexBuffer buffer;
        Gfx::BasicEffect effect;
    };

    DebugDraw::DebugDraw(Gfx::GraphicsDevice& device)
        : impl_(std::make_unique<Impl>(device))
    {
        lines_.reserve(4096);
        triangles_.reserve(1024);
    }

    DebugDraw::~DebugDraw() = default;

    void DebugDraw::Begin(const Matrix& view, const Matrix& projection) noexcept
    {
        lines_.clear();
        triangles_.clear();
        dropped_ = 0;
        impl_->effect.setViewProperty(view);
        impl_->effect.setProjectionProperty(projection);
    }

    void DebugDraw::Push(std::vector<Vertex>& target, const Vector3& position, Color colour)
    {
        if (lines_.size() + triangles_.size() >= kMaxLineVertices)
        {
            // Dropped rather than grown. A runaway caller inside a frame is a bug, and an allocation
            // storm would hide it behind a hitch instead of showing it as missing lines plus a count.
            ++dropped_;
            return;
        }
        target.push_back(Vertex{position, Pack(colour)});
    }

    void DebugDraw::Line(const Vector3& from, const Vector3& to, Color colour)
    {
        Push(lines_, from, colour);
        Push(lines_, to, colour);
    }

    void DebugDraw::Box(const Microsoft::Xna::Framework::BoundingBox& box, Color colour)
    {
        const Vector3& lo = box.Min;
        const Vector3& hi = box.Max;
        const Vector3 corners[8] = {
            Vector3(lo.X, lo.Y, lo.Z),
            Vector3(hi.X, lo.Y, lo.Z),
            Vector3(hi.X, hi.Y, lo.Z),
            Vector3(lo.X, hi.Y, lo.Z),
            Vector3(lo.X, lo.Y, hi.Z),
            Vector3(hi.X, lo.Y, hi.Z),
            Vector3(hi.X, hi.Y, hi.Z),
            Vector3(lo.X, hi.Y, hi.Z),
        };
        static constexpr int kEdges[12][2] = {
            {0, 1}, {1, 2}, {2, 3}, {3, 0}, {4, 5}, {5, 6}, {6, 7}, {7, 4}, {0, 4}, {1, 5}, {2, 6}, {3, 7}};
        for (const auto& edge : kEdges)
        {
            Line(corners[edge[0]], corners[edge[1]], colour);
        }
    }

    void
    DebugDraw::Sphere(const Microsoft::Xna::Framework::BoundingSphere& sphere, Color colour, int segments)
    {
        if (segments < 3)
        {
            segments = 3;
        }
        const float step = Microsoft::Xna::Framework::MathHelper::TwoPi / static_cast<float>(segments);
        // Three orthogonal circles rather than a mesh: a mesh reads as a solid at a glance and hides
        // what is behind it, which is the opposite of what a debug overlay is for.
        for (int axis = 0; axis < 3; ++axis)
        {
            for (int i = 0; i < segments; ++i)
            {
                const float a = static_cast<float>(i) * step;
                const float b = static_cast<float>(i + 1) * step;
                const float ca = std::cos(a) * sphere.Radius;
                const float sa = std::sin(a) * sphere.Radius;
                const float cb = std::cos(b) * sphere.Radius;
                const float sb = std::sin(b) * sphere.Radius;
                Vector3 from;
                Vector3 to;
                if (axis == 0)
                {
                    from = Vector3(ca, sa, 0.0f);
                    to = Vector3(cb, sb, 0.0f);
                }
                else if (axis == 1)
                {
                    from = Vector3(ca, 0.0f, sa);
                    to = Vector3(cb, 0.0f, sb);
                }
                else
                {
                    from = Vector3(0.0f, ca, sa);
                    to = Vector3(0.0f, cb, sb);
                }
                Line(sphere.Center + from, sphere.Center + to, colour);
            }
        }
    }

    void DebugDraw::Frustum(const Microsoft::Xna::Framework::BoundingFrustum& frustum, Color colour)
    {
        // `HOUSE-00104` verified `GetCorners` against analytic answers, so what is drawn here is the
        // frustum the visibility system is actually using rather than a reconstruction of it.
        const std::vector<Vector3> corners = frustum.GetCorners();
        if (corners.size() < 8)
        {
            return;
        }
        for (int i = 0; i < 4; ++i)
        {
            Line(
                corners[static_cast<std::size_t>(i)], corners[static_cast<std::size_t>((i + 1) % 4)], colour);
            Line(corners[static_cast<std::size_t>(4 + i)],
                 corners[static_cast<std::size_t>(4 + (i + 1) % 4)],
                 colour);
            Line(corners[static_cast<std::size_t>(i)], corners[static_cast<std::size_t>(4 + i)], colour);
        }
    }

    void DebugDraw::Quad(const Vector3& a, const Vector3& b, const Vector3& c, const Vector3& d, Color colour)
    {
        Push(triangles_, a, colour);
        Push(triangles_, b, colour);
        Push(triangles_, c, colour);
        Push(triangles_, a, colour);
        Push(triangles_, c, colour);
        Push(triangles_, d, colour);
    }

    void DebugDraw::Flush()
    {
        if (lines_.empty() && triangles_.empty())
        {
            return;
        }
        Gfx::GraphicsDevice& device = impl_->device;
        // `CullNone`, because a debug wireframe is meant to be visible from behind. `DepthRead` rather
        // than `None`, so a line that is genuinely behind a wall is hidden -- which is what makes the
        // overlay legible in a house rather than a thicket.
        device.setRasterizerStateProperty(Gfx::RasterizerState::CullNone);
        device.setDepthStencilStateProperty(Gfx::DepthStencilState::DepthRead);
        device.setBlendStateProperty(Gfx::BlendState::AlphaBlend);

        Gfx::EffectPassCollection& passes = impl_->effect.getCurrentTechniqueProperty()->getPassesProperty();

        auto submit = [&](std::vector<Vertex>& vertices, Gfx::PrimitiveType type, int perPrimitive)
        {
            if (vertices.empty())
            {
                return;
            }
            // ONE upload and ONE draw for every primitive of a type. `HOUSE-00106` measured 8.15 us of
            // CPU per draw call, so a call per wire box would cost more than the scene it annotates and
            // would move the very timings the overlay reports.
            impl_->buffer.SetData(
                vertices.data(), 0, static_cast<int>(vertices.size()), Gfx::SetDataOptions::Discard);
            device.SetVertexBuffer(&impl_->buffer);
            for (int p = 0; p < passes.getCountProperty(); ++p)
            {
                passes[p]->Apply();
                device.DrawPrimitives(type, 0, static_cast<int>(vertices.size()) / perPrimitive);
            }
        };

        submit(lines_, Gfx::PrimitiveType::LineList, 2);
        submit(triangles_, Gfx::PrimitiveType::TriangleList, 3);
    }

} // namespace cnahouse::debug
