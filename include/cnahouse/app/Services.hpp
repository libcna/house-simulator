// SPDX-License-Identifier: MIT
#pragma once

#include <cstddef>
#include <memory>
#include <string>
#include <string_view>
#include <typeindex>
#include <unordered_map>
#include <vector>

#include "cnahouse/app/ISystem.hpp"
#include "cnahouse/util/Result.hpp"

namespace cnahouse::app
{

    /// @brief The typed service container: constructs every system in dependency order, destroys in
    ///        reverse, and refuses a resolution that would read a service constructed after the
    ///        resolver.
    ///
    /// **Why not a plain struct of members.** ADR-0006 chose composition over an ECS, and a struct of
    /// members is the obvious composition -- but it gives no place to state the *order*, and order is
    /// the thing that goes wrong. Sixty systems constructed in declaration order, one of which quietly
    /// reads another that has not been built yet, is a null-dereference at startup that reorders
    /// itself every time someone adds a field.
    ///
    /// So construction is explicit and sequenced, and `Resolve` **fails** if the requested service was
    /// registered after the one asking. That is `HOUSE-00128`'s second acceptance point, and it makes
    /// the ordering constraint a runtime error at the moment it is violated rather than a crash later.
    ///
    /// The container holds `void*` behind `std::type_index`, which is the small amount of type erasure
    /// that lets `Resolve<T>()` be one call. It does not own construction policy, lifetimes beyond
    /// "reverse of construction", or lazy instantiation -- all three are places a container becomes a
    /// framework, and this one is deliberately not one.
    class Services
    {
    public:
        Services() = default;
        ~Services();

        Services(const Services&) = delete;
        Services& operator=(const Services&) = delete;

        /// @brief Constructs and registers a service, recording its construction index.
        ///
        /// @return a reference to the constructed service, so a caller can wire it immediately.
        template<typename T, typename... Args>
        T& Add(Args&&... args)
        {
            auto owned = std::make_unique<T>(std::forward<Args>(args)...);
            T* raw = owned.get();
            Register(
                std::type_index(typeid(T)), raw, Entry::Deleter{[](void* p) { delete static_cast<T*>(p); }});
            owned.release();
            return *raw;
        }

        /// @brief Registers an already-constructed service the container does NOT own.
        ///
        /// For the few objects XNA owns -- `GraphicsDevice`, `ContentManager` -- which have to be
        /// resolvable but must not be destroyed by this container.
        template<typename T>
        void AddBorrowed(T& instance)
        {
            Register(std::type_index(typeid(T)), &instance, Entry::Deleter{});
        }

        /// @brief The service, or a `NotFound` error naming what was asked for.
        ///
        /// A `Result` rather than a throw or a null: a missing service is a wiring mistake the caller
        /// can report precisely, and the caller is always `Bootstrap`, which is the one place that
        /// knows what it was trying to build.
        template<typename T>
        [[nodiscard]] util::Result<T*> Resolve()
        {
            void* found = Find(std::type_index(typeid(T)));
            if (found == nullptr)
            {
                return util::Err(util::ErrorCode::NotFound,
                                 std::string("no service of type '") + typeid(T).name() +
                                     "' has been registered",
                                 "Services::Resolve");
            }
            return static_cast<T*>(found);
        }

        /// @brief `Resolve<T>()` for a service that must exist, from a service constructed later.
        ///
        /// Fails if @p T was registered AFTER the service identified by @p resolverIndex, which is the
        /// ordering violation `HOUSE-00128` requires to be impossible. A system holding a pointer to
        /// something built after it is the bug this catches.
        template<typename T>
        [[nodiscard]] util::Result<T*> ResolveFrom(std::size_t resolverIndex)
        {
            const std::type_index key(typeid(T));
            const std::size_t index = IndexOf(key);
            if (index == kNotRegistered)
            {
                return util::Err(util::ErrorCode::NotFound,
                                 std::string("no service of type '") + typeid(T).name() +
                                     "' has been registered",
                                 "Services::ResolveFrom");
            }
            if (index > resolverIndex)
            {
                return util::Err(util::ErrorCode::InvalidData,
                                 std::string("'") + typeid(T).name() + "' is constructed at position " +
                                     std::to_string(index) + ", after the service at position " +
                                     std::to_string(resolverIndex) +
                                     " that is asking for it; move it earlier in Bootstrap",
                                 "Services::ResolveFrom");
            }
            return static_cast<T*>(entries_[index].instance);
        }

        /// @brief How many services have been registered. Also the next construction index.
        [[nodiscard]] std::size_t Count() const noexcept
        {
            return entries_.size();
        }

        /// @brief The construction position of @p T, for `ResolveFrom`.
        template<typename T>
        [[nodiscard]] std::size_t IndexOfService() const
        {
            return IndexOf(std::type_index(typeid(T)));
        }

        /// @brief The names of every registered service, in construction order. For diagnostics.
        [[nodiscard]] std::vector<std::string> ConstructionOrder() const;

        static constexpr std::size_t kNotRegistered = static_cast<std::size_t>(-1);

    private:
        struct Entry
        {
            using Deleter = void (*)(void*);
            std::type_index key;
            void* instance = nullptr;
            /// Null for a borrowed service the container must not destroy.
            Deleter deleter = nullptr;
        };

        void Register(std::type_index key, void* instance, Entry::Deleter deleter);
        [[nodiscard]] void* Find(std::type_index key) const;
        [[nodiscard]] std::size_t IndexOf(std::type_index key) const;

        std::vector<Entry> entries_;
        std::unordered_map<std::type_index, std::size_t> byType_;
    };

} // namespace cnahouse::app
