// SPDX-License-Identifier: MIT
#include "cnahouse/content/ContentRegistry.hpp"

#include <algorithm>
#include <format>

#include "cnahouse/util/Json.hpp"
#include "cnahouse/util/Log.hpp"

namespace cnahouse::content
{
    namespace
    {
        constexpr std::string_view kSchema = "cna-house/assets/1";
    } // namespace

    std::string_view AssetKindName(AssetKind kind) noexcept
    {
        switch (kind)
        {
            case AssetKind::Model:
                return "model";
            case AssetKind::Texture:
                return "texture";
            case AssetKind::Sound:
                return "sound";
            case AssetKind::Font:
                return "font";
            case AssetKind::Effect:
                return "effect";
            case AssetKind::Video:
                return "video";
            case AssetKind::Unknown:
                return "unknown";
        }
        return "unknown";
    }

    AssetKind ParseAssetKind(std::string_view name) noexcept
    {
        if (name == "model")
        {
            return AssetKind::Model;
        }
        if (name == "texture")
        {
            return AssetKind::Texture;
        }
        if (name == "sound")
        {
            return AssetKind::Sound;
        }
        if (name == "font")
        {
            return AssetKind::Font;
        }
        if (name == "effect")
        {
            return AssetKind::Effect;
        }
        if (name == "video")
        {
            return AssetKind::Video;
        }
        return AssetKind::Unknown;
    }

    util::Result<void> ContentRegistry::LoadFromJson(std::string_view text, std::string_view name)
    {
        auto document = util::JsonDocument::Parse(text, std::string(name));
        if (!document)
        {
            return document.Error();
        }
        const util::JsonValue& root = document->Root();

        auto schema = root.RequireString("schema");
        if (!schema)
        {
            return schema.Error();
        }
        if (*schema != kSchema)
        {
            // A version mismatch is its own error code, not a generic parse failure: the caller may
            // one day migrate, and it cannot distinguish "wrong version" from "corrupt" without this.
            return util::Err(util::ErrorCode::VersionMismatch,
                             std::format("expected schema '{}', found '{}'", kSchema, *schema),
                             std::string(name));
        }

        auto assets = root.RequireArray("assets");
        if (!assets)
        {
            return assets.Error();
        }
        auto rows = assets->Elements();
        if (!rows)
        {
            return rows.Error();
        }

        Clear();
        // EVERY bad row is reported, not the first. Fixing forty authoring mistakes one build at a
        // time is intolerable, and the manifest is exactly the kind of file that accumulates them
        // (`docs/conventions.md` §5.1).
        std::vector<std::string> problems;
        entries_.reserve(rows->size());

        for (const util::JsonValue& row : *rows)
        {
            AssetEntry entry;

            auto id = row.RequireString("id");
            if (!id)
            {
                problems.push_back(id.Error().ToString());
                continue;
            }
            entry.name = *id;
            entry.id = util::Intern(entry.name);
            if (util::IdRegistry::HadConflict())
            {
                // `HOUSE-00026`: a hash collision would make two assets the same asset, which would
                // look like manifest corruption and be nearly impossible to trace back to a hash.
                problems.push_back(std::format("asset id '{}' collides with '{}' -- rename one of them",
                                               util::IdRegistry::ConflictIncoming(),
                                               util::IdRegistry::ConflictExisting()));
                util::IdRegistry::ClearConflict();
                continue;
            }

            auto contentName = row.RequireString("contentName");
            if (!contentName)
            {
                problems.push_back(contentName.Error().ToString());
                continue;
            }
            entry.contentName = *contentName;

            auto kind = row.RequireString("kind");
            if (!kind)
            {
                problems.push_back(kind.Error().ToString());
                continue;
            }
            entry.kind = ParseAssetKind(*kind);
            if (entry.kind == AssetKind::Unknown)
            {
                problems.push_back(std::format("{}: unknown asset kind '{}'", row.Path(), *kind));
                continue;
            }

            auto pack = row.OptionalString("residencyPack", "core");
            if (!pack)
            {
                problems.push_back(pack.Error().ToString());
                continue;
            }
            entry.pack = *pack;

            if (byName_.contains(entry.name))
            {
                problems.push_back(std::format("{}: asset id '{}' appears twice", row.Path(), entry.name));
                continue;
            }

            byId_.emplace(entry.id, entries_.size());
            byName_.emplace(entry.name, entries_.size());
            entries_.push_back(std::move(entry));
        }

        if (!problems.empty())
        {
            std::string combined = std::format("{} row(s) are invalid:", problems.size());
            for (const std::string& problem : problems)
            {
                combined += "\n  " + problem;
            }
            return util::Err(util::ErrorCode::InvalidData, std::move(combined), std::string(name));
        }

        util::Log::Info(util::LogCat::Content,
                        "manifest '{}': {} assets across {} pack(s)",
                        name,
                        entries_.size(),
                        Packs().size());
        return {};
    }

    const AssetEntry* ContentRegistry::Find(util::Id id) const
    {
        const auto it = byId_.find(id);
        return it == byId_.end() ? nullptr : &entries_[it->second];
    }

    const AssetEntry* ContentRegistry::Find(std::string_view name) const
    {
        const auto it = byName_.find(std::string(name));
        return it == byName_.end() ? nullptr : &entries_[it->second];
    }

    std::vector<const AssetEntry*> ContentRegistry::InPack(std::string_view pack) const
    {
        std::vector<const AssetEntry*> found;
        for (const AssetEntry& entry : entries_)
        {
            if (entry.pack == pack)
            {
                found.push_back(&entry);
            }
        }
        return found;
    }

    std::vector<std::string> ContentRegistry::Packs() const
    {
        std::vector<std::string> packs;
        for (const AssetEntry& entry : entries_)
        {
            if (std::find(packs.begin(), packs.end(), entry.pack) == packs.end())
            {
                packs.push_back(entry.pack);
            }
        }
        return packs;
    }

    void ContentRegistry::Clear()
    {
        entries_.clear();
        byId_.clear();
        byName_.clear();
    }

} // namespace cnahouse::content
