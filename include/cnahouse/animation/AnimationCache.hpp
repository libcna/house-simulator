// SPDX-License-Identifier: MIT
#pragma once

#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>

#include "cnahouse/animation/Animation.hpp"
#include "cnahouse/util/Result.hpp"

namespace cnahouse::anim
{

    /// @brief Loads each `.chanim` once and keeps it.
    ///
    /// **Deliberately NOT a `content::AssetCache`**, and the difference is not the loader signature.
    /// `AssetCache` exists to substitute a fallback: a missing texture becomes neutral grey and the
    /// room still reads. **There is no fallback skeleton.** A character whose skeleton did not load
    /// cannot be drawn at all, and substituting one would produce a silently wrong deformation --
    /// the exact failure `HOUSE-00074` and `BindTo` exist to prevent. So this cache returns an error
    /// and the caller decides, rather than handing back something plausible.
    ///
    /// It still keeps `AssetCache`'s "log once" property: a path that failed is remembered, so a
    /// per-frame retry cannot fill the log with one message per frame.
    class AnimationCache
    {
    public:
        /// @param root the directory `.chanim` files live under, e.g. `content/Anim`.
        explicit AnimationCache(std::string root)
            : root_(std::move(root))
        {
        }

        /// @brief The library for @p name (without the extension), loading it the first time.
        [[nodiscard]] util::Result<ClipLibrary*> Get(std::string_view name);

        /// @brief Whether @p name is already loaded. Does not load it.
        [[nodiscard]] bool IsLoaded(std::string_view name) const;

        /// @brief How many libraries are resident.
        [[nodiscard]] std::size_t Count() const noexcept
        {
            return loaded_.size();
        }

        /// @brief How many distinct names have failed. A residency diagnostic, not a counter of
        ///        attempts -- a name that failed is only attempted once.
        [[nodiscard]] std::size_t FailedCount() const noexcept
        {
            return failed_.size();
        }

        void Clear();

        /// @brief The path `Get` would open for @p name.
        [[nodiscard]] std::string PathFor(std::string_view name) const;

    private:
        std::string root_;
        std::unordered_map<std::string, std::unique_ptr<ClipLibrary>> loaded_;
        std::unordered_map<std::string, util::Error> failed_;
    };

} // namespace cnahouse::anim
