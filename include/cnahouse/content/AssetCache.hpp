// SPDX-License-Identifier: MIT
#pragma once

#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "cnahouse/util/Log.hpp"

namespace Microsoft::Xna::Framework::Content
{
    class ContentManager;
}

namespace cnahouse::content
{

    /// @brief What a cache did when an asset failed to load.
    enum class LoadOutcome
    {
        Loaded,   ///< The asset loaded.
        Cached,   ///< Already loaded; returned from the cache.
        FellBack, ///< The asset failed and the typed fallback was returned instead.
    };

    /// @brief A typed cache over `ContentManager`, with a fallback and a load-failure policy.
    ///
    /// **The policy, which is the whole design (`HOUSE-00143`'s acceptance):** a missing asset yields
    /// the typed fallback, logs **once**, and does **not throw**.
    ///
    ///  * **Yields the fallback**, because a house with one missing prop should still be walkable. An
    ///    exception here would turn a content mistake into an unplayable build, and content mistakes
    ///    are the most common kind this project will have.
    ///  * **Logs once**, because the alternative is a per-frame message: a missing texture is requested
    ///    every time the room is drawn, and at 60 Hz one bad asset would bury the entire log within a
    ///    second. `util::Log` rate-limits identical messages per frame, but per-frame is not enough
    ///    here -- the same asset fails every frame forever, so the cache remembers what it has already
    ///    complained about.
    ///  * **Does not throw**, because the caller is a draw path, and `docs/conventions.md` §5.2 forbids
    ///    exceptions across a per-frame path.
    ///
    /// The cache is a template over the asset type and the loader, so `TextureCache`, `ModelCache`,
    /// `SoundCache` and `EffectCache` are one implementation rather than four -- and so their policies
    /// cannot drift apart, which is what would happen if they were written separately.
    template<typename T>
    class AssetCache
    {
    public:
        using Loader =
            std::function<T(Microsoft::Xna::Framework::Content::ContentManager&, const std::string&)>;

        AssetCache(Microsoft::Xna::Framework::Content::ContentManager& content,
                   Loader loader,
                   std::string fallbackContentName,
                   util::LogCat category)
            : content_(&content)
            , loader_(std::move(loader))
            , fallbackName_(std::move(fallbackContentName))
            , category_(category)
        {
        }

        /// @brief The asset named @p contentName, or the fallback if it cannot be loaded.
        [[nodiscard]] T* Get(std::string_view contentName)
        {
            LoadOutcome ignored = LoadOutcome::Cached;
            return Get(contentName, ignored);
        }

        /// @brief `Get`, reporting what happened. For tests and for the residency diagnostics.
        [[nodiscard]] T* Get(std::string_view contentName, LoadOutcome& outcome)
        {
            const std::string key(contentName);
            if (const auto it = loaded_.find(key); it != loaded_.end())
            {
                outcome = LoadOutcome::Cached;
                return it->second.get();
            }
            if (failed_.contains(key))
            {
                // Already known bad. Returning the fallback WITHOUT logging again is the "logs once"
                // half of the policy: this path runs every frame the room is drawn.
                outcome = LoadOutcome::FellBack;
                return Fallback();
            }

            try
            {
                auto asset = std::make_unique<T>(loader_(*content_, key));
                T* raw = asset.get();
                loaded_.emplace(key, std::move(asset));
                outcome = LoadOutcome::Loaded;
                return raw;
            }
            catch (const std::exception& e)
            {
                // The XNA content boundary throws (`ContentLoadException`), and this is where it stops
                // (`docs/conventions.md` §5.2). One message, naming the asset and the reason.
                failed_.insert(key);
                util::Log::Error(
                    category_, "content '{}' did not load, using the fallback: {}", key, e.what());
                outcome = LoadOutcome::FellBack;
                return Fallback();
            }
        }

        /// @brief The fallback itself, loaded on first use.
        ///
        /// If even the fallback cannot load, this returns null and says so once -- a build whose
        /// content tree does not exist at all is a legitimate state (a fresh clone), and it must not
        /// crash in the failure handler.
        [[nodiscard]] T* Fallback()
        {
            if (fallback_ != nullptr)
            {
                return fallback_.get();
            }
            if (fallbackFailed_ || fallbackName_.empty())
            {
                return nullptr;
            }
            try
            {
                fallback_ = std::make_unique<T>(loader_(*content_, fallbackName_));
            }
            catch (const std::exception& e)
            {
                fallbackFailed_ = true;
                util::Log::Error(category_,
                                 "the fallback asset '{}' did not load either, so this cache has "
                                 "nothing to return: {}",
                                 fallbackName_,
                                 e.what());
                return nullptr;
            }
            return fallback_.get();
        }

        [[nodiscard]] bool IsLoaded(std::string_view contentName) const
        {
            return loaded_.contains(std::string(contentName));
        }

        [[nodiscard]] bool HasFailed(std::string_view contentName) const
        {
            return failed_.contains(std::string(contentName));
        }

        [[nodiscard]] std::size_t LoadedCount() const noexcept
        {
            return loaded_.size();
        }

        [[nodiscard]] std::size_t FailedCount() const noexcept
        {
            return failed_.size();
        }

        /// @brief Drops one asset, for the residency system's eviction.
        void Evict(std::string_view contentName)
        {
            loaded_.erase(std::string(contentName));
        }

        /// @brief Drops everything, including the record of what failed.
        ///
        /// Clearing the failure record is deliberate: after a content rebuild an asset that was missing
        /// may now be present, and a cache that remembered forever would keep showing the fallback.
        void Clear()
        {
            loaded_.clear();
            failed_.clear();
        }

    private:
        Microsoft::Xna::Framework::Content::ContentManager* content_ = nullptr;
        Loader loader_;
        std::string fallbackName_;
        util::LogCat category_ = util::LogCat::Content;

        std::unordered_map<std::string, std::unique_ptr<T>> loaded_;
        std::unordered_set<std::string> failed_;
        std::unique_ptr<T> fallback_;
        bool fallbackFailed_ = false;
    };

} // namespace cnahouse::content
