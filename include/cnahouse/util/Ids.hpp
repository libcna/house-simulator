// SPDX-License-Identifier: MIT
#pragma once

#include <cstdint>
#include <string>
#include <string_view>

namespace cnahouse::util
{

    /// @brief An interned string id: a stable 32-bit hash of a name, comparable at compile time.
    ///
    /// The house is data (`CLAUDE.md` §3), so rooms, portals, props and interactables are addressed by
    /// **name** in JSON and by **id** at runtime. `Id` is that bridge. It is a value type the size of a
    /// `std::uint32_t`, so it goes in a map key, a packed struct or a comparison without a thought.
    ///
    /// **Collisions are fatal, not improbable.** A 32-bit hash over a few thousand ids has a real
    /// birthday-problem chance of a collision, and a silent one would make two rooms the same room --
    /// a bug that would look like world-data corruption and be nearly impossible to find. So every id
    /// created from a runtime string is registered, and registering a different string under an
    /// existing hash is reported as a `Conflict`. `HOUSE-00026`'s acceptance requires exactly this.
    class Id
    {
    public:
        constexpr Id() noexcept = default;

        constexpr explicit Id(std::uint32_t value) noexcept
            : value_(value)
        {
        }

        /// @brief Hashes a name at compile time. Does NOT register it -- see `Intern`.
        ///
        /// This is the form for a literal in code (`Id::Of("L0_KITCHEN")`), which is comparable in a
        /// `switch`-like chain and costs nothing at run time. Names that come from data go through
        /// `Intern`, which registers them and can therefore detect a collision.
        [[nodiscard]] static constexpr Id Of(std::string_view name) noexcept
        {
            return Id(Hash(name));
        }

        [[nodiscard]] constexpr std::uint32_t Value() const noexcept
        {
            return value_;
        }

        [[nodiscard]] constexpr bool IsValid() const noexcept
        {
            return value_ != 0;
        }

        constexpr bool operator==(const Id& other) const noexcept = default;
        constexpr auto operator<=>(const Id& other) const noexcept = default;

        /// @brief FNV-1a, 32-bit.
        ///
        /// Chosen because it is trivially `constexpr`, is stable across compilers and platforms -- an
        /// id in a save file must mean the same thing on Linux and in a browser -- and has no seed to
        /// accidentally differ between two builds of the same game.
        [[nodiscard]] static constexpr std::uint32_t Hash(std::string_view name) noexcept
        {
            std::uint32_t hash = 2166136261u;
            for (const char c : name)
            {
                hash ^= static_cast<std::uint32_t>(static_cast<unsigned char>(c));
                hash *= 16777619u;
            }
            // 0 is reserved for "no id", so a name that hashes there is nudged. One name in four
            // billion takes this path and it costs nothing to be correct about it.
            return hash == 0u ? 1u : hash;
        }

    private:
        std::uint32_t value_ = 0;
    };

    /// @brief The registry backing `Intern`: the collision check and the debug reverse map.
    class IdRegistry
    {
    public:
        /// @brief Interns a name, registering it so collisions can be detected.
        ///
        /// @return the id. If a *different* name already occupies this hash, the collision is reported
        ///         through `LastConflict` and the id is still returned -- the caller decides whether to
        ///         make it fatal, and the world loader does (`HOUSE-00026`'s acceptance).
        [[nodiscard]] static Id Intern(std::string_view name);

        /// @brief Whether the most recent `Intern` collided, and with what.
        [[nodiscard]] static bool HadConflict() noexcept;
        [[nodiscard]] static const std::string& ConflictExisting() noexcept;
        [[nodiscard]] static const std::string& ConflictIncoming() noexcept;
        static void ClearConflict() noexcept;

        /// @brief The name an id was interned from, or an empty view if it was never interned.
        ///
        /// Present in every build, not only debug ones: a log line naming `0x9a3f21c4` instead of
        /// `L0_KITCHEN` is a log line nobody can act on, and the map costs a few hundred kilobytes for
        /// a house with a few thousand ids.
        [[nodiscard]] static std::string_view NameOf(Id id);

        [[nodiscard]] static std::size_t Size() noexcept;
        static void ResetForTesting();
    };

    /// @brief Interns @p name and returns its id.
    [[nodiscard]] inline Id Intern(std::string_view name)
    {
        return IdRegistry::Intern(name);
    }

} // namespace cnahouse::util

template<>
struct std::hash<cnahouse::util::Id>
{
    [[nodiscard]] std::size_t operator()(const cnahouse::util::Id& id) const noexcept
    {
        return static_cast<std::size_t>(id.Value());
    }
};
