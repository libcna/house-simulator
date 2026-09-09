// SPDX-License-Identifier: MIT
#include "cnahouse/debug/VisibilityCommands.hpp"

#include <format>

namespace cnahouse::debug
{

    void RegisterVisibilityCommands(Console& console, VisibilityCommandContext context)
    {
        console.Register(
            "cull",
            "cull [off|on]  -- build the draw list from §25's visible set, or from everything",
            [context](std::span<const std::string_view> arguments) -> CommandResult
            {
                if (context.cullingEnabled == nullptr)
                {
                    return CommandResult{false, "cull: this session has no visibility to turn off"};
                }
                if (arguments.empty())
                {
                    // No argument REPORTS rather than toggling. A toggle would make the command's
                    // effect depend on a state the person typing it cannot see, and the one thing
                    // they are about to do with it is compare two frames.
                    return CommandResult{true,
                                         std::format("cull is {}", *context.cullingEnabled ? "on" : "off")};
                }
                if (arguments[0] == "off")
                {
                    *context.cullingEnabled = false;
                    return CommandResult{true,
                                         "cull off: the draw list is everything resident; §25's "
                                         "walk still runs and F3 still reports it"};
                }
                if (arguments[0] == "on")
                {
                    *context.cullingEnabled = true;
                    return CommandResult{true, "cull on: the draw list is §25's visible set"};
                }
                return CommandResult{false, std::format("cull: '{}' is not off or on", arguments[0])};
            });
    }

} // namespace cnahouse::debug
