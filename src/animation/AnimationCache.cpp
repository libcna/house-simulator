// SPDX-License-Identifier: MIT
#include "cnahouse/animation/AnimationCache.hpp"

#include <format>

#include "cnahouse/animation/ChanimReader.hpp"
#include "cnahouse/util/Log.hpp"

namespace cnahouse::anim
{

    std::string AnimationCache::PathFor(std::string_view name) const
    {
        return std::format("{}/{}.chanim", root_, name);
    }

    bool AnimationCache::IsLoaded(std::string_view name) const
    {
        return loaded_.contains(std::string(name));
    }

    util::Result<ClipLibrary*> AnimationCache::Get(std::string_view name)
    {
        const std::string key(name);
        if (const auto it = loaded_.find(key); it != loaded_.end())
        {
            return it->second.get();
        }
        if (const auto it = failed_.find(key); it != failed_.end())
        {
            // Remembered, and NOT logged again. This path runs every frame the character is drawn,
            // and one message per frame is how a log stops being read.
            return it->second;
        }

        auto library = ChanimReader::ReadFromTitle(PathFor(name));
        if (!library)
        {
            util::Log::Error(
                util::LogCat::Content, "animation '{}' did not load: {}", key, library.Error().ToString());
            failed_.emplace(key, library.Error());
            return library.Error();
        }

        auto owned = std::make_unique<ClipLibrary>(std::move(*library));
        ClipLibrary* raw = owned.get();
        loaded_.emplace(key, std::move(owned));
        return raw;
    }

    void AnimationCache::Clear()
    {
        loaded_.clear();
        // The failure set is cleared TOO. `Clear` is what a content reload calls, and the point of a
        // reload is to try again -- keeping the failures would make a fixed asset stay broken until
        // the process restarted.
        failed_.clear();
    }

} // namespace cnahouse::anim
