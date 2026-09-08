// SPDX-License-Identifier: MIT
#pragma once

#include <functional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace cnahouse::debug
{

    /// @brief What a command did, and what to print.
    struct CommandResult
    {
        bool ok = false;
        /// @brief One line, for the console to show. A failure says what was wrong AND what the
        ///        command wanted -- a bare "error" makes the user guess the syntax.
        std::string message;
    };

    /// @brief §71's developer console: a name, a usage line and a handler (`HOUSE-00563`).
    ///
    /// §71 lists eighteen commands and this is the first task that needs one, so the registry
    /// arrives with it and every later command task (`HOUSE-00684`, `HOUSE-01272`, `HOUSE-01536`,
    /// `HOUSE-01694`, `HOUSE-02056` …) plugs into it rather than growing a parser of its own.
    ///
    /// **The console owns parsing and nothing else.** It splits a line into words and finds a
    /// handler; what the words MEAN is the command's business, because a shared argument type
    /// would have to know about cells, times, weather archetypes and asset ids at once.
    class Console
    {
    public:
        using Handler = std::function<CommandResult(std::span<const std::string_view>)>;

        /// @brief Registers @p name. Re-registering a name replaces it, which is what a hot
        ///        reload of a subsystem needs.
        void Register(std::string_view name, std::string_view usage, Handler handler);

        /// @brief Runs one line: the first word is the command, the rest are its arguments.
        ///
        /// An empty line does nothing and succeeds -- pressing return at a prompt is not an error.
        [[nodiscard]] CommandResult Execute(std::string_view line);

        /// @brief Registered names, in registration order. For `help` and for completion.
        [[nodiscard]] std::vector<std::string_view> Names() const;

        /// @brief The usage line @p name was registered with, or empty.
        [[nodiscard]] std::string_view Usage(std::string_view name) const;

        /// @brief Splits @p line on runs of whitespace. Exposed because it is the one piece of
        ///        parsing every command test needs and duplicating it would let the two drift.
        [[nodiscard]] static std::vector<std::string_view> Split(std::string_view line);

    private:
        struct Entry
        {
            std::string name;
            std::string usage;
            Handler handler;
        };

        std::vector<Entry> entries_;
    };

} // namespace cnahouse::debug
