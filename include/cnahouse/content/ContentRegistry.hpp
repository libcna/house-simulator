// SPDX-License-Identifier: MIT
#pragma once

#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "cnahouse/util/Ids.hpp"
#include "cnahouse/util/Result.hpp"

namespace cnahouse::content
{

    /// @brief What kind of thing an asset is. Decides which cache loads it.
    enum class AssetKind
    {
        Model,
        Texture,
        Sound,
        Font,
        Effect,
        Video,
        Unknown,
    };

    [[nodiscard]] std::string_view AssetKindName(AssetKind kind) noexcept;
    [[nodiscard]] AssetKind ParseAssetKind(std::string_view name) noexcept;

    /// @brief One row of `assets.manifest.json`, reduced to what the RUNTIME needs.
    ///
    /// The manifest on disk carries far more (`cna-house.md` §20.3): source hashes, licence, review
    /// status, triangle counts. All of that is for the offline tooling and the phase-4 audit, and none
    /// of it belongs in memory at run time -- so this struct is deliberately the small subset the game
    /// actually reads, and the loader ignores the rest rather than modelling it.
    struct AssetEntry
    {
        /// @brief `MODEL_PROP_KITCHEN_FRIDGE_01`. Interned, so lookups are integer comparisons.
        util::Id id;
        std::string name;
        AssetKind kind = AssetKind::Unknown;
        /// @brief The CONTENT NAME, e.g. `Models/Kitchen/fridge_01` -- never an OS path (§8.3).
        std::string contentName;
        /// @brief Which residency pack this asset belongs to, e.g. `house-l0`.
        std::string pack;
    };

    /// @brief The content name ↔ asset id ↔ pack mapping, loaded once from `assets.manifest.json`.
    ///
    /// **Why an indirection at all**, when `ContentManager::Load<T>("Models/…")` already works. Three
    /// reasons the design needs, none of them optional:
    ///
    ///  * **Residency.** The streaming system promotes and evicts by *pack*, and nothing in a content
    ///    name says which pack an asset is in. That mapping has to live somewhere, and here is where it
    ///    is authored.
    ///  * **Ids.** World data references props by id, not by path, so that renaming a file is not a
    ///    world-data migration. The registry is what turns one into the other.
    ///  * **Validation.** A missing asset should be caught when the manifest is loaded -- at startup,
    ///    naming the row -- rather than as a `ContentLoadException` at the moment a player walks into
    ///    the room.
    class ContentRegistry
    {
    public:
        /// @brief Parses a manifest. Reports EVERY bad row, not the first.
        ///
        /// Accumulating errors is `docs/conventions.md` §5.1's rule for a validated batch, and the
        /// reason is practical: fixing forty authoring mistakes one build at a time is intolerable.
        [[nodiscard]] util::Result<void> LoadFromJson(std::string_view text, std::string_view name);

        [[nodiscard]] const AssetEntry* Find(util::Id id) const;
        [[nodiscard]] const AssetEntry* Find(std::string_view name) const;

        [[nodiscard]] std::size_t Count() const noexcept
        {
            return entries_.size();
        }

        [[nodiscard]] const std::vector<AssetEntry>& All() const noexcept
        {
            return entries_;
        }

        /// @brief Every asset in a pack, for the residency system to promote or evict together.
        [[nodiscard]] std::vector<const AssetEntry*> InPack(std::string_view pack) const;

        /// @brief The distinct pack names, in first-seen order.
        [[nodiscard]] std::vector<std::string> Packs() const;

        void Clear();

    private:
        std::vector<AssetEntry> entries_;
        std::unordered_map<util::Id, std::size_t> byId_;
        std::unordered_map<std::string, std::size_t> byName_;
    };

} // namespace cnahouse::content
