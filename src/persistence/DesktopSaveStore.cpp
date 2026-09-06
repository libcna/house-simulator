// SPDX-License-Identifier: MIT
#include "cnahouse/persistence/DesktopSaveStore.hpp"

#include <format>
#include <vector>

#include "Microsoft/Xna/Framework/Storage/StorageContainer.hpp"
#include "Microsoft/Xna/Framework/Storage/StorageDevice.hpp"
#include "System/IAsyncResult.hpp"
#include "System/IO/FileMode.hpp"
#include "System/IO/Stream.hpp"

#include "cnahouse/util/Log.hpp"

namespace cnahouse::persistence
{
    namespace Storage = Microsoft::Xna::Framework::Storage;

    class DesktopSaveStore::Impl
    {
    public:
        std::unique_ptr<Storage::StorageDevice> device;
        std::unique_ptr<Storage::StorageContainer> container;
    };

    DesktopSaveStore::DesktopSaveStore(std::unique_ptr<Impl> impl)
        : impl_(std::move(impl))
    {
    }

    DesktopSaveStore::~DesktopSaveStore() = default;

    std::string DesktopSaveStore::TempName(std::string_view name)
    {
        return std::string(name) + ".tmp";
    }

    std::string DesktopSaveStore::BackupName(std::string_view name)
    {
        return std::string(name) + ".bak";
    }

    util::Result<std::unique_ptr<DesktopSaveStore>> DesktopSaveStore::Open()
    {
        auto impl = std::make_unique<Impl>();
        try
        {
            // MEASURED (`HOUSE-00102`): `BeginShowSelector`/`EndShowSelector` complete synchronously
            // with no UI on desktop, so this is not the asynchronous dance the XNA 360 API implies.
            auto deviceAsync = Storage::StorageDevice::BeginShowSelector(nullptr, nullptr);
            impl->device = Storage::StorageDevice::EndShowSelector(deviceAsync.get());
            if (impl->device == nullptr || !impl->device->getIsConnectedProperty())
            {
                return util::Err(util::ErrorCode::IoFailure,
                                 "no storage device is available for saving",
                                 "DesktopSaveStore::Open");
            }
            auto containerAsync = impl->device->BeginOpenContainer(kContainerName, nullptr, nullptr);
            impl->container = impl->device->EndOpenContainer(containerAsync.get());
            if (impl->container == nullptr)
            {
                return util::Err(util::ErrorCode::IoFailure,
                                 std::format("the storage container '{}' did not open", kContainerName),
                                 "DesktopSaveStore::Open");
            }
        }
        catch (const std::exception& e)
        {
            return util::Err(util::ErrorCode::IoFailure, e.what(), "DesktopSaveStore::Open");
        }

        auto store = std::unique_ptr<DesktopSaveStore>(new DesktopSaveStore(std::move(impl)));
        util::Log::Info(util::LogCat::Persistence, "saves are in {}", store->Location());
        return store;
    }

    util::Result<void> DesktopSaveStore::Write(std::string_view name, std::string_view contents)
    {
        const std::string temp = TempName(name);
        const std::string backup = BackupName(name);
        try
        {
            // ADR-0008's sequence, in order. A crash between any two steps loses the NEWEST state and
            // can never produce a half-written save -- which is the failure that matters, because a
            // half-written save parses far enough to look valid and then puts the house in a state the
            // player cannot recover from.
            //
            // 1. write the whole new save to a temporary name
            {
                auto stream = impl_->container->CreateFile(temp);
                stream->Write(reinterpret_cast<const std::uint8_t*>(contents.data()),
                              0,
                              static_cast<int>(contents.size()));
                stream->Flush();
            }
            // 2. the current save becomes the backup, so there is always one good file on disk
            if (impl_->container->FileExists(std::string(name)))
            {
                const std::string previous = *Read(name);
                {
                    auto stream = impl_->container->CreateFile(backup);
                    stream->Write(reinterpret_cast<const std::uint8_t*>(previous.data()),
                                  0,
                                  static_cast<int>(previous.size()));
                    stream->Flush();
                }
                impl_->container->DeleteFile(std::string(name));
            }
            // 3. the temporary becomes the save
            {
                auto stream = impl_->container->CreateFile(std::string(name));
                stream->Write(reinterpret_cast<const std::uint8_t*>(contents.data()),
                              0,
                              static_cast<int>(contents.size()));
                stream->Flush();
            }
            impl_->container->DeleteFile(temp);
        }
        catch (const std::exception& e)
        {
            return util::Err(util::ErrorCode::IoFailure, e.what(), std::string(name));
        }
        return {};
    }

    namespace
    {
        util::Result<std::string> ReadFile(Storage::StorageContainer& container, const std::string& name)
        {
            if (!container.FileExists(name))
            {
                return util::Err(util::ErrorCode::NotFound, "the save does not exist", name);
            }
            try
            {
                auto stream = container.OpenFile(name, System::IO::FileMode::Open);
                std::string out;
                std::vector<std::uint8_t> buffer(8192);
                for (;;)
                {
                    const auto read = stream->Read(buffer.data(), 0, static_cast<int>(buffer.size()));
                    if (read <= 0)
                    {
                        break;
                    }
                    out.append(reinterpret_cast<const char*>(buffer.data()), static_cast<size_t>(read));
                }
                return out;
            }
            catch (const std::exception& e)
            {
                return util::Err(util::ErrorCode::IoFailure, e.what(), name);
            }
        }
    } // namespace

    util::Result<std::string> DesktopSaveStore::Read(std::string_view name)
    {
        return ReadFile(*impl_->container, std::string(name));
    }

    util::Result<std::string> DesktopSaveStore::ReadBackup(std::string_view name)
    {
        return ReadFile(*impl_->container, BackupName(name));
    }

    bool DesktopSaveStore::Exists(std::string_view name)
    {
        return impl_->container->FileExists(std::string(name));
    }

    util::Result<void> DesktopSaveStore::Delete(std::string_view name)
    {
        try
        {
            if (impl_->container->FileExists(std::string(name)))
            {
                impl_->container->DeleteFile(std::string(name));
            }
        }
        catch (const std::exception& e)
        {
            return util::Err(util::ErrorCode::IoFailure, e.what(), std::string(name));
        }
        return {};
    }

    util::Result<std::vector<std::string>> DesktopSaveStore::List()
    {
        try
        {
            return impl_->container->GetFileNames();
        }
        catch (const std::exception& e)
        {
            return util::Err(util::ErrorCode::IoFailure, e.what(), "DesktopSaveStore::List");
        }
    }

    std::string DesktopSaveStore::Location() const
    {
        // MEASURED (`HOUSE-00102`): the resolved root is `$XDG_DATA_HOME/game/<container>` or
        // `~/.local/share/game/<container>` -- `game` literally, because the only way to change that
        // component is `SetAppNameEXT`, which ADR-0001 forbids. The CONTAINER name is what identifies
        // this game, and it is plain XNA.
        const char* xdg = std::getenv("XDG_DATA_HOME");
        const char* home = std::getenv("HOME");
        const std::string base = (xdg != nullptr && *xdg != '\0')
                                     ? std::string(xdg)
                                     : std::string(home != nullptr ? home : "?") + "/.local/share";
        return std::format("{}/game/{}", base, kContainerName);
    }

} // namespace cnahouse::persistence
