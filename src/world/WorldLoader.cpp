// SPDX-License-Identifier: MIT
#include "cnahouse/world/WorldLoader.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <optional>
#include <utility>
#include <vector>

#include "Microsoft/Xna/Framework/Vector2.hpp"
#include "Microsoft/Xna/Framework/Vector3.hpp"

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
            Material material;
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
                     {"navMeshRegion", &cell.navMeshRegion}})
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
        BoundaryRuns(const Cell& cell, PlaneAxis axis, float value)
        {
            std::vector<std::pair<float, float>> runs;
            for (const Footprint& box : cell.boxes)
            {
                if (axis == PlaneAxis::X)
                {
                    if (std::abs(box.minX - value) <= kPlaneTolerance ||
                        std::abs(box.maxX - value) <= kPlaneTolerance)
                    {
                        runs.emplace_back(box.minZ, box.maxZ);
                    }
                }
                else
                {
                    if (std::abs(box.minZ - value) <= kPlaneTolerance ||
                        std::abs(box.maxZ - value) <= kPlaneTolerance)
                    {
                        runs.emplace_back(box.minX, box.maxX);
                    }
                }
            }
            return runs;
        }

        /// @brief §15.7 rule 4 for one side of one portal.
        [[nodiscard]] Result<void> CheckPortalSide(const WorldData::Contents& contents,
                                                   const Portal& portal,
                                                   util::Id cellId,
                                                   std::string_view side,
                                                   const std::string& where)
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
                BoundaryRuns(*cell, portal.axis, portal.planeValue);
            if (runs.empty())
            {
                return Err(ErrorCode::InvalidData,
                           "cell " + Name(cellId) + " (" + std::string(side) + ") has no face on " +
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
            for (const auto& [cellId, side] : std::initializer_list<std::pair<util::Id, std::string_view>>{
                     {portal.cellA, "cellA"}, {portal.cellB, "cellB"}})
            {
                if (const Result<void> checked = CheckPortalSide(contents, portal, cellId, side, where);
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

            const auto text = [&row](std::string_view field) -> Result<std::string>
            {
                if (!row.Has(field) || row.IsNull(field))
                {
                    return std::string{};
                }
                return row.RequireString(field);
            };
            const Result<std::string> swing = text("swing");
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

        util::Log::Info(util::LogCat::World,
                        "loaded {} level(s), {} material(s) and {} cell(s) from {}",
                        contents.levels.size(),
                        contents.materials.size(),
                        contents.cells.size(),
                        directory);
        return WorldData::Create(std::move(contents));
    }

} // namespace cnahouse::world
