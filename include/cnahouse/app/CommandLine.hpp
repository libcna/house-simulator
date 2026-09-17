// SPDX-License-Identifier: MIT
#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "cnahouse/util/Result.hpp"

namespace cnahouse::app
{

    /// @brief Which quality preset a session runs at.
    /// @brief The quality rows of `cna-house.md` §68.
    ///
    /// **`Custom` is deliberately absent.** §68 lists it as a fifth value, but "Custom" means
    /// "whatever the user set in the Graphics tab", and there is no Graphics tab yet — a `Custom`
    /// that resolved to a fixed row would be a placeholder pretending to be a feature. It arrives
    /// with the settings UI, where it has something to be custom about.
    enum class QualityPreset
    {
        Low,
        Medium,
        High,
        Ultra,
    };

    /// @brief Which render tier a session uses, subject to what the build actually contains.
    enum class RenderTier
    {
        /// @brief Stock XNA effects only. Always available; complete by design (ADR-0003).
        S,
        /// @brief Compiled `.fx`. Only reachable when `CNAHOUSE_TIER_E` was on at configure time.
        E,
    };

    /// @brief Everything the command line can say.
    ///
    /// **There is no `--renderer` option, and its absence is a design decision, not an omission.**
    /// `CNA_GRAPHICS_RENDERER` is fixed at CMake configure time and standard XNA 4.0 offers no way to
    /// change it afterwards (`cna-house.md` §7.3, §8.1), so a separate binary is produced per renderer.
    /// An option that appeared to change it would be a lie. `--renderer-info` prints what the
    /// application already knows about itself and queries nothing.
    struct Options
    {
        /// @brief What `--quality` asked for, or **unset for auto-detect**.
        ///
        /// Unset is the default because `cna-house.md` §68 gives the quality preset the default
        /// "auto-detected", and a fixed default would make `AutoDetect` unreachable for anyone who
        /// did not know to ask for it. What unset resolves to is decided in `LoadContent`, once the
        /// render tier is final.
        std::optional<QualityPreset> quality;
        /// @brief The tier the user asked for. What they actually get is `ResolveTier`.
        RenderTier tier = RenderTier::E;
        bool headless = false;
        bool noAudio = false;

        /// @brief `--no-cull`: build the draw list from everything resident, not from §25's
        ///        visible set.
        ///
        /// The same switch §71's `cull off` throws, on the command line because that is the only
        /// way a render test can reach it: `HOUSE-00688` renders one pose twice and compares the
        /// two frames, and the harness drives the game through `Options` and not through a
        /// console. §25's walk still runs and `F3` still reports it either way.
        bool noCull = false;
        /// @brief Keep the walk/collision/visibility scene but colour static geometry with the
        ///        deterministic diagnostic palette instead of room-dependent production lighting.
        ///
        /// Deliberately separate from `--scene=blockout`: render and culling tests need to drive
        /// the normal first-person path while isolating geometry visibility from lightmap ownership.
        bool debugBlockoutMaterials = false;
        bool rendererInfo = false;
        bool help = false;
        std::optional<std::string> scene;
        /// @brief `--camera=ex,ey,ez,tx,ty,tz`: where a fixed-camera scene looks from and at.
        ///
        /// Six metres in world space, which is what §69's screenshot harness drives a pose list
        /// of. Here rather than in a scene's own code so that a bug report can carry the view it
        /// was taken from, and so that `HOUSE-00483`'s twenty poses need no twenty scenes.
        std::optional<std::array<float, 6>> camera;

        /// @brief `--player=x,y,z,yawDeg,pitchDeg`: stand a body with its FEET there and look
        ///        through its eyes (`HOUSE-00633`).
        ///
        /// The feet and not the eye, because that is what a pose in this house IS: §43.1 puts the
        /// eye 1.68 m over the soles and §49.3 settles the body onto whatever it is standing on,
        /// so a first-person frame is reproducible from a floor position and a heading rather than
        /// from six numbers somebody measured off a plan. Degrees, and §14's yaw: 0 looks north.
        std::optional<std::array<float, 5>> player;
        std::optional<std::uint64_t> seed;
        /// @brief Time of day to start at, in hours since midnight.
        std::optional<float> timeOfDay;
        /// @brief Keep the simulation clock at its starting instant for deterministic review captures.
        bool freezeTime = false;
        std::optional<std::string> weather;
        /// @brief Repeatable deterministic review override for a real authored switch group.
        ///
        /// Normal play never supplies this: the authored/default/saved switch state remains the
        /// authority. Visual review uses it to photograph a manually controlled practical in its
        /// real on-state without rewriting world data or pretending it belongs on a dusk sensor.
        std::vector<std::string> lightGroupsOn;
        /// @brief Repeatable deterministic review override for a real authored group to be off.
        ///
        /// This is the matched control for `lightGroupsOn`: it lets a review prove what a
        /// default-on practical contributes without changing canonical world data. Off is applied
        /// after on if a caller deliberately names the same group in both lists.
        std::vector<std::string> lightGroupsOff;
        /// @brief Take a screenshot to this path and exit.
        std::optional<std::string> screenshot;
        /// @brief Which drawn frame `--screenshot` captures. 1 is the first, and the default.
        ///
        /// `HOUSE-00201`. Some content is not on screen in frame 1 and never will be: a video's
        /// first decoded frame arrives when the decoder produces it, not when `Play` is called, so
        /// a capture of frame 1 shows an empty panel however healthy the pipeline is. Waiting a
        /// fixed number of frames is honest about what is being waited for, and the alternative --
        /// widening a tolerance until the empty panel passes -- would hide a real failure too.
        std::uint64_t screenshotFrame = 1;
        /// @brief `--log=world,content`; empty means every category.
        std::optional<std::string> logCategories;

        /// @brief Where `ContentManager` looks. Not a command-line option, deliberately.
        ///
        /// The shell resolves this default against XNA's `TitleLocation` before constructing the
        /// game, so shipping loads beside the executable even when its launcher has another working
        /// directory. Tests can still inject their fixture roots directly through this field.
        /// Exposing it as a user-facing option would invite it to be used as one.
        std::string contentRoot = "content";

        /// @brief Where the Tier-E effect tree lives. A SECOND root, deliberately.
        ///
        /// `HOUSE-00111` measured that CNB texture schema 1 is frozen to `Rgba8`, so compressed
        /// textures must be `.xnb`; `HOUSE-00087` produces compiled effects as `.xnb` too. And
        /// `HOUSE-00064` measured that **`.xnb` wins** the content resolution order, so a single
        /// tree holding both containers lets a stale `.xnb` silently shadow the `.cnb` a build just
        /// produced. Two roots is what keeps that impossible rather than merely unlikely.
        std::string effectRoot = "content-fx";
    };

    /// @brief Parses `argv`. An unknown option is an error, never a silent no-op.
    ///
    /// A mistyped `--quailty=low` that ran anyway at the default quality would produce a bug report
    /// about a setting that was never applied, so the parser refuses and says what it did not
    /// understand (`docs/conventions.md` §5.4).
    [[nodiscard]] util::Result<Options> ParseCommandLine(int argc, const char* const* argv);

    /// @brief The usage text, so `--help` and a parse failure print the same thing.
    [[nodiscard]] std::string UsageText();

    /// @brief The tier this binary can actually provide, given what it was built with.
    ///
    /// Asymmetric on purpose: a user may force Tier E **off**, never **on**. A binary built without
    /// Tier E has no compiled effects in its content, so honouring `--tier=e` would produce a
    /// content-load failure at the first draw — and deciding at configure time exists precisely so
    /// that cannot happen.
    [[nodiscard]] RenderTier ResolveTier(RenderTier requested) noexcept;

    [[nodiscard]] std::string_view QualityPresetName(QualityPreset preset) noexcept;
    [[nodiscard]] std::string_view RenderTierName(RenderTier tier) noexcept;

    /// @brief The `--renderer-info` report: configured renderer, build facts, resolved tier.
    ///
    /// Calls no CNA-specific API. Everything in it is a compile-time constant this binary was built
    /// with, which is exactly `HOUSE-00132`'s acceptance criterion.
    [[nodiscard]] std::string RendererInfo(const Options& options);

} // namespace cnahouse::app
