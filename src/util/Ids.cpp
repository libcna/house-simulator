// SPDX-License-Identifier: MIT
#include "cnahouse/util/Ids.hpp"

#include <unordered_map>

namespace cnahouse::util
{
    namespace
    {

        struct Registry
        {
            std::unordered_map<std::uint32_t, std::string> names;
            bool hadConflict = false;
            std::string conflictExisting;
            std::string conflictIncoming;
        };

        Registry& Get()
        {
            static Registry registry;
            return registry;
        }

    } // namespace

    Id IdRegistry::Intern(std::string_view name)
    {
        Registry& registry = Get();
        const Id id = Id::Of(name);

        const auto [it, inserted] = registry.names.try_emplace(id.Value(), name);
        if (!inserted && it->second != name)
        {
            // A real collision: two different names, one hash. Recorded rather than thrown, because
            // the caller that knows which file the name came from is the one that can produce a
            // message worth reading -- and because a registry that throws cannot be unit-tested for
            // this behaviour without exception plumbing in every caller.
            registry.hadConflict = true;
            registry.conflictExisting = it->second;
            registry.conflictIncoming = name;
        }
        return id;
    }

    bool IdRegistry::HadConflict() noexcept
    {
        return Get().hadConflict;
    }

    const std::string& IdRegistry::ConflictExisting() noexcept
    {
        return Get().conflictExisting;
    }

    const std::string& IdRegistry::ConflictIncoming() noexcept
    {
        return Get().conflictIncoming;
    }

    void IdRegistry::ClearConflict() noexcept
    {
        Registry& registry = Get();
        registry.hadConflict = false;
        registry.conflictExisting.clear();
        registry.conflictIncoming.clear();
    }

    std::string_view IdRegistry::NameOf(Id id)
    {
        const Registry& registry = Get();
        const auto it = registry.names.find(id.Value());
        return it == registry.names.end() ? std::string_view{} : std::string_view{it->second};
    }

    std::size_t IdRegistry::Size() noexcept
    {
        return Get().names.size();
    }

    void IdRegistry::ResetForTesting()
    {
        Registry& registry = Get();
        registry.names.clear();
        ClearConflict();
    }

} // namespace cnahouse::util
