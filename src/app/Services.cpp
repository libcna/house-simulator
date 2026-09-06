// SPDX-License-Identifier: MIT
#include "cnahouse/app/Services.hpp"

#include "cnahouse/util/Log.hpp"

namespace cnahouse::app
{

    Services::~Services()
    {
        // Reverse of construction, always. A service destroyed before something that holds a pointer
        // to it is a use-after-free at shutdown, which is the hardest kind to reproduce because it
        // happens after the last thing anyone was watching.
        for (auto it = entries_.rbegin(); it != entries_.rend(); ++it)
        {
            if (it->deleter != nullptr)
            {
                it->deleter(it->instance);
            }
        }
        entries_.clear();
        byType_.clear();
    }

    void Services::Register(std::type_index key, void* instance, Entry::Deleter deleter)
    {
        if (const auto existing = byType_.find(key); existing != byType_.end())
        {
            // Registering twice is a wiring mistake, and silently replacing would leave the first
            // instance leaked and every pointer to it dangling. The second registration is refused and
            // said out loud.
            util::Log::Error(util::LogCat::App,
                             "Services: '{}' is already registered at position {}; the second "
                             "registration is ignored",
                             key.name(),
                             existing->second);
            if (deleter != nullptr)
            {
                deleter(instance);
            }
            return;
        }
        byType_.emplace(key, entries_.size());
        entries_.push_back(Entry{key, instance, deleter});
    }

    void* Services::Find(std::type_index key) const
    {
        const auto it = byType_.find(key);
        return it == byType_.end() ? nullptr : entries_[it->second].instance;
    }

    std::size_t Services::IndexOf(std::type_index key) const
    {
        const auto it = byType_.find(key);
        return it == byType_.end() ? kNotRegistered : it->second;
    }

    std::vector<std::string> Services::ConstructionOrder() const
    {
        std::vector<std::string> names;
        names.reserve(entries_.size());
        for (const Entry& entry : entries_)
        {
            names.emplace_back(entry.key.name());
        }
        return names;
    }

} // namespace cnahouse::app
