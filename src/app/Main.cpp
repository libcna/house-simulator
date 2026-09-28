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
#include <string>
#include <utility>

#include "Microsoft/Xna/Framework/TitleLocation.hpp"

#include "cnahouse/app/CnaHouseGame.hpp"
#include "cnahouse/app/CommandLine.hpp"
#include "cnahouse/app/Settings.hpp"
#include "cnahouse/persistence/DesktopSaveStore.hpp"
#include "cnahouse/util/Log.hpp"

#if defined(__ANDROID__)
// SDLActivity resolves the entry point as a C symbol. The Android target
// renames main at compile time; desktop keeps its ordinary entry point.
extern "C" int main(int argc, char** argv)
#else
int main(int argc, char** argv)
#endif
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

    // ContentManager resolves a relative RootDirectory against the *process working directory*,
    // while TitleContainer resolves the canonical world against XNA's title location. A launcher
    // started from the repository root could therefore pair fresh build/content/world with stale
    // repository-root content/Textures. Keep both content managers beside the executable, using
    // the same XNA title base as TitleContainer, regardless of the launcher's working directory.
    std::string titleDirectory = Microsoft::Xna::Framework::TitleLocation::getPathProperty();
    if (!titleDirectory.empty() && titleDirectory.back() != '/' && titleDirectory.back() != '\\')
    {
        titleDirectory += '/';
    }
    options->contentRoot = titleDirectory + "content";
    options->effectRoot = titleDirectory + "content-fx";

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
#if !defined(__EMSCRIPTEN__)
    // Standard-XNA storage serves both desktop and Android; no second format/store.
    auto settingsStore = persistence::DesktopSaveStore::Open();
    if (!settingsStore)
    {
        util::Log::Warn(
            util::LogCat::Persistence, "settings store unavailable: {}", settingsStore.Error().ToString());
    }
    else if ((*settingsStore)->Exists("settings.json"))
    {
        auto saved = (*settingsStore)->Read("settings.json");
        if (saved)
        {
            auto parsed = app::Settings::FromJson(*saved, "settings.json");
            if (parsed)
            {
                settings = std::move(*parsed);
                // A saved preference wins over auto-detection; an explicit CLI override still wins.
                if (!options->quality.has_value())
                {
                    options->quality = settings.quality;
                }
                util::Log::Info(
                    util::LogCat::Persistence, "loaded settings from {}", (*settingsStore)->Location());
            }
            else
            {
                util::Log::Warn(util::LogCat::Persistence,
                                "settings invalid; using defaults: {}",
                                parsed.Error().ToString());
            }
        }
        else
        {
            util::Log::Warn(util::LogCat::Persistence,
                            "settings unreadable; using defaults: {}",
                            saved.Error().ToString());
        }
    }
#endif
#if defined(__EMSCRIPTEN__)
    // HOUSE-02895: the Web budget is measured at 1280x720. A browser does not inherit the
    // desktop monitor's 1600x900 default, which also overflows the launch page before fullscreen.
    settings.backBufferWidth = 1280;
    settings.backBufferHeight = 720;
#endif
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
#if !defined(__EMSCRIPTEN__)
        // Persist first-run defaults only after the actual render tier/preset is resolved.
        // Settings edits already save immediately, including before Android process suspension.
        if (game.ExitCode() == 0 && settingsStore && !(*settingsStore)->Exists("settings.json"))
        {
            if (auto written = (*settingsStore)->Write("settings.json", game.UserSettings().ToJson());
                !written)
            {
                util::Log::Warn(util::LogCat::Persistence,
                                "initial settings could not be saved: {}",
                                written.Error().ToString());
            }
        }
#endif
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
