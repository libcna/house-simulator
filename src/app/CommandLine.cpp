// SPDX-License-Identifier: MIT
#include <array>

#include "cnahouse/app/CommandLine.hpp"

#include <charconv>
#include <format>
#include <string_view>

namespace cnahouse::app
{
    namespace
    {

        using util::Error;
        using util::ErrorCode;

        /// Splits `--name=value` into its halves. A bare `--name` yields an empty value.
        struct Argument
        {
            std::string_view name;
            std::string_view value;
            bool hasValue = false;
        };

        Argument Split(std::string_view raw)
        {
            Argument argument;
            const std::size_t equals = raw.find('=');
            if (equals == std::string_view::npos)
            {
                argument.name = raw;
                return argument;
            }
            argument.name = raw.substr(0, equals);
            argument.value = raw.substr(equals + 1);
            argument.hasValue = true;
            return argument;
        }

        util::Result<float> ParseFloat(std::string_view text, std::string_view option)
        {
            float value = 0.0f;
            const auto* const end = text.data() + text.size();
            const auto [ptr, ec] = std::from_chars(text.data(), end, value);
            if (ec != std::errc{} || ptr != end)
            {
                return Error(ErrorCode::InvalidData,
                             std::format("expected a number, found '{}'", text),
                             std::string(option));
            }
            return value;
        }

        util::Result<std::uint64_t> ParseUInt(std::string_view text, std::string_view option)
        {
            std::uint64_t value = 0;
            const auto* const end = text.data() + text.size();
            const auto [ptr, ec] = std::from_chars(text.data(), end, value);
            if (ec != std::errc{} || ptr != end)
            {
                return Error(ErrorCode::InvalidData,
                             std::format("expected a whole number, found '{}'", text),
                             std::string(option));
            }
            return value;
        }

        util::Result<QualityPreset> ParseQuality(std::string_view text)
        {
            if (text == "low")
            {
                return QualityPreset::Low;
            }
            if (text == "medium")
            {
                return QualityPreset::Medium;
            }
            if (text == "high")
            {
                return QualityPreset::High;
            }
            if (text == "ultra")
            {
                return QualityPreset::Ultra;
            }
            return Error(ErrorCode::InvalidData,
                         std::format("expected low, medium, high or ultra, found '{}'", text),
                         "--quality");
        }

        util::Result<RenderTier> ParseTier(std::string_view text)
        {
            if (text == "s" || text == "S")
            {
                return RenderTier::S;
            }
            if (text == "e" || text == "E")
            {
                return RenderTier::E;
            }
            return Error(ErrorCode::InvalidData, std::format("expected s or e, found '{}'", text), "--tier");
        }

    } // namespace

    std::string_view QualityPresetName(QualityPreset preset) noexcept
    {
        switch (preset)
        {
            case QualityPreset::Low:
                return "low";
            case QualityPreset::Medium:
                return "medium";
            case QualityPreset::High:
                return "high";
            case QualityPreset::Ultra:
                return "ultra";
        }
        return "?";
    }

    std::string_view RenderTierName(RenderTier tier) noexcept
    {
        return tier == RenderTier::E ? "E" : "S";
    }

    RenderTier ResolveTier(RenderTier requested) noexcept
    {
#if CNAHOUSE_TIER_E
        return requested;
#else
        // Built without Tier E: there are no compiled effects in this binary's content, so `--tier=e`
        // cannot be honoured and is not silently pretended to be.
        (void)requested;
        return RenderTier::S;
#endif
    }

    std::string UsageText()
    {
        return "cna-house " CNAHOUSE_VERSION "\n"
               "\n"
               "  --quality=low|medium|high|ultra\n"
               "                              Quality preset (default: auto-detected)\n"
               "  --tier=s|e                  Render tier. Tier E can be turned OFF, never ON: a binary\n"
               "                              built without it has no compiled effects to load.\n"
               "  --headless                  Run with no window (requires a HEADLESS build)\n"
               "  --scene=<name>              Start in a named test scene instead of the house\n"
               "  --camera=<ex,ey,ez,tx,ty,tz>  Where a fixed-camera scene looks from and at\n"
               "  --player=<x,y,z,yaw,pitch>  Stand a body here (feet, metres; degrees) and look\n"
               "                              through its eyes\n"
               "  --screenshot-frame=<n>      Capture the nth drawn frame (default 1)\n"
               "  --seed=<n>                  Session seed; the same seed reproduces a session exactly\n"
               "  --time=<hours>              Time of day to start at, 0..24\n"
               "  --weather=<name>            Weather archetype to start in\n"
               "  --no-audio                  Start with audio disabled\n"
               "  --screenshot=<path>         Write one screenshot and exit\n"
               "  --log=<categories>          Comma-separated log categories, e.g. world,content\n"
               "  --renderer-info             Print this build's renderer and tier facts, then exit\n"
               "  --help                      This text\n"
               "\n"
               "There is deliberately no --renderer option. The renderer is fixed at CMake configure\n"
               "time and XNA 4.0 offers no way to change it afterwards, so one binary is built per\n"
               "renderer. --renderer-info reports what this binary already knows about itself.\n";
    }

    util::Result<Options> ParseCommandLine(int argc, const char* const* argv)
    {
        Options options;
        for (int i = 1; i < argc; ++i)
        {
            const std::string_view raw{argv[i]};
            const Argument argument = Split(raw);

            auto requireValue = [&](std::string_view option) -> util::Result<std::string_view>
            {
                if (!argument.hasValue || argument.value.empty())
                {
                    return Error(
                        ErrorCode::InvalidData, "expected a value, as --name=value", std::string(option));
                }
                return argument.value;
            };

            if (argument.name == "--help" || argument.name == "-h")
            {
                options.help = true;
            }
            else if (argument.name == "--headless")
            {
                options.headless = true;
            }
            else if (argument.name == "--no-audio")
            {
                options.noAudio = true;
            }
            else if (argument.name == "--renderer-info")
            {
                options.rendererInfo = true;
            }
            else if (argument.name == "--quality")
            {
                auto value = requireValue("--quality");
                if (!value)
                {
                    return value.Error();
                }
                auto parsed = ParseQuality(*value);
                if (!parsed)
                {
                    return parsed.Error();
                }
                options.quality = *parsed;
            }
            else if (argument.name == "--tier")
            {
                auto value = requireValue("--tier");
                if (!value)
                {
                    return value.Error();
                }
                auto parsed = ParseTier(*value);
                if (!parsed)
                {
                    return parsed.Error();
                }
                options.tier = *parsed;
            }
            else if (argument.name == "--scene")
            {
                auto value = requireValue("--scene");
                if (!value)
                {
                    return value.Error();
                }
                options.scene = std::string(*value);
            }
            else if (argument.name == "--camera")
            {
                auto value = requireValue("--camera");
                if (!value)
                {
                    return value.Error();
                }
                std::array<float, 6> pose{};
                std::size_t index = 0;
                std::size_t start = 0;
                const std::string text(*value);
                while (index < pose.size())
                {
                    const std::size_t comma = text.find(',', start);
                    const std::string field =
                        text.substr(start, comma == std::string::npos ? comma : comma - start);
                    try
                    {
                        std::size_t used = 0;
                        pose[index] = std::stof(field, &used);
                        if (used != field.size() || field.empty())
                        {
                            throw std::invalid_argument("trailing");
                        }
                    }
                    catch (const std::exception&)
                    {
                        // Named and refused rather than defaulted: a camera silently at the origin
                        // is a screenshot of the inside of a floor, and it looks like a bug in the
                        // renderer.
                        return Err(
                            ErrorCode::InvalidArgument,
                            std::format("--camera field {} is '{}', which is not a number", index + 1, field),
                            "--camera");
                    }
                    ++index;
                    if (comma == std::string::npos)
                    {
                        break;
                    }
                    start = comma + 1;
                }
                if (index != pose.size())
                {
                    return Err(ErrorCode::InvalidArgument,
                               std::format("--camera needs six comma-separated metres "
                                           "(eye x,y,z then target x,y,z); got {}",
                                           index),
                               "--camera");
                }
                options.camera = pose;
            }
            else if (argument.name == "--player")
            {
                auto value = requireValue("--player");
                if (!value)
                {
                    return value.Error();
                }
                std::array<float, 5> stand{};
                std::size_t index = 0;
                std::size_t start = 0;
                const std::string text(*value);
                while (index < stand.size())
                {
                    const std::size_t comma = text.find(',', start);
                    const std::string field =
                        text.substr(start, comma == std::string::npos ? comma : comma - start);
                    try
                    {
                        std::size_t used = 0;
                        stand[index] = std::stof(field, &used);
                        if (used != field.size() || field.empty())
                        {
                            throw std::invalid_argument("trailing");
                        }
                    }
                    catch (const std::exception&)
                    {
                        // Named and refused rather than defaulted, for the same reason `--camera`
                        // refuses: a body silently at the origin is standing in the middle of the
                        // road, and the frame it draws looks like a bug in the world data.
                        return Err(
                            ErrorCode::InvalidArgument,
                            std::format("--player field {} is '{}', which is not a number", index + 1, field),
                            "--player");
                    }
                    ++index;
                    if (comma == std::string::npos)
                    {
                        break;
                    }
                    start = comma + 1;
                }
                if (index != stand.size())
                {
                    return Err(ErrorCode::InvalidArgument,
                               std::format("--player needs five comma-separated numbers "
                                           "(feet x,y,z then yaw and pitch in degrees); got {}",
                                           index),
                               "--player");
                }
                options.player = stand;
            }
            else if (argument.name == "--seed")
            {
                auto value = requireValue("--seed");
                if (!value)
                {
                    return value.Error();
                }
                auto parsed = ParseUInt(*value, "--seed");
                if (!parsed)
                {
                    return parsed.Error();
                }
                options.seed = *parsed;
            }
            else if (argument.name == "--time")
            {
                auto value = requireValue("--time");
                if (!value)
                {
                    return value.Error();
                }
                auto parsed = ParseFloat(*value, "--time");
                if (!parsed)
                {
                    return parsed.Error();
                }
                if (*parsed < 0.0f || *parsed > 24.0f)
                {
                    return Error(ErrorCode::OutOfRange,
                                 std::format("expected 0..24 hours, found {}", *parsed),
                                 "--time");
                }
                options.timeOfDay = *parsed;
            }
            else if (argument.name == "--weather")
            {
                auto value = requireValue("--weather");
                if (!value)
                {
                    return value.Error();
                }
                options.weather = std::string(*value);
            }
            else if (argument.name == "--screenshot")
            {
                auto value = requireValue("--screenshot");
                if (!value)
                {
                    return value.Error();
                }
                options.screenshot = std::string(*value);
            }
            else if (argument.name == "--screenshot-frame")
            {
                auto value = requireValue("--screenshot-frame");
                if (!value)
                {
                    return value.Error();
                }
                std::uint64_t frame = 0;
                const auto* first = value->data();
                const auto* last = first + value->size();
                const auto parsed = std::from_chars(first, last, frame);
                if (parsed.ec != std::errc{} || parsed.ptr != last || frame == 0)
                {
                    return Error(ErrorCode::InvalidArgument,
                                 "--screenshot-frame takes a frame number of 1 or more",
                                 std::string(*value));
                }
                options.screenshotFrame = frame;
            }
            else if (argument.name == "--log")
            {
                auto value = requireValue("--log");
                if (!value)
                {
                    return value.Error();
                }
                options.logCategories = std::string(*value);
            }
            else if (argument.name == "--renderer")
            {
                // Named explicitly so the refusal explains WHY rather than saying "unknown option",
                // because a user reaching for it has a reasonable expectation that needs correcting.
                return Error(ErrorCode::Unsupported,
                             "the renderer is fixed at build time and cannot be changed by an option; "
                             "build a separate binary per renderer, and use --renderer-info to see "
                             "which one this is",
                             "--renderer");
            }
            else
            {
                return Error(ErrorCode::InvalidData,
                             std::format("unknown option '{}'", argument.name),
                             "command line");
            }
        }
        return options;
    }

    std::string RendererInfo(const Options& options)
    {
        // Every value here is a compile-time constant this binary carries. Nothing is queried from
        // the device, which is `HOUSE-00132`'s acceptance criterion and ADR-0001's rule.
        const RenderTier resolved = ResolveTier(options.tier);
        return std::format("cna-house {}\n"
                           "  renderer (fixed at configure time): {}\n"
                           "  Tier E compiled into this build:    {}\n"
                           "  render tier requested:              {}\n"
                           "  render tier resolved:               {}\n"
                           "  debug tools compiled in:            {}\n"
                           "  quality preset:                     {}\n",
                           CNAHOUSE_VERSION,
                           CNAHOUSE_RENDERER_NAME,
                           CNAHOUSE_TIER_E ? "yes" : "no",
                           RenderTierName(options.tier),
                           RenderTierName(resolved),
                           CNAHOUSE_DEBUG_TOOLS ? "yes" : "no",
                           options.quality.has_value() ? QualityPresetName(*options.quality)
                                                       : std::string_view("auto-detect"));
    }

} // namespace cnahouse::app
