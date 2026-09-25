// SPDX-License-Identifier: MIT
#include "cnahouse/util/Log.hpp"

#include <algorithm>
#include <array>
#include <cstdio>
#include <unordered_map>

namespace cnahouse::util
{
    namespace
    {

        constexpr std::size_t kCategoryCount = static_cast<std::size_t>(LogCat::Count);

        /// How many records the ring buffer retains. Sized for the debug overlay's scrollback and for a
        /// crash report to carry enough context to be worth reading, not for offline analysis -- the file
        /// sink is what keeps everything.
        constexpr std::size_t kRingCapacity = 512;

        constexpr std::array<std::string_view, 7> kLevelNames{
            "TRACE", "DEBUG", "INFO", "WARN", "ERROR", "FATAL", "OFF"};

        constexpr std::array<std::string_view, kCategoryCount> kCategoryNames{"app",
                                                                              "world",
                                                                              "visibility",
                                                                              "rendering",
                                                                              "content",
                                                                              "physics",
                                                                              "player",
                                                                              "animation",
                                                                              "animals",
                                                                              "interaction",
                                                                              "environment",
                                                                              "weather",
                                                                              "lighting",
                                                                              "audio",
                                                                              "persistence",
                                                                              "ui",
                                                                              "debug",
                                                                              "util"};

        struct State
        {
            LogLevel minimumLevel = LogLevel::Info;
            std::array<bool, kCategoryCount> enabled{};
            std::uint64_t frame = 0;
            std::vector<LogRecord> ring;
            std::uint64_t suppressed = 0;
            /// Message text -> index in `ring`, for the current frame only. Cleared by `BeginFrame`.
            std::unordered_map<std::string, std::size_t> seenThisFrame;

            State()
            {
                enabled.fill(true);
                ring.reserve(kRingCapacity);
            }
        };

        State& Get()
        {
            static State state;
            return state;
        }

        char LevelInitial(LogLevel level)
        {
            const auto name = LogLevelName(level);
            return name.empty() ? '?' : name.front();
        }

    } // namespace

    std::string_view LogLevelName(LogLevel level) noexcept
    {
        const auto index = static_cast<std::size_t>(level);
        return index < kLevelNames.size() ? kLevelNames[index] : std::string_view{"?"};
    }

    std::string_view LogCatName(LogCat category) noexcept
    {
        const auto index = static_cast<std::size_t>(category);
        return index < kCategoryNames.size() ? kCategoryNames[index] : std::string_view{"?"};
    }

    LogCat ParseLogCat(std::string_view name) noexcept
    {
        for (std::size_t i = 0; i < kCategoryNames.size(); ++i)
        {
            if (kCategoryNames[i].size() != name.size())
            {
                continue;
            }
            bool same = true;
            for (std::size_t c = 0; c < name.size(); ++c)
            {
                const char lhs =
                    static_cast<char>((name[c] >= 'A' && name[c] <= 'Z') ? name[c] - 'A' + 'a' : name[c]);
                if (lhs != kCategoryNames[i][c])
                {
                    same = false;
                    break;
                }
            }
            if (same)
            {
                return static_cast<LogCat>(i);
            }
        }
        return LogCat::Count;
    }

    void Log::SetMinimumLevel(LogLevel level) noexcept
    {
        Get().minimumLevel = level;
    }

    LogLevel Log::MinimumLevel() noexcept
    {
        return Get().minimumLevel;
    }

    void Log::SetCategoryEnabled(LogCat category, bool enabled) noexcept
    {
        const auto index = static_cast<std::size_t>(category);
        if (index < kCategoryCount)
        {
            Get().enabled[index] = enabled;
        }
    }

    bool Log::IsCategoryEnabled(LogCat category) noexcept
    {
        const auto index = static_cast<std::size_t>(category);
        return index < kCategoryCount && Get().enabled[index];
    }

    std::vector<std::string> Log::SetEnabledCategories(std::string_view commaSeparated)
    {
        State& state = Get();
        std::vector<std::string> unknown;
        state.enabled.fill(false);

        std::size_t start = 0;
        while (start <= commaSeparated.size())
        {
            const std::size_t comma = commaSeparated.find(',', start);
            const std::size_t end = comma == std::string_view::npos ? commaSeparated.size() : comma;
            std::string_view token = commaSeparated.substr(start, end - start);
            while (!token.empty() && token.front() == ' ')
            {
                token.remove_prefix(1);
            }
            while (!token.empty() && token.back() == ' ')
            {
                token.remove_suffix(1);
            }
            if (!token.empty())
            {
                const LogCat category = ParseLogCat(token);
                if (category == LogCat::Count)
                {
                    // Reported, never ignored: a misspelled category looks exactly like a subsystem
                    // that is simply quiet, which is the worst failure a diagnostic tool can have.
                    unknown.emplace_back(token);
                }
                else
                {
                    state.enabled[static_cast<std::size_t>(category)] = true;
                }
            }
            if (comma == std::string_view::npos)
            {
                break;
            }
            start = comma + 1;
        }
        return unknown;
    }

    void Log::BeginFrame(std::uint64_t frameIndex) noexcept
    {
        State& state = Get();
        state.frame = frameIndex;
        state.seenThisFrame.clear();
    }

    bool Log::WouldLog(LogLevel level, LogCat category) noexcept
    {
        const State& state = Get();
        return level >= state.minimumLevel && state.minimumLevel != LogLevel::Off &&
               IsCategoryEnabled(category);
    }

    const std::vector<LogRecord>& Log::Ring() noexcept
    {
        return Get().ring;
    }

    void Log::ClearRing() noexcept
    {
        State& state = Get();
        state.ring.clear();
        state.seenThisFrame.clear();
        state.suppressed = 0;
    }

    std::uint64_t Log::SuppressedCount() noexcept
    {
        return Get().suppressed;
    }

    void Log::Emit(LogLevel level, LogCat category, std::string message)
    {
        if (!WouldLog(level, category))
        {
            return;
        }
        State& state = Get();

        // Rate limiting: an identical message already seen this frame updates that record's repeat
        // count instead of adding a line. The information survives -- it happened, this many times --
        // while a per-frame failure stops burying everything else.
        if (const auto it = state.seenThisFrame.find(message); it != state.seenThisFrame.end())
        {
            if (it->second < state.ring.size())
            {
                ++state.ring[it->second].repeats;
                ++state.suppressed;
                return;
            }
            // The record has aged out of the ring; fall through and emit it again.
            state.seenThisFrame.erase(it);
        }

        if (state.ring.size() >= kRingCapacity)
        {
            state.ring.erase(state.ring.begin());
            // Every retained index shifted by one; rebuilding the small per-frame map is cheaper and
            // far less error-prone than adjusting it, and this happens at most once per message.
            for (auto& entry : state.seenThisFrame)
            {
                entry.second = entry.second == 0 ? 0 : entry.second - 1;
            }
        }

        state.ring.push_back(LogRecord{level, category, state.frame, 1, message});
        state.seenThisFrame.emplace(std::move(message), state.ring.size() - 1);

        const LogRecord& record = state.ring.back();
        std::FILE* const console = level >= LogLevel::Warn ? stderr : stdout;
        std::fprintf(console,
                     "[%c][%s] %s\n",
                     LevelInitial(level),
                     std::string(LogCatName(category)).c_str(),
                     record.message.c_str());
    }

    void Log::ResetForTesting()
    {
        State& state = Get();
        state.minimumLevel = LogLevel::Info;
        state.enabled.fill(true);
        state.frame = 0;
        state.ring.clear();
        state.seenThisFrame.clear();
        state.suppressed = 0;
    }

} // namespace cnahouse::util
