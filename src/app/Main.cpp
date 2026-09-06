// SPDX-License-Identifier: MIT
//
// The shell. Everything it does is: parse the command line, load the settings, construct the game,
// run it, and report why it stopped.
//
// **There is no `CNA/Platform/Entrypoint.hpp`.** The CNA samples include it, but `HOUSE-00062`
// measured that under SDL3 it expands to nothing off Android and iOS, and it is a forbidden `CNA/`
// include under ADR-0001. A plain `int main` links and runs.
#include <cstdio>
#include <exception>

#include "cnahouse/app/CnaHouseGame.hpp"
#include "cnahouse/app/CommandLine.hpp"
#include "cnahouse/app/Settings.hpp"
#include "cnahouse/util/Log.hpp"

int main(int argc, char** argv)
{
    using namespace cnahouse;

    auto options = app::ParseCommandLine(argc, argv);
    if (!options)
    {
        std::fprintf(
            stderr, "cna-house: %s\n\n%s", options.Error().ToString().c_str(), app::UsageText().c_str());
        return 2;
    }

    if (options->help)
    {
        std::fputs(app::UsageText().c_str(), stdout);
        return 0;
    }
    if (options->rendererInfo)
    {
        std::fputs(app::RendererInfo(*options).c_str(), stdout);
        return 0;
    }

    if (options->logCategories.has_value())
    {
        const auto unknown = util::Log::SetEnabledCategories(*options->logCategories);
        for (const std::string& name : unknown)
        {
            // Reported, not ignored: a misspelled category is indistinguishable from a quiet
            // subsystem, which is the worst failure a diagnostic option can have.
            std::fprintf(stderr, "cna-house: --log: unknown category '%s'\n", name.c_str());
        }
        if (!unknown.empty())
        {
            return 2;
        }
    }

    app::Settings settings = app::Settings::Defaults();
    // `--quality` is NOT applied here. The preset is resolved in `LoadContent`, because auto-detect
    // needs the adapter and the FINAL render tier, and the tier is not final until the Tier-E
    // effect set has been tried (`HOUSE-00161`). Applying it here as well would give two places
    // that decide, and the earlier one would be the wrong one.
    if (const std::string clamped = settings.ClampToSupportedRanges(); !clamped.empty())
    {
        util::Log::Warn(util::LogCat::App, "settings clamped into range: {}", clamped);
    }

    try
    {
        app::CnaHouseGame game(*options, settings);
        game.Run();
        return game.ExitCode();
    }
    catch (const std::exception& e)
    {
        // The `Game` boundary of `docs/conventions.md` §5.2. Anything that reaches here means the
        // program genuinely could not start or could not continue, and it is reported rather than
        // swallowed.
        util::Log::Fatal(util::LogCat::App, "unhandled: {}", e.what());
        std::fprintf(stderr, "cna-house: fatal: %s\n", e.what());
        return 1;
    }
}
