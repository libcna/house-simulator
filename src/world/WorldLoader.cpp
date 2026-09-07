// SPDX-License-Identifier: MIT
#include "cnahouse/world/WorldLoader.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <utility>
#include <vector>

#include "System/IO/Directory.hpp"
#include "System/IO/File.hpp"

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

    Result<WorldData> WorldLoader::Load(std::string_view directory)
    {
        const Result<WorldManifest> manifest = LoadManifest(directory);
        if (!manifest)
        {
            return manifest.Error();
        }

        WorldData::Contents contents;
        if (const Result<void> levels = LoadLevels(directory, contents); !levels)
        {
            return levels.Error();
        }

        util::Log::Info(util::LogCat::World, "loaded {} level(s) from {}", contents.levels.size(), directory);
        return WorldData::Create(std::move(contents));
    }

} // namespace cnahouse::world
