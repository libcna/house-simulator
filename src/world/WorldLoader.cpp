// SPDX-License-Identifier: MIT
#include "cnahouse/world/WorldLoader.hpp"

#include "cnahouse/util/Log.hpp"
#include "cnahouse/world/WorldValidator.hpp"

#include <algorithm>
#include <array>
#include <charconv>
#include <cmath>
#include <cstddef>
#include <optional>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#include "Microsoft/Xna/Framework/Vector2.hpp"
#include "Microsoft/Xna/Framework/Vector3.hpp"

#include "System/IO/Directory.hpp"
#include "System/IO/File.hpp"
#include "System/Security/Cryptography/SHA256.hpp"

#include "cnahouse/util/Ids.hpp"
#include "cnahouse/util/Log.hpp"

namespace cnahouse::world
{
    namespace
    {
        using util::Err;
        using util::ErrorCode;
        using util::JsonDocument;
        using util::JsonValue;
        using util::Result;

        /// §15.1's table, in the order the loader reads it, which is the dependency order:
        /// cells name levels, portals name cells, props name materials.
        constexpr std::array<std::string_view, 16> kFileNames{
            "world.manifest.json",
            "layout.levels.json",
            "layout.materials.json",
            "layout.cells.json",
            "layout.portals.json",
            "layout.openings.json",
            "layout.stairs.json",
            "layout.lights.json",
            "layout.props.json",
            "layout.nav.json",
            "layout.audio.json",
            "layout.exterior.json",
            "layout.weather.json",
            "layout.sky.json",
            "interactables.json",
            "initialstate.json",
        };

        /// @brief The name an id was interned from, or its numeric value if it never was.
        ///
        /// A message that says `id 2582577921 has no face on x = 2.5` is a message nobody can act
        /// on. Everything the loader interns goes through `Intern`, so in practice the name is
        /// always there; the fallback exists so a diagnostic can never be worse than useless.
        [[nodiscard]] std::string Name(util::Id id)
        {
            const std::string_view name = util::IdRegistry::NameOf(id);
            return name.empty() ? "id " + std::to_string(id.Value()) : std::string(name);
        }

        /// @brief The last path component of @p path, whatever separator produced it.
        [[nodiscard]] std::string_view BaseName(std::string_view path) noexcept
        {
            const std::size_t slash = path.find_last_of("/\\");
            return slash == std::string_view::npos ? path : path.substr(slash + 1);
        }

        /// @brief Splits `cna-house/<kind>/<version>`.
        ///
        /// Hand-parsed rather than matched with a regular expression: this runs sixteen times per
        /// load and the grammar is two slashes.
        [[nodiscard]] Result<std::int32_t> ReadSchemaHeader(const JsonDocument& document,
                                                            std::string_view kind)
        {
            const Result<std::string> header = document.Root().RequireString("schema");
            if (!header)
            {
                return header.Error();
            }

            const std::string& text = header.Value();
            const std::size_t first = text.find('/');
            const std::size_t second = text.find('/', first == std::string::npos ? 0 : first + 1);
            if (first == std::string::npos || second == std::string::npos ||
                text.substr(0, first) != "cna-house")
            {
                return Err(ErrorCode::SchemaMismatch,
                           "\"" + text + "\" is not \"cna-house/<kind>/<version>\"",
                           std::string(document.Name()) + "/schema");
            }

            const std::string found = text.substr(first + 1, second - first - 1);
            if (found != kind)
            {
                return Err(ErrorCode::SchemaMismatch,
                           "this file says it holds \"" + found + "\" and the loader is reading it " +
                               "as \"" + std::string(kind) + "\"",
                           std::string(document.Name()) + "/schema");
            }

            const std::string tail = text.substr(second + 1);
            if (tail.empty() ||
                !std::all_of(tail.begin(), tail.end(), [](unsigned char c) { return c >= '0' && c <= '9'; }))
            {
                return Err(ErrorCode::SchemaMismatch,
                           "the version in \"" + text + "\" is not an integer",
                           std::string(document.Name()) + "/schema");
            }
            return static_cast<std::int32_t>(std::stol(tail));
        }
    } // namespace

    std::span<const std::string_view> WorldLoader::FileNames() noexcept
    {
        return kFileNames;
    }

    std::string WorldLoader::Join(std::string_view directory, std::string_view name)
    {
        std::string path(directory);
        while (!path.empty() && (path.back() == '/' || path.back() == '\\'))
        {
            path.pop_back();
        }
        path += '/';
        path += name;
        return path;
    }

    Result<JsonDocument> WorldLoader::Open(std::string_view directory,
                                           std::string_view name,
                                           std::string_view kind,
                                           std::int32_t& version)
    {
        const std::string path = Join(directory, name);
        if (!System::IO::File::Exists(path))
        {
            return Err(
                ErrorCode::NotFound, "no such file; `cna-house.md` §15.1 requires it", std::string(name));
        }

        Result<JsonDocument> document = JsonDocument::Load(path);
        if (!document)
        {
            return document.Error().WithContext(name);
        }

        const Result<std::int32_t> header = ReadSchemaHeader(document.Value(), kind);
        if (!header)
        {
            return header.Error();
        }
        version = header.Value();
        return document;
    }

    Result<util::Id> WorldLoader::RequireId(const JsonValue& object, std::string_view field)
    {
        const Result<std::string> name = object.RequireString(field);
        if (!name)
        {
            return name.Error();
        }
        if (name.Value().empty())
        {
            return Err(
                ErrorCode::InvalidData, "an id may not be empty", object.Path() + "/" + std::string(field));
        }

        util::IdRegistry::ClearConflict();
        const util::Id id = util::Intern(name.Value());
        if (util::IdRegistry::HadConflict())
        {
            // `HOUSE-00026`'s acceptance: a 32-bit hash over a few thousand ids has a real
            // birthday-problem chance of a collision, and a silent one would make two rooms the
            // same room. The loader is the caller that makes it fatal.
            return Err(ErrorCode::Duplicate,
                       "\"" + name.Value() + "\" hashes to the same id as \"" +
                           util::IdRegistry::ConflictExisting() +
                           "\"; rename one of them, because two ids that compare equal are two "
                           "rooms the game cannot tell apart",
                       object.Path() + "/" + std::string(field));
        }
        return id;
    }

    Result<util::Id> WorldLoader::OptionalId(const JsonValue& object, std::string_view field)
    {
        if (!object.Has(field) || object.IsNull(field))
        {
            return util::Id{};
        }
        return RequireId(object, field);
    }

    Result<Footprint> WorldLoader::ReadFootprint(const JsonValue& object)
    {
        const auto axis = [&object](std::string_view name) -> Result<std::pair<float, float>>
        {
            const Result<JsonValue> range = object.RequireArray(name);
            if (!range)
            {
                return range.Error();
            }
            const Result<std::vector<JsonValue>> parts = range.Value().Elements();
            if (!parts)
            {
                return parts.Error();
            }
            if (parts.Value().size() != 2U)
            {
                return Err(ErrorCode::InvalidData,
                           "a range is [min, max]; this has " + std::to_string(parts.Value().size()) +
                               " element(s)",
                           object.Path() + "/" + std::string(name));
            }
            const Result<float> low = parts.Value()[0].AsFloat();
            if (!low)
            {
                return low.Error();
            }
            const Result<float> high = parts.Value()[1].AsFloat();
            if (!high)
            {
                return high.Error();
            }
            if (!(low.Value() < high.Value()))
            {
                return Err(ErrorCode::InvalidData,
                           "a range is [min, max] with min < max; this is [" + std::to_string(low.Value()) +
                               ", " + std::to_string(high.Value()) + "]",
                           object.Path() + "/" + std::string(name));
            }
            return std::pair<float, float>{low.Value(), high.Value()};
        };

        const Result<std::pair<float, float>> x = axis("x");
        if (!x)
        {
            return x.Error();
        }
        const Result<std::pair<float, float>> z = axis("z");
        if (!z)
        {
            return z.Error();
        }
        return Footprint{x.Value().first, x.Value().second, z.Value().first, z.Value().second};
    }

    namespace
    {
        /// @brief `sha256:` plus 64 lower-case hex characters.
        [[nodiscard]] std::string Spell(const std::vector<SharpRuntime::bytecs>& digest)
        {
            static constexpr char kHex[] = "0123456789abcdef";
            std::string text = "sha256:";
            text.reserve(text.size() + digest.size() * 2U);
            for (const SharpRuntime::bytecs byte : digest)
            {
                const auto value = static_cast<unsigned char>(byte);
                text.push_back(kHex[value >> 4U]);
                text.push_back(kHex[value & 0x0FU]);
            }
            return text;
        }
    } // namespace

    Result<std::string> WorldLoader::HashFile(std::string_view path)
    {
        std::vector<SharpRuntime::bytecs> bytes;
        try
        {
            bytes = System::IO::File::ReadAllBytes(std::string(path));
        }
        catch (const std::exception& e)
        {
            return Err(ErrorCode::IoFailure, e.what(), std::string(path));
        }
        System::Security::Cryptography::SHA256 sha;
        return Spell(sha.ComputeHash(bytes));
    }

    std::string WorldLoader::ComputeWorldHash(std::span<const WorldManifest::Member> members)
    {
        std::string joined;
        for (const WorldManifest::Member& member : members)
        {
            joined += member.file;
            joined += '\n';
            joined += member.sha256;
            joined += '\n';
        }
        std::vector<SharpRuntime::bytecs> bytes(joined.size());
        for (std::size_t index = 0; index < joined.size(); ++index)
        {
            bytes[index] = static_cast<SharpRuntime::bytecs>(static_cast<unsigned char>(joined[index]));
        }
        System::Security::Cryptography::SHA256 sha;
        return Spell(sha.ComputeHash(bytes));
    }

    Result<void> WorldLoader::VerifyManifest(std::string_view directory, const WorldManifest& manifest)
    {
        for (std::size_t index = 0; index < manifest.members.size(); ++index)
        {
            const WorldManifest::Member& member = manifest.members[index];
            const Result<std::string> actual = HashFile(Join(directory, member.file));
            if (!actual)
            {
                return actual.Error().WithContext("world.manifest.json");
            }
            if (actual.Value() != member.sha256)
            {
                // Naming the file and both hashes, because the two ways to reach this are a file
                // edited without regenerating the manifest and a file that arrived corrupt, and
                // the author can tell those apart at a glance and this code cannot.
                return Err(ErrorCode::ChecksumMismatch,
                           member.file + " does not match the manifest: it lists " + member.sha256 +
                               " and the file on disk is " + actual.Value() +
                               ". Run tools/world/world_manifest.py --emit if you meant to "
                               "change it",
                           "world.manifest.json/members[" + std::to_string(index) + "]");
            }
        }

        const std::string expected = ComputeWorldHash(manifest.members);
        if (expected != manifest.worldHash)
        {
            return Err(ErrorCode::ChecksumMismatch,
                       "the worldHash does not cover this member list: it says " + manifest.worldHash +
                           " and the list hashes to " + expected +
                           ". A save compares this number to decide whether the world moved "
                           "under it, so a wrong one is a decision made on nothing",
                       "world.manifest.json/worldHash");
        }
        return util::Ok();
    }

    Result<WorldManifest> WorldLoader::LoadManifest(std::string_view directory)
    {
        if (!System::IO::Directory::Exists(std::string(directory)))
        {
            return Err(ErrorCode::NotFound, "no such world directory", std::string(directory));
        }

        WorldManifest manifest;
        const Result<JsonDocument> document =
            Open(directory, "world.manifest.json", "manifest", manifest.version);
        if (!document)
        {
            return document.Error();
        }
        const JsonValue& root = document.Value().Root();

        const Result<std::string> worldHash = root.RequireString("worldHash");
        if (!worldHash)
        {
            return worldHash.Error().WithContext("world.manifest.json");
        }
        manifest.worldHash = worldHash.Value();

        const Result<JsonValue> members = root.RequireArray("members");
        if (!members)
        {
            return members.Error().WithContext("world.manifest.json");
        }
        const Result<std::vector<JsonValue>> rows = members.Value().Elements();
        if (!rows)
        {
            return rows.Error().WithContext("world.manifest.json");
        }

        std::vector<std::string> listed;
        for (const JsonValue& row : rows.Value())
        {
            const Result<std::string> file = row.RequireString("file");
            if (!file)
            {
                return file.Error().WithContext("world.manifest.json");
            }
            const Result<std::string> sha = row.RequireString("sha256");
            if (!sha)
            {
                return sha.Error().WithContext("world.manifest.json");
            }
            if (!System::IO::File::Exists(Join(directory, file.Value())))
            {
                return Err(ErrorCode::NotFound,
                           "the manifest lists \"" + file.Value() + "\" and it is not here",
                           "world.manifest.json/" + row.Path());
            }
            if (std::find(listed.begin(), listed.end(), file.Value()) != listed.end())
            {
                return Err(ErrorCode::Duplicate,
                           "\"" + file.Value() + "\" is listed twice",
                           "world.manifest.json/" + row.Path());
            }
            listed.push_back(file.Value());
            manifest.members.push_back({file.Value(), sha.Value()});
        }

        // The other direction. A world file in the directory that no member names is not a
        // harmless extra: it is a second source of truth that nothing hashes, so a save's
        // `worldHash` would not change when it did.
        for (const std::string& path : System::IO::Directory::GetFiles(std::string(directory)))
        {
            const std::string name(BaseName(path));
            if (name == "world.manifest.json" || name == "assets.manifest.json")
            {
                continue; // the index itself, and the ASSET manifest, which §20.3 owns
            }
            if (std::find(kFileNames.begin(), kFileNames.end(), name) == kFileNames.end())
            {
                continue; // not a world file at all; §15.1 does not speak for it
            }
            if (std::find(listed.begin(), listed.end(), name) == listed.end())
            {
                return Err(ErrorCode::InvalidData,
                           "\"" + name + "\" is in the world directory and no manifest member " +
                               "names it; silent extra data is how two sources of truth begin",
                           "world.manifest.json/members");
            }
        }

        return manifest;
    }

    Result<void> WorldLoader::LoadLevels(std::string_view directory, WorldData::Contents& contents)
    {
        std::int32_t version = 0;
        const Result<JsonDocument> document = Open(directory, "layout.levels.json", "levels", version);
        if (!document)
        {
            return document.Error();
        }
        const JsonValue& root = document.Value().Root();

        const Result<JsonValue> levels = root.RequireArray("levels");
        if (!levels)
        {
            return levels.Error().WithContext("layout.levels.json");
        }
        const Result<std::vector<JsonValue>> rows = levels.Value().Elements();
        if (!rows)
        {
            return rows.Error().WithContext("layout.levels.json");
        }

        for (const JsonValue& row : rows.Value())
        {
            Level level;
            const Result<util::Id> id = RequireId(row, "id");
            if (!id)
            {
                return id.Error().WithContext("layout.levels.json");
            }
            level.id = id.Value();

            const Result<std::string> name = row.OptionalString("name", "");
            if (!name)
            {
                return name.Error().WithContext("layout.levels.json");
            }
            level.name = name.Value();

            const Result<float> ffl = row.RequireFloat("ffl");
            if (!ffl)
            {
                return ffl.Error().WithContext("layout.levels.json");
            }
            level.ffl = ffl.Value();

            // `ceiling` is `null` on a rafter-bounded level, and `null` there means "there is no
            // ceiling plane", not "the ceiling is at zero". An `optional` is the only reading that
            // keeps those two apart, and `WorldData::ExtentOf` refuses rather than inventing one.
            if (row.Has("ceiling") && !row.IsNull("ceiling"))
            {
                const Result<float> ceiling = row.RequireFloat("ceiling");
                if (!ceiling)
                {
                    return ceiling.Error().WithContext("layout.levels.json");
                }
                level.ceiling = ceiling.Value();
            }

            const Result<float> depth = row.OptionalFloat("structureDepth", 0.0F);
            if (!depth)
            {
                return depth.Error().WithContext("layout.levels.json");
            }
            level.structureDepth = depth.Value();

            const Result<util::Id> roof = OptionalId(row, "roof");
            if (!roof)
            {
                return roof.Error().WithContext("layout.levels.json");
            }
            level.roof = roof.Value();

            contents.levels.push_back(std::move(level));
        }

        if (contents.levels.empty())
        {
            return Err(ErrorCode::InvalidData,
                       "there are no levels; every cell names one, so a world without them has no "
                       "geometry to place",
                       "layout.levels.json/levels");
        }

        // `construction` is required. Every wall thickness in the house derives from it, and a
        // missing block would give a house built entirely from zero-thickness walls -- which looks
        // like a rendering bug and is a data bug.
        const Result<JsonValue> construction = root.RequireObject("construction");
        if (!construction)
        {
            return construction.Error().WithContext("layout.levels.json");
        }

        struct Field
        {
            std::string_view name;
            float Construction::* member;
        };

        constexpr std::array<Field, 12> kFields{{
            {"wallExterior", &Construction::wallExterior},
            {"wallPartition", &Construction::wallPartition},
            {"wallPlumbing", &Construction::wallPlumbing},
            {"wallGarage", &Construction::wallGarage},
            {"foundationWall", &Construction::foundationWall},
            {"kneeWallHeight", &Construction::kneeWallHeight},
            {"ridgeY", &Construction::ridgeY},
            {"roofPitch", &Construction::roofPitch},
            {"skirting", &Construction::skirting},
            {"cornice", &Construction::cornice},
            {"balustrade", &Construction::balustrade},
            {"railing", &Construction::railing},
        }};
        for (const Field& field : kFields)
        {
            const Result<float> value = construction.Value().OptionalFloat(field.name, 0.0F);
            if (!value)
            {
                return value.Error().WithContext("layout.levels.json");
            }
            contents.construction.*field.member = value.Value();
        }

        // §12.5's stacks, optional because a world under construction may not have them yet and
        // `validate_world.py` rule 9 is the thing that decides whether that is acceptable.
        // §12.5's HVAC row (`HOUSE-00387`). Read for the same reason as `plumbing`: §62.6 places
        // the duct rumble and tick at the registers, so the registers have to reach the runtime.
        if (root.Has("hvac") && !root.IsNull("hvac"))
        {
            const Result<JsonValue> hvac = root.RequireObject("hvac");
            if (!hvac)
            {
                return hvac.Error().WithContext("layout.levels.json");
            }
            const Result<JsonValue> branches = hvac.Value().RequireArray("branches");
            if (!branches)
            {
                return branches.Error().WithContext("layout.levels.json");
            }
            const Result<std::vector<JsonValue>> branchRows = branches.Value().Elements();
            if (!branchRows)
            {
                return branchRows.Error().WithContext("layout.levels.json");
            }
            for (const JsonValue& row : branchRows.Value())
            {
                HvacBranch branch;
                const Result<util::Id> id = RequireId(row, "id");
                if (!id)
                {
                    return id.Error().WithContext("layout.levels.json");
                }
                branch.id = id.Value();

                const Result<JsonValue> served = row.RequireArray("cells");
                if (!served)
                {
                    return served.Error().WithContext("layout.levels.json");
                }
                const Result<std::vector<JsonValue>> servedRows = served.Value().Elements();
                if (!servedRows)
                {
                    return servedRows.Error().WithContext("layout.levels.json");
                }
                for (const JsonValue& cell : servedRows.Value())
                {
                    const Result<std::string> name = cell.AsString();
                    if (!name)
                    {
                        return name.Error().WithContext("layout.levels.json");
                    }
                    branch.cells.push_back(util::Intern(name.Value()));
                }
                if (branch.cells.empty())
                {
                    return Err(ErrorCode::InvalidData,
                               "a duct branch serves at least one cell; this one serves none",
                               "layout.levels.json/" + row.Path() + "/cells");
                }

                if (row.Has("trunk") && !row.IsNull("trunk"))
                {
                    const Result<std::string> trunk = row.RequireString("trunk");
                    if (!trunk)
                    {
                        return trunk.Error().WithContext("layout.levels.json");
                    }
                    branch.trunk = trunk.Value();
                }

                if (row.Has("registers") && !row.IsNull("registers"))
                {
                    const Result<JsonValue> registers = row.RequireArray("registers");
                    if (!registers)
                    {
                        return registers.Error().WithContext("layout.levels.json");
                    }
                    const Result<std::vector<JsonValue>> registerRows = registers.Value().Elements();
                    if (!registerRows)
                    {
                        return registerRows.Error().WithContext("layout.levels.json");
                    }
                    for (const JsonValue& entry : registerRows.Value())
                    {
                        HvacRegister grille;
                        const Result<util::Id> cell = RequireId(entry, "cell");
                        if (!cell)
                        {
                            return cell.Error().WithContext("layout.levels.json");
                        }
                        grille.cell = cell.Value();

                        const Result<Microsoft::Xna::Framework::Vector3> position =
                            entry.RequireVector3("position");
                        if (!position)
                        {
                            return position.Error().WithContext("layout.levels.json");
                        }
                        grille.position = position.Value();

                        if (entry.Has("kind") && !entry.IsNull("kind"))
                        {
                            const Result<std::string> kind = entry.RequireString("kind");
                            if (!kind)
                            {
                                return kind.Error().WithContext("layout.levels.json");
                            }
                            grille.kind = kind.Value();
                        }
                        branch.registers.push_back(std::move(grille));
                    }
                }

                contents.hvac.push_back(std::move(branch));
            }
        }

        if (root.Has("plumbing"))
        {
            const Result<JsonValue> plumbing = root.RequireObject("plumbing");
            if (!plumbing)
            {
                return plumbing.Error().WithContext("layout.levels.json");
            }
            const Result<JsonValue> stacks = plumbing.Value().RequireArray("stacks");
            if (!stacks)
            {
                return stacks.Error().WithContext("layout.levels.json");
            }
            const Result<std::vector<JsonValue>> stackRows = stacks.Value().Elements();
            if (!stackRows)
            {
                return stackRows.Error().WithContext("layout.levels.json");
            }
            for (const JsonValue& row : stackRows.Value())
            {
                PlumbingStack stack;
                const Result<util::Id> id = RequireId(row, "id");
                if (!id)
                {
                    return id.Error().WithContext("layout.levels.json");
                }
                stack.id = id.Value();

                const Result<JsonValue> cells = row.RequireArray("cells");
                if (!cells)
                {
                    return cells.Error().WithContext("layout.levels.json");
                }
                const Result<std::vector<JsonValue>> cellRows = cells.Value().Elements();
                if (!cellRows)
                {
                    return cellRows.Error().WithContext("layout.levels.json");
                }
                for (const JsonValue& cell : cellRows.Value())
                {
                    const Result<std::string> name = cell.AsString();
                    if (!name)
                    {
                        return name.Error().WithContext("layout.levels.json");
                    }
                    stack.cells.push_back(util::Intern(name.Value()));
                }

                const Result<JsonValue> chase = row.RequireObject("chase");
                if (!chase)
                {
                    return chase.Error().WithContext("layout.levels.json");
                }
                const Result<Footprint> box = ReadFootprint(chase.Value());
                if (!box)
                {
                    return box.Error().WithContext("layout.levels.json");
                }
                stack.chase = box.Value();

                const Result<util::Id> dropTo = OptionalId(row, "dropTo");
                if (!dropTo)
                {
                    return dropTo.Error().WithContext("layout.levels.json");
                }
                stack.dropTo = dropTo.Value();

                contents.plumbing.push_back(std::move(stack));
            }
        }

        return util::Ok();
    }

    Result<void> WorldLoader::LoadMaterials(std::string_view directory, WorldData::Contents& contents)
    {
        std::int32_t version = 0;
        const Result<JsonDocument> document = Open(directory, "layout.materials.json", "materials", version);
        if (!document)
        {
            return document.Error();
        }

        const Result<JsonValue> materials = document.Value().Root().RequireArray("materials");
        if (!materials)
        {
            return materials.Error().WithContext("layout.materials.json");
        }
        const Result<std::vector<JsonValue>> rows = materials.Value().Elements();
        if (!rows)
        {
            return rows.Error().WithContext("layout.materials.json");
        }

        for (const JsonValue& row : rows.Value())
        {
            MaterialDef material;
            const Result<util::Id> id = RequireId(row, "id");
            if (!id)
            {
                return id.Error().WithContext("layout.materials.json");
            }
            material.id = id.Value();

            const Result<std::string> className = row.RequireString("class");
            if (!className)
            {
                return className.Error().WithContext("layout.materials.json");
            }
            const Result<MaterialClassSpec> spec = ParseMaterialClass(className.Value());
            if (!spec)
            {
                return spec.Error().WithContext(row.Path() + "/class").WithContext("layout.materials.json");
            }
            material.materialClass = spec.Value().base;
            material.surfaceState = spec.Value().state;

            // The texture paths are content names, not ids: they are handed to `ContentManager`
            // and never compared to anything, so interning them would put a few hundred
            // never-looked-up names into the id registry for nothing.
            const auto text = [&row](std::string_view field) -> Result<std::string>
            {
                if (!row.Has(field) || row.IsNull(field))
                {
                    return std::string{};
                }
                return row.RequireString(field);
            };
            for (const auto& [field, target] :
                 std::initializer_list<std::pair<std::string_view, std::string*>>{
                     {"albedo", &material.albedo},
                     {"normal", &material.normal},
                     {"footstepSurface", &material.footstepSurface},
                     {"effectTierE", &material.effectTierE}})
            {
                const Result<std::string> value = text(field);
                if (!value)
                {
                    return value.Error().WithContext("layout.materials.json");
                }
                *target = value.Value();
            }

            const Result<std::int64_t> channel = row.OptionalInt("lightmapChannel", 0);
            if (!channel)
            {
                return channel.Error().WithContext("layout.materials.json");
            }
            if (channel.Value() < 0 || channel.Value() > 1)
            {
                return Err(ErrorCode::OutOfRange,
                           "a lightmap channel is 0 or 1; this is " + std::to_string(channel.Value()),
                           "layout.materials.json/" + row.Path() + "/lightmapChannel");
            }
            material.lightmapChannel = static_cast<std::int32_t>(channel.Value());

            for (const auto& [field, target] :
                 std::initializer_list<std::pair<std::string_view, Microsoft::Xna::Framework::Vector3*>>{
                     {"tint", &material.tint}, {"specularColor", &material.specularColor}})
            {
                if (!row.Has(field) || row.IsNull(field))
                {
                    continue;
                }
                const Result<Microsoft::Xna::Framework::Vector3> value = row.RequireVector3(field);
                if (!value)
                {
                    return value.Error().WithContext("layout.materials.json");
                }
                *target = value.Value();
            }

            const Result<float> power = row.OptionalFloat("specularPower", 0.0F);
            if (!power)
            {
                return power.Error().WithContext("layout.materials.json");
            }
            material.specularPower = power.Value();

            const Result<std::string> alphaMode = row.OptionalString("alphaMode", "opaque");
            if (!alphaMode)
            {
                return alphaMode.Error().WithContext("layout.materials.json");
            }
            const Result<AlphaMode> mode = ParseAlphaMode(alphaMode.Value());
            if (!mode)
            {
                return mode.Error()
                    .WithContext(row.Path() + "/alphaMode")
                    .WithContext("layout.materials.json");
            }
            material.alphaMode = mode.Value();

            const Result<float> alpha = row.OptionalFloat("alpha", 1.0F);
            if (!alpha)
            {
                return alpha.Error().WithContext("layout.materials.json");
            }
            if (alpha.Value() < 0.0F || alpha.Value() > 1.0F)
            {
                return Err(ErrorCode::OutOfRange,
                           "alpha is a unit value; this is " + std::to_string(alpha.Value()),
                           "layout.materials.json/" + row.Path() + "/alpha");
            }
            material.alpha = alpha.Value();

            if (row.Has("alphaCutoff") && !row.IsNull("alphaCutoff"))
            {
                const Result<float> cutoff = row.RequireFloat("alphaCutoff");
                if (!cutoff)
                {
                    return cutoff.Error().WithContext("layout.materials.json");
                }
                material.alphaCutoff = cutoff.Value();
            }
            // A masked material without a cutoff has no threshold to test against, and the stock
            // `AlphaTestEffect` would silently use its own default rather than the author's.
            if (material.alphaMode == AlphaMode::Mask && !material.alphaCutoff.has_value())
            {
                return Err(ErrorCode::InvalidData,
                           "alphaMode is \"mask\" and alphaCutoff is not set; there is no threshold "
                           "to test against",
                           "layout.materials.json/" + row.Path() + "/alphaCutoff");
            }

            const Result<bool> twoSided = row.OptionalBool("twoSided", false);
            if (!twoSided)
            {
                return twoSided.Error().WithContext("layout.materials.json");
            }
            material.twoSided = twoSided.Value();

            if (row.Has("uvScale") && !row.IsNull("uvScale"))
            {
                const Result<Microsoft::Xna::Framework::Vector2> scale = row.RequireVector2("uvScale");
                if (!scale)
                {
                    return scale.Error().WithContext("layout.materials.json");
                }
                material.uvScaleU = scale.Value().X;
                material.uvScaleV = scale.Value().Y;
            }

            if (row.Has("wetResponse") && !row.IsNull("wetResponse"))
            {
                const Result<JsonValue> wet = row.RequireObject("wetResponse");
                if (!wet)
                {
                    return wet.Error().WithContext("layout.materials.json");
                }
                const Result<float> darken = wet.Value().OptionalFloat("albedoDarken", 0.0F);
                if (!darken)
                {
                    return darken.Error().WithContext("layout.materials.json");
                }
                const Result<float> boost = wet.Value().OptionalFloat("specularBoost", 0.0F);
                if (!boost)
                {
                    return boost.Error().WithContext("layout.materials.json");
                }
                const Result<float> powerBoost = wet.Value().OptionalFloat("powerBoost", 0.0F);
                if (!powerBoost)
                {
                    return powerBoost.Error().WithContext("layout.materials.json");
                }
                material.wet = {darken.Value(), boost.Value(), powerBoost.Value()};
            }

            if (row.Has("snowResponse") && !row.IsNull("snowResponse"))
            {
                const Result<JsonValue> snow = row.RequireObject("snowResponse");
                if (!snow)
                {
                    return snow.Error().WithContext("layout.materials.json");
                }
                const Result<bool> coverable = snow.Value().OptionalBool("coverable", false);
                if (!coverable)
                {
                    return coverable.Error().WithContext("layout.materials.json");
                }
                const Result<float> limit = snow.Value().OptionalFloat("slopeLimitDeg", 0.0F);
                if (!limit)
                {
                    return limit.Error().WithContext("layout.materials.json");
                }
                if (limit.Value() < 0.0F || limit.Value() > 90.0F)
                {
                    return Err(ErrorCode::OutOfRange,
                               "a slope limit is 0..90 degrees; this is " + std::to_string(limit.Value()),
                               "layout.materials.json/" + row.Path() + "/snowResponse/slopeLimitDeg");
                }
                material.snow = {coverable.Value(), limit.Value()};
            }

            const Result<float> absorption = row.OptionalFloat("audioAbsorption", 0.0F);
            if (!absorption)
            {
                return absorption.Error().WithContext("layout.materials.json");
            }
            material.audioAbsorption = absorption.Value();

            // `effectTierS` is stated rather than inferred, and §22.2's class table is the
            // documented fallback when it is absent. `build_chunks.py` reads the same two rules to
            // choose a chunk's vertex layout: a chunk built with one layout and drawn with the
            // effect the other chose is a wrong-looking surface nobody can trace.
            if (row.Has("effectTierS") && !row.IsNull("effectTierS"))
            {
                const Result<std::string> tier = row.RequireString("effectTierS");
                if (!tier)
                {
                    return tier.Error().WithContext("layout.materials.json");
                }
                const Result<EffectTier> parsed = ParseEffectTier(tier.Value());
                if (!parsed)
                {
                    return parsed.Error()
                        .WithContext(row.Path() + "/effectTierS")
                        .WithContext("layout.materials.json");
                }
                material.effectTierS = parsed.Value();
            }
            else
            {
                material.effectTierS = DefaultEffectTier(material.materialClass);
            }

            contents.materials.push_back(std::move(material));
        }

        return util::Ok();
    }

    Result<void> WorldLoader::LoadCells(std::string_view directory, WorldData::Contents& contents)
    {
        std::int32_t version = 0;
        const Result<JsonDocument> document = Open(directory, "layout.cells.json", "cells", version);
        if (!document)
        {
            return document.Error();
        }

        const Result<JsonValue> cells = document.Value().Root().RequireArray("cells");
        if (!cells)
        {
            return cells.Error().WithContext("layout.cells.json");
        }
        const Result<std::vector<JsonValue>> rows = cells.Value().Elements();
        if (!rows)
        {
            return rows.Error().WithContext("layout.cells.json");
        }

        for (const JsonValue& row : rows.Value())
        {
            Cell cell;
            const Result<util::Id> id = RequireId(row, "id");
            if (!id)
            {
                return id.Error().WithContext("layout.cells.json");
            }
            cell.id = id.Value();

            const Result<util::Id> level = RequireId(row, "level");
            if (!level)
            {
                return level.Error().WithContext("layout.cells.json");
            }
            cell.level = level.Value();

            const Result<std::string> name = row.OptionalString("name", "");
            if (!name)
            {
                return name.Error().WithContext("layout.cells.json");
            }
            cell.name = name.Value();

            const Result<std::string> kind = row.RequireString("kind");
            if (!kind)
            {
                return kind.Error().WithContext("layout.cells.json");
            }
            const Result<CellKind> parsedKind = ParseCellKind(kind.Value());
            if (!parsedKind)
            {
                return parsedKind.Error().WithContext(row.Path() + "/kind").WithContext("layout.cells.json");
            }
            cell.kind = parsedKind.Value();

            // A cell is a UNION of axis-aligned boxes, not one box. That is what an L-shaped room
            // is, and it is why `WorldData::CellContains` walks a list: the bounding box of an L
            // includes the notch, which belongs to the room next door.
            const Result<JsonValue> boxes = row.RequireArray("boxes");
            if (!boxes)
            {
                return boxes.Error().WithContext("layout.cells.json");
            }
            const Result<std::vector<JsonValue>> boxRows = boxes.Value().Elements();
            if (!boxRows)
            {
                return boxRows.Error().WithContext("layout.cells.json");
            }
            if (boxRows.Value().empty())
            {
                return Err(ErrorCode::InvalidData,
                           "a cell has at least one box; with none it has no floor to stand on and "
                           "no wall to draw",
                           "layout.cells.json/" + row.Path() + "/boxes");
            }
            for (const JsonValue& box : boxRows.Value())
            {
                const Result<Footprint> footprint = ReadFootprint(box);
                if (!footprint)
                {
                    return footprint.Error().WithContext("layout.cells.json");
                }
                cell.boxes.push_back(footprint.Value());
            }

            // `yOverride` is `[min, max]`, and `null` means "use the level's ffl..ceiling". The
            // difference matters most on a rafter-bounded level, where there is no level ceiling to
            // fall back to and `WorldData::ExtentOf` refuses rather than inventing one.
            if (row.Has("yOverride") && !row.IsNull("yOverride"))
            {
                const Result<JsonValue> range = row.RequireArray("yOverride");
                if (!range)
                {
                    return range.Error().WithContext("layout.cells.json");
                }
                const Result<std::vector<JsonValue>> parts = range.Value().Elements();
                if (!parts)
                {
                    return parts.Error().WithContext("layout.cells.json");
                }
                if (parts.Value().size() != 2U)
                {
                    return Err(ErrorCode::InvalidData,
                               "yOverride is [floor, ceiling]; this has " +
                                   std::to_string(parts.Value().size()) + " element(s)",
                               "layout.cells.json/" + row.Path() + "/yOverride");
                }
                const Result<float> low = parts.Value()[0].AsFloat();
                if (!low)
                {
                    return low.Error().WithContext("layout.cells.json");
                }
                const Result<float> high = parts.Value()[1].AsFloat();
                if (!high)
                {
                    return high.Error().WithContext("layout.cells.json");
                }
                if (!(low.Value() < high.Value()))
                {
                    return Err(ErrorCode::InvalidData,
                               "yOverride is [floor, ceiling] with floor < ceiling; this is [" +
                                   std::to_string(low.Value()) + ", " + std::to_string(high.Value()) + "]",
                               "layout.cells.json/" + row.Path() + "/yOverride");
                }
                cell.yOverride = Extent{low.Value(), high.Value()};
            }

            for (const auto& [field, target] : std::initializer_list<std::pair<std::string_view, util::Id*>>{
                     {"floorMaterial", &cell.floorMaterial},
                     {"wallMaterial", &cell.wallMaterial},
                     {"ceilingMaterial", &cell.ceilingMaterial},
                     {"trimMaterial", &cell.trimMaterial},
                     {"navMeshRegion", &cell.navMeshRegion},
                     {"parent", &cell.parent}})
            {
                const Result<util::Id> value = OptionalId(row, field);
                if (!value)
                {
                    return value.Error().WithContext("layout.cells.json");
                }
                *target = value.Value();
            }

            const auto text = [&row](std::string_view field) -> Result<std::string>
            {
                if (!row.Has(field) || row.IsNull(field))
                {
                    return std::string{};
                }
                return row.RequireString(field);
            };
            for (const auto& [field, target] :
                 std::initializer_list<std::pair<std::string_view, std::string*>>{
                     {"footstepSurface", &cell.footstepSurface}, {"residencyPack", &cell.residencyPack}})
            {
                const Result<std::string> value = text(field);
                if (!value)
                {
                    return value.Error().WithContext("layout.cells.json");
                }
                *target = value.Value();
            }

            if (row.Has("acoustic") && !row.IsNull("acoustic"))
            {
                const Result<JsonValue> acoustic = row.RequireObject("acoustic");
                if (!acoustic)
                {
                    return acoustic.Error().WithContext("layout.cells.json");
                }
                const Result<util::Id> tone = OptionalId(acoustic.Value(), "roomTone");
                if (!tone)
                {
                    return tone.Error().WithContext("layout.cells.json");
                }
                cell.acoustic.roomTone = tone.Value();
                const Result<float> absorption = acoustic.Value().OptionalFloat("absorption", 0.0F);
                if (!absorption)
                {
                    return absorption.Error().WithContext("layout.cells.json");
                }
                cell.acoustic.absorption = absorption.Value();
                const Result<std::string> hint = acoustic.Value().OptionalString("reverbHint", "");
                if (!hint)
                {
                    return hint.Error().WithContext("layout.cells.json");
                }
                cell.acoustic.reverbHint = hint.Value();
            }

            if (row.Has("thermal") && !row.IsNull("thermal"))
            {
                const Result<JsonValue> thermal = row.RequireObject("thermal");
                if (!thermal)
                {
                    return thermal.Error().WithContext("layout.cells.json");
                }
                const Result<bool> heated = thermal.Value().OptionalBool("heated", false);
                if (!heated)
                {
                    return heated.Error().WithContext("layout.cells.json");
                }
                cell.thermal.heated = heated.Value();
                const Result<util::Id> duct = OptionalId(thermal.Value(), "ductBranch");
                if (!duct)
                {
                    return duct.Error().WithContext("layout.cells.json");
                }
                cell.thermal.ductBranch = duct.Value();
            }

            const auto idList = [](const JsonValue& parent,
                                   std::string_view field,
                                   std::vector<util::Id>& target) -> Result<void>
            {
                if (!parent.Has(field) || parent.IsNull(field))
                {
                    return util::Ok();
                }
                const Result<JsonValue> array = parent.RequireArray(field);
                if (!array)
                {
                    return array.Error();
                }
                const Result<std::vector<JsonValue>> entries = array.Value().Elements();
                if (!entries)
                {
                    return entries.Error();
                }
                for (const JsonValue& entry : entries.Value())
                {
                    const Result<std::string> spelling = entry.AsString();
                    if (!spelling)
                    {
                        return spelling.Error();
                    }
                    target.push_back(util::Intern(spelling.Value()));
                }
                return util::Ok();
            };

            if (const Result<void> groups = idList(row, "lightGroups", cell.lightGroups); !groups)
            {
                return groups.Error().WithContext("layout.cells.json");
            }

            if (row.Has("daylight") && !row.IsNull("daylight"))
            {
                const Result<JsonValue> daylight = row.RequireObject("daylight");
                if (!daylight)
                {
                    return daylight.Error().WithContext("layout.cells.json");
                }
                if (const Result<void> windows =
                        idList(daylight.Value(), "windowIds", cell.daylight.windowIds);
                    !windows)
                {
                    return windows.Error().WithContext("layout.cells.json");
                }
                if (daylight.Value().Has("orientation") && !daylight.Value().IsNull("orientation"))
                {
                    const Result<std::string> compass = daylight.Value().RequireString("orientation");
                    if (!compass)
                    {
                        return compass.Error().WithContext("layout.cells.json");
                    }
                    const Result<Orientation> parsed = ParseOrientation(compass.Value());
                    if (!parsed)
                    {
                        return parsed.Error()
                            .WithContext(row.Path() + "/daylight/orientation")
                            .WithContext("layout.cells.json");
                    }
                    cell.daylight.orientation = parsed.Value();
                }
                const Result<float> exposure = daylight.Value().OptionalFloat("exposure", 0.0F);
                if (!exposure)
                {
                    return exposure.Error().WithContext("layout.cells.json");
                }
                cell.daylight.exposure = exposure.Value();
            }

            if (row.Has("lightmaps") && !row.IsNull("lightmaps"))
            {
                const Result<JsonValue> lightmaps = row.RequireObject("lightmaps");
                if (!lightmaps)
                {
                    return lightmaps.Error().WithContext("layout.cells.json");
                }
                const Result<std::string> shellHash = lightmaps.Value().RequireString("shellHash");
                if (!shellHash)
                {
                    return shellHash.Error().WithContext("layout.cells.json");
                }
                const std::string_view hash = shellHash.Value();
                const bool validHash = hash.size() == 71U && hash.starts_with("sha256:") &&
                                       std::all_of(hash.begin() + 7,
                                                   hash.end(),
                                                   [](char character)
                                                   {
                                                       return (character >= '0' && character <= '9') ||
                                                              (character >= 'a' && character <= 'f');
                                                   });
                if (!validHash)
                {
                    return Err(ErrorCode::InvalidData,
                               "lightmaps.shellHash must be 'sha256:' followed by 64 lower-case "
                               "hexadecimal digits",
                               "layout.cells.json/" + row.Path() + "/lightmaps/shellHash");
                }
                cell.lightmaps.shellHash = shellHash.Value();

                const auto readTexture = [&row](const JsonValue& value,
                                                std::string_view path) -> Result<CellLightmapTexture>
                {
                    CellLightmapTexture texture;
                    const Result<std::string> contentName = value.RequireString("contentName");
                    if (!contentName)
                    {
                        return contentName.Error();
                    }
                    const Result<float> scale = value.RequireFloat("scale");
                    if (!scale)
                    {
                        return scale.Error();
                    }
                    if (contentName.Value().empty())
                    {
                        return Err(ErrorCode::InvalidData,
                                   "a lightmap contentName cannot be empty",
                                   "layout.cells.json/" + row.Path() + "/" + std::string(path));
                    }
                    if (!std::isfinite(scale.Value()) || !(scale.Value() > 0.0F))
                    {
                        return Err(ErrorCode::OutOfRange,
                                   "a lightmap scale must be finite and greater than zero",
                                   "layout.cells.json/" + row.Path() + "/" + std::string(path));
                    }
                    texture.contentName = contentName.Value();
                    texture.scale = scale.Value();
                    return texture;
                };

                if (lightmaps.Value().Has("daylight") && !lightmaps.Value().IsNull("daylight"))
                {
                    const Result<JsonValue> daylight = lightmaps.Value().RequireObject("daylight");
                    if (!daylight)
                    {
                        return daylight.Error().WithContext("layout.cells.json");
                    }
                    const Result<CellLightmapTexture> texture =
                        readTexture(daylight.Value(), "lightmaps/daylight");
                    if (!texture)
                    {
                        return texture.Error().WithContext("layout.cells.json");
                    }
                    cell.lightmaps.daylight = texture.Value();
                }

                const Result<JsonValue> artificial = lightmaps.Value().RequireArray("artificial");
                if (!artificial)
                {
                    return artificial.Error().WithContext("layout.cells.json");
                }
                const Result<std::vector<JsonValue>> lightmapRows = artificial.Value().Elements();
                if (!lightmapRows)
                {
                    return lightmapRows.Error().WithContext("layout.cells.json");
                }
                for (const JsonValue& lightmapRow : lightmapRows.Value())
                {
                    const Result<util::Id> group = RequireId(lightmapRow, "group");
                    if (!group)
                    {
                        return group.Error().WithContext("layout.cells.json");
                    }
                    const Result<CellLightmapTexture> texture =
                        readTexture(lightmapRow, "lightmaps/artificial");
                    if (!texture)
                    {
                        return texture.Error().WithContext("layout.cells.json");
                    }
                    cell.lightmaps.artificial.push_back(CellLightmapGroup{group.Value(), texture.Value()});
                }
            }

            const Result<std::int64_t> bias = row.OptionalInt("lodBias", 0);
            if (!bias)
            {
                return bias.Error().WithContext("layout.cells.json");
            }
            cell.lodBias = static_cast<std::int32_t>(bias.Value());

            const Result<std::string> hint = row.OptionalString("visibilityHint", "opaque");
            if (!hint)
            {
                return hint.Error().WithContext("layout.cells.json");
            }
            const Result<VisibilityHint> parsedHint = ParseVisibilityHint(hint.Value());
            if (!parsedHint)
            {
                return parsedHint.Error()
                    .WithContext(row.Path() + "/visibilityHint")
                    .WithContext("layout.cells.json");
            }
            cell.visibilityHint = parsedHint.Value();

            contents.cells.push_back(std::move(cell));
        }

        return util::Ok();
    }

    namespace
    {
        /// §15.7 rule 4's tolerance: "within 1 cm".
        constexpr float kPlaneTolerance = 0.01F;

        /// @brief A cell's floor and ceiling from rows that are not a `WorldData` yet.
        ///
        /// The same rule as `WorldData::ExtentOf` -- `yOverride` wins, else the level's
        /// `ffl`..`ceiling`, and a rafter-bounded level with no override has no answer. It cannot
        /// call it: `Create` has not run, because the portal check happens while the contents are
        /// still being filled.
        [[nodiscard]] std::optional<Extent> ExtentDuringLoad(const WorldData::Contents& contents,
                                                             const Cell& cell)
        {
            if (cell.yOverride.has_value())
            {
                return cell.yOverride;
            }
            for (const Level& level : contents.levels)
            {
                if (level.id == cell.level)
                {
                    if (!level.ceiling.has_value())
                    {
                        return std::nullopt;
                    }
                    return Extent{level.ffl, *level.ceiling};
                }
            }
            return std::nullopt;
        }

        [[nodiscard]] const Cell* FindCellDuringLoad(const WorldData::Contents& contents, util::Id id)
        {
            for (const Cell& cell : contents.cells)
            {
                if (cell.id == id)
                {
                    return &cell;
                }
            }
            return nullptr;
        }

        /// @brief The spans of the other horizontal axis where @p cell has a face on `axis=value`.
        ///
        /// A cell is a union of boxes, so a wall on one plane can be several disjoint runs.
        /// Returning them all is what accepts a portal that lies in one run of an L-shaped room and
        /// rejects one that lies in the gap between two runs.
        [[nodiscard]] std::vector<std::pair<float, float>>
        BoundaryRuns(const Cell& cell, PlaneAxis axis, float value, float tolerance = kPlaneTolerance)
        {
            std::vector<std::pair<float, float>> runs;
            for (const Footprint& box : cell.boxes)
            {
                if (axis == PlaneAxis::X)
                {
                    if (std::abs(box.minX - value) <= tolerance || std::abs(box.maxX - value) <= tolerance)
                    {
                        runs.emplace_back(box.minZ, box.maxZ);
                    }
                }
                else
                {
                    if (std::abs(box.minZ - value) <= tolerance || std::abs(box.maxZ - value) <= tolerance)
                    {
                        runs.emplace_back(box.minX, box.maxX);
                    }
                }
            }
            return runs;
        }

        /// @brief The thickest wall `layout.levels.json` declares, or 0 when it declares none.
        ///
        /// §15.7 rule 4 needs it because a window is **in** a wall and not on either face of it: a
        /// room stops at the interior face and the yard outside at the exterior one, so the two
        /// cells are `wallExterior` apart with the opening between them.
        [[nodiscard]] float ThickestWall(const Construction& construction) noexcept
        {
            return std::max({construction.wallExterior,
                             construction.wallPartition,
                             construction.wallPlumbing,
                             construction.wallGarage,
                             construction.foundationWall});
        }

        /// @brief The faces of @p cell on @p axis within @p reach of @p value that span `u`.
        [[nodiscard]] std::vector<float>
        FacesNear(const Cell& cell, PlaneAxis axis, float value, float reach, float minU, float maxU)
        {
            std::vector<float> found;
            for (const Footprint& box : cell.boxes)
            {
                const std::array<float, 2> faces = axis == PlaneAxis::X
                                                       ? std::array<float, 2>{box.minX, box.maxX}
                                                       : std::array<float, 2>{box.minZ, box.maxZ};
                const float spanLow = axis == PlaneAxis::X ? box.minZ : box.minX;
                const float spanHigh = axis == PlaneAxis::X ? box.maxZ : box.maxX;
                for (const float face : faces)
                {
                    if (std::abs(face - value) > reach + kPlaneTolerance)
                    {
                        continue;
                    }
                    if (spanLow - kPlaneTolerance <= minU && maxU <= spanHigh + kPlaneTolerance)
                    {
                        found.push_back(face);
                    }
                }
            }
            return found;
        }

        /// @brief True when the portal plane lies in the **wall** between two cells that do not abut.
        ///
        /// Two rooms either side of a partition share a coordinate (§13.1), so "in both cells'
        /// planes within 1 cm" holds for them. A room and the yard outside it do not. The check
        /// stays as strong as it was otherwise: the faces must straddle the portal, each must span
        /// the opening, and they must be no further apart than the thickest declared wall.
        [[nodiscard]] bool
        InTheWall(const WorldData::Contents& contents, const Cell& a, const Cell& b, const Portal& portal)
        {
            const float wall = ThickestWall(contents.construction);
            if (wall <= 0.0F)
            {
                return false;
            }
            const std::vector<float> facesA =
                FacesNear(a, portal.axis, portal.planeValue, wall, portal.minU, portal.maxU);
            const std::vector<float> facesB =
                FacesNear(b, portal.axis, portal.planeValue, wall, portal.minU, portal.maxU);
            for (const float faceA : facesA)
            {
                for (const float faceB : facesB)
                {
                    if (std::abs(faceA - faceB) > wall + kPlaneTolerance)
                    {
                        continue;
                    }
                    const float low = std::min(faceA, faceB);
                    const float high = std::max(faceA, faceB);
                    if (low - kPlaneTolerance <= portal.planeValue &&
                        portal.planeValue <= high + kPlaneTolerance)
                    {
                        return true;
                    }
                }
            }
            return false;
        }

        /// @brief §15.7 rule 4 for a portal between a sub-cell and the cell it nests in.
        ///
        /// A portal into a container or a mezzanine is not in a shared wall: the sub-cell is inside
        /// its parent, so the opening is in the sub-cell's OWN face -- the fridge door, the chest
        /// lid, the loft hatch -- and the parent has no face there at all.
        [[nodiscard]] Result<void> CheckNestedPortal(const WorldData::Contents& contents,
                                                     const Portal& portal,
                                                     const Cell& child,
                                                     const Cell& parent,
                                                     const std::string& where)
        {
            if (portal.axis == PlaneAxis::Y)
            {
                const std::optional<Extent> extent = ExtentDuringLoad(contents, child);
                if (extent.has_value() &&
                    std::min(std::abs(extent->floorY - portal.planeValue),
                             std::abs(extent->ceilingY - portal.planeValue)) > kPlaneTolerance)
                {
                    return Err(ErrorCode::InvalidData,
                               "y = " + std::to_string(portal.planeValue) +
                                   " is neither the floor nor the lid of cell " + Name(child.id),
                               where + "/plane");
                }
                return util::Ok();
            }

            const std::vector<std::pair<float, float>> runs =
                BoundaryRuns(child, portal.axis, portal.planeValue);
            if (runs.empty())
            {
                return Err(ErrorCode::InvalidData,
                           "cell " + Name(child.id) + " nests in " + Name(parent.id) +
                               ", so the opening is in ITS face -- and it has none on " +
                               std::string(ToStringView(portal.axis)) + " = " +
                               std::to_string(portal.planeValue) + " within 1 cm",
                           where + "/plane");
            }
            const bool spans = std::any_of(runs.begin(),
                                           runs.end(),
                                           [&portal](const std::pair<float, float>& run)
                                           {
                                               return run.first - kPlaneTolerance <= portal.minU &&
                                                      portal.maxU <= run.second + kPlaneTolerance;
                                           });
            if (!spans)
            {
                return Err(ErrorCode::InvalidData,
                           "the rectangle is not inside any run of cell " + Name(child.id) +
                               "'s face on that plane",
                           where + "/rect/u");
            }
            const std::optional<Extent> extent = ExtentDuringLoad(contents, child);
            if (extent.has_value() && (portal.minV < extent->floorY - kPlaneTolerance ||
                                       portal.maxV > extent->ceilingY + kPlaneTolerance))
            {
                return Err(ErrorCode::InvalidData,
                           "the opening is taller than " + Name(child.id),
                           where + "/rect/v");
            }
            return util::Ok();
        }

        /// @brief §15.7 rule 4 for one side of one portal.
        [[nodiscard]] Result<void> CheckPortalSide(const WorldData::Contents& contents,
                                                   const Portal& portal,
                                                   util::Id cellId,
                                                   std::string_view side,
                                                   const std::string& where,
                                                   float reach)
        {
            const Cell* cell = FindCellDuringLoad(contents, cellId);
            if (cell == nullptr)
            {
                // Not this check's business: a dangling cell reference is rule 6, and reporting it
                // twice with two different messages is worse than reporting it once.
                return util::Ok();
            }
            const std::optional<Extent> extent = ExtentDuringLoad(contents, *cell);

            if (portal.axis == PlaneAxis::Y)
            {
                // A horizontal portal -- a stair well or a hatch. `u` is world X and `v` is world Z,
                // the rectangle must lie inside the footprint, and the plane must be a boundary the
                // two cells actually share: one's ceiling is the other's floor.
                const bool inside =
                    std::any_of(cell->boxes.begin(),
                                cell->boxes.end(),
                                [&portal](const Footprint& box)
                                {
                                    return box.Contains(portal.minU, portal.minV, kPlaneTolerance) &&
                                           box.Contains(portal.maxU, portal.maxV, kPlaneTolerance);
                                });
                if (!inside)
                {
                    return Err(ErrorCode::InvalidData,
                               "the opening is not inside cell " + Name(cellId) + "'s footprint",
                               where + "/rect");
                }
                if (extent.has_value() &&
                    std::min(std::abs(extent->floorY - portal.planeValue),
                             std::abs(extent->ceilingY - portal.planeValue)) > kPlaneTolerance)
                {
                    return Err(ErrorCode::InvalidData,
                               "y = " + std::to_string(portal.planeValue) +
                                   " is neither the floor nor the ceiling of cell " + Name(cellId),
                               where + "/plane");
                }
                return util::Ok();
            }

            const std::vector<std::pair<float, float>> runs =
                BoundaryRuns(*cell, portal.axis, portal.planeValue, reach);
            if (runs.empty())
            {
                return Err(ErrorCode::InvalidData,
                           "cell " + Name(cellId) + " (" + std::string(side) + ") has no face on " +
                               std::string(ToStringView(portal.axis)) + " = " +
                               std::to_string(portal.planeValue) +
                               " within 1 cm, and no face within a wall of it on the far side either",
                           where + "/plane");
            }
            const bool spans = std::any_of(runs.begin(),
                                           runs.end(),
                                           [&portal](const std::pair<float, float>& run)
                                           {
                                               return run.first - kPlaneTolerance <= portal.minU &&
                                                      portal.maxU <= run.second + kPlaneTolerance;
                                           });
            if (!spans)
            {
                return Err(ErrorCode::InvalidData,
                           "the rectangle is not inside any run of cell " + Name(cellId) +
                               "'s face on that plane",
                           where + "/rect/u");
            }
            if (extent.has_value() && (portal.minV < extent->floorY - kPlaneTolerance ||
                                       portal.maxV > extent->ceilingY + kPlaneTolerance))
            {
                return Err(ErrorCode::InvalidData,
                           "the rectangle's height is outside cell " + Name(cellId) + "'s vertical extent",
                           where + "/rect/v");
            }
            return util::Ok();
        }
    } // namespace

    Result<void> WorldLoader::LoadPortals(std::string_view directory, WorldData::Contents& contents)
    {
        std::int32_t version = 0;
        const Result<JsonDocument> document = Open(directory, "layout.portals.json", "portals", version);
        if (!document)
        {
            return document.Error();
        }

        const Result<JsonValue> portals = document.Value().Root().RequireArray("portals");
        if (!portals)
        {
            return portals.Error().WithContext("layout.portals.json");
        }
        const Result<std::vector<JsonValue>> rows = portals.Value().Elements();
        if (!rows)
        {
            return rows.Error().WithContext("layout.portals.json");
        }

        for (const JsonValue& row : rows.Value())
        {
            Portal portal;
            const Result<util::Id> id = RequireId(row, "id");
            if (!id)
            {
                return id.Error().WithContext("layout.portals.json");
            }
            portal.id = id.Value();

            const Result<util::Id> cellA = RequireId(row, "cellA");
            if (!cellA)
            {
                return cellA.Error().WithContext("layout.portals.json");
            }
            portal.cellA = cellA.Value();
            const Result<util::Id> cellB = RequireId(row, "cellB");
            if (!cellB)
            {
                return cellB.Error().WithContext("layout.portals.json");
            }
            portal.cellB = cellB.Value();
            if (portal.cellA == portal.cellB)
            {
                return Err(ErrorCode::InvalidData,
                           "a portal joins two cells and both sides name " + Name(portal.cellA) +
                               "; a hole from a room into itself is not a portal",
                           "layout.portals.json/" + row.Path() + "/cellB");
            }

            const Result<JsonValue> plane = row.RequireObject("plane");
            if (!plane)
            {
                return plane.Error().WithContext("layout.portals.json");
            }
            const Result<std::string> axis = plane.Value().RequireString("axis");
            if (!axis)
            {
                return axis.Error().WithContext("layout.portals.json");
            }
            const Result<PlaneAxis> parsedAxis = ParsePlaneAxis(axis.Value());
            if (!parsedAxis)
            {
                return parsedAxis.Error()
                    .WithContext(row.Path() + "/plane/axis")
                    .WithContext("layout.portals.json");
            }
            portal.axis = parsedAxis.Value();
            const Result<float> value = plane.Value().RequireFloat("value");
            if (!value)
            {
                return value.Error().WithContext("layout.portals.json");
            }
            portal.planeValue = value.Value();

            const Result<JsonValue> rect = row.RequireObject("rect");
            if (!rect)
            {
                return rect.Error().WithContext("layout.portals.json");
            }
            const auto range = [&rect, &row](std::string_view field, float& low, float& high) -> Result<void>
            {
                const Result<JsonValue> array = rect.Value().RequireArray(field);
                if (!array)
                {
                    return array.Error();
                }
                const Result<std::vector<JsonValue>> parts = array.Value().Elements();
                if (!parts)
                {
                    return parts.Error();
                }
                if (parts.Value().size() != 2U)
                {
                    return Err(ErrorCode::InvalidData,
                               "a portal range is [min, max]; this has " +
                                   std::to_string(parts.Value().size()) + " element(s)",
                               row.Path() + "/rect/" + std::string(field));
                }
                const Result<float> first = parts.Value()[0].AsFloat();
                if (!first)
                {
                    return first.Error();
                }
                const Result<float> second = parts.Value()[1].AsFloat();
                if (!second)
                {
                    return second.Error();
                }
                if (!(first.Value() < second.Value()))
                {
                    return Err(ErrorCode::InvalidData,
                               "a portal range is [min, max] with min < max; this is [" +
                                   std::to_string(first.Value()) + ", " + std::to_string(second.Value()) +
                                   "]",
                               row.Path() + "/rect/" + std::string(field));
                }
                low = first.Value();
                high = second.Value();
                return util::Ok();
            };
            if (const Result<void> u = range("u", portal.minU, portal.maxU); !u)
            {
                return u.Error().WithContext("layout.portals.json");
            }
            if (const Result<void> v = range("v", portal.minV, portal.maxV); !v)
            {
                return v.Error().WithContext("layout.portals.json");
            }

            const Result<std::string> kind = row.RequireString("kind");
            if (!kind)
            {
                return kind.Error().WithContext("layout.portals.json");
            }
            const Result<PortalKind> parsedKind = ParsePortalKind(kind.Value());
            if (!parsedKind)
            {
                return parsedKind.Error()
                    .WithContext(row.Path() + "/kind")
                    .WithContext("layout.portals.json");
            }
            portal.kind = parsedKind.Value();

            const Result<util::Id> aperture = OptionalId(row, "aperture");
            if (!aperture)
            {
                return aperture.Error().WithContext("layout.portals.json");
            }
            portal.aperture = aperture.Value();

            const Result<std::string> opacity = row.OptionalString("opacity", "open");
            if (!opacity)
            {
                return opacity.Error().WithContext("layout.portals.json");
            }
            const Result<PortalOpacity> parsedOpacity = ParsePortalOpacity(opacity.Value());
            if (!parsedOpacity)
            {
                return parsedOpacity.Error()
                    .WithContext(row.Path() + "/opacity")
                    .WithContext("layout.portals.json");
            }
            portal.opacity = parsedOpacity.Value();

            // `maxDepth` is a per-portal traversal cap and `null` means "no cap". Read as 0 it
            // would mean the opposite -- see through nothing at all -- which is the difference
            // between a glazed door and a bricked-up one.
            if (row.Has("maxDepth") && !row.IsNull("maxDepth"))
            {
                const Result<std::int64_t> depth = row.RequireInt("maxDepth");
                if (!depth)
                {
                    return depth.Error().WithContext("layout.portals.json");
                }
                if (depth.Value() < 0)
                {
                    return Err(ErrorCode::OutOfRange,
                               "a traversal cap is not negative; this is " + std::to_string(depth.Value()),
                               "layout.portals.json/" + row.Path() + "/maxDepth");
                }
                portal.maxDepth = static_cast<std::int32_t>(depth.Value());
            }

            if (row.Has("soundLoss") && !row.IsNull("soundLoss"))
            {
                const Result<JsonValue> loss = row.RequireObject("soundLoss");
                if (!loss)
                {
                    return loss.Error().WithContext("layout.portals.json");
                }
                const Result<float> open = loss.Value().OptionalFloat("open", 0.0F);
                if (!open)
                {
                    return open.Error().WithContext("layout.portals.json");
                }
                const Result<float> closed = loss.Value().OptionalFloat("closed", 0.0F);
                if (!closed)
                {
                    return closed.Error().WithContext("layout.portals.json");
                }
                portal.soundLossOpen = open.Value();
                portal.soundLossClosed = closed.Value();
            }

            const Result<bool> crouch = row.OptionalBool("crouch", false);
            if (!crouch)
            {
                return crouch.Error().WithContext("layout.portals.json");
            }
            portal.crouch = crouch.Value();

            const std::string where = row.Path();
            const Cell* sideA = FindCellDuringLoad(contents, portal.cellA);
            const Cell* sideB = FindCellDuringLoad(contents, portal.cellB);

            // A portal into a sub-cell is checked against the CHILD, because the parent has no
            // face where the opening is; and a portal in a wall is checked with the wall's own
            // tolerance, because the two cells there are a wall apart on purpose. Both are decided
            // for the pair: they are statements about the two cells together.
            const Cell* child = nullptr;
            const Cell* parent = nullptr;
            if (sideA != nullptr && sideB != nullptr)
            {
                if (sideA->parent == sideB->id)
                {
                    child = sideA;
                    parent = sideB;
                }
                else if (sideB->parent == sideA->id)
                {
                    child = sideB;
                    parent = sideA;
                }
            }
            if (child != nullptr)
            {
                if (const Result<void> checked = CheckNestedPortal(contents, portal, *child, *parent, where);
                    !checked)
                {
                    return checked.Error().WithContext("layout.portals.json");
                }
                contents.portals.push_back(std::move(portal));
                continue;
            }

            // A wall PLUS the same 1 cm of slack. The slack is not cosmetic: `-14.0 - -14.3` is
            // 0.30000001 in floating point, so a bare `<= 0.30` rejects every window in a 0.30 m
            // wall.
            const bool walled =
                sideA != nullptr && sideB != nullptr && InTheWall(contents, *sideA, *sideB, portal);
            const float reach =
                walled ? ThickestWall(contents.construction) + kPlaneTolerance : kPlaneTolerance;

            for (const auto& [cellId, side] : std::initializer_list<std::pair<util::Id, std::string_view>>{
                     {portal.cellA, "cellA"}, {portal.cellB, "cellB"}})
            {
                if (const Result<void> checked =
                        CheckPortalSide(contents, portal, cellId, side, where, reach);
                    !checked)
                {
                    return checked.Error().WithContext("layout.portals.json");
                }
            }

            contents.portals.push_back(std::move(portal));
        }

        return util::Ok();
    }

    Result<void> WorldLoader::LoadOpenings(std::string_view directory, WorldData::Contents& contents)
    {
        std::int32_t version = 0;
        const Result<JsonDocument> document = Open(directory, "layout.openings.json", "openings", version);
        if (!document)
        {
            return document.Error();
        }

        const Result<JsonValue> openings = document.Value().Root().RequireArray("openings");
        if (!openings)
        {
            return openings.Error().WithContext("layout.openings.json");
        }
        const Result<std::vector<JsonValue>> rows = openings.Value().Elements();
        if (!rows)
        {
            return rows.Error().WithContext("layout.openings.json");
        }

        for (const JsonValue& row : rows.Value())
        {
            Opening opening;
            const Result<util::Id> id = RequireId(row, "id");
            if (!id)
            {
                return id.Error().WithContext("layout.openings.json");
            }
            opening.id = id.Value();

            const Result<std::string> kind = row.RequireString("kind");
            if (!kind)
            {
                return kind.Error().WithContext("layout.openings.json");
            }
            const Result<OpeningKind> parsedKind = ParseOpeningKind(kind.Value());
            if (!parsedKind)
            {
                return parsedKind.Error()
                    .WithContext(row.Path() + "/kind")
                    .WithContext("layout.openings.json");
            }
            opening.kind = parsedKind.Value();

            const Result<util::Id> portal = RequireId(row, "portal");
            if (!portal)
            {
                return portal.Error().WithContext("layout.openings.json");
            }
            opening.portal = portal.Value();

            const Result<util::Id> type = OptionalId(row, "type");
            if (!type)
            {
                return type.Error().WithContext("layout.openings.json");
            }
            opening.type = type.Value();

            // The leaf is what swings, and all three of its numbers are load-bearing: `width` and
            // `height` are §70.5's realism check, and `thickness` is what tells a closed door from
            // a hole with a picture of a door in it.
            const Result<JsonValue> leaf = row.RequireObject("leaf");
            if (!leaf)
            {
                return leaf.Error().WithContext("layout.openings.json");
            }
            for (const auto& [field, target] : std::initializer_list<std::pair<std::string_view, float*>>{
                     {"width", &opening.leaf.width},
                     {"height", &opening.leaf.height},
                     {"thickness", &opening.leaf.thickness}})
            {
                const Result<float> value = field == std::string_view("thickness")
                                                ? leaf.Value().OptionalFloat(field, 0.0F)
                                                : leaf.Value().RequireFloat(field);
                if (!value)
                {
                    return value.Error().WithContext("layout.openings.json");
                }
                if (value.Value() <= 0.0F && field != std::string_view("thickness"))
                {
                    return Err(ErrorCode::InvalidData,
                               "a leaf " + std::string(field) + " is positive; this is " +
                                   std::to_string(value.Value()),
                               "layout.openings.json/" + row.Path() + "/leaf/" + std::string(field));
                }
                *target = value.Value();
            }

            // `hinge` is `null` on a slider and on a window that does not swing, and "not hinged"
            // is a real state rather than "hinged left".
            if (row.Has("hinge") && !row.IsNull("hinge"))
            {
                const Result<std::string> hinge = row.RequireString("hinge");
                if (!hinge)
                {
                    return hinge.Error().WithContext("layout.openings.json");
                }
                const Result<HingeSide> parsedHinge = ParseHingeSide(hinge.Value());
                if (!parsedHinge)
                {
                    return parsedHinge.Error()
                        .WithContext(row.Path() + "/hinge")
                        .WithContext("layout.openings.json");
                }
                opening.hinge = parsedHinge.Value();
            }

            // `swing` names a cell, so it is read as one. Null is a leaf that does not swing at
            // all -- a slider, a sectional door, a fixed light -- and is not the same as a leaf
            // that swings somewhere unstated.
            const Result<util::Id> swing = OptionalId(row, "swing");
            if (!swing)
            {
                return swing.Error().WithContext("layout.openings.json");
            }
            opening.swing = swing.Value();

            const Result<float> angle = row.OptionalFloat("maxAngleDeg", 0.0F);
            if (!angle)
            {
                return angle.Error().WithContext("layout.openings.json");
            }
            if (angle.Value() < 0.0F || angle.Value() > 180.0F)
            {
                return Err(ErrorCode::OutOfRange,
                           "a leaf opens 0..180 degrees; this is " + std::to_string(angle.Value()),
                           "layout.openings.json/" + row.Path() + "/maxAngleDeg");
            }
            opening.maxAngleDeg = angle.Value();

            if (row.Has("frame") && !row.IsNull("frame"))
            {
                const Result<JsonValue> frame = row.RequireObject("frame");
                if (!frame)
                {
                    return frame.Error().WithContext("layout.openings.json");
                }
                const Result<util::Id> asset = OptionalId(frame.Value(), "asset");
                if (!asset)
                {
                    return asset.Error().WithContext("layout.openings.json");
                }
                opening.frameAsset = asset.Value();
                const Result<float> casing = frame.Value().OptionalFloat("casing", 0.0F);
                if (!casing)
                {
                    return casing.Error().WithContext("layout.openings.json");
                }
                opening.casing = casing.Value();
            }

            for (const auto& [field, target] : std::initializer_list<std::pair<std::string_view, util::Id*>>{
                     {"asset", &opening.asset}, {"material", &opening.material}})
            {
                const Result<util::Id> value = OptionalId(row, field);
                if (!value)
                {
                    return value.Error().WithContext("layout.openings.json");
                }
                *target = value.Value();
            }

            // `solid` is not decoration: §64.3 gives a hollow-core door 16 dB closed and a solid
            // one 24, and the audio solve reads this field and not the asset.
            const Result<bool> solid = row.OptionalBool("solid", false);
            if (!solid)
            {
                return solid.Error().WithContext("layout.openings.json");
            }
            opening.solid = solid.Value();

            const Result<bool> lockable = row.OptionalBool("lockable", false);
            if (!lockable)
            {
                return lockable.Error().WithContext("layout.openings.json");
            }
            opening.lockable = lockable.Value();

            contents.openings.push_back(std::move(opening));
        }

        return util::Ok();
    }

    Result<void> WorldLoader::LoadStairs(std::string_view directory, WorldData::Contents& contents)
    {
        std::int32_t version = 0;
        const Result<JsonDocument> document = Open(directory, "layout.stairs.json", "stairs", version);
        if (!document)
        {
            return document.Error();
        }

        const Result<JsonValue> flights = document.Value().Root().RequireArray("flights");
        if (!flights)
        {
            return flights.Error().WithContext("layout.stairs.json");
        }
        const Result<std::vector<JsonValue>> rows = flights.Value().Elements();
        if (!rows)
        {
            return rows.Error().WithContext("layout.stairs.json");
        }

        for (const JsonValue& row : rows.Value())
        {
            StairFlight flight;
            const Result<util::Id> id = RequireId(row, "id");
            if (!id)
            {
                return id.Error().WithContext("layout.stairs.json");
            }
            flight.id = id.Value();

            const Result<util::Id> fromCell = RequireId(row, "fromCell");
            if (!fromCell)
            {
                return fromCell.Error().WithContext("layout.stairs.json");
            }
            flight.fromCell = fromCell.Value();
            const Result<util::Id> toCell = RequireId(row, "toCell");
            if (!toCell)
            {
                return toCell.Error().WithContext("layout.stairs.json");
            }
            flight.toCell = toCell.Value();

            const Result<std::int64_t> risers = row.RequireInt("risers");
            if (!risers)
            {
                return risers.Error().WithContext("layout.stairs.json");
            }
            if (risers.Value() < 1)
            {
                return Err(ErrorCode::InvalidData,
                           "a flight has at least one riser; this has " + std::to_string(risers.Value()),
                           "layout.stairs.json/" + row.Path() + "/risers");
            }
            flight.risers = static_cast<std::int32_t>(risers.Value());

            for (const auto& [field, target] : std::initializer_list<std::pair<std::string_view, float*>>{
                     {"rise", &flight.rise}, {"going", &flight.going}, {"width", &flight.width}})
            {
                const Result<float> value = row.RequireFloat(field);
                if (!value)
                {
                    return value.Error().WithContext("layout.stairs.json");
                }
                if (value.Value() <= 0.0F)
                {
                    return Err(ErrorCode::InvalidData,
                               "a stair " + std::string(field) + " is positive; this is " +
                                   std::to_string(value.Value()),
                               "layout.stairs.json/" + row.Path() + "/" + std::string(field));
                }
                *target = value.Value();
            }

            if (row.Has("landings") && !row.IsNull("landings"))
            {
                const Result<JsonValue> landings = row.RequireArray("landings");
                if (!landings)
                {
                    return landings.Error().WithContext("layout.stairs.json");
                }
                const Result<std::vector<JsonValue>> landingRows = landings.Value().Elements();
                if (!landingRows)
                {
                    return landingRows.Error().WithContext("layout.stairs.json");
                }
                for (const JsonValue& landingRow : landingRows.Value())
                {
                    const Result<std::int64_t> at = landingRow.RequireInt("at");
                    if (!at)
                    {
                        return at.Error().WithContext("layout.stairs.json");
                    }
                    if (at.Value() < 0 || at.Value() > flight.risers)
                    {
                        return Err(ErrorCode::OutOfRange,
                                   "a landing sits at a riser of this flight, 0.." +
                                       std::to_string(flight.risers) + "; this is at " +
                                       std::to_string(at.Value()),
                                   "layout.stairs.json/" + landingRow.Path() + "/at");
                    }
                    const Result<float> depth = landingRow.RequireFloat("depth");
                    if (!depth)
                    {
                        return depth.Error().WithContext("layout.stairs.json");
                    }
                    if (depth.Value() <= 0.0F)
                    {
                        return Err(ErrorCode::InvalidData,
                                   "a landing has depth; this is " + std::to_string(depth.Value()),
                                   "layout.stairs.json/" + landingRow.Path() + "/depth");
                    }
                    flight.landings.push_back(Landing{static_cast<std::int32_t>(at.Value()), depth.Value()});
                }
            }

            // `collisionRamp` defaults to TRUE, and the default is the one `build_collision.py`
            // uses. Both branches are real: a ramp is a closed wedge per run, and the alternative
            // is one box per step. Defaulting the other way would silently give every flight in
            // the house a hundred boxes where it asked for two.
            const Result<bool> ramp = row.OptionalBool("collisionRamp", true);
            if (!ramp)
            {
                return ramp.Error().WithContext("layout.stairs.json");
            }
            flight.collisionRamp = ramp.Value();

            if (row.Has("surface") && !row.IsNull("surface"))
            {
                const Result<std::string> surface = row.RequireString("surface");
                if (!surface)
                {
                    return surface.Error().WithContext("layout.stairs.json");
                }
                flight.surface = surface.Value();
            }

            // `fromY`/`toY` are how a flight between two cells on ONE level says what it climbs;
            // §15.7 rule 8 has nothing else to check it against there. Absent is a real state and
            // not zero -- a flight between storeys declares neither, and reading a missing field
            // as 0.00 would make the porch steps climb from the basement.
            for (const auto& [field, target] :
                 std::initializer_list<std::pair<std::string_view, std::optional<float>*>>{
                     {"fromY", &flight.fromY}, {"toY", &flight.toY}})
            {
                if (row.Has(field) && !row.IsNull(field))
                {
                    const Result<float> value = row.RequireFloat(field);
                    if (!value)
                    {
                        return value.Error().WithContext("layout.stairs.json");
                    }
                    *target = value.Value();
                }
            }
            if (flight.fromY.has_value() != flight.toY.has_value())
            {
                return Err(ErrorCode::InvalidData,
                           "a flight declares both fromY and toY or neither; this one declares "
                           "only one, and one end of a climb is not a climb",
                           "layout.stairs.json/" + row.Path());
            }

            contents.stairs.push_back(std::move(flight));
        }

        return util::Ok();
    }

    Result<void> WorldLoader::LoadLights(std::string_view directory, WorldData::Contents& contents)
    {
        std::int32_t version = 0;
        const Result<JsonDocument> document = Open(directory, "layout.lights.json", "lights", version);
        if (!document)
        {
            return document.Error();
        }

        const Result<JsonValue> lights = document.Value().Root().RequireArray("lights");
        if (!lights)
        {
            return lights.Error().WithContext("layout.lights.json");
        }
        const Result<std::vector<JsonValue>> rows = lights.Value().Elements();
        if (!rows)
        {
            return rows.Error().WithContext("layout.lights.json");
        }

        for (const JsonValue& row : rows.Value())
        {
            Light light;
            for (const auto& [field, target] : std::initializer_list<std::pair<std::string_view, util::Id*>>{
                     {"id", &light.id}, {"cell", &light.cell}, {"group", &light.group}})
            {
                const Result<util::Id> value = RequireId(row, field);
                if (!value)
                {
                    return value.Error().WithContext("layout.lights.json");
                }
                *target = value.Value();
            }

            const Result<std::string> type = row.RequireString("type");
            if (!type)
            {
                return type.Error().WithContext("layout.lights.json");
            }
            const Result<LightType> parsedType = ParseLightType(type.Value());
            if (!parsedType)
            {
                return parsedType.Error().WithContext(row.Path() + "/type").WithContext("layout.lights.json");
            }
            light.type = parsedType.Value();

            const Result<Microsoft::Xna::Framework::Vector3> position = row.RequireVector3("position");
            if (!position)
            {
                return position.Error().WithContext("layout.lights.json");
            }
            light.position = position.Value();

            if (row.Has("direction") && !row.IsNull("direction"))
            {
                const Result<Microsoft::Xna::Framework::Vector3> direction = row.RequireVector3("direction");
                if (!direction)
                {
                    return direction.Error().WithContext("layout.lights.json");
                }
                light.direction = direction.Value();
            }
            // A spot or a directional light with no direction points nowhere, and `Vector3::Zero`
            // normalises to a NaN. That is a black room at run time and a load error here.
            if (light.type == LightType::Spot || light.type == LightType::Directional)
            {
                const float lengthSquared = light.direction.X * light.direction.X +
                                            light.direction.Y * light.direction.Y +
                                            light.direction.Z * light.direction.Z;
                if (lengthSquared <= 0.0F)
                {
                    return Err(ErrorCode::InvalidData,
                               "a " + std::string(ToStringView(light.type)) +
                                   " light needs a direction; this one has none",
                               "layout.lights.json/" + row.Path() + "/direction");
                }
            }

            // §70.5 does not give a colour-temperature range, but the physical one is not open:
            // 1 000 K is a candle and 12 000 K is a clear north sky, and a value outside that is a
            // typo -- a missing zero on 2 700 puts a kitchen under a match.
            const Result<float> colorK = row.OptionalFloat("colorK", 2700.0F);
            if (!colorK)
            {
                return colorK.Error().WithContext("layout.lights.json");
            }
            if (colorK.Value() < 1000.0F || colorK.Value() > 12000.0F)
            {
                return Err(ErrorCode::OutOfRange,
                           "a colour temperature is 1000..12000 K; this is " + std::to_string(colorK.Value()),
                           "layout.lights.json/" + row.Path() + "/colorK");
            }
            light.colorK = colorK.Value();

            for (const auto& [field, target] : std::initializer_list<std::pair<std::string_view, float*>>{
                     {"intensityLm", &light.intensityLm}, {"range", &light.range}})
            {
                const Result<float> value = row.OptionalFloat(field, 0.0F);
                if (!value)
                {
                    return value.Error().WithContext("layout.lights.json");
                }
                if (value.Value() < 0.0F)
                {
                    return Err(ErrorCode::OutOfRange,
                               "a light " + std::string(field) + " is not negative; this is " +
                                   std::to_string(value.Value()),
                               "layout.lights.json/" + row.Path() + "/" + std::string(field));
                }
                *target = value.Value();
            }

            for (const auto& [field, target] : std::initializer_list<std::pair<std::string_view, float*>>{
                     {"coneInnerDeg", &light.coneInnerDeg}, {"coneOuterDeg", &light.coneOuterDeg}})
            {
                const Result<float> value = row.OptionalFloat(field, 0.0F);
                if (!value)
                {
                    return value.Error().WithContext("layout.lights.json");
                }
                if (value.Value() < 0.0F || value.Value() > 180.0F)
                {
                    return Err(ErrorCode::OutOfRange,
                               "a cone angle is 0..180 degrees; this is " + std::to_string(value.Value()),
                               "layout.lights.json/" + row.Path() + "/" + std::string(field));
                }
                *target = value.Value();
            }
            // Inner inside outer. The other way round the falloff runs backwards and the spot has
            // a dark centre, which reads as a shader bug and is a data bug.
            if (light.coneInnerDeg > light.coneOuterDeg)
            {
                return Err(ErrorCode::InvalidData,
                           "the inner cone is inside the outer one; this is " +
                               std::to_string(light.coneInnerDeg) + " inside " +
                               std::to_string(light.coneOuterDeg),
                           "layout.lights.json/" + row.Path() + "/coneInnerDeg");
            }

            const Result<util::Id> fixture = OptionalId(row, "fixtureProp");
            if (!fixture)
            {
                return fixture.Error().WithContext("layout.lights.json");
            }
            light.fixtureProp = fixture.Value();

            if (row.Has("emissiveMaterialSlot") && !row.IsNull("emissiveMaterialSlot"))
            {
                const Result<std::string> slot = row.RequireString("emissiveMaterialSlot");
                if (!slot)
                {
                    return slot.Error().WithContext("layout.lights.json");
                }
                light.emissiveMaterialSlot = slot.Value();
            }

            // `bakedIntoLightmap` and `castsBlobShadow` are independent, and the file says so: a
            // baked light still needs a blob shadow for the dynamic objects the bake never saw.
            for (const auto& [field, target] : std::initializer_list<std::pair<std::string_view, bool*>>{
                     {"castsBlobShadow", &light.castsBlobShadow},
                     {"bakedIntoLightmap", &light.bakedIntoLightmap},
                     {"defaultOn", &light.defaultOn}})
            {
                const Result<bool> value = row.OptionalBool(field, false);
                if (!value)
                {
                    return value.Error().WithContext("layout.lights.json");
                }
                *target = value.Value();
            }

            contents.lights.push_back(std::move(light));
        }

        return util::Ok();
    }

    Result<void> WorldLoader::LoadProps(std::string_view directory, WorldData::Contents& contents)
    {
        std::int32_t version = 0;
        const Result<JsonDocument> document = Open(directory, "layout.props.json", "props", version);
        if (!document)
        {
            return document.Error();
        }

        const Result<JsonValue> props = document.Value().Root().RequireArray("props");
        if (!props)
        {
            return props.Error().WithContext("layout.props.json");
        }
        const Result<std::vector<JsonValue>> rows = props.Value().Elements();
        if (!rows)
        {
            return rows.Error().WithContext("layout.props.json");
        }

        for (const JsonValue& row : rows.Value())
        {
            Prop prop;
            for (const auto& [field, target] : std::initializer_list<std::pair<std::string_view, util::Id*>>{
                     {"id", &prop.id}, {"asset", &prop.asset}, {"cell", &prop.cell}})
            {
                const Result<util::Id> value = RequireId(row, field);
                if (!value)
                {
                    return value.Error().WithContext("layout.props.json");
                }
                *target = value.Value();
            }

            const Result<Microsoft::Xna::Framework::Vector3> position = row.RequireVector3("position");
            if (!position)
            {
                return position.Error().WithContext("layout.props.json");
            }
            prop.position = position.Value();

            const Result<float> yaw = row.OptionalFloat("yawDeg", 0.0F);
            if (!yaw)
            {
                return yaw.Error().WithContext("layout.props.json");
            }
            prop.yawDeg = yaw.Value();

            const Result<float> scale = row.OptionalFloat("scale", 1.0F);
            if (!scale)
            {
                return scale.Error().WithContext("layout.props.json");
            }
            if (scale.Value() <= 0.0F)
            {
                return Err(ErrorCode::InvalidData,
                           "a scale is positive; this is " + std::to_string(scale.Value()) +
                               ". Zero collapses the prop to a point and a negative scale turns "
                               "it inside out, which reads as a broken model",
                           "layout.props.json/" + row.Path() + "/scale");
            }
            prop.scale = scale.Value();

            // `static` defaults to TRUE, which is what §17.4's batching assumes and what the file's
            // prose says: a prop that never moves is batched offline, and a row has to SAY `false`
            // to become a `DynamicInstance`. The two failure modes are not symmetric -- defaulting
            // to dynamic would silently un-batch the whole house, a regression the budget report
            // would show as a draw-call number and nobody would trace to a default.
            const Result<bool> isStatic = row.OptionalBool("static", true);
            if (!isStatic)
            {
                return isStatic.Error().WithContext("layout.props.json");
            }
            prop.isStatic = isStatic.Value();

            const Result<std::string> collision = row.OptionalString("collision", "proxy");
            if (!collision)
            {
                return collision.Error().WithContext("layout.props.json");
            }
            const Result<PropCollision> parsedCollision = ParsePropCollision(collision.Value());
            if (!parsedCollision)
            {
                return parsedCollision.Error()
                    .WithContext(row.Path() + "/collision")
                    .WithContext("layout.props.json");
            }
            prop.collision = parsedCollision.Value();

            for (const auto& [field, target] : std::initializer_list<std::pair<std::string_view, util::Id*>>{
                     {"lodGroup", &prop.lodGroup},
                     {"material", &prop.material},
                     {"interactable", &prop.interactable},
                     {"plumbing", &prop.plumbing}})
            {
                const Result<util::Id> value = OptionalId(row, field);
                if (!value)
                {
                    return value.Error().WithContext("layout.props.json");
                }
                *target = value.Value();
            }

            contents.props.push_back(std::move(prop));
        }

        return util::Ok();
    }

    namespace
    {
        /// @brief `["dog", "cat"]` as a `Species` mask.
        ///
        /// An absent list means **both**, which is §61's answer for most of the graph: a corridor
        /// edge no animal is excluded from is an edge both animals may walk. An empty list is not
        /// the same thing and is refused -- a row that says "for nobody" is a row that does
        /// nothing, and it is far more likely to be a mistake than an intention.
        [[nodiscard]] Result<Species> ReadSpecies(const JsonValue& row, std::string_view field)
        {
            if (!row.Has(field) || row.IsNull(field))
            {
                return Species::Both;
            }
            const Result<JsonValue> array = row.RequireArray(field);
            if (!array)
            {
                return array.Error();
            }
            const Result<std::vector<JsonValue>> entries = array.Value().Elements();
            if (!entries)
            {
                return entries.Error();
            }
            if (entries.Value().empty())
            {
                return Err(ErrorCode::InvalidData,
                           "a species list names at least one animal; an empty list is a row that "
                           "does nothing",
                           row.Path() + "/" + std::string(field));
            }
            Species species = Species::None;
            for (const JsonValue& entry : entries.Value())
            {
                const Result<std::string> name = entry.AsString();
                if (!name)
                {
                    return name.Error();
                }
                const Result<Species> parsed = ParseSpecies(name.Value());
                if (!parsed)
                {
                    return parsed.Error().WithContext(row.Path() + "/" + std::string(field));
                }
                species = species | parsed.Value();
            }
            return species;
        }

        /// @brief The three marker arrays of `layout.nav.json`, which differ only in their name.
        [[nodiscard]] Result<void> ReadMarkers(const JsonValue& root,
                                               std::string_view field,
                                               MarkerKind kind,
                                               std::vector<NavMarker>& target)
        {
            if (!root.Has(field) || root.IsNull(field))
            {
                return util::Ok();
            }
            const Result<JsonValue> array = root.RequireArray(field);
            if (!array)
            {
                return array.Error();
            }
            const Result<std::vector<JsonValue>> rows = array.Value().Elements();
            if (!rows)
            {
                return rows.Error();
            }
            for (const JsonValue& row : rows.Value())
            {
                NavMarker marker;
                marker.kind = kind;
                const Result<util::Id> id = WorldLoader::RequireId(row, "id");
                if (!id)
                {
                    return id.Error();
                }
                marker.id = id.Value();

                for (const auto& [name, slot] : std::initializer_list<std::pair<std::string_view, util::Id*>>{
                         {"cell", &marker.cell}, {"prop", &marker.prop}})
                {
                    const Result<util::Id> value = WorldLoader::OptionalId(row, name);
                    if (!value)
                    {
                        return value.Error();
                    }
                    *slot = value.Value();
                }

                // A marker is either placed by position or attached to a prop, and §61 uses both:
                // a windowsill perch is a point, a dog bed is wherever the bed prop ended up. One
                // of the two is required, because a marker with neither is a marker nowhere.
                if (row.Has("position") && !row.IsNull("position"))
                {
                    const Result<Microsoft::Xna::Framework::Vector3> position =
                        row.RequireVector3("position");
                    if (!position)
                    {
                        return position.Error();
                    }
                    marker.position = position.Value();
                }
                else if (!marker.prop.IsValid())
                {
                    return Err(ErrorCode::InvalidData,
                               "a nav marker needs a position or a prop to sit on; this has "
                               "neither",
                               row.Path());
                }

                const Result<Species> species = ReadSpecies(row, "species");
                if (!species)
                {
                    return species.Error();
                }
                marker.species = species.Value();

                target.push_back(std::move(marker));
            }
            return util::Ok();
        }
    } // namespace

    Result<void> WorldLoader::LoadNav(std::string_view directory, WorldData::Contents& contents)
    {
        std::int32_t version = 0;
        const Result<JsonDocument> document = Open(directory, "layout.nav.json", "nav", version);
        if (!document)
        {
            return document.Error();
        }
        const JsonValue& root = document.Value().Root();

        const Result<JsonValue> nodes = root.RequireArray("nodes");
        if (!nodes)
        {
            return nodes.Error().WithContext("layout.nav.json");
        }
        const Result<std::vector<JsonValue>> nodeRows = nodes.Value().Elements();
        if (!nodeRows)
        {
            return nodeRows.Error().WithContext("layout.nav.json");
        }
        for (const JsonValue& row : nodeRows.Value())
        {
            NavNode node;
            const Result<util::Id> id = RequireId(row, "id");
            if (!id)
            {
                return id.Error().WithContext("layout.nav.json");
            }
            node.id = id.Value();
            const Result<util::Id> cell = RequireId(row, "cell");
            if (!cell)
            {
                return cell.Error().WithContext("layout.nav.json");
            }
            node.cell = cell.Value();
            const Result<Microsoft::Xna::Framework::Vector3> position = row.RequireVector3("position");
            if (!position)
            {
                return position.Error().WithContext("layout.nav.json");
            }
            node.position = position.Value();
            const Result<std::string> kind = row.OptionalString("kind", "");
            if (!kind)
            {
                return kind.Error().WithContext("layout.nav.json");
            }
            node.kind = kind.Value();
            contents.navNodes.push_back(std::move(node));
        }

        const Result<JsonValue> edges = root.RequireArray("edges");
        if (!edges)
        {
            return edges.Error().WithContext("layout.nav.json");
        }
        const Result<std::vector<JsonValue>> edgeRows = edges.Value().Elements();
        if (!edgeRows)
        {
            return edgeRows.Error().WithContext("layout.nav.json");
        }
        for (const JsonValue& row : edgeRows.Value())
        {
            NavEdge edge;
            const Result<util::Id> a = RequireId(row, "a");
            if (!a)
            {
                return a.Error().WithContext("layout.nav.json");
            }
            edge.a = a.Value();
            const Result<util::Id> b = RequireId(row, "b");
            if (!b)
            {
                return b.Error().WithContext("layout.nav.json");
            }
            edge.b = b.Value();
            if (edge.a == edge.b)
            {
                return Err(ErrorCode::InvalidData,
                           "an edge joins two nodes and both ends name " + Name(edge.a),
                           "layout.nav.json/" + row.Path() + "/b");
            }

            // An edge crossing a portal NAMES it, so a closed door closes the route for the pets
            // exactly as it does for vision and sound. One authored graph, four consumers.
            const Result<util::Id> portal = OptionalId(row, "portal");
            if (!portal)
            {
                return portal.Error().WithContext("layout.nav.json");
            }
            edge.portal = portal.Value();

            const Result<float> cost = row.OptionalFloat("cost", 0.0F);
            if (!cost)
            {
                return cost.Error().WithContext("layout.nav.json");
            }
            if (cost.Value() < 0.0F)
            {
                return Err(ErrorCode::OutOfRange,
                           "an edge cost is not negative; this is " + std::to_string(cost.Value()) +
                               ", and a negative edge makes the shortest path meaningless",
                           "layout.nav.json/" + row.Path() + "/cost");
            }
            edge.cost = cost.Value();

            const Result<Species> species = ReadSpecies(row, "species");
            if (!species)
            {
                return species.Error().WithContext("layout.nav.json");
            }
            edge.species = species.Value();

            contents.navEdges.push_back(std::move(edge));
        }

        for (const auto& [field, kind] : std::initializer_list<std::pair<std::string_view, MarkerKind>>{
                 {"perches", MarkerKind::Perch}, {"beds", MarkerKind::Bed}, {"bowls", MarkerKind::Bowl}})
        {
            if (const Result<void> markers = ReadMarkers(root, field, kind, contents.navMarkers); !markers)
            {
                return markers.Error().WithContext("layout.nav.json");
            }
        }

        if (root.Has("forbidden") && !root.IsNull("forbidden"))
        {
            const Result<JsonValue> forbidden = root.RequireArray("forbidden");
            if (!forbidden)
            {
                return forbidden.Error().WithContext("layout.nav.json");
            }
            const Result<std::vector<JsonValue>> rows = forbidden.Value().Elements();
            if (!rows)
            {
                return rows.Error().WithContext("layout.nav.json");
            }
            for (const JsonValue& row : rows.Value())
            {
                NavForbidden zone;
                const Result<util::Id> cell = RequireId(row, "cell");
                if (!cell)
                {
                    return cell.Error().WithContext("layout.nav.json");
                }
                zone.cell = cell.Value();
                // Here, and nowhere else, an absent species list would mean "forbidden to nobody"
                // -- a row that reads as a rule and does nothing. So it is required.
                if (!row.Has("species") || row.IsNull("species"))
                {
                    return Err(ErrorCode::InvalidData,
                               "a forbidden zone says which animals it forbids; without that it "
                               "is a rule that forbids nobody",
                               "layout.nav.json/" + row.Path() + "/species");
                }
                const Result<Species> species = ReadSpecies(row, "species");
                if (!species)
                {
                    return species.Error().WithContext("layout.nav.json");
                }
                zone.species = species.Value();
                contents.navForbidden.push_back(zone);
            }
        }

        return util::Ok();
    }

    Result<void> WorldLoader::LoadAudio(std::string_view directory, WorldData::Contents& contents)
    {
        std::int32_t version = 0;
        const Result<JsonDocument> document = Open(directory, "layout.audio.json", "audio", version);
        if (!document)
        {
            return document.Error();
        }
        const JsonValue& root = document.Value().Root();

        // A gain is a linear multiplier and 0..1 is the whole of it. Above 1 it clips, and a clip
        // in an ambience bed is a distortion that follows the player from room to room.
        const auto gain = [](const JsonValue& row, const std::string& where) -> Result<float>
        {
            const Result<float> value = row.OptionalFloat("gain", 1.0F);
            if (!value)
            {
                return value.Error();
            }
            if (value.Value() < 0.0F || value.Value() > 1.0F)
            {
                return Err(ErrorCode::OutOfRange,
                           "a gain is 0..1; this is " + std::to_string(value.Value()),
                           where + "/gain");
            }
            return value.Value();
        };

        const Result<JsonValue> zones = root.RequireArray("zones");
        if (!zones)
        {
            return zones.Error().WithContext("layout.audio.json");
        }
        const Result<std::vector<JsonValue>> zoneRows = zones.Value().Elements();
        if (!zoneRows)
        {
            return zoneRows.Error().WithContext("layout.audio.json");
        }
        for (const JsonValue& row : zoneRows.Value())
        {
            AudioZone zone;
            const Result<util::Id> id = RequireId(row, "id");
            if (!id)
            {
                return id.Error().WithContext("layout.audio.json");
            }
            zone.id = id.Value();
            const Result<util::Id> cell = RequireId(row, "cell");
            if (!cell)
            {
                return cell.Error().WithContext("layout.audio.json");
            }
            zone.cell = cell.Value();
            const Result<util::Id> bed = OptionalId(row, "bed");
            if (!bed)
            {
                return bed.Error().WithContext("layout.audio.json");
            }
            zone.bed = bed.Value();
            const Result<float> zoneGain = gain(row, row.Path());
            if (!zoneGain)
            {
                return zoneGain.Error().WithContext("layout.audio.json");
            }
            zone.gain = zoneGain.Value();
            contents.audioZones.push_back(std::move(zone));
        }

        if (root.Has("emitters") && !root.IsNull("emitters"))
        {
            const Result<JsonValue> emitters = root.RequireArray("emitters");
            if (!emitters)
            {
                return emitters.Error().WithContext("layout.audio.json");
            }
            const Result<std::vector<JsonValue>> rows = emitters.Value().Elements();
            if (!rows)
            {
                return rows.Error().WithContext("layout.audio.json");
            }
            for (const JsonValue& row : rows.Value())
            {
                AudioEmitter emitter;
                const Result<util::Id> id = RequireId(row, "id");
                if (!id)
                {
                    return id.Error().WithContext("layout.audio.json");
                }
                emitter.id = id.Value();

                // Every emitter carries a cell id, because the portal-path solver (ADR-0010)
                // starts from cells and not from positions: a point alone would have to be
                // located first, on every voice, every frame.
                const Result<util::Id> cell = RequireId(row, "cell");
                if (!cell)
                {
                    return cell.Error().WithContext("layout.audio.json");
                }
                emitter.cell = cell.Value();

                const Result<Microsoft::Xna::Framework::Vector3> position = row.RequireVector3("position");
                if (!position)
                {
                    return position.Error().WithContext("layout.audio.json");
                }
                emitter.position = position.Value();

                for (const auto& [field, target] :
                     std::initializer_list<std::pair<std::string_view, util::Id*>>{
                         {"loop", &emitter.loop}, {"interactable", &emitter.interactable}})
                {
                    const Result<util::Id> value = OptionalId(row, field);
                    if (!value)
                    {
                        return value.Error().WithContext("layout.audio.json");
                    }
                    *target = value.Value();
                }

                const Result<float> emitterGain = gain(row, row.Path());
                if (!emitterGain)
                {
                    return emitterGain.Error().WithContext("layout.audio.json");
                }
                emitter.gain = emitterGain.Value();

                const Result<float> radius = row.OptionalFloat("radius", 0.0F);
                if (!radius)
                {
                    return radius.Error().WithContext("layout.audio.json");
                }
                if (radius.Value() < 0.0F)
                {
                    return Err(ErrorCode::OutOfRange,
                               "a radius is not negative; this is " + std::to_string(radius.Value()),
                               "layout.audio.json/" + row.Path() + "/radius");
                }
                emitter.radius = radius.Value();

                contents.audioEmitters.push_back(std::move(emitter));
            }
        }

        if (root.Has("transmission") && !root.IsNull("transmission"))
        {
            const Result<JsonValue> transmission = root.RequireObject("transmission");
            if (!transmission)
            {
                return transmission.Error().WithContext("layout.audio.json");
            }
            // The table is an object keyed by §64.3's class name, and the classes are the
            // document's, not this reader's: it used to hold a hardcoded list of six names
            // invented before §64.3 was authored, and the seven the design actually names --
            // `door_exterior`, `slider_glass`, `window_single`, `window_hopper`, `door_garage`,
            // `hatch_loft`, `appliance` -- were silently dropped, which `HOUSE-00388` found by
            // authoring them. The comment beside that list claimed a count comparison "below"
            // that did not exist. Enumerating the object has no list to keep in step.
            const Result<std::vector<std::pair<std::string, JsonValue>>> kinds =
                transmission.Value().Members();
            if (!kinds)
            {
                return kinds.Error().WithContext("layout.audio.json");
            }
            for (const auto& [kind, entry] : kinds.Value())
            {
                const Result<JsonValue> pair = transmission.Value().RequireObject(kind);
                if (!pair)
                {
                    return pair.Error().WithContext("layout.audio.json");
                }
                AudioTransmission row;
                row.kind = kind;
                for (const auto& [field, target] : std::initializer_list<std::pair<std::string_view, float*>>{
                         {"open", &row.open}, {"closed", &row.closed}})
                {
                    const Result<float> value = pair.Value().RequireFloat(field);
                    if (!value)
                    {
                        return value.Error().WithContext("layout.audio.json");
                    }
                    if (value.Value() < 0.0F || value.Value() > 1.0F)
                    {
                        return Err(ErrorCode::OutOfRange,
                                   "a transmission loss is 0 (transparent) to 1 (inaudible); this "
                                   "is " +
                                       std::to_string(value.Value()),
                                   "layout.audio.json/transmission/" + std::string(kind) + "/" +
                                       std::string(field));
                    }
                    *target = value.Value();
                }
                // Closing a door does not make it quieter to shut than to leave open. The other
                // way round is a sign-flipped pair, which sounds like the audio system is broken.
                if (row.closed < row.open)
                {
                    return Err(ErrorCode::InvalidData,
                               "closing a " + std::string(kind) +
                                   " loses at least as much as "
                                   "leaving it open; this is " +
                                   std::to_string(row.closed) + " closed against " +
                                   std::to_string(row.open) + " open",
                               "layout.audio.json/transmission/" + std::string(kind));
                }
                contents.audioTransmission.push_back(std::move(row));
            }
        }

        return util::Ok();
    }

    namespace
    {
        /// @brief An array of `[x, y, z]` triples: a road centreline or a fence path.
        [[nodiscard]] Result<std::vector<Microsoft::Xna::Framework::Vector3>>
        ReadPolyline(const JsonValue& parent, std::string_view field, std::size_t least)
        {
            std::vector<Microsoft::Xna::Framework::Vector3> points;
            const Result<JsonValue> array = parent.RequireArray(field);
            if (!array)
            {
                return array.Error();
            }
            const Result<std::vector<JsonValue>> rows = array.Value().Elements();
            if (!rows)
            {
                return rows.Error();
            }
            for (const JsonValue& row : rows.Value())
            {
                const Result<std::vector<JsonValue>> parts = row.Elements();
                if (!parts)
                {
                    return parts.Error();
                }
                if (parts.Value().size() != 3U)
                {
                    return Err(ErrorCode::InvalidData,
                               "a point is [x, y, z]; this has " + std::to_string(parts.Value().size()) +
                                   " element(s)",
                               row.Path());
                }
                float xyz[3] = {0.0F, 0.0F, 0.0F};
                for (std::size_t axis = 0; axis < 3U; ++axis)
                {
                    const Result<float> value = parts.Value()[axis].AsFloat();
                    if (!value)
                    {
                        return value.Error();
                    }
                    xyz[axis] = value.Value();
                }
                points.emplace_back(xyz[0], xyz[1], xyz[2]);
            }
            // A path of one point is not a path. It would draw nothing and, worse, a fence built
            // from it would silently occupy no ground at all -- a garden with a gap nobody
            // authored.
            if (points.size() < least)
            {
                return Err(ErrorCode::InvalidData,
                           "a path needs at least " + std::to_string(least) + " points; this has " +
                               std::to_string(points.size()),
                           parent.Path() + "/" + std::string(field));
            }
            return points;
        }
    } // namespace

    Result<void> WorldLoader::LoadExterior(std::string_view directory, WorldData::Contents& contents)
    {
        std::int32_t version = 0;
        const Result<JsonDocument> document = Open(directory, "layout.exterior.json", "exterior", version);
        if (!document)
        {
            return document.Error();
        }
        const JsonValue& root = document.Value().Root();
        Exterior& exterior = contents.exterior;

        const Result<JsonValue> terrain = root.RequireObject("terrain");
        if (!terrain)
        {
            return terrain.Error().WithContext("layout.exterior.json");
        }
        const Result<std::string> heightfield = terrain.Value().RequireString("heightfield");
        if (!heightfield)
        {
            return heightfield.Error().WithContext("layout.exterior.json");
        }
        exterior.terrain.heightfield = heightfield.Value();

        const Result<Microsoft::Xna::Framework::Vector2> size = terrain.Value().RequireVector2("size");
        if (!size)
        {
            return size.Error().WithContext("layout.exterior.json");
        }
        if (size.Value().X <= 0.0F || size.Value().Y <= 0.0F)
        {
            return Err(ErrorCode::InvalidData,
                       "the terrain has extent in both axes; this is " + std::to_string(size.Value().X) +
                           " x " + std::to_string(size.Value().Y),
                       "layout.exterior.json/terrain/size");
        }
        exterior.terrain.sizeX = size.Value().X;
        exterior.terrain.sizeZ = size.Value().Y;

        const Result<Microsoft::Xna::Framework::Vector3> origin = terrain.Value().RequireVector3("origin");
        if (!origin)
        {
            return origin.Error().WithContext("layout.exterior.json");
        }
        exterior.terrain.origin = origin.Value();

        const Result<float> yScale = terrain.Value().OptionalFloat("yScale", 1.0F);
        if (!yScale)
        {
            return yScale.Error().WithContext("layout.exterior.json");
        }
        exterior.terrain.yScale = yScale.Value();

        const Result<util::Id> terrainMaterial = OptionalId(terrain.Value(), "material");
        if (!terrainMaterial)
        {
            return terrainMaterial.Error().WithContext("layout.exterior.json");
        }
        exterior.terrain.material = terrainMaterial.Value();

        if (root.Has("road") && !root.IsNull("road"))
        {
            const Result<JsonValue> road = root.RequireObject("road");
            if (!road)
            {
                return road.Error().WithContext("layout.exterior.json");
            }
            const Result<std::vector<Microsoft::Xna::Framework::Vector3>> centreline =
                ReadPolyline(road.Value(), "centreline", 2U);
            if (!centreline)
            {
                return centreline.Error().WithContext("layout.exterior.json");
            }
            exterior.road.centreline = centreline.Value();
            const Result<float> width = road.Value().OptionalFloat("width", 0.0F);
            if (!width)
            {
                return width.Error().WithContext("layout.exterior.json");
            }
            exterior.road.width = width.Value();
            const Result<util::Id> material = OptionalId(road.Value(), "material");
            if (!material)
            {
                return material.Error().WithContext("layout.exterior.json");
            }
            exterior.road.material = material.Value();
        }

        if (root.Has("fences") && !root.IsNull("fences"))
        {
            const Result<JsonValue> fences = root.RequireArray("fences");
            if (!fences)
            {
                return fences.Error().WithContext("layout.exterior.json");
            }
            const Result<std::vector<JsonValue>> rows = fences.Value().Elements();
            if (!rows)
            {
                return rows.Error().WithContext("layout.exterior.json");
            }
            for (const JsonValue& row : rows.Value())
            {
                Fence fence;
                const Result<util::Id> id = RequireId(row, "id");
                if (!id)
                {
                    return id.Error().WithContext("layout.exterior.json");
                }
                fence.id = id.Value();
                const Result<util::Id> asset = RequireId(row, "asset");
                if (!asset)
                {
                    return asset.Error().WithContext("layout.exterior.json");
                }
                fence.asset = asset.Value();
                const Result<std::vector<Microsoft::Xna::Framework::Vector3>> path =
                    ReadPolyline(row, "path", 2U);
                if (!path)
                {
                    return path.Error().WithContext("layout.exterior.json");
                }
                fence.path = path.Value();
                const Result<float> height = row.OptionalFloat("height", 0.0F);
                if (!height)
                {
                    return height.Error().WithContext("layout.exterior.json");
                }
                fence.height = height.Value();
                const Result<util::Id> gate = OptionalId(row, "gate");
                if (!gate)
                {
                    return gate.Error().WithContext("layout.exterior.json");
                }
                fence.gate = gate.Value();
                exterior.fences.push_back(std::move(fence));
            }
        }

        if (root.Has("neighbourhood") && !root.IsNull("neighbourhood"))
        {
            const Result<JsonValue> neighbourhood = root.RequireArray("neighbourhood");
            if (!neighbourhood)
            {
                return neighbourhood.Error().WithContext("layout.exterior.json");
            }
            const Result<std::vector<JsonValue>> rows = neighbourhood.Value().Elements();
            if (!rows)
            {
                return rows.Error().WithContext("layout.exterior.json");
            }
            for (const JsonValue& row : rows.Value())
            {
                NeighbourBuilding building;
                const Result<util::Id> id = RequireId(row, "id");
                if (!id)
                {
                    return id.Error().WithContext("layout.exterior.json");
                }
                building.id = id.Value();
                const Result<util::Id> asset = RequireId(row, "asset");
                if (!asset)
                {
                    return asset.Error().WithContext("layout.exterior.json");
                }
                building.asset = asset.Value();
                const Result<Microsoft::Xna::Framework::Vector3> position = row.RequireVector3("position");
                if (!position)
                {
                    return position.Error().WithContext("layout.exterior.json");
                }
                building.position = position.Value();
                const Result<float> yaw = row.OptionalFloat("yawDeg", 0.0F);
                if (!yaw)
                {
                    return yaw.Error().WithContext("layout.exterior.json");
                }
                building.yawDeg = yaw.Value();
                const Result<util::Id> lodGroup = OptionalId(row, "lodGroup");
                if (!lodGroup)
                {
                    return lodGroup.Error().WithContext("layout.exterior.json");
                }
                building.lodGroup = lodGroup.Value();

                // The distance at which the building becomes an impostor. Zero means "always an
                // impostor", which is a real choice for the far row of houses, so it is not
                // refused -- but a NEGATIVE distance is a sign error that would swap the two
                // branches and draw a full mesh at the horizon.
                const Result<float> impostor = row.OptionalFloat("impostorFrom", 0.0F);
                if (!impostor)
                {
                    return impostor.Error().WithContext("layout.exterior.json");
                }
                if (impostor.Value() < 0.0F)
                {
                    return Err(ErrorCode::OutOfRange,
                               "an impostor distance is not negative; this is " +
                                   std::to_string(impostor.Value()),
                               "layout.exterior.json/" + row.Path() + "/impostorFrom");
                }
                building.impostorFrom = impostor.Value();

                exterior.neighbourhood.push_back(std::move(building));
            }
        }

        if (root.Has("vegetation") && !root.IsNull("vegetation"))
        {
            const Result<JsonValue> vegetation = root.RequireArray("vegetation");
            if (!vegetation)
            {
                return vegetation.Error().WithContext("layout.exterior.json");
            }
            const Result<std::vector<JsonValue>> rows = vegetation.Value().Elements();
            if (!rows)
            {
                return rows.Error().WithContext("layout.exterior.json");
            }
            for (const JsonValue& row : rows.Value())
            {
                VegetationGroup group;
                const Result<util::Id> id = RequireId(row, "id");
                if (!id)
                {
                    return id.Error().WithContext("layout.exterior.json");
                }
                group.id = id.Value();
                const Result<util::Id> asset = RequireId(row, "asset");
                if (!asset)
                {
                    return asset.Error().WithContext("layout.exterior.json");
                }
                group.asset = asset.Value();

                // Instances are an ARRAY under one asset, not one row per plant: §17.4 draws them
                // instanced where that measures faster, and that needs them grouped by asset in
                // the data rather than sorted into groups at load.
                const Result<JsonValue> instances = row.RequireArray("instances");
                if (!instances)
                {
                    return instances.Error().WithContext("layout.exterior.json");
                }
                const Result<std::vector<JsonValue>> instanceRows = instances.Value().Elements();
                if (!instanceRows)
                {
                    return instanceRows.Error().WithContext("layout.exterior.json");
                }
                for (const JsonValue& instanceRow : instanceRows.Value())
                {
                    VegetationInstance instance;
                    const Result<Microsoft::Xna::Framework::Vector3> position =
                        instanceRow.RequireVector3("position");
                    if (!position)
                    {
                        return position.Error().WithContext("layout.exterior.json");
                    }
                    instance.position = position.Value();
                    const Result<float> yaw = instanceRow.OptionalFloat("yawDeg", 0.0F);
                    if (!yaw)
                    {
                        return yaw.Error().WithContext("layout.exterior.json");
                    }
                    instance.yawDeg = yaw.Value();
                    const Result<float> scale = instanceRow.OptionalFloat("scale", 1.0F);
                    if (!scale)
                    {
                        return scale.Error().WithContext("layout.exterior.json");
                    }
                    if (scale.Value() <= 0.0F)
                    {
                        return Err(ErrorCode::InvalidData,
                                   "a scale is positive; this is " + std::to_string(scale.Value()),
                                   "layout.exterior.json/" + instanceRow.Path() + "/scale");
                    }
                    instance.scale = scale.Value();
                    group.instances.push_back(instance);
                }

                exterior.vegetation.push_back(std::move(group));
            }
        }

        return util::Ok();
    }

    Result<void> WorldLoader::LoadWeather(std::string_view directory, WorldData::Contents& contents)
    {
        std::int32_t version = 0;
        const Result<JsonDocument> document = Open(directory, "layout.weather.json", "weather", version);
        if (!document)
        {
            return document.Error();
        }
        if (version != 1)
        {
            return Err(ErrorCode::VersionMismatch,
                       "only weather schema version 1 is supported; this is version " +
                           std::to_string(version),
                       "layout.weather.json/schema");
        }

        const Result<JsonValue> array = document.Value().Root().RequireArray("archetypes");
        if (!array)
        {
            return array.Error().WithContext("layout.weather.json");
        }
        const Result<std::vector<JsonValue>> rows = array.Value().Elements();
        if (!rows)
        {
            return rows.Error().WithContext("layout.weather.json");
        }
        if (rows.Value().size() != 14U)
        {
            return Err(ErrorCode::InvalidData,
                       "§36.2 defines thirteen weather states and one modifier; expected 14 "
                       "archetypes, found " +
                           std::to_string(rows.Value().size()),
                       "layout.weather.json/archetypes");
        }

        const auto range = [](const JsonValue& row,
                              std::string_view field,
                              std::optional<std::pair<float, float>> bounds) -> Result<weather::WeatherRange>
        {
            const Result<JsonValue> value = row.RequireArray(field);
            if (!value)
            {
                return value.Error();
            }
            const Result<std::vector<JsonValue>> parts = value.Value().Elements();
            if (!parts)
            {
                return parts.Error();
            }
            if (parts.Value().size() != 2U)
            {
                return Err(ErrorCode::InvalidData,
                           "an archetype target is [min, max]; this has " +
                               std::to_string(parts.Value().size()) + " element(s)",
                           row.Path() + "/" + std::string(field));
            }
            const Result<float> minimum = parts.Value()[0].AsFloat();
            if (!minimum)
            {
                return minimum.Error();
            }
            const Result<float> maximum = parts.Value()[1].AsFloat();
            if (!maximum)
            {
                return maximum.Error();
            }
            if (!std::isfinite(minimum.Value()) || !std::isfinite(maximum.Value()))
            {
                return Err(ErrorCode::InvalidData,
                           "an archetype target must have finite endpoints",
                           row.Path() + "/" + std::string(field));
            }
            if (!(minimum.Value() <= maximum.Value()))
            {
                return Err(ErrorCode::InvalidData,
                           "an archetype target has min <= max; this is [" + std::to_string(minimum.Value()) +
                               ", " + std::to_string(maximum.Value()) + "]",
                           row.Path() + "/" + std::string(field));
            }
            if (bounds.has_value() &&
                (!(minimum.Value() >= bounds->first) || !(maximum.Value() <= bounds->second)))
            {
                return Err(ErrorCode::OutOfRange,
                           "the target [" + std::to_string(minimum.Value()) + ", " +
                               std::to_string(maximum.Value()) + "] is outside " +
                               std::to_string(bounds->first) + ".." + std::to_string(bounds->second),
                           row.Path() + "/" + std::string(field));
            }
            return weather::WeatherRange{minimum.Value(), maximum.Value()};
        };

        std::vector<weather::WeatherArchetype> parsed;
        parsed.reserve(rows.Value().size());
        std::unordered_set<util::Id> ids;
        std::size_t modifiers = 0;
        for (const JsonValue& row : rows.Value())
        {
            weather::WeatherArchetype archetype;
            const Result<util::Id> id = RequireId(row, "id");
            if (!id)
            {
                return id.Error().WithContext("layout.weather.json");
            }
            archetype.id = id.Value();
            if (!ids.insert(archetype.id).second)
            {
                return Err(ErrorCode::Duplicate,
                           Name(archetype.id) + " appears more than once",
                           "layout.weather.json/" + row.Path() + "/id");
            }

            const auto readRange = [&](std::string_view field,
                                       weather::WeatherRange& target,
                                       std::optional<std::pair<float, float>> bounds) -> Result<void>
            {
                const Result<weather::WeatherRange> value = range(row, field, bounds);
                if (!value)
                {
                    return value.Error().WithContext("layout.weather.json");
                }
                target = value.Value();
                return util::Ok();
            };
            constexpr std::pair<float, float> kUnit{0.0F, 1.0F};
            if (auto value = readRange("cloudCover", archetype.cloudCover, kUnit); !value)
            {
                return value.Error();
            }
            if (auto value = readRange("cloudCumuliform", archetype.cloudCumuliform, kUnit); !value)
            {
                return value.Error();
            }
            const Result<std::string> precip = row.RequireString("precipType");
            if (!precip)
            {
                return precip.Error().WithContext("layout.weather.json");
            }
            const Result<weather::PrecipType> precipType =
                weather::ParsePrecipType(precip.Value(), row.Path() + "/precipType");
            if (!precipType)
            {
                return precipType.Error().WithContext("layout.weather.json");
            }
            archetype.precipType = precipType.Value();
            if (auto value = readRange("precipIntensity", archetype.precipIntensity, kUnit); !value)
            {
                return value.Error();
            }
            if (auto value =
                    readRange("windSpeed", archetype.windSpeed, std::pair<float, float>{0.0F, 30.0F});
                !value)
            {
                return value.Error();
            }
            if (auto value = readRange("gustFactor", archetype.gustFactor, kUnit); !value)
            {
                return value.Error();
            }
            if (auto value = readRange("fogDensity", archetype.fogDensity, kUnit); !value)
            {
                return value.Error();
            }
            if (auto value = readRange("temperatureOffsetC", archetype.temperatureOffsetC, std::nullopt);
                !value)
            {
                return value.Error();
            }
            if (auto value = readRange("humidity", archetype.humidity, kUnit); !value)
            {
                return value.Error();
            }
            if (auto value = readRange("thunderProbability", archetype.thunderProbability, kUnit); !value)
            {
                return value.Error();
            }

            const Result<bool> modifier = row.RequireBool("modifier");
            if (!modifier)
            {
                return modifier.Error().WithContext("layout.weather.json");
            }
            archetype.modifier = modifier.Value();
            modifiers += archetype.modifier ? 1U : 0U;
            const Result<float> weight = row.RequireFloat("weight");
            if (!weight)
            {
                return weight.Error().WithContext("layout.weather.json");
            }
            if (!std::isfinite(weight.Value()) || !(weight.Value() >= 0.0F))
            {
                return Err(ErrorCode::OutOfRange,
                           "an archetype weight is non-negative; this is " + std::to_string(weight.Value()),
                           "layout.weather.json/" + row.Path() + "/weight");
            }
            archetype.weight = weight.Value();
            parsed.push_back(archetype);
        }

        if (modifiers != 1U)
        {
            return Err(ErrorCode::InvalidData,
                       "§36.2 defines exactly one modifier; found " + std::to_string(modifiers),
                       "layout.weather.json/archetypes");
        }
        const auto windy =
            std::ranges::find_if(parsed, [](const weather::WeatherArchetype& row) { return row.modifier; });
        if (windy == parsed.end() || windy->id != util::Intern("W_WINDY") || windy->weight != 0.0F)
        {
            return Err(ErrorCode::InvalidData,
                       "the sole modifier is W_WINDY and has weight 0 because transitions never "
                       "select it",
                       "layout.weather.json/archetypes");
        }

        std::unordered_map<util::Id, std::size_t> archetypeById;
        for (std::size_t index = 0; index < parsed.size(); ++index)
        {
            archetypeById.emplace(parsed[index].id, index);
        }
        const auto internKey = [](std::string_view name, std::string context) -> Result<util::Id>
        {
            if (name.empty())
            {
                return Err(ErrorCode::InvalidData, "an id may not be empty", std::move(context));
            }
            util::IdRegistry::ClearConflict();
            const util::Id id = util::Intern(name);
            if (util::IdRegistry::HadConflict())
            {
                return Err(ErrorCode::Duplicate,
                           "\"" + std::string(name) + "\" hashes to the same id as \"" +
                               util::IdRegistry::ConflictExisting() + "\"",
                           std::move(context));
            }
            return id;
        };

        const Result<JsonValue> transitionObject = document.Value().Root().RequireObject("transitions");
        if (!transitionObject)
        {
            return transitionObject.Error().WithContext("layout.weather.json");
        }
        const Result<std::vector<std::pair<std::string, JsonValue>>> transitionMembers =
            transitionObject.Value().Members();
        if (!transitionMembers)
        {
            return transitionMembers.Error().WithContext("layout.weather.json");
        }

        std::vector<weather::WeatherTransitionRow> transitions;
        transitions.reserve(transitionMembers.Value().size());
        std::unordered_set<util::Id> transitionSources;
        for (const auto& [sourceName, targetObject] : transitionMembers.Value())
        {
            const Result<util::Id> source =
                internKey(sourceName, "layout.weather.json/transitions/" + sourceName);
            if (!source)
            {
                return source.Error();
            }
            const auto sourceArchetype = archetypeById.find(source.Value());
            if (sourceArchetype == archetypeById.end() || parsed[sourceArchetype->second].modifier)
            {
                return Err(ErrorCode::InvalidData,
                           sourceName + " has a transition row and is not a weather state",
                           "layout.weather.json/transitions/" + sourceName);
            }
            if (!transitionSources.insert(source.Value()).second)
            {
                return Err(ErrorCode::Duplicate,
                           sourceName + " has more than one transition row",
                           "layout.weather.json/transitions/" + sourceName);
            }

            const Result<std::vector<std::pair<std::string, JsonValue>>> targetMembers =
                targetObject.Members();
            if (!targetMembers)
            {
                return targetMembers.Error().WithContext("layout.weather.json");
            }
            if (targetMembers.Value().empty())
            {
                return Err(ErrorCode::InvalidData,
                           "a transition row must have somewhere to go",
                           "layout.weather.json/transitions/" + sourceName);
            }

            weather::WeatherTransitionRow transitionRow;
            transitionRow.source = source.Value();
            std::unordered_set<util::Id> targets;
            double total = 0.0;
            for (const auto& [targetName, probabilityValue] : targetMembers.Value())
            {
                const Result<util::Id> target =
                    internKey(targetName, "layout.weather.json/transitions/" + sourceName + "/" + targetName);
                if (!target)
                {
                    return target.Error();
                }
                const auto targetArchetype = archetypeById.find(target.Value());
                if (targetArchetype == archetypeById.end() || parsed[targetArchetype->second].modifier)
                {
                    return Err(ErrorCode::InvalidData,
                               sourceName + " can become " + targetName + ", which is not a weather state",
                               "layout.weather.json/transitions/" + sourceName + "/" + targetName);
                }
                if (!targets.insert(target.Value()).second)
                {
                    return Err(ErrorCode::Duplicate,
                               targetName + " appears twice in " + sourceName + "'s transition row",
                               "layout.weather.json/transitions/" + sourceName + "/" + targetName);
                }
                const Result<float> probability = probabilityValue.AsFloat();
                if (!probability)
                {
                    return probability.Error().WithContext("layout.weather.json");
                }
                if (!(probability.Value() >= 0.0F && probability.Value() <= 1.0F))
                {
                    return Err(ErrorCode::OutOfRange,
                               "a transition probability is inside 0..1; this is " +
                                   std::to_string(probability.Value()),
                               "layout.weather.json/transitions/" + sourceName + "/" + targetName);
                }
                transitionRow.targets.push_back({target.Value(), probability.Value()});
                total += static_cast<double>(probability.Value());
            }
            if (std::abs(total - 1.0) > 1e-3)
            {
                return Err(ErrorCode::InvalidData,
                           sourceName + "'s transition row sums to " + std::to_string(total) + ", not 1",
                           "layout.weather.json/transitions/" + sourceName);
            }
            transitions.push_back(std::move(transitionRow));
        }
        if (transitions.size() != parsed.size() - 1U)
        {
            return Err(ErrorCode::InvalidData,
                       "every one of the thirteen weather states needs one transition row; found " +
                           std::to_string(transitions.size()),
                       "layout.weather.json/transitions");
        }

        const Result<JsonValue> timingObject = document.Value().Root().RequireObject("timing");
        if (!timingObject)
        {
            return timingObject.Error().WithContext("layout.weather.json");
        }
        const Result<std::vector<std::pair<std::string, JsonValue>>> timingMembers =
            timingObject.Value().Members();
        if (!timingMembers)
        {
            return timingMembers.Error().WithContext("layout.weather.json");
        }
        std::unordered_set<util::Id> timedArchetypes;
        for (const auto& [name, timingRow] : timingMembers.Value())
        {
            const Result<util::Id> id = internKey(name, "layout.weather.json/timing/" + name);
            if (!id)
            {
                return id.Error();
            }
            const auto archetype = archetypeById.find(id.Value());
            if (archetype == archetypeById.end() || parsed[archetype->second].modifier)
            {
                return Err(ErrorCode::InvalidData,
                           name + " has timing but is not a weather state",
                           "layout.weather.json/timing/" + name);
            }
            if (!timedArchetypes.insert(id.Value()).second)
            {
                return Err(ErrorCode::Duplicate,
                           name + " has more than one timing distribution",
                           "layout.weather.json/timing/" + name);
            }
            const Result<weather::WeatherRange> dwell =
                range(timingRow, "dwellMinutes", std::pair<float, float>{25.0F, 380.0F});
            if (!dwell)
            {
                return dwell.Error().WithContext("layout.weather.json");
            }
            const Result<weather::WeatherRange> transition =
                range(timingRow, "transitionMinutes", std::pair<float, float>{5.0F, 45.0F});
            if (!transition)
            {
                return transition.Error().WithContext("layout.weather.json");
            }
            parsed[archetype->second].dwellMinutes = dwell.Value();
            parsed[archetype->second].transitionMinutes = transition.Value();
        }
        if (timedArchetypes.size() != parsed.size() - 1U)
        {
            return Err(ErrorCode::InvalidData,
                       "every one of the thirteen weather states needs timing distributions; found " +
                           std::to_string(timedArchetypes.size()),
                       "layout.weather.json/timing");
        }

        const Result<JsonValue> ratesObject = document.Value().Root().RequireObject("rates");
        if (!ratesObject)
        {
            return ratesObject.Error().WithContext("layout.weather.json");
        }
        weather::WeatherRates rates;
        constexpr std::array<std::pair<std::string_view, float weather::WeatherRates::*>, 10> kRates{
            std::pair{"cloudCoverPerMin", &weather::WeatherRates::cloudCover},
            std::pair{"cloudCumuliformPerMin", &weather::WeatherRates::cloudCumuliform},
            std::pair{"precipIntensityPerMin", &weather::WeatherRates::precipIntensity},
            std::pair{"windSpeedPerMin", &weather::WeatherRates::windSpeed},
            std::pair{"windDirectionDegPerMin", &weather::WeatherRates::windDirectionDeg},
            std::pair{"gustFactorPerMin", &weather::WeatherRates::gustFactor},
            std::pair{"fogDensityPerMin", &weather::WeatherRates::fogDensity},
            std::pair{"thunderIntensityPerMin", &weather::WeatherRates::thunderIntensity},
            std::pair{"temperatureCPerMin", &weather::WeatherRates::temperatureC},
            std::pair{"humidityPerMin", &weather::WeatherRates::humidity},
        };
        const Result<std::vector<std::pair<std::string, JsonValue>>> rateMembers =
            ratesObject.Value().Members();
        if (!rateMembers)
        {
            return rateMembers.Error().WithContext("layout.weather.json");
        }
        if (rateMembers.Value().size() != kRates.size())
        {
            return Err(ErrorCode::InvalidData,
                       "the rate table has exactly ten archetype-driven channels; found " +
                           std::to_string(rateMembers.Value().size()),
                       "layout.weather.json/rates");
        }
        for (const auto& [name, member] : kRates)
        {
            const Result<float> value = ratesObject.Value().RequireFloat(name);
            if (!value)
            {
                return value.Error().WithContext("layout.weather.json");
            }
            if (!std::isfinite(value.Value()) || !(value.Value() > 0.0F))
            {
                return Err(ErrorCode::OutOfRange,
                           std::string(name) + " must be finite and positive",
                           "layout.weather.json/rates/" + std::string(name));
            }
            rates.*member = value.Value();
        }

        const Result<JsonValue> seasonArray = document.Value().Root().RequireArray("seasons");
        if (!seasonArray)
        {
            return seasonArray.Error().WithContext("layout.weather.json");
        }
        const Result<std::vector<JsonValue>> seasonRows = seasonArray.Value().Elements();
        if (!seasonRows)
        {
            return seasonRows.Error().WithContext("layout.weather.json");
        }
        if (seasonRows.Value().size() != 4U)
        {
            return Err(ErrorCode::InvalidData,
                       "§36.3 defines four seasonal weight vectors; found " +
                           std::to_string(seasonRows.Value().size()),
                       "layout.weather.json/seasons");
        }

        constexpr std::array<std::string_view, 4> kSeasonNames{"SPRING", "SUMMER", "AUTUMN", "WINTER"};
        constexpr std::array<std::array<std::int64_t, 3>, 4> kSeasonMonths{
            std::array<std::int64_t, 3>{3, 4, 5},
            std::array<std::int64_t, 3>{6, 7, 8},
            std::array<std::int64_t, 3>{9, 10, 11},
            std::array<std::int64_t, 3>{12, 1, 2},
        };
        std::array<bool, 4> seenSeasons{};
        for (const JsonValue& seasonRow : seasonRows.Value())
        {
            const Result<std::string> name = seasonRow.RequireString("id");
            if (!name)
            {
                return name.Error().WithContext("layout.weather.json");
            }
            const auto found = std::ranges::find(kSeasonNames, name.Value());
            if (found == kSeasonNames.end())
            {
                return Err(ErrorCode::InvalidData,
                           name.Value() + " is not SPRING, SUMMER, AUTUMN or WINTER",
                           "layout.weather.json/" + seasonRow.Path() + "/id");
            }
            const std::size_t seasonIndex =
                static_cast<std::size_t>(std::distance(kSeasonNames.begin(), found));
            if (seenSeasons[seasonIndex])
            {
                return Err(ErrorCode::Duplicate,
                           name.Value() + " appears more than once",
                           "layout.weather.json/" + seasonRow.Path() + "/id");
            }
            seenSeasons[seasonIndex] = true;

            const Result<JsonValue> months = seasonRow.RequireArray("months");
            if (!months)
            {
                return months.Error().WithContext("layout.weather.json");
            }
            const Result<std::vector<JsonValue>> monthValues = months.Value().Elements();
            if (!monthValues || monthValues.Value().size() != 3U)
            {
                return Err(ErrorCode::InvalidData,
                           "a season has exactly three months",
                           "layout.weather.json/" + seasonRow.Path() + "/months");
            }
            for (std::size_t monthIndex = 0; monthIndex < 3U; ++monthIndex)
            {
                const Result<std::int64_t> month = monthValues.Value()[monthIndex].AsInt();
                if (!month)
                {
                    return month.Error().WithContext("layout.weather.json");
                }
                if (month.Value() != kSeasonMonths[seasonIndex][monthIndex])
                {
                    return Err(ErrorCode::InvalidData,
                               name.Value() + " must name its calendar months in order",
                               "layout.weather.json/" + seasonRow.Path() + "/months");
                }
            }

            const Result<JsonValue> weightsObject = seasonRow.RequireObject("weights");
            if (!weightsObject)
            {
                return weightsObject.Error().WithContext("layout.weather.json");
            }
            const Result<std::vector<std::pair<std::string, JsonValue>>> weights =
                weightsObject.Value().Members();
            if (!weights)
            {
                return weights.Error().WithContext("layout.weather.json");
            }
            std::unordered_set<util::Id> weighted;
            for (const auto& [targetName, weightValue] : weights.Value())
            {
                const Result<util::Id> target = internKey(
                    targetName, "layout.weather.json/" + seasonRow.Path() + "/weights/" + targetName);
                if (!target)
                {
                    return target.Error();
                }
                const auto targetArchetype = archetypeById.find(target.Value());
                if (targetArchetype == archetypeById.end() || parsed[targetArchetype->second].modifier)
                {
                    return Err(ErrorCode::InvalidData,
                               name.Value() + " weights " + targetName + ", which is not a weather state",
                               "layout.weather.json/" + seasonRow.Path() + "/weights/" + targetName);
                }
                if (!weighted.insert(target.Value()).second)
                {
                    return Err(ErrorCode::Duplicate,
                               targetName + " is weighted twice",
                               "layout.weather.json/" + seasonRow.Path() + "/weights/" + targetName);
                }
                const Result<float> weightValueFloat = weightValue.AsFloat();
                if (!weightValueFloat)
                {
                    return weightValueFloat.Error().WithContext("layout.weather.json");
                }
                if (!std::isfinite(weightValueFloat.Value()) || !(weightValueFloat.Value() >= 0.0F))
                {
                    return Err(ErrorCode::OutOfRange,
                               "a seasonal weight is non-negative; this is " +
                                   std::to_string(weightValueFloat.Value()),
                               "layout.weather.json/" + seasonRow.Path() + "/weights/" + targetName);
                }
                parsed[targetArchetype->second].seasonalWeights[seasonIndex] = weightValueFloat.Value();
            }

            const Result<JsonValue> dwellScaleObject = seasonRow.RequireObject("dwellScale");
            if (!dwellScaleObject)
            {
                return dwellScaleObject.Error().WithContext("layout.weather.json");
            }
            const Result<std::vector<std::pair<std::string, JsonValue>>> dwellScales =
                dwellScaleObject.Value().Members();
            if (!dwellScales)
            {
                return dwellScales.Error().WithContext("layout.weather.json");
            }
            std::unordered_set<util::Id> dwellScaled;
            for (const auto& [targetName, scaleValue] : dwellScales.Value())
            {
                const Result<util::Id> target = internKey(
                    targetName, "layout.weather.json/" + seasonRow.Path() + "/dwellScale/" + targetName);
                if (!target)
                {
                    return target.Error();
                }
                const auto targetArchetype = archetypeById.find(target.Value());
                if (targetArchetype == archetypeById.end() || parsed[targetArchetype->second].modifier)
                {
                    return Err(ErrorCode::InvalidData,
                               name.Value() + " scales dwell for " + targetName +
                                   ", which is not a weather state",
                               "layout.weather.json/" + seasonRow.Path() + "/dwellScale/" + targetName);
                }
                if (!dwellScaled.insert(target.Value()).second)
                {
                    return Err(ErrorCode::Duplicate,
                               targetName + " has its dwell scaled twice",
                               "layout.weather.json/" + seasonRow.Path() + "/dwellScale/" + targetName);
                }
                const Result<float> scale = scaleValue.AsFloat();
                if (!scale)
                {
                    return scale.Error().WithContext("layout.weather.json");
                }
                if (!std::isfinite(scale.Value()) || !(scale.Value() > 0.0F))
                {
                    return Err(ErrorCode::OutOfRange,
                               "a seasonal dwell scale is positive; this is " + std::to_string(scale.Value()),
                               "layout.weather.json/" + seasonRow.Path() + "/dwellScale/" + targetName);
                }
                parsed[targetArchetype->second].seasonalDwellScales[seasonIndex] = scale.Value();
            }
        }

        for (const weather::WeatherArchetype& archetype : parsed)
        {
            if (archetype.modifier)
            {
                continue;
            }
            for (std::size_t seasonIndex = 0; seasonIndex < 4U; ++seasonIndex)
            {
                const float minimum =
                    archetype.dwellMinutes.minimum * archetype.seasonalDwellScales[seasonIndex];
                const float maximum =
                    archetype.dwellMinutes.maximum * archetype.seasonalDwellScales[seasonIndex];
                if (!(minimum >= 25.0F && maximum <= 380.0F))
                {
                    return Err(ErrorCode::OutOfRange,
                               Name(archetype.id) + " has seasonal dwell bounds " + std::to_string(minimum) +
                                   ".." + std::to_string(maximum) +
                                   ", outside §42.1's 25..380 simulated minutes",
                               "layout.weather.json/seasons");
                }
            }
        }

        contents.weatherArchetypes = std::move(parsed);
        contents.weatherTransitions = std::move(transitions);
        contents.weatherRates = rates;
        return util::Ok();
    }

    Result<void> WorldLoader::LoadInteractables(std::string_view directory, WorldData::Contents& contents)
    {
        std::int32_t version = 0;
        const Result<JsonDocument> document = Open(directory, "interactables.json", "interactables", version);
        if (!document)
        {
            return document.Error();
        }

        const Result<JsonValue> array = document.Value().Root().RequireArray("interactables");
        if (!array)
        {
            return array.Error().WithContext("interactables.json");
        }
        const Result<std::vector<JsonValue>> rows = array.Value().Elements();
        if (!rows)
        {
            return rows.Error().WithContext("interactables.json");
        }

        for (const JsonValue& row : rows.Value())
        {
            Interactable item;
            const Result<util::Id> id = RequireId(row, "id");
            if (!id)
            {
                return id.Error().WithContext("interactables.json");
            }
            item.id = id.Value();

            const Result<std::string> kind = row.RequireString("kind");
            if (!kind)
            {
                return kind.Error().WithContext("interactables.json");
            }
            item.kind = kind.Value();

            const Result<util::Id> cell = RequireId(row, "cell");
            if (!cell)
            {
                return cell.Error().WithContext("interactables.json");
            }
            item.cell = cell.Value();

            for (const auto& [field, target] : std::initializer_list<std::pair<std::string_view, util::Id*>>{
                     {"prop", &item.prop}, {"portal", &item.portal}})
            {
                const Result<util::Id> value = OptionalId(row, field);
                if (!value)
                {
                    return value.Error().WithContext("interactables.json");
                }
                *target = value.Value();
            }

            // `focus` is what §15.7 rule 11 proves reachable, so it is required: an interactable
            // with no focus point is one the reachability proof silently skips.
            const Result<JsonValue> focus = row.RequireObject("focus");
            if (!focus)
            {
                return focus.Error().WithContext("interactables.json");
            }
            const Result<Microsoft::Xna::Framework::Vector3> point = focus.Value().RequireVector3("point");
            if (!point)
            {
                return point.Error().WithContext("interactables.json");
            }
            item.focusPoint = point.Value();
            if (focus.Value().Has("normal") && !focus.Value().IsNull("normal"))
            {
                const Result<Microsoft::Xna::Framework::Vector3> normal =
                    focus.Value().RequireVector3("normal");
                if (!normal)
                {
                    return normal.Error().WithContext("interactables.json");
                }
                item.focusNormal = normal.Value();
            }
            const Result<float> radius = focus.Value().OptionalFloat("radius", 0.0F);
            if (!radius)
            {
                return radius.Error().WithContext("interactables.json");
            }
            if (radius.Value() < 0.0F)
            {
                return Err(ErrorCode::OutOfRange,
                           "a focus radius is not negative; this is " + std::to_string(radius.Value()),
                           "interactables.json/" + row.Path() + "/focus/radius");
            }
            item.focusRadius = radius.Value();

            if (row.Has("bounds") && !row.IsNull("bounds"))
            {
                const Result<JsonValue> bounds = row.RequireObject("bounds");
                if (!bounds)
                {
                    return bounds.Error().WithContext("interactables.json");
                }
                const Result<Microsoft::Xna::Framework::Vector3> low = bounds.Value().RequireVector3("min");
                if (!low)
                {
                    return low.Error().WithContext("interactables.json");
                }
                const Result<Microsoft::Xna::Framework::Vector3> high = bounds.Value().RequireVector3("max");
                if (!high)
                {
                    return high.Error().WithContext("interactables.json");
                }
                if (high.Value().X < low.Value().X || high.Value().Y < low.Value().Y ||
                    high.Value().Z < low.Value().Z)
                {
                    return Err(ErrorCode::InvalidData,
                               "bounds are min..max; this box has a max below its min on at least "
                               "one axis, which makes it a box nothing is ever inside",
                               "interactables.json/" + row.Path() + "/bounds");
                }
                item.boundsMin = low.Value();
                item.boundsMax = high.Value();
            }

            // The state comes BEFORE the actions, because the actions are parsed against it. That
            // ordering is the whole reason the vocabulary can be closed without a global list of
            // setter verbs.
            if (row.Has("state") && !row.IsNull("state"))
            {
                const Result<JsonValue> state = row.RequireObject("state");
                if (!state)
                {
                    return state.Error().WithContext("interactables.json");
                }
                const Result<std::vector<std::pair<std::string, JsonValue>>> fields = state.Value().Members();
                if (!fields)
                {
                    return fields.Error().WithContext("interactables.json");
                }
                for (const auto& [name, value] : fields.Value())
                {
                    switch (value.GetKind())
                    {
                        case util::JsonValue::Kind::Boolean:
                        {
                            const Result<bool> flag = state.Value().RequireBool(name);
                            if (!flag)
                            {
                                return flag.Error().WithContext("interactables.json");
                            }
                            item.state.Declare(name, flag.Value());
                            break;
                        }
                        case util::JsonValue::Kind::Number:
                        {
                            const Result<double> number = state.Value().RequireNumber(name);
                            if (!number)
                            {
                                return number.Error().WithContext("interactables.json");
                            }
                            item.state.Declare(name, number.Value());
                            break;
                        }
                        case util::JsonValue::Kind::String:
                        {
                            const Result<std::string> text = state.Value().RequireString(name);
                            if (!text)
                            {
                                return text.Error().WithContext("interactables.json");
                            }
                            item.state.Declare(name, text.Value());
                            break;
                        }
                        default:
                            // Arrays and objects are not state the expression vocabulary can talk
                            // about. `contents[]` in §50.4 is a behaviour's own storage, not an
                            // expression field, and silently accepting it here would let a
                            // predicate name something it can never compare.
                            return Err(ErrorCode::InvalidData,
                                       "a state field is a boolean, a number or a string; \"" + name +
                                           "\" is neither, and an expression could not compare it",
                                       "interactables.json/" + row.Path() + "/state/" + name);
                    }
                }
            }

            const Result<JsonValue> actions = row.RequireArray("actions");
            if (!actions)
            {
                return actions.Error().WithContext("interactables.json");
            }
            const Result<std::vector<JsonValue>> actionRows = actions.Value().Elements();
            if (!actionRows)
            {
                return actionRows.Error().WithContext("interactables.json");
            }
            for (const JsonValue& actionRow : actionRows.Value())
            {
                InteractableAction action;
                const Result<std::string> verb = actionRow.RequireString("verb");
                if (!verb)
                {
                    return verb.Error().WithContext("interactables.json");
                }
                action.verb = verb.Value();

                const auto expression = [&actionRow](std::string_view field) -> Result<std::string>
                {
                    if (!actionRow.Has(field) || actionRow.IsNull(field))
                    {
                        return std::string{};
                    }
                    return actionRow.RequireString(field);
                };

                const Result<std::string> when = expression("when");
                if (!when)
                {
                    return when.Error().WithContext("interactables.json");
                }
                const Result<Predicate> predicate = Predicate::Parse(when.Value(), item.state);
                if (!predicate)
                {
                    // The acceptance criterion: the file, the id, and the token. The parser
                    // supplies the token and the offset; this supplies the other two.
                    return predicate.Error()
                        .WithContext(Name(item.id) + " " + action.verb + " when")
                        .WithContext("interactables.json");
                }
                action.when = predicate.Value();

                const Result<std::string> effectText = expression("do");
                if (!effectText)
                {
                    return effectText.Error().WithContext("interactables.json");
                }
                const Result<Effect> effect = Effect::Parse(effectText.Value(), item.state);
                if (!effect)
                {
                    return effect.Error()
                        .WithContext(Name(item.id) + " " + action.verb + " do")
                        .WithContext("interactables.json");
                }
                action.effect = effect.Value();

                const Result<util::Id> sound = OptionalId(actionRow, "sound");
                if (!sound)
                {
                    return sound.Error().WithContext("interactables.json");
                }
                action.sound = sound.Value();

                if (actionRow.Has("anim") && !actionRow.IsNull("anim"))
                {
                    const Result<std::string> anim = actionRow.RequireString("anim");
                    if (!anim)
                    {
                        return anim.Error().WithContext("interactables.json");
                    }
                    action.anim = anim.Value();
                }

                const Result<float> duration = actionRow.OptionalFloat("duration", 0.0F);
                if (!duration)
                {
                    return duration.Error().WithContext("interactables.json");
                }
                if (duration.Value() < 0.0F)
                {
                    return Err(ErrorCode::OutOfRange,
                               "a duration is not negative; this is " + std::to_string(duration.Value()),
                               "interactables.json/" + actionRow.Path() + "/duration");
                }
                action.duration = duration.Value();

                item.actions.push_back(std::move(action));
            }

            if (row.Has("childInteractables") && !row.IsNull("childInteractables"))
            {
                const Result<JsonValue> children = row.RequireArray("childInteractables");
                if (!children)
                {
                    return children.Error().WithContext("interactables.json");
                }
                const Result<std::vector<JsonValue>> childRows = children.Value().Elements();
                if (!childRows)
                {
                    return childRows.Error().WithContext("interactables.json");
                }
                for (const JsonValue& child : childRows.Value())
                {
                    const Result<std::string> name = child.AsString();
                    if (!name)
                    {
                        return name.Error().WithContext("interactables.json");
                    }
                    item.childInteractables.push_back(util::Intern(name.Value()));
                }
            }

            // `persist` names exactly the fields the save carries, so a name that is not a state
            // field is a field the save would look for and never find -- and §65's ~90 KB budget
            // depends on the list being exactly right.
            if (row.Has("persist") && !row.IsNull("persist"))
            {
                const Result<JsonValue> persist = row.RequireArray("persist");
                if (!persist)
                {
                    return persist.Error().WithContext("interactables.json");
                }
                const Result<std::vector<JsonValue>> persistRows = persist.Value().Elements();
                if (!persistRows)
                {
                    return persistRows.Error().WithContext("interactables.json");
                }
                for (const JsonValue& entry : persistRows.Value())
                {
                    const Result<std::string> name = entry.AsString();
                    if (!name)
                    {
                        return name.Error().WithContext("interactables.json");
                    }
                    if (item.state.Find(name.Value()) == nullptr)
                    {
                        return Err(ErrorCode::InvalidData,
                                   "\"" + name.Value() +
                                       "\" is persisted and is not a state field; this "
                                       "interactable declares " +
                                       item.state.Names(),
                                   "interactables.json/" + entry.Path());
                    }
                    item.persist.push_back(name.Value());
                }
            }

            if (row.Has("audio") && !row.IsNull("audio"))
            {
                const Result<JsonValue> audio = row.RequireObject("audio");
                if (!audio)
                {
                    return audio.Error().WithContext("interactables.json");
                }
                const Result<util::Id> loop = OptionalId(audio.Value(), "loop");
                if (!loop)
                {
                    return loop.Error().WithContext("interactables.json");
                }
                item.audioLoop = loop.Value();
                if (audio.Value().Has("emitter") && !audio.Value().IsNull("emitter"))
                {
                    const Result<Microsoft::Xna::Framework::Vector3> emitter =
                        audio.Value().RequireVector3("emitter");
                    if (!emitter)
                    {
                        return emitter.Error().WithContext("interactables.json");
                    }
                    item.audioEmitter = emitter.Value();
                }
            }

            contents.interactables.push_back(std::move(item));
        }

        return util::Ok();
    }

    Result<void> WorldLoader::LoadInitialState(std::string_view directory, WorldData::Contents& contents)
    {
        std::int32_t version = 0;
        const Result<JsonDocument> document = Open(directory, "initialstate.json", "initialstate", version);
        if (!document)
        {
            return document.Error();
        }
        const JsonValue& root = document.Value().Root();
        InitialState& initial = contents.initialState;

        const Result<JsonValue> player = root.RequireObject("player");
        if (!player)
        {
            return player.Error().WithContext("initialstate.json");
        }
        const Result<util::Id> playerCell = RequireId(player.Value(), "cell");
        if (!playerCell)
        {
            return playerCell.Error().WithContext("initialstate.json");
        }
        initial.player.cell = playerCell.Value();
        const Result<Microsoft::Xna::Framework::Vector3> spawn = player.Value().RequireVector3("position");
        if (!spawn)
        {
            return spawn.Error().WithContext("initialstate.json");
        }
        initial.player.position = spawn.Value();
        const Result<float> yaw = player.Value().OptionalFloat("yawDeg", 0.0F);
        if (!yaw)
        {
            return yaw.Error().WithContext("initialstate.json");
        }
        initial.player.yawDeg = yaw.Value();

        const Result<JsonValue> clock = root.RequireObject("clock");
        if (!clock)
        {
            return clock.Error().WithContext("initialstate.json");
        }
        const Result<double> epoch = clock.Value().RequireNumber("epochSeconds");
        if (!epoch)
        {
            return epoch.Error().WithContext("initialstate.json");
        }
        initial.clock.epochSeconds = epoch.Value();
        const Result<float> scale = clock.Value().RequireFloat("timeScale");
        if (!scale)
        {
            return scale.Error().WithContext("initialstate.json");
        }
        if (scale.Value() < 0.0F)
        {
            return Err(ErrorCode::OutOfRange,
                       "a time scale is not negative; this is " + std::to_string(scale.Value()) +
                           ", and a clock that runs backwards is a sunrise in the west",
                       "initialstate.json/clock/timeScale");
        }
        initial.clock.timeScale = scale.Value();

        const Result<float> latitude = clock.Value().OptionalFloat("latitudeDeg", 0.0F);
        if (!latitude)
        {
            return latitude.Error().WithContext("initialstate.json");
        }
        if (latitude.Value() < -90.0F || latitude.Value() > 90.0F)
        {
            return Err(ErrorCode::OutOfRange,
                       "a latitude is -90..90; this is " + std::to_string(latitude.Value()),
                       "initialstate.json/clock/latitudeDeg");
        }
        initial.clock.latitudeDeg = latitude.Value();

        const Result<float> longitude = clock.Value().OptionalFloat("longitudeDeg", 0.0F);
        if (!longitude)
        {
            return longitude.Error().WithContext("initialstate.json");
        }
        if (longitude.Value() < -180.0F || longitude.Value() > 180.0F)
        {
            return Err(ErrorCode::OutOfRange,
                       "a longitude is -180..180; this is " + std::to_string(longitude.Value()),
                       "initialstate.json/clock/longitudeDeg");
        }
        initial.clock.longitudeDeg = longitude.Value();

        const Result<std::int64_t> offset = clock.Value().OptionalInt("utcOffsetMinutes", 0);
        if (!offset)
        {
            return offset.Error().WithContext("initialstate.json");
        }
        initial.clock.utcOffsetMinutes = static_cast<std::int32_t>(offset.Value());

        if (root.Has("weather") && !root.IsNull("weather"))
        {
            const Result<JsonValue> weather = root.RequireObject("weather");
            if (!weather)
            {
                return weather.Error().WithContext("initialstate.json");
            }
            const Result<util::Id> target = OptionalId(weather.Value(), "target");
            if (!target)
            {
                return target.Error().WithContext("initialstate.json");
            }
            initial.weather.target = target.Value();
            weather::WeatherState& state = initial.weather.state;
            const auto readFloat = [&](std::string_view field, float& destination) -> Result<void>
            {
                const Result<float> value = weather.Value().OptionalFloat(field, destination);
                if (!value)
                {
                    return value.Error().WithContext("initialstate.json");
                }
                destination = value.Value();
                return util::Ok();
            };
            for (const auto& [field, destination] :
                 std::initializer_list<std::pair<std::string_view, float*>>{
                     {"cloudCover", &state.cloudCover},
                     {"cloudCumuliform", &state.cloudCumuliform},
                     {"precipIntensity", &state.precipIntensity},
                     {"windSpeed", &state.windSpeed},
                     {"windDirectionDeg", &state.windDirectionDeg},
                     {"gustFactor", &state.gustFactor},
                     {"fogDensity", &state.fogDensity},
                     {"thunderIntensity", &state.thunderIntensity},
                     {"temperatureC", &state.temperatureC},
                     {"humidity", &state.humidity},
                     {"surfaceWetness", &state.surfaceWetness},
                     {"snowDepth", &state.snowDepth}})
            {
                if (const Result<void> read = readFloat(field, *destination); !read)
                {
                    return read.Error();
                }
            }

            const Result<std::string> precip = weather.Value().OptionalString(
                "precipType", std::string(weather::PrecipTypeName(state.precipType)));
            if (!precip)
            {
                return precip.Error().WithContext("initialstate.json");
            }
            const Result<weather::PrecipType> precipType =
                weather::ParsePrecipType(precip.Value(), "initialstate.json/weather/precipType");
            if (!precipType)
            {
                return precipType.Error();
            }
            state.precipType = precipType.Value();

            const Result<float> expiry = weather.Value().OptionalFloat("targetExpiryMinutes", 0.0F);
            if (!expiry)
            {
                return expiry.Error().WithContext("initialstate.json");
            }
            if (!std::isfinite(expiry.Value()) || expiry.Value() < 0.0F || expiry.Value() > 380.0F)
            {
                return Err(ErrorCode::OutOfRange,
                           "target expiry must be 0..380 simulated minutes",
                           "initialstate.json/weather/targetExpiryMinutes");
            }
            initial.weather.targetExpiryMinutes = expiry.Value();

            const Result<std::string> seedText = weather.Value().OptionalString("rngState", "");
            if (!seedText)
            {
                return seedText.Error().WithContext("initialstate.json");
            }
            if (!seedText.Value().empty())
            {
                const std::string_view text = seedText.Value();
                std::uint64_t seed = 0U;
                const char* first = text.data() + (text.starts_with("0x") ? 2 : 0);
                const char* last = text.data() + text.size();
                const auto parsed = std::from_chars(first, last, seed, 16);
                if (!text.starts_with("0x") || text.size() != 18U || parsed.ec != std::errc{} ||
                    parsed.ptr != last)
                {
                    return Err(ErrorCode::InvalidData,
                               "a fresh weather RNG seed is 0x followed by 16 hexadecimal digits",
                               "initialstate.json/weather/rngState");
                }
                state.rngState = util::Rng(seed).GetState();
            }
            if (const Result<void> valid = state.Validate(); !valid)
            {
                return valid.Error().WithContext("initialstate.json/weather");
            }
        }

        // The interactable block is the canonical state table's opening values. Each override is
        // checked against the field the interactable DECLARES -- name and type -- because this file
        // is what every delta save is taken against: a field here that the interactable does not
        // have is a value the save would carry for ever and nothing would ever read.
        //
        // A block naming an interactable that does not exist is NOT reported here: that is §15.7
        // rule 6, it has one owner, and a second message for it would be a worse one.
        if (root.Has("interactables") && !root.IsNull("interactables"))
        {
            const Result<JsonValue> block = root.RequireObject("interactables");
            if (!block)
            {
                return block.Error().WithContext("initialstate.json");
            }
            const Result<std::vector<std::pair<std::string, JsonValue>>> rows = block.Value().Members();
            if (!rows)
            {
                return rows.Error().WithContext("initialstate.json");
            }
            for (const auto& [name, values] : rows.Value())
            {
                InteractableStart start;
                start.id = util::Intern(name);

                const Interactable* declared = nullptr;
                for (const Interactable& candidate : contents.interactables)
                {
                    if (candidate.id == start.id)
                    {
                        declared = &candidate;
                        break;
                    }
                }

                if (values.GetKind() != util::JsonValue::Kind::Object)
                {
                    return Err(ErrorCode::InvalidData,
                               "an interactable's initial state is an object of field values",
                               "initialstate.json/interactables/" + name);
                }
                const Result<std::vector<std::pair<std::string, JsonValue>>> fields = values.Members();
                if (!fields)
                {
                    return fields.Error().WithContext("initialstate.json");
                }
                for (const auto& [field, value] : fields.Value())
                {
                    StateValue read{false};
                    switch (value.GetKind())
                    {
                        case util::JsonValue::Kind::Boolean:
                        {
                            const Result<bool> flag = values.RequireBool(field);
                            if (!flag)
                            {
                                return flag.Error().WithContext("initialstate.json");
                            }
                            read = flag.Value();
                            break;
                        }
                        case util::JsonValue::Kind::Number:
                        {
                            const Result<double> number = values.RequireNumber(field);
                            if (!number)
                            {
                                return number.Error().WithContext("initialstate.json");
                            }
                            read = number.Value();
                            break;
                        }
                        case util::JsonValue::Kind::String:
                        {
                            const Result<std::string> text = values.RequireString(field);
                            if (!text)
                            {
                                return text.Error().WithContext("initialstate.json");
                            }
                            read = text.Value();
                            break;
                        }
                        default:
                            return Err(ErrorCode::InvalidData,
                                       "a state value is a boolean, a number or a string; \"" + field +
                                           "\" is neither",
                                       "initialstate.json/interactables/" + name + "/" + field);
                    }

                    if (declared != nullptr)
                    {
                        const StateTable::Field* slot = declared->state.Find(field);
                        if (slot == nullptr)
                        {
                            return Err(ErrorCode::InvalidData,
                                       "\"" + field + "\" is not a state field of " + name +
                                           "; it declares " + declared->state.Names(),
                                       "initialstate.json/interactables/" + name + "/" + field);
                        }
                        if (slot->value.index() != read.index())
                        {
                            return Err(ErrorCode::InvalidData,
                                       "\"" + field + "\" starts as a different type from the one " + name +
                                           " declares for it",
                                       "initialstate.json/interactables/" + name + "/" + field);
                        }
                    }
                    start.overrides.Declare(field, read);
                }
                initial.interactables.push_back(std::move(start));
            }
        }

        if (root.Has("pets") && !root.IsNull("pets"))
        {
            const Result<JsonValue> block = root.RequireObject("pets");
            if (!block)
            {
                return block.Error().WithContext("initialstate.json");
            }
            const Result<std::vector<std::pair<std::string, JsonValue>>> rows = block.Value().Members();
            if (!rows)
            {
                return rows.Error().WithContext("initialstate.json");
            }
            for (const auto& [name, values] : rows.Value())
            {
                PetStart pet;
                pet.id = util::Intern(name);
                const Result<util::Id> cell = OptionalId(values, "cell");
                if (!cell)
                {
                    return cell.Error().WithContext("initialstate.json");
                }
                pet.cell = cell.Value();
                const Result<std::string> state = values.OptionalString("state", "");
                if (!state)
                {
                    return state.Error().WithContext("initialstate.json");
                }
                pet.state = state.Value();
                initial.pets.push_back(std::move(pet));
            }
        }

        return util::Ok();
    }

    Result<WorldData> WorldLoader::Load(std::string_view directory)
    {
        const Result<WorldManifest> manifest = LoadManifest(directory);
        if (!manifest)
        {
            return manifest.Error();
        }
        if (const Result<void> verified = VerifyManifest(directory, manifest.Value()); !verified)
        {
            return verified.Error();
        }

        WorldData::Contents contents;
        if (const Result<void> levels = LoadLevels(directory, contents); !levels)
        {
            return levels.Error();
        }

        if (const Result<void> materials = LoadMaterials(directory, contents); !materials)
        {
            return materials.Error();
        }

        if (const Result<void> cells = LoadCells(directory, contents); !cells)
        {
            return cells.Error();
        }

        if (const Result<void> portals = LoadPortals(directory, contents); !portals)
        {
            return portals.Error();
        }
        if (const Result<void> openings = LoadOpenings(directory, contents); !openings)
        {
            return openings.Error();
        }
        if (const Result<void> stairs = LoadStairs(directory, contents); !stairs)
        {
            return stairs.Error();
        }
        if (const Result<void> lights = LoadLights(directory, contents); !lights)
        {
            return lights.Error();
        }
        if (const Result<void> props = LoadProps(directory, contents); !props)
        {
            return props.Error();
        }
        if (const Result<void> nav = LoadNav(directory, contents); !nav)
        {
            return nav.Error();
        }
        if (const Result<void> audio = LoadAudio(directory, contents); !audio)
        {
            return audio.Error();
        }
        if (const Result<void> exterior = LoadExterior(directory, contents); !exterior)
        {
            return exterior.Error();
        }
        if (const Result<void> weather = LoadWeather(directory, contents); !weather)
        {
            return weather.Error();
        }
        if (const Result<void> interactables = LoadInteractables(directory, contents); !interactables)
        {
            return interactables.Error();
        }
        if (const Result<void> initial = LoadInitialState(directory, contents); !initial)
        {
            return initial.Error();
        }

        util::Log::Info(util::LogCat::World,
                        "loaded {} level(s), {} material(s) and {} cell(s) from {}",
                        contents.levels.size(),
                        contents.materials.size(),
                        contents.cells.size(),
                        directory);
        Result<WorldData> world = WorldData::Create(std::move(contents));
        if (!world)
        {
            return world;
        }

        // §15.7's whole-world rules, in a debug build, at load (`HOUSE-00357`). The gate on every
        // commit is `tools/world/validate_world.py`; this is the same statement made by the code
        // that reads the house, and it runs here so that a world edited by hand between commits --
        // or one shipped in a build somebody else made -- is still checked before anything trusts
        // it. `Fast` and not `Full`: rule 11 samples a floor per interactable and belongs in the
        // test that mirrors the Python gate, not in every debug launch.
        //
        // It reports rather than refuses. A world that breaks a whole-world rule is still a world
        // the loader read successfully, and a designer running the game to look at a room they
        // have half-moved should see the list, not a black screen.
#if !defined(NDEBUG)
        for (const ValidationProblem& problem : WorldValidator::Validate(world.Value()))
        {
            util::Log::Warn(util::LogCat::World, "§15.7 rule {}: {}", problem.rule, problem.ToString());
        }
#endif
        return world;
    }

} // namespace cnahouse::world
