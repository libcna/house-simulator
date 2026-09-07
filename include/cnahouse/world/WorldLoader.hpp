// SPDX-License-Identifier: MIT
#pragma once

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
    /// levels, `layout.portals.json` names cells, `layout.props.json` names materials. The loader
    /// reads them in the order `cna-house.md` §15.1 lists them so that a reference is always into
    /// something already read, and a dangling one can be reported against the row that carries it
    /// rather than at the end of the load.
    ///
    /// **Every failure names the file and the JSON path.** `util::JsonValue` carries the path, this
    /// class adds the file, and the message that reaches a log or a test reads
    /// `layout.levels.json/levels[2]/ffl: expected a number, found a string`. That is
    /// `HOUSE-00028`'s whole point and there is no reason to lose it one layer up.
    ///
    /// **It does not validate the house.** A portal whose rectangle is not in its cells' plane, a
    /// room nobody can reach, a stair that does not climb a storey: those are §15.7's eleven rules,
    /// which `validate_world.py` (`HOUSE-00358`) runs before the build and `WorldValidator`
    /// (`HOUSE-00357`) runs at load in debug builds. What the loader refuses is what it cannot
    /// build a `WorldData` from at all: a missing file, a malformed value, a duplicate id.
    class WorldLoader
    {
    public:
        /// @brief The file names of §15.1, in the order they are read.
        [[nodiscard]] static std::span<const std::string_view> FileNames() noexcept;

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
