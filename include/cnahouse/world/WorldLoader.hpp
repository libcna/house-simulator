// SPDX-License-Identifier: MIT
#pragma once

#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "cnahouse/util/Json.hpp"
#include "cnahouse/util/Result.hpp"
#include "cnahouse/world/WorldData.hpp"

/// @file
/// `WorldLoader` -- `assets-src/world/`'s deployed JSON into a `WorldData` (`HOUSE-00343`…).

namespace cnahouse::world
{

    /// @brief The index file: what the world is made of, and what a save's `worldHash` covers.
    struct WorldManifest
    {
        struct Member
        {
            std::string file;
            /// `sha256:<64 hex>`, exactly as authored. Kept as text rather than as 32 bytes because
            /// this is what a diagnostic prints and what `HOUSE-00364` compares against.
            std::string sha256;
        };

        std::int32_t version = 0;
        std::string worldHash;
        std::vector<Member> members;
    };

    /// @brief Reads a world directory into a `WorldData`.
    ///
    /// **One file at a time, and the order is the dependency order.** `layout.cells.json` names
    /// levels and materials, `layout.portals.json` names cells, `layout.props.json` names both.
    /// The loader reads them so that a reference is always into something already read -- which is
    /// what lets `WorldValidator` (`HOUSE-00357`) report a dangling one against the row that
    /// carries it rather than after the whole load.
    ///
    /// The loader itself does **not** resolve those references. Resolution is §15.7 rule 6, it has
    /// one owner in `validate_world.py` and one mirror in `WorldValidator`, and a third reading
    /// here could disagree with both.
    ///
    /// The one exception is a portal's **plane** (`HOUSE-00345`, §15.7 rule 4), and it is an
    /// exception for a reason that does not generalise: the rectangle is what the visibility clip
    /// uses directly, every frame, so a rectangle that is not in the wall it claims does not fail —
    /// it produces a frustum that is silently wrong and a room that flickers. The check also needs
    /// nothing outside the two files the loader has just finished reading.
    ///
    /// **Every failure names the file and the JSON path.** `util::JsonValue` carries the path, this
    /// class adds the file, and the message that reaches a log or a test reads
    /// `layout.levels.json/levels[2]/ffl: expected a number, found a string`. That is
    /// `HOUSE-00028`'s whole point and there is no reason to lose it one layer up.
    ///
    /// **It does not validate the house.** A portal whose rectangle is not in its cells' plane, a
    /// room nobody can reach, a stair that does not climb a storey: those are §15.7's twelve rules,
    /// which `validate_world.py` (`HOUSE-00358`) runs before the build and `WorldValidator`
    /// (`HOUSE-00357`) runs at load in debug builds. What the loader refuses is what it cannot
    /// build a `WorldData` from at all: a missing file, a malformed value, a duplicate id.
    class WorldLoader
    {
    public:
        /// @brief The file names of §15.1, in the order they are read.
        [[nodiscard]] static std::span<const std::string_view> FileNames() noexcept;

        /// @brief Hashes @p path's **bytes** as `sha256:<64 lower-case hex>`.
        ///
        /// Of the bytes, not of the parsed JSON: a reformatted file is a different file, the
        /// deployed copy is what this reads, and "the bytes on disk" is the only definition a C++
        /// reader and `tools/world/world_manifest.py` can both implement without first agreeing on
        /// a JSON canonicalisation.
        [[nodiscard]] static util::Result<std::string> HashFile(std::string_view path);

        /// @brief The `worldHash` @p members imply.
        ///
        /// `sha256:` over `<file>\n<sha256>\n` for each member, **in the order given**, encoded
        /// UTF-8. In the order given because the load order is part of what a save was taken
        /// against; over the list rather than over the contents because it must change when any
        /// member changes, and that costs one hash of a few hundred bytes instead of a rehash of
        /// the world.
        [[nodiscard]] static std::string ComputeWorldHash(std::span<const WorldManifest::Member> members);

        /// @brief Rehashes every member and checks the manifest against them.
        ///
        /// `cna-house.md` §15.1: a member whose hash does not match is a **load-time error**. The
        /// save system reads the same `worldHash` to decide whether the world moved under a save
        /// (`HOUSE-02317`), so a manifest that lied would make that decision on a number nothing
        /// had verified.
        [[nodiscard]] static util::Result<void> VerifyManifest(std::string_view directory,
                                                               const WorldManifest& manifest);

        /// @brief Reads `world.manifest.json` from @p directory.
        ///
        /// Checks both directions of §15.1's rule. A member the manifest lists and the directory
        /// does not have is a load error; **so is a world file the directory has and the manifest
        /// does not list**, because silent extra data is how two sources of truth begin. Hash
        /// verification is `HOUSE-00364`'s: it needs `System::Security::Cryptography` and the save
        /// system's definition of `worldHash`, and neither belongs in a file reader.
        [[nodiscard]] static util::Result<WorldManifest> LoadManifest(std::string_view directory);

        /// @brief Reads `layout.levels.json` into @p contents.
        [[nodiscard]] static util::Result<void> LoadLevels(std::string_view directory,
                                                           WorldData::Contents& contents);

        /// @brief Reads `layout.materials.json` into @p contents.
        ///
        /// Read before the cells, because a cell names three of them.
        [[nodiscard]] static util::Result<void> LoadMaterials(std::string_view directory,
                                                              WorldData::Contents& contents);

        /// @brief Reads `layout.cells.json` into @p contents.
        [[nodiscard]] static util::Result<void> LoadCells(std::string_view directory,
                                                          WorldData::Contents& contents);

        /// @brief Reads `layout.portals.json` into @p contents.
        ///
        /// Checks §15.7 rule 4 as it goes: the rectangle lies in both cells' boundary planes within
        /// 1 cm, and its `v` range lies inside both cells' vertical extent. @p contents must
        /// already hold the levels and the cells.
        [[nodiscard]] static util::Result<void> LoadPortals(std::string_view directory,
                                                            WorldData::Contents& contents);

        /// @brief Reads `layout.openings.json` into @p contents.
        ///
        /// Doors and windows as geometry plus entity. Which portal each one belongs to is checked
        /// by §15.7 rule 7 (`validate_world.py`, `WorldValidator`), not here: the bijection is a
        /// statement about two whole files and the loader has only read one of them.
        [[nodiscard]] static util::Result<void> LoadOpenings(std::string_view directory,
                                                             WorldData::Contents& contents);

        /// @brief Reads `layout.stairs.json` into @p contents.
        ///
        /// The collision-ramp parameters are **derived**, not stored: `SegmentFlight` splits a
        /// flight into the same runs and landings `build_collision.py` builds offline, from the
        /// same authored row. Storing them would be a second copy that can disagree with the
        /// wedges the content build actually baked.
        [[nodiscard]] static util::Result<void> LoadStairs(std::string_view directory,
                                                           WorldData::Contents& contents);

        /// @brief Reads `layout.lights.json` into @p contents.
        ///
        /// A light belongs to exactly one **group**, and the group is what a switch toggles and
        /// what a lightmap is baked per. `WorldData::LightsInGroup` is the index that makes a
        /// switch O(1).
        [[nodiscard]] static util::Result<void> LoadLights(std::string_view directory,
                                                           WorldData::Contents& contents);

        /// @brief Reads `layout.props.json` into @p contents.
        ///
        /// Read after the materials, because a prop may override the asset's own with one of them.
        [[nodiscard]] static util::Result<void> LoadProps(std::string_view directory,
                                                          WorldData::Contents& contents);

        /// @brief Reads `layout.nav.json` into @p contents.
        ///
        /// Nodes, edges, and the three marker arrays -- perches, beds and bowls -- which differ
        /// only in their name and are read into one list with a `MarkerKind`.
        [[nodiscard]] static util::Result<void> LoadNav(std::string_view directory,
                                                        WorldData::Contents& contents);

        /// @brief Reads `layout.audio.json` into @p contents.
        ///
        /// Zones, emitters and §64.3's named transmission table.
        [[nodiscard]] static util::Result<void> LoadAudio(std::string_view directory,
                                                          WorldData::Contents& contents);

        /// @brief Reads `layout.exterior.json` into @p contents.
        ///
        /// Terrain, road, fences, neighbourhood buildings and vegetation. Vegetation stays grouped
        /// by asset as the file writes it, because §17.4 draws it instanced where that measures
        /// faster and that needs the grouping in the data rather than rebuilt at load.
        [[nodiscard]] static util::Result<void> LoadExterior(std::string_view directory,
                                                             WorldData::Contents& contents);

        /// @brief Reads and validates §36.2's fourteen target bands and wind modifier.
        ///
        /// Transition rows and seasonal weights join in `HOUSE-01683`, timing distributions in
        /// `HOUSE-01684`, and the complete per-channel rate table in `HOUSE-01685`.
        [[nodiscard]] static util::Result<void> LoadWeather(std::string_view directory,
                                                            WorldData::Contents& contents);

        /// @brief Reads `interactables.json` into @p contents.
        ///
        /// A row's `state` is read **before** its `actions`, because every `when` and `do` is
        /// parsed against it. That ordering is the whole reason the expression vocabulary can be
        /// closed without a global list of setter verbs (`InteractableExpr.hpp`).
        [[nodiscard]] static util::Result<void> LoadInteractables(std::string_view directory,
                                                                  WorldData::Contents& contents);

        /// @brief Reads `initialstate.json` into @p contents.
        ///
        /// Read **last**, because its `interactables` block is checked against the state each
        /// interactable declares -- name and type. This file is what every delta save is taken
        /// against (`cna-house.md` §65.6), so a field here that the interactable does not have is
        /// a value the save would carry for ever and nothing would ever read.
        [[nodiscard]] static util::Result<void> LoadInitialState(std::string_view directory,
                                                                 WorldData::Contents& contents);

        /// @brief Reads every file §15.1 lists and builds the model.
        ///
        /// As each `HOUSE-00344`…`HOUSE-00355` lands, its file joins this sequence. Until then the
        /// world it returns is the levels and the construction constants, which is a world a
        /// `SpatialIndex` cannot be built over and `WorldValidator` will refuse -- and that is the
        /// honest state of the loader rather than an empty model that claims to be complete.
        [[nodiscard]] static util::Result<WorldData> Load(std::string_view directory);

        /// @brief `<directory>/<name>`, with exactly one separator.
        [[nodiscard]] static std::string Join(std::string_view directory, std::string_view name);

        /// @brief Opens @p name and checks its `"schema"` header names @p kind.
        ///
        /// The header is checked here and not by each reader because getting it wrong is the one
        /// mistake that makes every subsequent message misleading: a `layout.cells.json` deployed
        /// under the name `layout.portals.json` produces forty "expected a portal, found ..."
        /// errors, none of which say the real thing.
        [[nodiscard]] static util::Result<util::JsonDocument>
        Open(std::string_view directory, std::string_view name, std::string_view kind, std::int32_t& version);

        /// @brief `{"x": [min, max], "z": [min, max]}` as a `Footprint`.
        ///
        /// Public because every task from `HOUSE-00344` on reads one: a cell's boxes, a plumbing
        /// chase, a terrain extent. `min < max` is checked here rather than left to rule 2, because
        /// an inverted rectangle read into a `Footprint` would make `Contains` answer `false`
        /// everywhere and look like a missing room.
        [[nodiscard]] static util::Result<Footprint> ReadFootprint(const util::JsonValue& object);

        /// @brief Interns @p field's value as an id, or an invalid id when it is absent or `null`.
        ///
        /// `null` means "not specified" (`docs/world-format.md`), so an unspecified reference is
        /// an invalid `Id` and **not** the id of the empty string -- those two would compare equal
        /// to each other and to every other unspecified reference in the file. A field that is
        /// present and is not a string is still an error: "optional" is about absence, not about
        /// type.
        [[nodiscard]] static util::Result<util::Id> OptionalId(const util::JsonValue& object,
                                                               std::string_view field);

        /// @brief Interns @p field's value, refusing an empty id and a hash collision.
        ///
        /// The collision check is `HOUSE-00026`'s acceptance criterion and this is the caller that
        /// makes it fatal: two ids that compare equal are two rooms the game cannot tell apart,
        /// and the failure would look like world-data corruption anywhere else.
        [[nodiscard]] static util::Result<util::Id> RequireId(const util::JsonValue& object,
                                                              std::string_view field);
    };

} // namespace cnahouse::world
