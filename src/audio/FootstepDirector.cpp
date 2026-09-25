// SPDX-License-Identifier: MIT
#include "cnahouse/audio/FootstepDirector.hpp"

#include <algorithm>
#include <cmath>
#include <utility>

namespace cnahouse::audio
{

    FootstepDirector::FootstepDirector(const AudioSystem& audio, std::uint64_t seed) noexcept
        : audio_(audio)
        , rng_(seed)
    {
    }

    void FootstepDirector::BindSurface(std::string surface, util::Id bank)
    {
        const auto existing =
            std::find_if(surfaceBanks_.begin(),
                         surfaceBanks_.end(),
                         [&surface](const SurfaceBinding& binding) { return binding.surface == surface; });
        if (existing != surfaceBanks_.end())
        {
            existing->bank = bank;
            return;
        }
        surfaceBanks_.push_back(SurfaceBinding{std::move(surface), bank});
    }

    std::optional<Footstep> FootstepDirector::Advance(const FootstepStep& step)
    {
        if (!step.onGround || !std::isfinite(step.distanceMeters) || step.distanceMeters < 0.0F ||
            !std::isfinite(step.verticalDistanceMeters))
        {
            Reset();
            return std::nullopt;
        }

        const auto binding = std::find_if(surfaceBanks_.begin(),
                                          surfaceBanks_.end(),
                                          [&step](const SurfaceBinding& candidate)
                                          { return candidate.surface == step.surface; });
        if (binding == surfaceBanks_.end() || audio_.Bank(binding->bank).empty())
        {
            // Do not carry distance gathered on an unknown/silent material into the next valid
            // floor: that would emit a sound for the wrong surface at the boundary.
            Reset();
            return std::nullopt;
        }

        if (step.OnStairs())
        {
            if (!std::isfinite(step.riserHeightMeters))
            {
                Reset();
                return std::nullopt;
            }
            if (!wasOnStairs_)
            {
                stairHeight_ = 0.0F;
                strideDistance_ = 0.0F;
            }
            wasOnStairs_ = true;
            stairHeight_ += std::fabs(step.verticalDistanceMeters);
            if (stairHeight_ + 1.0e-6F < step.riserHeightMeters)
            {
                return std::nullopt;
            }
            stairHeight_ = std::max(0.0F, stairHeight_ - step.riserHeightMeters);
            return Select(binding->bank);
        }

        if (wasOnStairs_)
        {
            // A stair foot plant and the first flat-floor plant must not land on the same boundary.
            stairHeight_ = 0.0F;
            strideDistance_ = 0.0F;
            wasOnStairs_ = false;
        }

        strideDistance_ += step.distanceMeters;
        const float stride = step.fastWalk ? kFastStrideMeters : kWalkStrideMeters;
        if (strideDistance_ + 1.0e-6F < stride)
        {
            return std::nullopt;
        }
        strideDistance_ = std::max(0.0F, strideDistance_ - stride);
        return Select(binding->bank);
    }

    void FootstepDirector::Reset() noexcept
    {
        strideDistance_ = 0.0F;
        stairHeight_ = 0.0F;
        wasOnStairs_ = false;
    }

    std::optional<Footstep> FootstepDirector::Select(util::Id bank)
    {
        const std::span<const std::string> samples = audio_.Bank(bank);
        if (samples.empty())
        {
            return std::nullopt;
        }

        std::size_t& next = nextSamples_[bank];
        const std::size_t selected = next % samples.size();
        next = (selected + 1U) % samples.size();

        Footstep result;
        result.sample = samples[selected];
        result.bank = bank;
        result.pitch = rng_.NextFloat(-0.04F, 0.04F);
        result.volume = audio_.BankGain(bank) * rng_.NextFloat(0.90F, 1.10F);
        return result;
    }

} // namespace cnahouse::audio
