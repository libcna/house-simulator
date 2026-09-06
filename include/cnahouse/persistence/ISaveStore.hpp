// SPDX-License-Identifier: MIT
#pragma once

#include <string>
#include <string_view>
#include <vector>

#include "cnahouse/util/Result.hpp"

namespace cnahouse::persistence
{

    /// @brief Where saves live. One interface, two implementations, so the Web port is a swap.
    ///
    /// ADR-0008: the desktop implementation is `StorageDevice`/`StorageContainer`; the Web port swaps
    /// in `System::IO::IsolatedStorage` (IndexedDB-backed) behind this same interface. Everything above
    /// this line -- the delta format, the checksum, the migration chain -- is shared, which is the
    /// whole point of the seam.
    ///
    /// The interface is **text in, text out**. Saves are JSON by ADR-0008 ("readable by a human, which
    /// is what makes 'the fridge was open when I quit' a five-second investigation rather than a hex
    /// dump"), and a byte-oriented interface would invite someone to put a struct through it.
    class ISaveStore
    {
    public:
        virtual ~ISaveStore() = default;

        /// @brief Writes @p contents to @p name, atomically.
        ///
        /// **Atomic is a requirement, not an aspiration.** A crash mid-save may lose the newest state;
        /// it must never produce a half-written save, because a half-written save is worse than none --
        /// it parses far enough to look valid and then puts the house in a state the player cannot
        /// recover from. ADR-0008 fixes the sequence: write `<name>.tmp`, flush, rename over `<name>`,
        /// keep `<name>.bak`.
        [[nodiscard]] virtual util::Result<void> Write(std::string_view name, std::string_view contents) = 0;

        [[nodiscard]] virtual util::Result<std::string> Read(std::string_view name) = 0;

        /// @brief Reads the backup written by the previous `Write`.
        ///
        /// The backup exists precisely for the case where the current save fails its checksum, so
        /// reading it has to be a first-class operation rather than a caller assembling a filename.
        [[nodiscard]] virtual util::Result<std::string> ReadBackup(std::string_view name) = 0;

        [[nodiscard]] virtual bool Exists(std::string_view name) = 0;
        [[nodiscard]] virtual util::Result<void> Delete(std::string_view name) = 0;
        [[nodiscard]] virtual util::Result<std::vector<std::string>> List() = 0;

        /// @brief A human-readable description of where saves are, for the log and for support.
        [[nodiscard]] virtual std::string Location() const = 0;
    };

} // namespace cnahouse::persistence
