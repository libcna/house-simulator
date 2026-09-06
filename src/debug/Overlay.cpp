// SPDX-License-Identifier: MIT
#include "cnahouse/debug/Overlay.hpp"

#include <algorithm>
#include <format>

#include "Microsoft/Xna/Framework/Color.hpp"
#include "Microsoft/Xna/Framework/Graphics/SpriteBatch.hpp"
#include "Microsoft/Xna/Framework/Vector2.hpp"

#include "cnahouse/app/ISystem.hpp"
#include "cnahouse/app/Platform.hpp"
#include "cnahouse/ui/TextRenderer.hpp"

namespace cnahouse::debug
{
    namespace
    {
        /// Eight levels, low to high. Chosen so the eye reads height rather than character identity, which
        /// a mix of letters and punctuation would not give.
        constexpr std::string_view kBars[] = {"_", ".", ",", "-", "=", "+", "*", "#"};
        constexpr std::size_t kBarCount = sizeof(kBars) / sizeof(kBars[0]);
    } // namespace

    void Overlay::PushFrameTime(float milliseconds)
    {
        frameTimes_.push_back(milliseconds);
        if (frameTimes_.size() > kGraphFrames)
        {
            frameTimes_.erase(frameTimes_.begin());
        }
    }

    std::string Overlay::GraphRow() const
    {
        std::string row;
        row.reserve(frameTimes_.size());
        for (const float milliseconds : frameTimes_)
        {
            const float normalised = std::clamp(milliseconds / kGraphCeilingMilliseconds, 0.0f, 1.0f);
            const auto level = static_cast<std::size_t>(normalised * static_cast<float>(kBarCount - 1));
            row += kBars[std::min(level, kBarCount - 1)];
        }
        return row;
    }

    std::vector<std::string>
    Overlay::Lines(const app::Platform& platform, const Timing& timing, const Counters& counters) const
    {
        std::vector<std::string> lines;
        lines.push_back(platform.Summary());

        const double total = timing.TotalAverageMilliseconds();
        lines.push_back(std::format("CPU {:6.2f} ms avg   (budget 16.67 ms at 60 Hz)", total));
        lines.push_back(GraphRow());

        lines.emplace_back("stage             last     avg     max");
        for (std::size_t i = 0; i < static_cast<std::size_t>(app::UpdateStage::Count); ++i)
        {
            const auto stage = static_cast<app::UpdateStage>(i);
            const double average = timing.AverageMilliseconds(stage);
            // A stage that has never run is noise in a twelve-line table; one that has is always shown,
            // even at 0.00, because its absence would be indistinguishable from it not existing.
            if (average <= 0.0 && timing.MaxMilliseconds(stage) <= 0.0)
            {
                continue;
            }
            lines.push_back(std::format("{:<16} {:6.2f}  {:6.2f}  {:6.2f}",
                                        app::UpdateStageName(stage),
                                        timing.LastMilliseconds(stage),
                                        average,
                                        timing.MaxMilliseconds(stage)));
        }

        if (!counters.All().empty())
        {
            lines.emplace_back("counter            last     avg     max");
            for (const Counter& counter : counters.All())
            {
                lines.push_back(std::format("{:<16} {:6}  {:6.1f}  {:6}",
                                            counter.name,
                                            counter.current,
                                            counter.Average(),
                                            counter.Max()));
            }
        }
        return lines;
    }

    void Overlay::Draw(Microsoft::Xna::Framework::Graphics::SpriteBatch& batch,
                       const ui::TextRenderer& text,
                       const app::Platform& platform,
                       const Timing& timing,
                       const Counters& counters) const
    {
        if (!visible_ || !text.HasFont())
        {
            return;
        }
        // Virtual units, so the overlay is laid out once and is legible at every window size.
        constexpr float kLineHeight = 18.0f;
        constexpr float kLeft = 12.0f;
        constexpr float kTop = 40.0f;

        const std::vector<std::string> lines = Lines(platform, timing, counters);
        for (std::size_t i = 0; i < lines.size(); ++i)
        {
            text.DrawShadowed(
                batch,
                lines[i],
                Microsoft::Xna::Framework::Vector2(kLeft, kTop + static_cast<float>(i) * kLineHeight),
                ui::Anchor::TopLeft,
                Microsoft::Xna::Framework::Color::White);
        }
    }

} // namespace cnahouse::debug
