// SPDX-License-Identifier: MIT
#include "cnahouse/rendering/StateTracker.hpp"

#include "Microsoft/Xna/Framework/Graphics/BlendState.hpp"
#include "Microsoft/Xna/Framework/Graphics/DepthStencilState.hpp"
#include "Microsoft/Xna/Framework/Graphics/GraphicsDevice.hpp"
#include "Microsoft/Xna/Framework/Graphics/RasterizerState.hpp"

namespace cnahouse::rendering
{

    void StateTracker::SetBlend(const Microsoft::Xna::Framework::Graphics::BlendState& state)
    {
        if (blend_ == &state)
        {
            ++current_.blendSkipped;
            return;
        }
        blend_ = &state;
        ++current_.blendApplied;
        device_->setBlendStateProperty(state);
    }

    void StateTracker::SetDepthStencil(const Microsoft::Xna::Framework::Graphics::DepthStencilState& state)
    {
        if (depth_ == &state)
        {
            ++current_.depthSkipped;
            return;
        }
        depth_ = &state;
        ++current_.depthApplied;
        device_->setDepthStencilStateProperty(state);
    }

    void StateTracker::SetRasterizer(const Microsoft::Xna::Framework::Graphics::RasterizerState& state)
    {
        if (raster_ == &state)
        {
            ++current_.rasterSkipped;
            return;
        }
        raster_ = &state;
        ++current_.rasterApplied;
        device_->setRasterizerStateProperty(state);
    }

    void StateTracker::Invalidate() noexcept
    {
        // Everything, not just the one thing that changed. `SpriteBatch::End` restores several states
        // at once and a render-target change resets others; tracking which would be a second model of
        // XNA's behaviour to keep in sync, and a tracker that believes a stale binding skips the set
        // that was actually needed -- a frame drawn with someone else's blend state, which is far worse
        // than a redundant set.
        blend_ = nullptr;
        depth_ = nullptr;
        raster_ = nullptr;
    }

    void StateTracker::BeginFrame() noexcept
    {
        lastFrame_ = current_;
        current_ = Counts{};
        Invalidate();
    }

} // namespace cnahouse::rendering
