// SPDX-License-Identifier: MIT
#include "cnahouse/debug/Timing.hpp"

#include <algorithm>
#include <numeric>

namespace cnahouse::debug
{

    void Timing::Record(app::UpdateStage stage, double milliseconds) noexcept
    {
        const auto index = static_cast<std::size_t>(stage);
        if (index < kStageCount)
        {
            // Accumulated within the frame: `Physics` runs up to four times, and the budget cares what
            // the frame cost rather than what the last substep cost.
            stages_[index].accumulator += milliseconds;
        }
    }

    void Timing::BeginFrame() noexcept
    {
        for (StageTiming& stage : stages_)
        {
            stage.last = stage.accumulator;
            stage.window[stage.next] = stage.accumulator;
            stage.next = (stage.next + 1) % kWindow;
            stage.count = std::min(stage.count + 1, kWindow);
            stage.accumulator = 0.0;
        }
    }

    double Timing::LastMilliseconds(app::UpdateStage stage) const noexcept
    {
        const auto index = static_cast<std::size_t>(stage);
        return index < kStageCount ? stages_[index].last : 0.0;
    }

    double Timing::AverageMilliseconds(app::UpdateStage stage) const noexcept
    {
        const auto index = static_cast<std::size_t>(stage);
        if (index >= kStageCount || stages_[index].count == 0)
        {
            return 0.0;
        }
        const StageTiming& timing = stages_[index];
        const double total = std::accumulate(
            timing.window.begin(), timing.window.begin() + static_cast<std::ptrdiff_t>(timing.count), 0.0);
        return total / static_cast<double>(timing.count);
    }

    double Timing::MaxMilliseconds(app::UpdateStage stage) const noexcept
    {
        const auto index = static_cast<std::size_t>(stage);
        if (index >= kStageCount || stages_[index].count == 0)
        {
            return 0.0;
        }
        const StageTiming& timing = stages_[index];
        return *std::max_element(timing.window.begin(),
                                 timing.window.begin() + static_cast<std::ptrdiff_t>(timing.count));
    }

    double Timing::TotalAverageMilliseconds() const noexcept
    {
        double total = 0.0;
        for (std::size_t i = 0; i < kStageCount; ++i)
        {
            total += AverageMilliseconds(static_cast<app::UpdateStage>(i));
        }
        return total;
    }

    void Timing::Reset() noexcept
    {
        stages_ = {};
    }

} // namespace cnahouse::debug
