// SPDX-License-Identifier: MIT
#include "cnahouse/debug/Console.hpp"

#include <algorithm>
#include <format>

namespace cnahouse::debug
{
    namespace
    {
        bool IsSpace(char c) noexcept
        {
            return c == ' ' || c == '\t' || c == '\r' || c == '\n';
        }
    } // namespace

    std::vector<std::string_view> Console::Split(std::string_view line)
    {
        std::vector<std::string_view> words;
        std::size_t at = 0;
        while (at < line.size())
        {
            while (at < line.size() && IsSpace(line[at]))
            {
                ++at;
            }
            const std::size_t start = at;
            while (at < line.size() && !IsSpace(line[at]))
            {
                ++at;
            }
            if (at > start)
            {
                words.push_back(line.substr(start, at - start));
            }
        }
        return words;
    }

    void Console::Register(std::string_view name, std::string_view usage, Handler handler)
    {
        const auto it = std::find_if(
            entries_.begin(), entries_.end(), [name](const Entry& entry) { return entry.name == name; });
        if (it != entries_.end())
        {
            it->usage = std::string(usage);
            it->handler = std::move(handler);
            return;
        }
        entries_.push_back(Entry{std::string(name), std::string(usage), std::move(handler)});
    }

    CommandResult Console::Execute(std::string_view line)
    {
        const std::vector<std::string_view> words = Split(line);
        if (words.empty())
        {
            // Pressing return at a prompt is not an error.
            return CommandResult{true, {}};
        }
        const auto it = std::find_if(
            entries_.begin(), entries_.end(), [&](const Entry& entry) { return entry.name == words[0]; });
        if (it == entries_.end())
        {
            return CommandResult{false, std::format("unknown command '{}'", words[0])};
        }
        return it->handler(std::span<const std::string_view>(words).subspan(1));
    }

    std::vector<std::string_view> Console::Names() const
    {
        std::vector<std::string_view> names;
        names.reserve(entries_.size());
        for (const Entry& entry : entries_)
        {
            names.push_back(entry.name);
        }
        return names;
    }

    std::string_view Console::Usage(std::string_view name) const
    {
        const auto it = std::find_if(
            entries_.begin(), entries_.end(), [name](const Entry& entry) { return entry.name == name; });
        return it == entries_.end() ? std::string_view() : std::string_view(it->usage);
    }

} // namespace cnahouse::debug
