// SPDX-License-Identifier: MIT
#pragma once

#include <memory>

#include "cnahouse/persistence/ISaveStore.hpp"

namespace Microsoft::Xna::Framework::Storage
{
    class StorageContainer;
    class StorageDevice;
} // namespace Microsoft::Xna::Framework::Storage

namespace cnahouse::persistence
{

    /// @brief The desktop save store, over `StorageDevice`/`StorageContainer`.
    ///
    /// **The container name is how `cna-house` identifies itself, and that is a measured decision.**
    /// `HOUSE-00102` found that `StorageDevice`'s `<app>` path component is the literal string `game`
    /// unless `SetAppNameEXT` is called -- and that call is `CNAEXT`, which ADR-0001 forbids. So an
    /// XNA-only game's saves land under a directory shared with every other CNA application. The
    /// container name *is* under our control through plain XNA, so `CnaHouse` is what distinguishes
    /// them, giving `~/.local/share/game/CnaHouse/`.
    ///
    /// **No `std::filesystem`.** `cna-house.md` §8.3 permits it only inside this file's implementation,
    /// and even here it is not needed: `StorageContainer` provides create, open, exists, delete and
    /// list, which is the whole interface.
    class DesktopSaveStore final : public ISaveStore
    {
    public:
        /// @brief The container every save of this game lives in.
        static constexpr const char* kContainerName = "CnaHouse";

        /// @brief Opens the container. Fails rather than throwing if the device is unavailable.
        [[nodiscard]] static util::Result<std::unique_ptr<DesktopSaveStore>> Open();

        ~DesktopSaveStore() override;

        [[nodiscard]] util::Result<void> Write(std::string_view name, std::string_view contents) override;
        [[nodiscard]] util::Result<std::string> Read(std::string_view name) override;
        [[nodiscard]] util::Result<std::string> ReadBackup(std::string_view name) override;
        [[nodiscard]] bool Exists(std::string_view name) override;
        [[nodiscard]] util::Result<void> Delete(std::string_view name) override;
        [[nodiscard]] util::Result<std::vector<std::string>> List() override;
        [[nodiscard]] std::string Location() const override;

        /// @brief `<name>.tmp` and `<name>.bak`, exposed so a test can assert the atomic sequence.
        [[nodiscard]] static std::string TempName(std::string_view name);
        [[nodiscard]] static std::string BackupName(std::string_view name);

    private:
        class Impl;
        explicit DesktopSaveStore(std::unique_ptr<Impl> impl);
        std::unique_ptr<Impl> impl_;
    };

} // namespace cnahouse::persistence
