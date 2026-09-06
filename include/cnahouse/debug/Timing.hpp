// SPDX-License-Identifier: MIT
#pragma once

#include <array>
#include <chrono>
#include <cstddef>
#include <string_view>

#include "cnahouse/app/ISystem.hpp"

namespace cnahouse::debug
{

    /// @brief Per-stage CPU time, measured with a scope guard and averaged over a rolling window.
    ///
    /// **Why per STAGE rather than per system.** `UpdateStage` is the unit a budget is written in and
    /// the unit a reader of `cna-house.md` §7.5 already knows. A per-system breakdown would be finer
    /// and would also be a list nobody can hold in their head; when one stage is over budget, that is
    /// the moment to go finer, and `Scope` is available for that too.
    ///
    /// **Why CPU time and not GPU.** `HOUSE-00106` measured that a draw call costs 8.15 µs of CPU
    /// submission on this machine, so 1 000 draws is 8.15 ms *before the GPU has done anything*. On
    /// this project the CPU side is the budget that binds, and it is the one a scope guard can measure
    /// honestly. GPU completion needs a sync, which distorts the thing being measured -- the phase-1
    /// probes paid that cost deliberately and a per-frame overlay must not.
    class Timing
    {
    public:
        /// @brief Four seconds at 60 Hz, matching `Counter::kWindow` so the overlay's columns agree.
        static constexpr std::size_t kWindow = 240;

        /// @brief Starts timing @p stage; stops at destruction. The only way to record a sample.
        class Scope
        {
        public:
            Scope(Timing& timing, app::UpdateStage stage) noexcept
                : timing_(timing)
                , stage_(stage)
                , start_(std::chrono::steady_clock::now())
            {
            }

            ~Scope() noexcept
            {
                timing_.Record(
                    stage_,
                    std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start_)
                        .count());
            }

            Scope(const Scope&) = delete;
            Scope& operator=(const Scope&) = delete;

        private:
            Timing& timing_;
            app::UpdateStage stage_;
            std::chrono::steady_clock::time_point start_;
        };

        void Record(app::UpdateStage stage, double milliseconds) noexcept;

        /// @brief Closes the frame, pushing each stage's accumulated time into its window.
        ///
        /// Accumulated, not overwritten: the `Physics` stage runs up to four times in a frame
        /// (`FrameTimer::kMaxFixedSteps`), and what a budget cares about is the total that frame cost,
        /// not the cost of the last substep.
        void BeginFrame() noexcept;

        [[nodiscard]] double LastMilliseconds(app::UpdateStage stage) const noexcept;
        [[nodiscard]] double AverageMilliseconds(app::UpdateStage stage) const noexcept;
        [[nodiscard]] double MaxMilliseconds(app::UpdateStage stage) const noexcept;
        /// @brief The sum of every stage's average. What the frame costs on the CPU.
        [[nodiscard]] double TotalAverageMilliseconds() const noexcept;

        void Reset() noexcept;

    private:
        static constexpr std::size_t kStageCount = static_cast<std::size_t>(app::UpdateStage::Count);

        struct StageTiming
        {
            double accumulator = 0.0;
            double last = 0.0;
            std::array<double, kWindow> window{};
            std::size_t next = 0;
            std::size_t count = 0;
        };

        std::array<StageTiming, kStageCount> stages_{};
    };

} // namespace cnahouse::debug
