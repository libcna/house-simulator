// SPDX-License-Identifier: MIT
#pragma once

#include <cstdint>
#include <format>
#include <string>
#include <string_view>
#include <vector>

namespace cnahouse::util
{

    /// @brief Severity. `Fatal` does not terminate: it records that the program is about to.
    enum class LogLevel : std::uint8_t
    {
        Trace = 0,
        Debug,
        Info,
        Warn,
        Error,
        Fatal,
        Off,
    };

    /// @brief The subsystem a message came from, so a developer can silence everything else.
    ///
    /// One value per `src/` subsystem plus `App`. Filtering by category is what makes the log usable
    /// while working on one thing: `--log=world,content` while debugging a loader.
    enum class LogCat : std::uint8_t
    {
        App = 0,
        World,
        Visibility,
        Rendering,
        Content,
        Physics,
        Player,
        Animation,
        Animals,
        Interaction,
        Environment,
        Weather,
        Lighting,
        Audio,
        Persistence,
        Ui,
        Debug,
        Util,
        Count,
    };

    [[nodiscard]] std::string_view LogLevelName(LogLevel level) noexcept;
    [[nodiscard]] std::string_view LogCatName(LogCat category) noexcept;
    /// @brief Parses a category name case-insensitively; `Count` means "not a category".
    [[nodiscard]] LogCat ParseLogCat(std::string_view name) noexcept;

    /// @brief One retained log line.
    struct LogRecord
    {
        LogLevel level = LogLevel::Info;
        LogCat category = LogCat::App;
        std::uint64_t frame = 0;
        /// @brief How many times this message was emitted, including suppressed repeats.
        std::uint32_t repeats = 1;
        std::string message;
    };

    /// @brief The project's logger: levelled, categorised, rate-limited, with a ring buffer.
    ///
    /// **Rate limiting is not a nicety.** A per-frame failure -- a missing asset in a draw loop, a bad
    /// portal in the visibility walk -- produces one message per frame per occurrence, which at 60 Hz
    /// buries every other line within a second and makes the log actively worse than no log. The
    /// limiter collapses identical messages within a frame to one line carrying a repeat count, which
    /// keeps the information (it happened, this many times) while keeping the log readable.
    ///
    /// Not thread-safe by design: `cna-house` is single-threaded per `cna-house.md` §7.5, and a mutex
    /// on a per-frame path would be a cost paid for a thread that does not exist.
    class Log
    {
    public:
        /// @brief Messages below this level are dropped, cheaply, before formatting.
        static void SetMinimumLevel(LogLevel level) noexcept;
        [[nodiscard]] static LogLevel MinimumLevel() noexcept;

        /// @brief Enables or disables one category at runtime.
        static void SetCategoryEnabled(LogCat category, bool enabled) noexcept;
        [[nodiscard]] static bool IsCategoryEnabled(LogCat category) noexcept;

        /// @brief Enables exactly the named categories and disables the rest.
        ///
        /// The form `--log=world,content` maps onto this. An unknown name is reported to the caller
        /// rather than ignored, because a silently misspelled category looks identical to a subsystem
        /// that is simply quiet -- which is the worst possible failure for a diagnostic tool.
        /// @return the names that were not recognised.
        static std::vector<std::string> SetEnabledCategories(std::string_view commaSeparated);

        /// @brief Advances the frame counter. Rate limiting is per frame, so this is what resets it.
        static void BeginFrame(std::uint64_t frameIndex) noexcept;

        /// @brief Whether a level/category pair would be emitted, for skipping expensive arguments.
        [[nodiscard]] static bool WouldLog(LogLevel level, LogCat category) noexcept;

        /// @brief The retained ring buffer, oldest first. Used by the debug overlay and by tests.
        [[nodiscard]] static const std::vector<LogRecord>& Ring() noexcept;
        static void ClearRing() noexcept;
        /// @brief How many messages were suppressed by the rate limiter since the last `ClearRing`.
        [[nodiscard]] static std::uint64_t SuppressedCount() noexcept;

        /// @brief Directs output at a file in addition to stderr. Empty disables the file sink.
        static void SetFileSink(std::string path);

        /// @brief Emits one already-formatted message. Prefer the `Trace`…`Fatal` helpers.
        static void Emit(LogLevel level, LogCat category, std::string message);

        template<typename... Args>
        static void Write(LogLevel level, LogCat category, std::format_string<Args...> fmt, Args&&... args)
        {
            if (!WouldLog(level, category))
            {
                return;
            }
            Emit(level, category, std::format(fmt, std::forward<Args>(args)...));
        }

#define CNAHOUSE_LOG_LEVEL_HELPER(NAME)                                                                      \
    template<typename... Args>                                                                               \
    static void NAME(LogCat category, std::format_string<Args...> fmt, Args&&... args)                       \
    {                                                                                                        \
        Write(LogLevel::NAME, category, fmt, std::forward<Args>(args)...);                                   \
    }

        CNAHOUSE_LOG_LEVEL_HELPER(Trace)
        CNAHOUSE_LOG_LEVEL_HELPER(Debug)
        CNAHOUSE_LOG_LEVEL_HELPER(Info)
        CNAHOUSE_LOG_LEVEL_HELPER(Warn)
        CNAHOUSE_LOG_LEVEL_HELPER(Error)
        CNAHOUSE_LOG_LEVEL_HELPER(Fatal)

#undef CNAHOUSE_LOG_LEVEL_HELPER

        /// @brief Restores every setting to its default. For tests, so one cannot leak into the next.
        static void ResetForTesting();
    };

} // namespace cnahouse::util
