// SPDX-License-Identifier: MIT
#include "cnahouse/rendering/Renderer.hpp"

#include <chrono>
#include <numeric>

#include "cnahouse/debug/Counters.hpp"
#include "cnahouse/rendering/RenderTier.hpp"
#include "cnahouse/rendering/StateTracker.hpp"

namespace cnahouse::rendering
{
    namespace
    {
        constexpr std::size_t Index(Pass pass) noexcept
        {
            return static_cast<std::size_t>(pass);
        }
    } // namespace

    std::string_view PassName(Pass pass) noexcept
    {
        switch (pass)
        {
            case Pass::Shadow:
                return "shadow";
            case Pass::Sky:
                return "sky";
            case Pass::OpaqueStatic:
                return "opaque-static";
            case Pass::OpaqueDynamic:
                return "opaque-dynamic";
            case Pass::AlphaTest:
                return "alpha-test";
            case Pass::Transparent:
                return "transparent";
            case Pass::GlareQueries:
                return "glare-queries";
            case Pass::Composite:
                return "composite";
            case Pass::Hud:
                return "hud";
            case Pass::Count:
                break;
        }
        return "?";
    }

    bool PassIsTierEOnly(Pass pass) noexcept
    {
        return pass == Pass::Shadow || pass == Pass::Composite;
    }

    void Renderer::Install(Pass pass, std::unique_ptr<IRenderPass> impl)
    {
        if (pass == Pass::Count)
        {
            return;
        }
        slots_[Index(pass)].impl = std::move(impl);
    }

    bool Renderer::IsInstalled(Pass pass) const noexcept
    {
        return pass != Pass::Count && slots_[Index(pass)].impl != nullptr;
    }

    void Renderer::SetEnabled(Pass pass, bool enabled) noexcept
    {
        if (pass == Pass::Count)
        {
            return;
        }
        slots_[Index(pass)].enabled = enabled;
    }

    bool Renderer::IsEnabled(Pass pass) const noexcept
    {
        return pass != Pass::Count && slots_[Index(pass)].enabled;
    }

    bool Renderer::WillRun(Pass pass) const noexcept
    {
        if (pass == Pass::Count || !IsInstalled(pass) || !IsEnabled(pass))
        {
            return false;
        }
        // The tier gate is here and nowhere else. A pass that checked the tier itself would be a
        // second place for the answer to live, and ADR-0003's whole point is that there is one.
        if (PassIsTierEOnly(pass) && !tier_->IsTierE())
        {
            return false;
        }
        return slots_[Index(pass)].impl->IsActive();
    }

    void Renderer::Draw(PassContext& context)
    {
        passesRun_ = 0;
        passesSkipped_ = 0;

        // Once, at the top: whatever drew the previous frame -- a screenshot capture, a resize, the
        // sprite batch -- is not something the tracker can have followed.
        context.states.BeginFrame();

        for (std::size_t i = 0; i < kPassCount; ++i)
        {
            const auto pass = static_cast<Pass>(i);
            Slot& slot = slots_[i];

            if (!WillRun(pass))
            {
                ++passesSkipped_;
                slot.last = 0.0;
                continue;
            }

            const auto start = std::chrono::steady_clock::now();
            slot.impl->Draw(context);
            slot.last =
                std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();

            slot.window[slot.next] = slot.last;
            slot.next = (slot.next + 1) % kWindow;
            if (slot.count < kWindow)
            {
                ++slot.count;
            }
            ++passesRun_;

            if (slot.impl->DisturbsDeviceState())
            {
                context.states.Invalidate();
            }
        }

        // Two counters rather than one, because "nine passes ran" and "one ran and eight had nothing
        // to do" are the same total and completely different frames.
        context.counters.Set(context.counters.Resolve("render.passes.run"),
                             static_cast<std::int64_t>(passesRun_));
        context.counters.Set(context.counters.Resolve("render.passes.skipped"),
                             static_cast<std::int64_t>(passesSkipped_));
        context.counters.Set(context.counters.Resolve("render.states.applied"),
                             static_cast<std::int64_t>(context.states.Current().TotalApplied()));
        context.counters.Set(context.counters.Resolve("render.states.skipped"),
                             static_cast<std::int64_t>(context.states.Current().TotalSkipped()));
    }

    double Renderer::LastMilliseconds(Pass pass) const noexcept
    {
        return pass == Pass::Count ? 0.0 : slots_[Index(pass)].last;
    }

    double Renderer::AverageMilliseconds(Pass pass) const noexcept
    {
        if (pass == Pass::Count)
        {
            return 0.0;
        }
        const Slot& slot = slots_[Index(pass)];
        if (slot.count == 0)
        {
            return 0.0;
        }
        const double total = std::accumulate(
            slot.window.begin(), slot.window.begin() + static_cast<std::ptrdiff_t>(slot.count), 0.0);
        return total / static_cast<double>(slot.count);
    }

    double Renderer::TotalAverageMilliseconds() const noexcept
    {
        double total = 0.0;
        for (std::size_t i = 0; i < kPassCount; ++i)
        {
            total += AverageMilliseconds(static_cast<Pass>(i));
        }
        return total;
    }

} // namespace cnahouse::rendering
